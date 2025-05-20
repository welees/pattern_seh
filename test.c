#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <setjmp.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#if !defined(_WIN32)
#include <sys/syscall.h>
#else //!defined(_WIN32)
#include <windows.h>
#endif //!defined(_WIN32)
#include <stdbool.h>

typedef unsigned long long UINT64;

#define IN
#define OUT
#define TRUE  1
#define FALSE 0

typedef unsigned int UINT,*PUINT;
#include <SEH.h>


UINT  g_uData;
PUINT g_p=NULL;


#ifdef _MINGW
#define GetCurrentThreadID   (unsigned long long)GetCurrentThreadId
#endif //_MINGW
#ifdef _MACOS
#define GetCurrentThreadID() (unsigned long long)pthread_self()
#endif //_MACOS
#ifdef _LINUX
#define GetCurrentThreadID() (unsigned long long)syscall(SYS_gettid)
#endif //_LINUX


/*
The exception filter, where the user should check whether the exception state can be repaired and instruct SEH how to perform the next action.
*/
#if defined(_MINGW)
int ExceptionFilter2(IN PEXCEPTION_POINTERS pContext)
#else //defined(_MINGW)
int ExceptionFilter2(IN int iSignal,IN siginfo_t *pSignalInfo,IN void *pContext)
#endif //defined(_MINGW)
{
	static int iCount=0;
	
	printf("  Enter %s %p for thread %llXH\n",__FUNCTION__,ExceptionFilter2,GetCurrentThreadID());
	if(!iCount)
	{
		//p=&a;
		printf("    Filter 2, We assume the exception was fixed and retry\n");
		iCount++;
		return EXCEPTION_CONTINUE_EXECUTION; //Exception was fixed, retry
	}
	else if(iCount==1)
	{
		printf("    Filter 2, Cannot fix point, go to handler\n");
		iCount++;
		return EXCEPTION_EXECUTE_HANDLER; //Exception can not be fixed, run except handler do some clean up work
	}
	else
	{
		printf("    Filter 2, Cannot handle, go previous\n");
		return EXCEPTION_CONTINUE_SEARCH; //Exception can not be fixed and go previous level to clean up
	}
}


void Test(void)
{
	TRY_START
	{
		printf("Step 4.9, start work in main thread, write point %p\n",g_p);
		*g_p=0;
		printf("Step 4.10 yes! exception was fixed\n");
	}
	TRY_EXCEPT(ExceptionFilter2)
	{
		printf("Step 4.11 fail, clean up\n");
	}
	TRY_END
}


int ExceptionFilter1
#if defined(_MINGW)
(IN PEXCEPTION_POINTERS pContext)
#else //defined(_MINGW)
(IN int iSignal,IN siginfo_t *pSignalInfo,IN void *pContext)
#endif //defined(_MINGW)
{
	static int iCount=0;
	
	printf("  Enter %s %p\n",__FUNCTION__,ExceptionFilter1);
	if(!iCount)
	{
		//p=&a;
		printf("    Filter 1,Fix point and retry\n");
		iCount++;
		return EXCEPTION_CONTINUE_EXECUTION;
	}
	else if(iCount==1)
	{
		printf("    Filter 1,Cannot fix point, go to handler\n");
		iCount++;
		return EXCEPTION_EXECUTE_HANDLER;
	}
	else
	{
		printf("    Filter 1,Cannot handle, search previous\n");
		return EXCEPTION_CONTINUE_SEARCH;
	}
}


int volatile g_iThreadRun=0;


int ThreadExceptionFilter
#if defined(_MINGW)
(IN PEXCEPTION_POINTERS pContext)
#else //defined(_MINGW)
(IN int iSignal,IN siginfo_t *pSignalInfo,IN void *pContext)
#endif //defined(_MINGW)
{
	printf("  Enter %s %p for thread %llXH\n",__FUNCTION__,ThreadExceptionFilter,GetCurrentThreadID());
	printf("  Cannot fix, do clean up work for thread %llXH\n",GetCurrentThreadID());
	return EXCEPTION_EXECUTE_HANDLER;
}


void* ThreadProc(IN void *pParameter)
{
	printf("  Step 4.2, Work thread %llXH\n",pthread_self());
	g_iThreadRun=1;
	TRY_START
	{
		printf("  Step 4.3, occur exception in exception\n");
		g_p=NULL;
		*g_p=0;
		printf("  Step 4.4, yes we've fixed current exception, you should not watch this message\n");
	}
	TRY_EXCEPT(ThreadExceptionFilter)
	{
		printf("  Step 4.5, do exception clean up work in thread\n");
	}
	TRY_END
	
	return NULL;
}



