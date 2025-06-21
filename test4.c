#include <stdio.h>
#include <setjmp.h>
#include <signal.h>
#include <string.h>

//3��SEH���쳣���˺�������ֵ
#define EXCEPTION_EXECUTE_HANDLER       1
#define EXCEPTION_CONTINUE_SEARCH       0
#define EXCEPTION_CONTINUE_EXECUTION    -1

//��Ȼ���쳣��ص����ݶ������ˣ����Ƕ���һ���ṹ���ڴ洢�쳣����ص���Ϣ
typedef struct _EXCEPTION_NODE
{
	struct _EXCEPTION_NODE *Prev;//�쳣�������
	int                    RunStatus;//ǰ�������������״̬(0��������л�����¼��1���쳣�޷��޸���ת����������У�2���쳣��Ϣ��ע��ɹ�����ת���쳣���������
	jmp_buf                SectionEntry;//�����쳣����������Լ����л���
	int                    (*FilterRoutine)(int iSignal,siginfo_t *pSignalInfo,void *pContext);//�쳣���˺���ָ��
}EXCEPTION_NODE,*PEXCEPTION_NODE;


static struct _SEH_SET
{
	PEXCEPTION_NODE  Chain;
	struct sigaction SignalHandler,OldHandler;
	int              Initialized;
}s_Maintain={0};//SEH�����

static void _ExceptionHandler(int iSignal,siginfo_t *pSignalInfo,void *pContext)
{ //�쳣���������ڱ�������ֱ����ת���쳣��������ʼλ�ã�������setjmp/longjmp������������ת���쳣������������
	int             iResult;
	PEXCEPTION_NODE pEntry,pPrev;

	printf("    Got SIGSEGV at address: %lXH, %p\n",(long) pSignalInfo->si_addr,pContext);
	pEntry=s_Maintain.Chain;//׼������ע����쳣�����
	do
	{
		iResult=pEntry->FilterRoutine(iSignal,pSignalInfo,pContext);//����ǰ�쳣����쳣���˺���
		switch(iResult)
		{
		case EXCEPTION_EXECUTE_HANDLER://��ǰ�쳣�޷������ִ���ƺ�������
			s_Maintain.Chain=pEntry->Prev;//ע����ǰ�쳣�����
			siglongjmp(pEntry->SectionEntry,1);//��ת��ע���쳣����ƺ��������
			break;
		case EXCEPTION_CONTINUE_SEARCH://ע���쳣���޷�����ǰ�쳣��������һ���쳣����
			pPrev=pEntry->Prev;
			s_Maintain.Chain=pEntry->Prev;//��Ȼ��Ҫ����һ���쳣�鴦����ô��ǰ�쳣���Ѿ������ˣ�ע����ǰ�쳣��
			pEntry=pPrev;//ת����һ���쳣�����
			break;
		case EXCEPTION_CONTINUE_EXECUTION://�쳣�޸���ɣ������쳣��������������
			return;
			break;
		default://�쳣���˳��򷵻��˴����ֵ
			printf("Bad exception filter result %d\n",iResult);
			break;
		}
	}while(pEntry);

	printf("    No more handler\n");
	sigaction(SIGSEGV,&s_Maintain.OldHandler,NULL);
}

int StartSEHService(void)
{ //�쳣�����ʼ��������Ӧ�ó�������֮������
	s_Maintain.SignalHandler.sa_sigaction=_ExceptionHandler;
	if(-1==sigaction(SIGSEGV,&s_Maintain.SignalHandler,&s_Maintain.OldHandler))
	{ //ע���Լ����쳣������
		perror("Register sigaction fail");
		return 0;
	}
	else
	{
		s_Maintain.Initialized=1;
		return 1;
	}
}

//�쳣���˺���
int ExceptionFilter(int iSignal,siginfo_t *pSignalInfo,void *pContext)
{
	printf("Exception occur! We cannot fix it now.\n");
	return EXCEPTION_EXECUTE_HANDLER;//����������ǿ�Ʒ������ֵ
}

int main(void)
{
	char sz[]="Exception critical section & exception clean up section was run in THE SAME FUNCTION, I show the same information as proof.";//����չʾ�쳣�������쳣��ӵ����ͬ�����Ŀʹ�
	printf("--------------Virtual SEH Test Start--------------\n");
	printf("  +Enter critical section @ %s %d\n",__FILE__,__LINE__); //�����쳣������

	/* TRY_START */
	{
		EXCEPTION_NODE __weLees_ExceptionNode;
		__weLees_ExceptionNode.Prev=NULL;
		if(!s_Maintain.Initialized)
		{
			StartSEHService();
		}
		__weLees_ExceptionNode.Prev=s_Maintain.Chain;
		s_Maintain.Chain=&__weLees_ExceptionNode;
		__weLees_ExceptionNode.RunStatus=sigsetjmp(__weLees_ExceptionNode.SectionEntry,1); //���浱ǰ����λ���Լ�������Ϣ
		if(2==__weLees_ExceptionNode.RunStatus)
			/* TRY_START end */
		{ //setjmp����2��ʾ�Ѿ�������쳣��������ע�Ṥ�����������п��ܲ����쳣�ĸ�Σ����
			//------------------------------- Exception monitor block start -------------------------------
			printf("Hello, we go into exception monitor section\n  This is the local string :'%s'\n",sz);
			printf("  Let's do some bad thing\n");
			*((char*)0)=0;
			//------------------------------- Exception monitor block end -------------------------------
			printf("  -Leave critical section @ %s %d\n",__FILE__,__LINE__); //��һ����ʵ��������
			s_Maintain.Chain=__weLees_ExceptionNode.Prev;//ע���쳣��
		}
		/* TRY_EXCEPT(filter) */
		else if(!__weLees_ExceptionNode.RunStatus)
		{ //setjmp����0��ʾ�ո�����˻������湤������ʱ������Ҫע���쳣��������
			printf("  *Register exception filter @ %s %d\n",__FILE__,__LINE__);
			__weLees_ExceptionNode.FilterRoutine=ExceptionFilter;//ע���쳣���˺���
			siglongjmp(__weLees_ExceptionNode.SectionEntry,2); //��ת���쳣���ӿ鿪ʼ��
		}
		else//��0/2��ʾ�Ǵ�ϵͳ�쳣��������е�longjmp������ת�������ڱ����б����������쳣���޷��ָ����У������ƺ���
			/* TRY_EXCEPT(filter) end */
		{ 
			//------------------------------- Exception clean block start -------------------------------
			printf("  Exception occur! do clean work\n  and we can access local data in the same environment of exception critical section:\n'%s'\n",sz);
			//------------------------------- Exception clean block end -------------------------------
		}
		/* TRY_END */
	}
	/* TRY_END end */

	sigaction(SIGSEGV,&s_Maintain.OldHandler,&s_Maintain.OldHandler); //�ָ�ԭ���쳣������
}
