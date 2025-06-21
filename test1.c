#include <stdio.h>
#include <setjmp.h>
#include <signal.h>
#include <string.h>

jmp_buf SectionEntry; //用于保存异常监视区入口以及运行环境
struct sigaction SignalHandler; //用于保存当前异常处理记录
struct sigaction OldHandler; //用于保存旧异常处理记录

static void _ExceptionHandler(int iSignal,siginfo_t *pSignalInfo,void *pContext)
//异常处理函数，在本例中是直接跳转到异常监视区开始位置，并利用longjmp函数跳转到异常环境回收例程
{
	printf("    Got SIGSEGV at address: %lXH, %p\n",(long) pSignalInfo->si_addr,pContext);
	siglongjmp(SectionEntry,1); //跳转到记录的异常监视块开始处
}

int StartSEHService(void)
//异常处理初始化函数，应该程序启动之后运行
{
	SignalHandler.sa_sigaction=_ExceptionHandler;//指向我们定义的异常处理函数
	if(-1==sigaction(SIGSEGV,&SignalHandler,&OldHandler)) //注册自己的异常处理函数并保存原来的异常处理函数
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
	char sz[]="Exception critical section & exception clean up section was run in THE SAME FUNCTION, I show the same information as proof.";//用于展示异常清理块跟异常颢块拥有相同环境的客串
	StartSEHService();//初始化异常处理服务，在程序开始时调用一次
	printf("--------------Virtual SEH Test Start--------------\n");
	{
		printf("  +Enter critical section @ %s %d\n",__FILE__,__LINE__); //进入异常监视区
		if(!sigsetjmp(SectionEntry,1)) //保存当前运行位置以及环境信息
		{ //setjmp返回0表示刚刚完成了环境保存工作，下面运行可能产生异常的代码
			//------------------------------- Exception monitor block start -------------------------------
			printf("Hello, we go into exception monitor section\n  This is the local string :'%s'\n",sz);
			printf("  Let's do some bad thing\n");
			*((char*)0)=0;//产生异常
			//------------------------------- Exception monitor block end -------------------------------
			printf("  -Leave critical section @ %s %d\n",__FILE__,__LINE__); //这一行其实不会运行
		}
		else
		{ //非0表示是从longjmp函数中跳转过来(在注册的系统异常处理程序中我们设置longjmp的参数是1)，在本例中表明发生了异常并无法恢复运行，执行环境清理工作
			//------------------------------- Exception clean block start -------------------------------
			printf("  Exception occur! do clean work\n  and we can access local data in the same environment of exception critical section:\n'%s'\n",sz);
			//------------------------------- Exception clean block end -------------------------------
		}
	}
	
	sigaction(SIGSEGV,&OldHandler,&OldHandler); //程序结束，恢复原有异常处理函数
}
