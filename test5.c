#include <stdio.h>
#include <setjmp.h>
#include <signal.h>
#include <string.h>

//SEH���쳣���˺�����3������ֵ�����������Ȱ����ǵĶ��������
#define EXCEPTION_EXECUTE_HANDLER       1
#define EXCEPTION_CONTINUE_SEARCH       0
#define EXCEPTION_CONTINUE_EXECUTION    -1

//��Ȼ���쳣��ص����ݶ������ˣ����Ƕ���һ���ṹ���ڴ洢�쳣����ص���Ϣ
typedef struct _EXCEPTION_NODE
{
	struct _EXCEPTION_NODE *Prev;
	int                    RunStatus;
	jmp_buf                SectionEntry;//�����쳣����������Լ����л���
	int                    (*FilterRoutine)(int iSignal,siginfo_t *pSignalInfo,void *pContext);//�쳣���˺���ָ��
}EXCEPTION_NODE,*PEXCEPTION_NODE;


static __thread struct _SEH_SET
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
			s_Maintain.Chain=pEntry->Prev;//ע����ǰ�쳣��
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

#define TRY_START \
{ \
	EXCEPTION_NODE __weLees_ExceptionNode; \
	__weLees_ExceptionNode.Prev=NULL; \
	if(!s_Maintain.Initialized) \
	{ \
		StartSEHService(); \
	} \
	__weLees_ExceptionNode.Prev=s_Maintain.Chain; \
	s_Maintain.Chain=&__weLees_ExceptionNode; \
	__weLees_ExceptionNode.RunStatus=sigsetjmp(__weLees_ExceptionNode.SectionEntry,1); /*���浱ǰ����λ���Լ�������Ϣ*/ \
	if(2==__weLees_ExceptionNode.RunStatus) \
	{

#define TRY_EXCEPT(filter) \
		s_Maintain.Chain=__weLees_ExceptionNode.Prev; \
	} \
	else if(!__weLees_ExceptionNode.RunStatus) \
	{/*setjmp����0��ʾ�ո�����˻������湤������ʱ������Ҫע���쳣��������*/ \
		printf("  *Register exception filter @ %s %d\n",__FILE__,__LINE__); \
		__weLees_ExceptionNode.FilterRoutine=filter;/*ע���쳣���˺���*/ \
		siglongjmp(__weLees_ExceptionNode.SectionEntry,2);/*��ת���쳣���ӿ鿪ʼ��*/ \
	} \
	else/*��0/2��ʾ�Ǵ�ϵͳ�쳣��������е�longjmp������ת�������ڱ����б����������쳣���޷��ָ����У������ƺ���*/

#define TRY_END }

//�쳣���˺���
int ExceptionFilter1(int iSignal,siginfo_t *pSignalInfo,void *pContext)
{
	printf("Test1 : Exception occur! We cannot fix it now.\n");
	return EXCEPTION_EXECUTE_HANDLER;//����������ǿ�Ʒ������ֵ
}

void Test1(void)
{
	char sz[]="Exception critical section & exception clean up section was run in THE SAME FUNCTION, I show the same information as proof.";
	printf("--------------Virtual SEH Test1 : simple exception handling--------------\n");
	printf("  +Enter critical section @ %s %d\n",__FILE__,__LINE__); //�����쳣������

	TRY_START
	{ //setjmp����2��ʾ�Ѿ�������쳣��������ע�Ṥ�����������п��ܲ����쳣�ĸ�Σ����
		//------------------------------- Exception monitor block start -------------------------------
		printf("Test1 : Hello, we go into exception monitor section\n  This is the local string :'%s'\n",sz);
		printf("Test1 :   Let's do some bad thing\n");
		*((char*)0)=0;
		//------------------------------- Exception monitor block end -------------------------------
		printf("  -Leave critical section @ %s %d\n",__FILE__,__LINE__); //��һ����ʵ��������
	}
	TRY_EXCEPT(ExceptionFilter1)
	{
		//------------------------------- Exception clean block start -------------------------------
		printf("Test1 :   Exception occur! do clean work\n  and we can access local data in the same environment of exception critical section:\n'%s'\n",sz);
		//------------------------------- Exception clean block end -------------------------------
	}
	TRY_END
}

//�쳣���˺���
int ExceptionFilter2(int iSignal,siginfo_t *pSignalInfo,void *pContext)
{
	printf("Test2 : Exception occur! We cannot fix it now.\n");
	return EXCEPTION_EXECUTE_HANDLER;//����������ǿ�Ʒ������ֵ
}

void Test2(void)
{
	char sz[]="Exception critical section & exception clean up section was run in THE SAME FUNCTION, I show the same information as proof.";
	printf("\n--------------Virtual SEH Test2 : nested exception handling--------------\n");

	TRY_START
	{ //setjmp����2��ʾ�Ѿ�������쳣��������ע�Ṥ�����������п��ܲ����쳣�ĸ�Σ����
		//------------------------------- Exception monitor block start -------------------------------
		printf("Test2 : +Enter outer critical section @ %s %d\n",__FILE__,__LINE__); //�����쳣������
		TRY_START
		{ //setjmp����2��ʾ�Ѿ�������쳣��������ע�Ṥ�����������п��ܲ����쳣�ĸ�Σ����
			//------------------------------- Exception monitor block start -------------------------------
			printf("Test2 : +Enter inner critical section @ %s %d\n",__FILE__,__LINE__); //�����쳣������
			printf("Test2 : Hello, we go into exception monitor section\n  This is the local string :'%s'\n",sz);
			printf("Test2 :   Let's do some bad thing\n");
			*((char*)0)=0;
			//------------------------------- Exception monitor block end -------------------------------
			printf("  -Leave critical section @ %s %d\n",__FILE__,__LINE__); //��һ����ʵ��������
		}
		TRY_EXCEPT(ExceptionFilter2)
		{
			//------------------------------- Exception clean block start -------------------------------
			printf("Test2 :   Exception occur in INNER level! do clean work\n  and we can access local data in the same environment of exception critical section:\n'%s'\n",sz);
			//------------------------------- Exception clean block end -------------------------------
		}
		TRY_END
	}
	TRY_EXCEPT(ExceptionFilter2)
	{
		//------------------------------- Exception clean block start -------------------------------
		printf("Test2 :   Exception occur in outer level! do clean work\n  and we can access local data in the same environment of exception critical section:\n'%s'\n",sz);
		//------------------------------- Exception clean block end -------------------------------
	}
	TRY_END
}

//�쳣���˺���
int ExceptionFilter3(int iSignal,siginfo_t *pSignalInfo,void *pContext)
{
	printf("Test3 : Exception occur! We cannot fix it now.\n");
	return EXCEPTION_EXECUTE_HANDLER;//����������ǿ�Ʒ������ֵ
}

//�쳣���˺���
int ExceptionFilter3_1(int iSignal,siginfo_t *pSignalInfo,void *pContext)
{
	printf("Test3 : Exception occur! We cannot fix it now, try upper level fixing.\n");
	return EXCEPTION_CONTINUE_SEARCH;//����������ǿ�Ʒ������ֵ
}

void Test3(void)
{
	char sz[]="Exception critical section & exception clean up section was run in THE SAME FUNCTION, I show the same information as proof.";
	printf("\n--------------Virtual SEH Test3 : nested exception handling-try outer exception handler --------------\n");

	TRY_START
	{ //setjmp����2��ʾ�Ѿ�������쳣��������ע�Ṥ�����������п��ܲ����쳣�ĸ�Σ����
		//------------------------------- Exception monitor block start -------------------------------
		printf("Test3 : +Enter outer critical section @ %s %d\n",__FILE__,__LINE__); //�����쳣������
		TRY_START
		{ //setjmp����2��ʾ�Ѿ�������쳣��������ע�Ṥ�����������п��ܲ����쳣�ĸ�Σ����
			//------------------------------- Exception monitor block start -------------------------------
			printf("Test3 : +Enter inner critical section @ %s %d\n",__FILE__,__LINE__); //�����쳣������
			printf("Test3 : Hello, we go into exception monitor section\n  This is the local string :'%s'\n",sz);
			printf("Test3 :   Let's do some bad thing\n");
			*((char*)0)=0;
			//------------------------------- Exception monitor block end -------------------------------
			printf("  -Leave critical section @ %s %d\n",__FILE__,__LINE__); //��һ����ʵ��������
		}
		TRY_EXCEPT(ExceptionFilter3_1)
		{
			//------------------------------- Exception clean block start -------------------------------
			printf("Test3 :   Exception occur in INNER level! do clean work\n  and we can access local data in the same environment of exception critical section:\n'%s'\n",sz);
			//------------------------------- Exception clean block end -------------------------------
		}
		TRY_END
	}
	TRY_EXCEPT(ExceptionFilter3)
	{
		//------------------------------- Exception clean block start -------------------------------
		printf("Test3 :   Exception occur in outer level! do clean work\n  and we can access local data in the same environment of exception critical section:\n'%s'\n",sz);
		//------------------------------- Exception clean block end -------------------------------
	}
	TRY_END
}

int main(void)
{
	Test1();
	Test2();
	Test3();

	sigaction(SIGSEGV,&s_Maintain.OldHandler,&s_Maintain.OldHandler); //�ָ�ԭ���쳣������
}

