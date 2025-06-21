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
	jmp_buf SectionEntry;//�����쳣����������Լ����л���
	int (*FilterRoutine)(int iSignal,siginfo_t *pSignalInfo,void *pContext);//�쳣���˺���ָ��
}EXCEPTION_NODE,*PEXCEPTION_NODE;

EXCEPTION_NODE ExceptionNode;//�쳣��������Ϣ��
struct sigaction SignalHandler; //��ǰ�쳣�����
struct sigaction OldHandler; //���쳣�����

static void _ExceptionHandler(int iSignal,siginfo_t *pSignalInfo,void *pContext)
{ //�쳣���������ڱ�������ֱ����ת���쳣��������ʼλ�ã�������setjmp/longjmp������������ת���쳣������������
	printf("    Got SIGSEGV at address: %lXH, %p\n",(long) pSignalInfo->si_addr,pContext);
	ExceptionNode.FilterRoutine(iSignal,pSignalInfo,pContext);
	siglongjmp(ExceptionNode.SectionEntry,1); //��ת����¼���쳣���ӿ鿪ʼ��
}

int StartSEHService(void)
{ //�쳣�����ʼ��������Ӧ�ó�������֮������
	SignalHandler.sa_sigaction=_ExceptionHandler;
	if(-1==sigaction(SIGSEGV,&SignalHandler,&OldHandler))
	{ //ע���Լ����쳣������
		perror("Register sigaction fail");
		return 0;
	}
	else
	{
		return 1;
	}
}

//�쳣���˺���
int ExceptionFilter(int iSignal,siginfo_t *pSignalInfo,void *pContext)
{
	printf("Exception occur! We cannot fix it now.\n");
	return EXCEPTION_EXECUTE_HANDLER;//����������ǿ�Ʒ������ֵ
}

#define TRY_START \
	int iResult; \
	iResult=sigsetjmp(ExceptionNode.SectionEntry,1); /*���浱ǰ����λ���Լ�������Ϣ*/ \
	if(2==iResult) \
	/*setjmp����2��ʾ�Ѿ�������쳣��������ע�Ṥ�����������п��ܲ����쳣�ĸ�Σ����*/

#define TRY_EXCEPT(filter) \
	else if(!iResult) \
{ /*setjmp����0��ʾ�ո�����˻������湤������ʱ������Ҫע���쳣��������*/ \
	printf("  *Register exception filter @ %s %d\n",__FILE__,__LINE__); \
	ExceptionNode.FilterRoutine=filter;/*ע���쳣���˺���*/ \
	siglongjmp(ExceptionNode.SectionEntry,2); /*��ת���쳣���ӿ鿪ʼ��*/ \
} \
	else \
	/*��0/2��ʾ�Ǵ�ϵͳ�쳣��������е�longjmp������ת�������ڱ����б����������쳣���޷��ָ����У������ƺ���*/

int main(void)
{
	char sz[]="Exception critical section & exception clean up section was run in THE SAME FUNCTION, I show the same information as proof.";//����չʾ�쳣�������쳣��ӵ����ͬ�����Ŀʹ�
	StartSEHService();
	printf("--------------Virtual SEH Test Start--------------\n");
	TRY_START
	{
		printf("  +Enter critical section @ %s %d\n",__FILE__,__LINE__); //�����쳣������
		//------------------------------- Exception monitor block start -------------------------------
		printf("Hello, we go into exception monitor section\n  This is the local string :'%s'\n",sz);
		printf("  Let's do some bad thing\n");
		*((char*)0)=0;
		//------------------------------- Exception monitor block end -------------------------------
		printf("  -Leave critical section @ %s %d\n",__FILE__,__LINE__); //��һ����ʵ��������
	}
	TRY_EXCEPT(ExceptionFilter)
	{
		//------------------------------- Exception clean block start -------------------------------
		printf("  Exception occur! do clean work\n  and we can access local data in the same environment of exception critical section:\n'%s'\n",sz);
		//------------------------------- Exception clean block end -------------------------------
	}

	sigaction(SIGSEGV,&OldHandler,&OldHandler); //�ָ�ԭ���쳣������
}