int ThreadExceptionFilter1
#if defined(_MINGW)
(IN PEXCEPTION_POINTERS pContext)
#else //defined(_MINGW)
(IN int iSignal,IN siginfo_t *pSignalInfo,IN void *pContext)
#endif //defined(_MINGW)
{
	static int iCount=0;

	printf("  Enter %s %p\n",__FUNCTION__,ExceptionFilter1);
	if(!iCount)
	{
		//p=&a;
		printf("    Filter 1,Fix point and retry\n");
		iCount++;
		return EXCEPTION_CONTINUE_EXECUTION;
	}
	else if(iCount==1)
	{
		printf("    Filter 1,Cannot fix point, go to handler\n");
		iCount++;
		return EXCEPTION_EXECUTE_HANDLER;
	}
	else
	{
		printf("    Filter 1,Cannot handle, search previous\n");
		return EXCEPTION_CONTINUE_SEARCH;
	}
}


void SEHThreadTest(void)
{
	pthread_t id;
	printf("\n%s %d--------------------------------------------\nMultiple thread Exception test\n",__FILE__,__LINE__);
	g_p=NULL;
	TRY_START
	{
		printf("Step 4.1, create new thread\n");
		pthread_create(&id,NULL,ThreadProc,NULL);
		while(!g_iThreadRun);
		
		printf("Step 4.6, main thread works well\n");
	}
	TRY_EXCEPT(ExceptionFilter1)
	{
		printf("Step 4.7 fail in main thread, clean up, you should not watch this message\n");
	}
	TRY_END
	
	g_p=NULL;
	TRY_START
	{
		printf("Step 4.8, write point %p\n",g_p);
		Test();
		printf("Step 4.12, yes! we pass\n");
	}
	TRY_EXCEPT(ThreadExceptionFilter1)
	{
		printf("Step 4.13 handler\n");
	}
	TRY_END
	
	printf("  press ENTER to continue ...\n");
	getchar();
}


int ExceptionFilterCleanUp
#if defined(_MINGW)
	(IN PEXCEPTION_POINTERS pContext)
#else //defined(_MINGW)
	(IN int iSignal,IN siginfo_t *pSignalInfo,IN void *pContext)
#endif //defined(_MINGW)
{
	printf("  Enter exception filter %s %p\n",__FUNCTION__,ExceptionFilterCleanUp);
	printf("    Current exception can not be handled, do clean work\n\n");
	return EXCEPTION_EXECUTE_HANDLER;
}


int ExceptionFilterCleanUp2
#if defined(_MINGW)
(IN PEXCEPTION_POINTERS pContext)
#else //defined(_MINGW)
(IN int iSignal,IN siginfo_t *pSignalInfo,IN void *pContext)
#endif //defined(_MINGW)
{
	printf("  Enter exception filter %s %p\n",__FUNCTION__,ExceptionFilterCleanUp2);
	printf("    2Current exception can not be handled, do clean work\n\n");
	return EXCEPTION_EXECUTE_HANDLER;
}


int ExceptionFilterFixed 
#if defined(_MINGW)
(IN PEXCEPTION_POINTERS pContext)
#else //defined(_MINGW)
(IN int iSignal,IN siginfo_t *pSignalInfo,IN void *pContext)
#endif //defined(_MINGW)
{
	printf("  Enter exception filter %s %p\n",__FUNCTION__,ExceptionFilterFixed);
	printf("    Modify pointer to some data and retry(as debugger)\n\n");
#ifdef _MINGW
	pContext->ContextRecord->Eax=(unsigned int)&g_uData;
#endif //_MINGW
	g_p=&g_uData;
	return EXCEPTION_CONTINUE_EXECUTION;
}


int ExceptionFilterContSearch
#if defined(_MINGW)
(IN PEXCEPTION_POINTERS pContext)
#else //defined(_MINGW)
(IN int iSignal,IN siginfo_t *pSignalInfo,IN void *pContext)
#endif //defined(_MINGW)
{
	printf("  Enter exception filter %s %p\n",__FUNCTION__,ExceptionFilterFixed);
	printf("    Fail to find method to fix exception, try outer filter\n\n");
	return EXCEPTION_CONTINUE_SEARCH;
}


int ExceptionFilterBreakCleanUp
#if defined(_MINGW)
(IN PEXCEPTION_POINTERS pContext)
#else //defined(_MINGW)
(IN int iSignal,IN siginfo_t *pSignalInfo,IN void *pContext)
#endif //defined(_MINGW)
{
	printf("  Enter exception filter %s %p\n",__FUNCTION__,ExceptionFilterCleanUp);
	printf("    Current exception can not be handled, do clean work\n\n");
	return EXCEPTION_EXECUTE_HANDLER;
}


