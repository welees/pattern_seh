#include <stdio.h>
#include <setjmp.h>
#include <signal.h>
#include <string.h>

//SEH中异常过滤函数有3个返回值，这里我们先把它们的定义放上来
#define EXCEPTION_EXECUTE_HANDLER       1
#define EXCEPTION_CONTINUE_SEARCH       0
#define EXCEPTION_CONTINUE_EXECUTION    -1

//既然跟异常相关的数据多起来了，我们定义一个结构用于存储异常块相关的信息
typedef struct _EXCEPTION_NODE
{
	jmp_buf SectionEntry;//保存异常监视区入口以及运行环境
	int (*FilterRoutine)(int iSignal,siginfo_t *pSignalInfo,void *pContext);//异常过滤函数指针
}EXCEPTION_NODE,*PEXCEPTION_NODE;

EXCEPTION_NODE ExceptionNode;//异常监视区信息块
struct sigaction SignalHandler; //当前异常处理块
struct sigaction OldHandler; //旧异常处理块

static void _ExceptionHandler(int iSignal,siginfo_t *pSignalInfo,void *pContext)
{ //异常处理函数，在本例中是直接跳转到异常监视区开始位置，并利用setjmp/longjmp函数的特性跳转到异常环境回收例程
	printf("    Got SIGSEGV at address: %lXH, %p\n",(long) pSignalInfo->si_addr,pContext);
	ExceptionNode.FilterRoutine(iSignal,pSignalInfo,pContext);
	siglongjmp(ExceptionNode.SectionEntry,1); //跳转到记录的异常监视块开始处
}

int StartSEHService(void)
{ //异常处理初始化函数，应该程序启动之后运行
	SignalHandler.sa_sigaction=_ExceptionHandler;
	if(-1==sigaction(SIGSEGV,&SignalHandler,&OldHandler))
	{ //注册自己的异常处理函数
		perror("Register sigaction fail");
		return 0;
	}
	else
	{
		return 1;
	}
}

//异常过滤函数
int ExceptionFilter(int iSignal,siginfo_t *pSignalInfo,void *pContext)
{
	printf("Exception occur! We cannot fix it now.\n");
	return EXCEPTION_EXECUTE_HANDLER;//这里我们先强制返回这个值
}

#define TRY_START \
	int iResult; \
	iResult=sigsetjmp(ExceptionNode.SectionEntry,1); /*保存当前运行位置以及环境信息*/ \
	if(2==iResult) \
	/*setjmp返回2表示已经完成了异常过滤例程注册工作，下面运行可能产生异常的高危代码*/

#define TRY_EXCEPT(filter) \
	else if(!iResult) \
{ /*setjmp返回0表示刚刚完成了环境保存工作，此时我们需要注册异常过滤例程*/ \
	printf("  *Register exception filter @ %s %d\n",__FILE__,__LINE__); \
	ExceptionNode.FilterRoutine=filter;/*注册异常过滤函数*/ \
	siglongjmp(ExceptionNode.SectionEntry,2); /*跳转到异常监视块开始处*/ \
} \
	else \
	/*非0/2表示是从系统异常处理程序中的longjmp函数跳转过来，在本例中表明发生了异常并无法恢复运行，处理善后工作*/

int main(void)
{
	char sz[]="Exception critical section & exception clean up section was run in THE SAME FUNCTION, I show the same information as proof.";//用于展示异常清理块跟异常颢块拥有相同环境的客串
	StartSEHService();
	printf("--------------Virtual SEH Test Start--------------\n");
	TRY_START
	{
		printf("  +Enter critical section @ %s %d\n",__FILE__,__LINE__); //进入异常监视区
		//------------------------------- Exception monitor block start -------------------------------
		printf("Hello, we go into exception monitor section\n  This is the local string :'%s'\n",sz);
		printf("  Let's do some bad thing\n");
		*((char*)0)=0;
		//------------------------------- Exception monitor block end -------------------------------
		printf("  -Leave critical section @ %s %d\n",__FILE__,__LINE__); //这一行其实不会运行
	}
	TRY_EXCEPT(ExceptionFilter)
	{
		//------------------------------- Exception clean block start -------------------------------
		printf("  Exception occur! do clean work\n  and we can access local data in the same environment of exception critical section:\n'%s'\n",sz);
		//------------------------------- Exception clean block end -------------------------------
	}

	sigaction(SIGSEGV,&OldHandler,&OldHandler); //恢复原有异常处理函数
}
