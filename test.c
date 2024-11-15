#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <setjmp.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <stdbool.h>
#include <SEH.h>


#ifdef Darwin
#define GetCurrentThreadID   pthread_self
#endif //Darwin
#ifdef Linux
#define GetCurrentThreadID() syscall(SYS_gettid)
#endif //Linux


char *p,a;


/*
The exception filter, where the user should check whether the exception state can be repaired and instruct SEH how to perform the next action.
*/
int ExceptionFilter2(IN int iSignal,IN siginfo_t *pSignalInfo,IN void *pContext)
{
	static int iCount=0;
	
	if(!iCount)
	{
		//p=&a;
		printf("    Filter 2,Fix point and retry\n");
		iCount++;
		return EXCEPTION_CONTINUE_EXECUTION; //Exception was fixed, retry
	}
	else if(iCount==1)
	{
		printf("    Filter 2,Cannot fix point, go to handler\n");
		iCount++;
		return EXCEPTION_EXECUTE_HANDLER; //Exception can not be fixed, run except handler do some clean up work
	}
	else
	{
		printf("    Filter 2,Cannot handle, go previous\n");
		return EXCEPTION_CONTINUE_SEARCH; //Exception can not be fixed and go previous level to clean up
	}
}


void Test(void)
{
	p=NULL;
	TRY_START
	{
		printf("Step 2.1 start, point %p\n",p);
		*p=0;
		printf("Step 2.2 yes!\n");
	}
	TRY_EXCEPT(ExceptionFilter2)
	{
		printf("Step 2.1 fail, clean up\n");
	}
	TRY_END
}


int ExceptionFilter1(IN int iSignal,IN siginfo_t *pSignalInfo,IN void *pContext)
{
	static int iCount=0;
	
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

volatile int iThreadRun=0;


int ThreadExceptionFilter(IN int iSignal,IN siginfo_t *pSignalInfo,IN void *pContext)
{
	static int iCount=0;

	if(!iCount)
	{
		//p=&a;
		printf("    Thread filter,Fix point and retry\n");
		iCount++;
		return EXCEPTION_CONTINUE_EXECUTION;
	}
	else if(iCount==1)
	{
		iCount++;
		printf("    Thread filter,Cannot handle, search previous\n");
		return EXCEPTION_CONTINUE_SEARCH;
	}
	else
	{
		iCount++;
		return EXCEPTION_EXECUTE_HANDLER;
	}
}


void* ThreadProc(IN void *pParameter)
{
	printf("Work thread %lXH\n",GetCurrentThreadID());
	iThreadRun=1;
	TRY_START
	{
		printf("multiple thread and multiple level try/except test\n");
		p=NULL;
		*p=0;
	}
	TRY_EXCEPT(ThreadExceptionFilter)
	{
		printf("multiple thread and multiple level fail, clean up\n");
	}
	TRY_END
	
	return NULL;
}


void ExceptionTest2(void)
{
	pthread_t id;
	printf("\n%s %d Multiple thread Exception test\n",__FILE__,__LINE__);
	p=NULL;
	TRY_START
	{
		printf("Step 3.1 write point %p\n",p);
		pthread_create(&id,NULL,ThreadProc,NULL);
		while(!iThreadRun);

		printf("Step 3.2, yes! we pass\n");
	}
	TRY_EXCEPT(ExceptionFilter1)
	{
		printf("Step 3.1 fail, clean up\n");
	}
	TRY_END
	
	p=NULL;
	TRY_START
	{
		printf("Step 3.3 write point %p\n",p);
		Test();
		printf("Step 3.4, yes! we pass\n");
	}
	TRY_EXCEPT(ExceptionFilter1)
	{
		p=&a;
		printf("Step 3.5 handler\n");
	}
	TRY_END
}


void ExceptionTest1(void)
{
	printf("%s %d Single Exception test\n",__FILE__,__LINE__);
	p=NULL;
	TRY_START
	{
		printf("Step 1.1 write point %p\n",p);
		Test();
		printf("Step 1.2, yes! we pass\n");
	}
	TRY_EXCEPT(ExceptionFilter1)
	{
		printf("Step 1.1 fail, clean up\n");
	}
	TRY_END

		p=NULL;
	TRY_START
	{
		printf("Step 1.3 write point %p\n",p);
		Test();
		printf("Step 1.4, yes! we pass\n");
	}
	TRY_EXCEPT(ExceptionFilter1)
	{
		p=&a;
		printf("Step 1.5 handler\n");
	}
	TRY_END
}


void ExceptionTest(void)
{
	ExceptionTest1();
	ExceptionTest2();
}

int main(int argc, char *argv[])
{
	StartSEHService();
	ExceptionTest();
	StopSEHService();
}
