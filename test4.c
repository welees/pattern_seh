#include <stdio.h>
#include <setjmp.h>
#include <signal.h>
#include <string.h>

//3种SEH中异常过滤函数返回值
#define EXCEPTION_EXECUTE_HANDLER       1
#define EXCEPTION_CONTINUE_SEARCH       0
#define EXCEPTION_CONTINUE_EXECUTION    -1

//既然跟异常相关的数据多起来了，我们定义一个结构用于存储异常块相关的信息
typedef struct _EXCEPTION_NODE
{
	struct _EXCEPTION_NODE *Prev;//异常处理块链
	int                    RunStatus;//前述例子里的运行状态(0：完成运行环境记录，1：异常无法修复，转到清理块运行，2：异常信息块注册成功，跳转到异常监测区运行
	jmp_buf                SectionEntry;//保存异常监视区入口以及运行环境
	int                    (*FilterRoutine)(int iSignal,siginfo_t *pSignalInfo,void *pContext);//异常过滤函数指针
}EXCEPTION_NODE,*PEXCEPTION_NODE;


static struct _SEH_SET
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
			s_Maintain.Chain=pEntry->Prev;//注销当前异常处理块
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

//异常过滤函数
int ExceptionFilter(int iSignal,siginfo_t *pSignalInfo,void *pContext)
{
	printf("Exception occur! We cannot fix it now.\n");
	return EXCEPTION_EXECUTE_HANDLER;//这里我们先强制返回这个值
}

int main(void)
{
	char sz[]="Exception critical section & exception clean up section was run in THE SAME FUNCTION, I show the same information as proof.";//用于展示异常清理块跟异常颢块拥有相同环境的客串
	printf("--------------Virtual SEH Test Start--------------\n");
	printf("  +Enter critical section @ %s %d\n",__FILE__,__LINE__); //进入异常监视区

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
		__weLees_ExceptionNode.RunStatus=sigsetjmp(__weLees_ExceptionNode.SectionEntry,1); //保存当前运行位置以及环境信息
		if(2==__weLees_ExceptionNode.RunStatus)
			/* TRY_START end */
		{ //setjmp返回2表示已经完成了异常过滤例程注册工作，下面运行可能产生异常的高危代码
			//------------------------------- Exception monitor block start -------------------------------
			printf("Hello, we go into exception monitor section\n  This is the local string :'%s'\n",sz);
			printf("  Let's do some bad thing\n");
			*((char*)0)=0;
			//------------------------------- Exception monitor block end -------------------------------
			printf("  -Leave critical section @ %s %d\n",__FILE__,__LINE__); //这一行其实不会运行
			s_Maintain.Chain=__weLees_ExceptionNode.Prev;//注销异常块
		}
		/* TRY_EXCEPT(filter) */
		else if(!__weLees_ExceptionNode.RunStatus)
		{ //setjmp返回0表示刚刚完成了环境保存工作，此时我们需要注册异常过滤例程
			printf("  *Register exception filter @ %s %d\n",__FILE__,__LINE__);
			__weLees_ExceptionNode.FilterRoutine=ExceptionFilter;//注册异常过滤函数
			siglongjmp(__weLees_ExceptionNode.SectionEntry,2); //跳转到异常监视块开始处
		}
		else//非0/2表示是从系统异常处理程序中的longjmp函数跳转过来，在本例中表明发生了异常并无法恢复运行，处理善后工作
			/* TRY_EXCEPT(filter) end */
		{ 
			//------------------------------- Exception clean block start -------------------------------
			printf("  Exception occur! do clean work\n  and we can access local data in the same environment of exception critical section:\n'%s'\n",sz);
			//------------------------------- Exception clean block end -------------------------------
		}
		/* TRY_END */
	}
	/* TRY_END end */

	sigaction(SIGSEGV,&s_Maintain.OldHandler,&s_Maintain.OldHandler); //恢复原有异常处理函数
}
