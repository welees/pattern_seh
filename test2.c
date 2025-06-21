#include <stdio.h>
#include <setjmp.h>
#include <signal.h>
#include <string.h>

jmp_buf SectionEntry; //���ڱ����쳣����������Լ����л���
struct sigaction SignalHandler; //���ڱ��浱ǰ�쳣�����¼
struct sigaction OldHandler; //���ڱ�����쳣�����¼

static void _ExceptionHandler(int iSignal,siginfo_t *pSignalInfo,void *pContext)
//�쳣���������ڱ�������ֱ����ת���쳣��������ʼλ�ã�������longjmp������ת���쳣������������
{
	printf("    Got SIGSEGV at address: %lXH, %p\n",(long) pSignalInfo->si_addr,pContext);
	siglongjmp(SectionEntry,1); //��ת����¼���쳣���ӿ鿪ʼ��
}

int StartSEHService(void)
//�쳣�����ʼ��������Ӧ�ó�������֮������
{
	SignalHandler.sa_sigaction=_ExceptionHandler;//ָ�����Ƕ�����쳣������
	if(-1==sigaction(SIGSEGV,&SignalHandler,&OldHandler)) //ע���Լ����쳣������������ԭ�����쳣������
	{
		perror("Register sigaction fail");
		return 0;
	}
	else
	{
		return 1;
	}
}

int main(void)
{
	char sz[]="Exception critical section & exception clean up section was run in THE SAME FUNCTION, I show the same information as proof.";//����չʾ�쳣�������쳣��ӵ����ͬ�����Ŀʹ�
	StartSEHService();//��ʼ���쳣��������ڳ���ʼʱ����һ��
	printf("--------------Virtual SEH Test Start--------------\n");
	{
		printf("  +Enter critical section @ %s %d\n",__FILE__,__LINE__); //�����쳣������
		if(!sigsetjmp(SectionEntry,1)) //���浱ǰ����λ���Լ�������Ϣ
		{ //setjmp����0��ʾ�ո�����˻������湤�����������п��ܲ����쳣�Ĵ���
			//------------------------------- Exception monitor block start -------------------------------
			printf("Hello, we go into exception monitor section\n  This is the local string :'%s'\n",sz);
			printf("  Let's do some bad thing\n");
			*((char*)0)=0;//�����쳣
			//------------------------------- Exception monitor block end -------------------------------
			printf("  -Leave critical section @ %s %d\n",__FILE__,__LINE__); //��һ����ʵ��������
		}
		else
		{ //��0��ʾ�Ǵ�longjmp��������ת����(��ע���ϵͳ�쳣�����������������longjmp�Ĳ�����1)���ڱ����б����������쳣���޷��ָ����У�ִ�л���������
			//------------------------------- Exception clean block start -------------------------------
			printf("  Exception occur! do clean work\n  and we can access local data in the same environment of exception critical section:\n'%s'\n",sz);
			//------------------------------- Exception clean block end -------------------------------
		}
	}
	
	sigaction(SIGSEGV,&OldHandler,&OldHandler); //����������ָ�ԭ���쳣������
}
