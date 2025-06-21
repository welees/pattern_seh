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
	struct _EXCEPTION_NODE *Prev;
	int                    RunStatus;
	jmp_buf                SectionEntry;//保存异常监视区入口以及运行环境
	int                    (*FilterRoutine)(int iSignal,siginfo_t *pSignalInfo,void *pContext);//异常过滤函数指针
}EXCEPTION_NODE,*PEXCEPTION_NODE;


static __thread struct _SEH_SET
{
	PEXCEPTION_NODE  Chain;
	struct sigaction SignalHandler,OldHandler;
	int              Initialized;
}s_Maintain={0};//SEH管理块

static void _ExceptionHandler(int iSignal,siginfo_t *pSignalInfo,void *pContext)
{ //异常处理函数，在本例中是直接跳转到异常监视区开始位置，并利用setjmp/longjmp函数的特性跳转到异常环境回收例程
	int             iResult;
	PEXCEPTION_NODE pEntry,pPrev;

	printf("    Got SIGSEGV at address: %lXH, %p\n",(long) pSignalInfo->si_addr,pContext);
	pEntry=s_Maintain.Chain;//准备遍历注册的异常处理块
	do
	{
		iResult=pEntry->FilterRoutine(iSignal,pSignalInfo,pContext);//调当前异常块的异常过滤函数
		switch(iResult)
		{
		case EXCEPTION_EXECUTE_HANDLER://当前异常无法解决，执行善后清理工作
			s_Maintain.Chain=pEntry->Prev;//注销当前异常块
			siglongjmp(pEntry->SectionEntry,1);//跳转到注册异常块的善后清理入口
			break;
		case EXCEPTION_CONTINUE_SEARCH://注册异常块无法处理当前异常，尝试上一级异常处理
			pPrev=pEntry->Prev;
			s_Maintain.Chain=pEntry->Prev;//既然需要到上一级异常块处理，那么当前异常块已经无用了，注销当前异常块
			pEntry=pPrev;//转到上一级异常处理块
			break;
		case EXCEPTION_CONTINUE_EXECUTION://异常修复完成，返回异常发生处继续运行
			return;
			break;
		default://异常过滤程序返回了错误的值
			printf("Bad exception filter result %d\n",iResult);
			break;
		}
	}while(pEntry);

	printf("    No more handler\n");
	sigaction(SIGSEGV,&s_Maintain.OldHandler,NULL);
}