void SEHTestSimple(void)
{
	printf("\n%s %d--------------------------------------------\nSimple exception test1\n  Fail to fix exception\n\n",__FILE__,__LINE__);
	g_p=NULL;
	TRY_START
	{
		printf("  Step 1.1, write point %p\n",g_p);
		*g_p=0;
		printf("  Step 1.2, you can not watch this message\n");
	}
	TRY_EXCEPT(ExceptionFilterCleanUp)
	{
		printf("  Step 1.3, Cannot fix current exception, do clean up work\n");
	}
	TRY_END
	
	printf("  press ENTER to continue ...\n");
	getchar();

#if defined(_WIN32)&&(!(defined(_WIN64)))
	printf("\n%s %d--------------------------------------------\nSimple exception test2\n  Fix and re-run exception, for Windows & IA32 only\n\n",__FILE__,__LINE__);
	g_p=NULL;
	TRY_START
	{
		printf("  Step 1.4 write point %p\n",g_p);
		*g_p=123;
		printf("  Step 1.5, yes! we've fixed the exception, the data was write to g_uData, it is %XH\n",g_uData);
	}
	TRY_EXCEPT(ExceptionFilterFixed)
	{
		printf("  Step 1.6 you cannot watch this message\n");
	}
	TRY_END
	
	printf("  press ENTER to continue ...\n");
	getchar();
#endif //defined(_WIN32)&&(!(defined(_WIN64)))
}


void NestInnerFunc(void)
{
	printf("  Step 2.14,enter inner function\n");
	TRY_START
	{
		printf("  Step 2.15, write point %p\n",g_p);
		*g_p=0x12345;
		printf("  Step 2.16, you cannot watch this message\n");
	}
	TRY_EXCEPT(ExceptionFilterContSearch)
	{
		printf("  Step 2.17, you cannot watch this message\n");
	}
	TRY_END
}


void SEHNestedTest(void)
{
	printf("\n%s %d--------------------------------------------\nNested Exception test\n",__FILE__,__LINE__);
	printf("  Step 2.1, enter top level exception section\n");
	TRY_START
	{
		printf("    Step 2.2, top level user code start\n");
		TRY_START
		{
			printf("      Step 2.3, middle level user code start\n");
			TRY_START
			{
				printf("        Step 2.4, low level user code start, we occur an exception\n");
				*((PUINT)0)=0;
				printf("        Step 2.5, low level user code, you should not watch this message\n");
			}
			TRY_EXCEPT( ExceptionFilterBreakCleanUp)
			{
				printf("          Step 2.6, low level exception clean up routine is working\n");
				printf("            Let's occur new exception in exception clean up routine >_<.\n");
				printf("            press ENTER to continue ...\n");
				getchar();
				*((PUINT)0)=0;
				printf("          Step 2.7, you should not watch this message\n");
			}
			TRY_END
		}
		TRY_EXCEPT(ExceptionFilterBreakCleanUp)
		{
			printf("        Step 2.8, middle level exception clean up routine is working\n");
			printf("          Let's occur new exception in exception clean up routine, AGAIN *_*\n");
			printf("          press ENTER to continue ...\n");
			getchar();
			*((PUINT)0)=0;
			printf("        Step 2.9, you should not watch this message\n");
		}
		TRY_END
	}
	TRY_EXCEPT(ExceptionFilterBreakCleanUp)
	{
		printf("      Step 2.10, yes!, the top level exception clean up routine is working now\n");
		printf("        Let's occur new exception in exception clean up routine...\n");
		printf("        No.... we have to obey the rule for next test\n");
	}
	TRY_END
	printf("  Step 2.11, leave top level exception section\n    And nest exception test finish, please press ENTER to continue...\n");
	getchar();
}


void SEHBreakTest(void)
{
	int i;
	
	printf("\n%s %d--------------------------------------------\nCircle break test\n",__FILE__,__LINE__);
	TRY_START
	{
		printf("  Step 3.1, entry outer critical section\n");
		printf("  Step 3.2, run 20 circles\n");
		for(i=0;i<20;i++)
		{
			printf("    Step 3.3, Do circle work, round %d\n",i);
			TRY_START
			{
				if(i>5)
				{
					TRY_BREAK;
				}
			}
			TRY_EXCEPT(ExceptionFilterBreakCleanUp)
			{
				printf("    Step 3.4, you should not watch this message\n");
			}
			TRY_END
			sleep(1);
		}
		printf("    Step 3.5, we use break to leave the circle, now let's occur an exception\n");
		printf("      press ENTER to continue...\n");
		getchar();
		*((PUINT)0)=0;
	}
	TRY_EXCEPT(ExceptionFilterBreakCleanUp)
	{
		printf("  Step 3.6, yes! we are in exception clean up rotuine for outer exception section!\n");
	}
	TRY_END
	
	printf("  press ENTER to continue...\n");
	getchar();
}


void ExceptionTest(void)
{
	int i;
	
	printf("Test start\n\n");
	SEHTestSimple();
	SEHNestedTest();
	SEHBreakTest();
	SEHThreadTest();
}


int main(int argc, char *argv[])
{
	StartSEHService();
	ExceptionTest();
	StopSEHService();
}