int StartSEHService(void)
{ //异常处理初始化函数，应该程序启动之后运行
	s_Maintain.SignalHandler.sa_sigaction=_ExceptionHandler;
	if(-1==sigaction(SIGSEGV,&s_Maintain.SignalHandler,&s_Maintain.OldHandler))
	{ //注册自己的异常处理函数
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
	__weLees_ExceptionNode.RunStatus=sigsetjmp(__weLees_ExceptionNode.SectionEntry,1); /*保存当前运行位置以及环境信息*/ \
	if(2==__weLees_ExceptionNode.RunStatus) \
	{

#define TRY_EXCEPT(filter) \
		s_Maintain.Chain=__weLees_ExceptionNode.Prev; \
	} \
	else if(!__weLees_ExceptionNode.RunStatus) \
	{/*setjmp返回0表示刚刚完成了环境保存工作，此时我们需要注册异常过滤例程*/ \
		printf("  *Register exception filter @ %s %d\n",__FILE__,__LINE__); \
		__weLees_ExceptionNode.FilterRoutine=filter;/*注册异常过滤函数*/ \
		siglongjmp(__weLees_ExceptionNode.SectionEntry,2);/*跳转到异常监视块开始处*/ \
	} \
	else/*非0/2表示是从系统异常处理程序中的longjmp函数跳转过来，在本例中表明发生了异常并无法恢复运行，处理善后工作*/

#define TRY_END }

//异常过滤函数
int ExceptionFilter1(int iSignal,siginfo_t *pSignalInfo,void *pContext)
{
	printf("Test1 : Exception occur! We cannot fix it now.\n");
	return EXCEPTION_EXECUTE_HANDLER;//这里我们先强制返回这个值
}

void Test1(void)
{
	char sz[]="Exception critical section & exception clean up section was run in THE SAME FUNCTION, I show the same information as proof.";
	printf("--------------Virtual SEH Test1 : simple exception handling--------------\n");
	printf("  +Enter critical section @ %s %d\n",__FILE__,__LINE__); //进入异常监视区

	TRY_START
	{ //setjmp返回2表示已经完成了异常过滤例程注册工作，下面运行可能产生异常的高危代码
		//------------------------------- Exception monitor block start -------------------------------
		printf("Test1 : Hello, we go into exception monitor section\n  This is the local string :'%s'\n",sz);
		printf("Test1 :   Let's do some bad thing\n");
		*((char*)0)=0;
		//------------------------------- Exception monitor block end -------------------------------
		printf("  -Leave critical section @ %s %d\n",__FILE__,__LINE__); //这一行其实不会运行
	}
	TRY_EXCEPT(ExceptionFilter1)
	{
		//------------------------------- Exception clean block start -------------------------------
		printf("Test1 :   Exception occur! do clean work\n  and we can access local data in the same environment of exception critical section:\n'%s'\n",sz);
		//------------------------------- Exception clean block end -------------------------------
	}
	TRY_END
}

//异常过滤函数
int ExceptionFilter2(int iSignal,siginfo_t *pSignalInfo,void *pContext)
{
	printf("Test2 : Exception occur! We cannot fix it now.\n");
	return EXCEPTION_EXECUTE_HANDLER;//这里我们先强制返回这个值
}

void Test2(void)
{
	char sz[]="Exception critical section & exception clean up section was run in THE SAME FUNCTION, I show the same information as proof.";
	printf("\n--------------Virtual SEH Test2 : nested exception handling--------------\n");

	TRY_START
	{ //setjmp返回2表示已经完成了异常过滤例程注册工作，下面运行可能产生异常的高危代码
		//------------------------------- Exception monitor block start -------------------------------
		printf("Test2 : +Enter outer critical section @ %s %d\n",__FILE__,__LINE__); //进入异常监视区
		TRY_START
		{ //setjmp返回2表示已经完成了异常过滤例程注册工作，下面运行可能产生异常的高危代码
			//------------------------------- Exception monitor block start -------------------------------
			printf("Test2 : +Enter inner critical section @ %s %d\n",__FILE__,__LINE__); //进入异常监视区
			printf("Test2 : Hello, we go into exception monitor section\n  This is the local string :'%s'\n",sz);
			printf("Test2 :   Let's do some bad thing\n");
			*((char*)0)=0;
			//------------------------------- Exception monitor block end -------------------------------
			printf("  -Leave critical section @ %s %d\n",__FILE__,__LINE__); //这一行其实不会运行
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

//异常过滤函数
int ExceptionFilter3(int iSignal,siginfo_t *pSignalInfo,void *pContext)
{
	printf("Test3 : Exception occur! We cannot fix it now.\n");
	return EXCEPTION_EXECUTE_HANDLER;//这里我们先强制返回这个值
}

//异常过滤函数
int ExceptionFilter3_1(int iSignal,siginfo_t *pSignalInfo,void *pContext)
{
	printf("Test3 : Exception occur! We cannot fix it now, try upper level fixing.\n");
	return EXCEPTION_CONTINUE_SEARCH;//这里我们先强制返回这个值
}

void Test3(void)
{
	char sz[]="Exception critical section & exception clean up section was run in THE SAME FUNCTION, I show the same information as proof.";
	printf("\n--------------Virtual SEH Test3 : nested exception handling-try outer exception handler --------------\n");

	TRY_START
	{ //setjmp返回2表示已经完成了异常过滤例程注册工作，下面运行可能产生异常的高危代码
		//------------------------------- Exception monitor block start -------------------------------
		printf("Test3 : +Enter outer critical section @ %s %d\n",__FILE__,__LINE__); //进入异常监视区
		TRY_START
		{ //setjmp返回2表示已经完成了异常过滤例程注册工作，下面运行可能产生异常的高危代码
			//------------------------------- Exception monitor block start -------------------------------
			printf("Test3 : +Enter inner critical section @ %s %d\n",__FILE__,__LINE__); //进入异常监视区
			printf("Test3 : Hello, we go into exception monitor section\n  This is the local string :'%s'\n",sz);
			printf("Test3 :   Let's do some bad thing\n");
			*((char*)0)=0;
			//------------------------------- Exception monitor block end -------------------------------
			printf("  -Leave critical section @ %s %d\n",__FILE__,__LINE__); //这一行其实不会运行
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

	sigaction(SIGSEGV,&s_Maintain.OldHandler,&s_Maintain.OldHandler); //恢复原有异常处理函数
}

