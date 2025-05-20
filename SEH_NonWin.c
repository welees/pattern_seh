#if !defined(_WIN32)
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <stdbool.h>
#include <execinfo.h>
#include "SEH.h"


#define IN
#define OUT
#define TRUE  1
#define FALSE 0


typedef unsigned long long UINT64;


#define THREAD_CONTEXT __thread


#ifdef _DEBUG
int g_iID=1;
#endif //_DEBUG


#ifdef _MACOS
#define GetCurrentThreadID   (unsigned long long)pthread_self
#endif //_MACOS
#ifdef _LINUX
#define GetCurrentThreadID() (unsigned long long)syscall(SYS_gettid)
#endif //_LINUX

/*
#ifdef _MACOS
#define SPINLOCK             pthread_mutex_t
#define INIT_SPINLOCK(lock)  pthread_mutex_init(lock,NULL)
#define LOCK_SPINLOCK        pthread_mutex_lock
#define UNLOCK_SPINLOCK      pthread_mutex_unlock
#endif //_MACOS
#ifdef _LINUX
#define SPINLOCK             pthread_spinlock_t
#define INIT_SPINLOCK(lock)  pthread_spin_init(lock,PTHREAD_PROCESS_PRIVATE)
#define LOCK_SPINLOCK        pthread_spin_lock
#define UNLOCK_SPINLOCK      pthread_spin_unlock
#endif //_LINUX


#if defined(LOCK_MONITOR)
#define LOCK(lock)   printf("  >>>Lock @ %s %d\n",__FILE__,__LINE__);LOCK_SPINLOCK(lock)
#define UNLOCK(lock) printf("  <<<Unlock @ %s %d\n",__FILE__,__LINE__);UNLOCK_SPINLOCK(lock)
#else //defined(LOCK_MONITOR)
#define LOCK(lock)   LOCK_SPINLOCK(lock)
#define UNLOCK(lock) UNLOCK_SPINLOCK(lock)
#endif //defined(LOCK_MONITOR)
*/


#define _INSERT_NODE(prev,next,previtem,nextitem) \
{ \
	(next)->previtem=(prev); \
	(next)->nextitem=(prev)->nextitem; \
	if((prev)->nextitem) \
	{ \
		(prev)->nextitem->previtem=(next); \
	} \
	(prev)->nextitem=next; \
}
#define _REMOVE_NODE(Node,previtem,nextitem) \
{ \
	(Node)->previtem->nextitem=(Node)->nextitem; \
	if((Node)->nextitem) \
	{ \
		(Node)->nextitem->previtem=(Node)->previtem; \
	} \
}


//#define INSERT_THREAD_NODE(Prev,Next)  _INSERT_NODE(Prev,Next,ThreadPrev,ThreadNext)
//#define REMOVE_THREAD_NODE(Node)       _REMOVE_NODE(Node,ThreadPrev,ThreadNext)

#define INSERT_HANDLER_NODE(prev,next) _INSERT_NODE(prev,next,Prev,Next)
#define REMOVE_HANDLER_NODE(Node)      _REMOVE_NODE(Node,Prev,Next)


static THREAD_CONTEXT struct
{
	EXCEPTION_ENTRY  ExceptionHandlerChain;
	struct sigaction SignalHandler,OldHandler;
	//SPINLOCK         Lock;
	int              Initialized;
}s_Maintain={0};


#define _MAX_STACK_COUNT 16

static int PipeRunExA(IN char *pCmd,OUT char *pResult,IN int iBufLength)
{
	int  rc   = 0;
	int  iRet = -1;
	FILE *fd  = NULL;
	
	do{
		fd=popen(pCmd,"r");
		if(!fd)
		{
			break;
		}
		
		while(fgets(pResult,iBufLength,fd))
		{
			pResult+=strlen(pResult);
		}
		
		rc=pclose(fd);
		
		if(-1==rc)
		{
			break;
		}
		
		if(!WIFEXITED(rc))
		{
			break;
		}
		else
		{
			iRet=WEXITSTATUS(rc);
		}
	}while(0);
	
	return iRet;
}


int ShowStackFrame(void)
{
	int   iStackFrameCount,i;
	int   bNoAddr2Line=FALSE;
	char  szCmd[512],szResult[512];
	void  *pStack[_MAX_STACK_COUNT]={0};
	char* *pSymbols;
	
	szResult[0]=0;
	i=PipeRunExA("bash -c \"addr2line --version|grep 'GNU'\"&>/dev/null",szResult,sizeof(szResult));
	if(szResult[0])
	{
		bNoAddr2Line=TRUE;
	}
	iStackFrameCount=backtrace(pStack,sizeof(pStack)/sizeof(pStack[0]));
	pSymbols=backtrace_symbols(pStack,iStackFrameCount);
	//catch_num = backtrace(buf, MAXBUF_SIZE);
	//symbols = backtrace_symbols(buf, catch_num);
	
	if(!pSymbols)
	{
		perror("backtrace_symbols");
		return -1;
	}
	
	if(bNoAddr2Line)
	{
		for(i=0;i<iStackFrameCount;i++)
		{
			snprintf(szCmd,sizeof(szCmd),"a=`echo '%s'|sed 's/()/(unknown)/g;s/\\[//g;s/\\]//g;s/(/ /g;s/)/ /g'`;addr=`echo $a|awk '{print $3}'`;mod=`echo $a|awk '{print $1}'`;src=`addr2line -Cfe $mod $addr|sed -n 2p`;echo -n $a' '$src|awk '{print $1\" @\"$3\"(\"$2\") | \"$4}'",pSymbols[i]);
			PipeRunExA(szCmd,szResult,sizeof(szResult));
			printf("%s",szResult);
		}
	}
	else
	{
		for(i=0;i<iStackFrameCount;i++)
		{
			printf("%s\n",pSymbols[i]);
		}
	}
	
	return 0;
}


/*
To find the exception handler chain for specified thread
*/
/*
PEXCEPTION_ENTRY SearchThreadExceptionHandlerList(IN PEXCEPTION_ENTRY pChain,IN UINT64 pidThreadID)
{
	SEH_LOG(("  Search exception handle chain for thread %llXH\n",pidThreadID));
	while(pChain)
	{
		if(pChain->ThreadID==pidThreadID)
		{
			SEH_LOG(("    Exception chain for current thread was found\n"));
			break;
		}
		pChain=pChain->ThreadNext;
	}
	
	return pChain;
}
*/


/*
Default exception handler, it enumerates can call the registered exception handlers
*/
static void _ExceptionHandler(IN int iSignal,IN siginfo_t *pSignalInfo,IN void *pContext)
{
	int              iFilter;
	PEXCEPTION_ENTRY pEntry,pNext;
	
	//printf("Signal %d\n",iSignal);
	//LOCK(&s_Maintain.Lock);
	SEH_LOG(("    Got SIGSEGV at address: %lXH, Thread %lXH %p\n",(long) pSignalInfo->si_addr,GetCurrentThreadID(),pContext));
	pEntry=s_Maintain.ExceptionHandlerChain.Next;
	do
	{
#ifdef _DEBUG
		SEH_LOG(("      Try handler %p for thread %llXH, id %d\n",pEntry,GetCurrentThreadID(),pEntry->ID));
#else //_DEBUG
		SEH_LOG(("      Try handler %p for thread %llXH\n",pEntry,GetCurrentThreadID()));
#endif //_DEBUG
		iFilter=pEntry->FilterRoutine(iSignal,pSignalInfo,pContext);
		switch(iFilter)
		{
			case EXCEPTION_EXECUTE_HANDLER://Fail to handle exception, do clean work
				//UNLOCK(&s_Maintain.Lock);
				siglongjmp(pEntry->SectionHandler,1);
				break;
			case EXCEPTION_CONTINUE_SEARCH://Search older handler
				pNext=pEntry->Next;
				REMOVE_HANDLER_NODE(pEntry);
				pEntry=pNext;
				break;
			case EXCEPTION_CONTINUE_EXECUTION://Exception was handled, retry
				//UNLOCK(&s_Maintain.Lock);
				return;
				//siglongjmp(pEntry->SectionEntry,1);
				break;
			default:
				SEH_LOG(("Bad exception filter result %d\n",iFilter));
				break;
		}
	}while(pEntry);
	//UNLOCK(&s_Maintain.Lock);
	
	SEH_LOG(("    No more handler\n"));
	sigaction(SIGSEGV,&s_Maintain.OldHandler,NULL);
}


/*
Initialize SEH service, register signal handler and prepare the exception chain
*/
int StartSEHService(void)
{
	//INIT_SPINLOCK(&s_Maintain.Lock);
	
	memset(&s_Maintain.ExceptionHandlerChain,0,sizeof(s_Maintain.ExceptionHandlerChain));
	s_Maintain.SignalHandler.sa_flags=SA_SIGINFO;
	sigemptyset(&s_Maintain.SignalHandler.sa_mask);
	s_Maintain.SignalHandler.sa_sigaction=_ExceptionHandler;
	s_Maintain.Initialized=TRUE;
	if(-1==sigaction(SIGSEGV,&s_Maintain.SignalHandler,&s_Maintain.OldHandler))
	{
		perror("Register sigaction fail");
		return FALSE;
	}
	else
	{
		return TRUE;
	}
}


/*
Show exception chain, for debug issue
*/
void ShowThreadExceptionChain(void)
{
	int              i,iCount=0;
	PEXCEPTION_ENTRY pEntry=&s_Maintain.ExceptionHandlerChain;
	
	printf("  Thread node %p,%p, thread ID %llXH\n",pEntry->Prev,pEntry->Next,GetCurrentThreadID());
	while(pEntry)
	{
#ifdef _DEBUG
		printf("  chain %p for thread %llXH. ID %d\n",pEntry,GetCurrentThreadID(),pEntry->ID);
#else //_DEBUG
		printf("  chain %p for thread %llXH.\n",pEntry,GetCurrentThreadID());
#endif //_DEBUG
		pEntry=pEntry->Next;
		iCount++;
		if((pEntry==&s_Maintain.ExceptionHandlerChain)||(iCount>20))
		{
			printf("Loop chain!\n");
			exit(EXIT_FAILURE);
		}
	}
}
/*
void ShowThreadExceptionChain(void)
{
	int              i,iCount=0;
	PEXCEPTION_ENTRY pEntry=&s_Maintain.ExceptionHandlerChain,pChild;
	
	while(pEntry)
	{
#ifdef _DEBUG
		printf("Thread chain %p for thread %llXH. ID %d\n",pEntry,pEntry->ThreadID,pEntry->ID);
#else //_DEBUG
		printf("Thread chain %p for thread %llXH.\n",pEntry,pEntry->ThreadID);
#endif //_DEBUG
		printf("  Global node %p,%p\n",pEntry->ThreadPrev,pEntry->ThreadNext);
		printf("  Thread node %p,%p\n",pEntry->Prev,pEntry->Next);
		pChild=pEntry->Next;
		while(pChild)
		{
#ifdef _DEBUG
			printf("  chain %p for thread %llXH. ID %d\n",pChild,pChild->ThreadID,pChild->ID);
#else //_DEBUG
			printf("  chain %p for thread %llXH.\n",pChild,pChild->ThreadID);
#endif //_DEBUG
			printf("    Global node %p,%p\n",pChild->ThreadPrev,pChild->ThreadNext);
			printf("    Thread node %p,%p\n",pChild->Prev,pChild->Next);
			pChild=pChild->Next;
		}
		
		pEntry=pEntry->ThreadNext;
		iCount++;
		if((pEntry==&s_Maintain.ExceptionHandlerChain)||(iCount>20))
		{
			printf("Loop chain!\n");
			exit(EXIT_FAILURE);
		}
	}
}
*/


/*
Stop SEH service. Recover the signal handler
*/
void StopSEHService(void)
{
	SEH_LOG(("Stop SEH service\n"));
	if(-1==sigaction(SIGSEGV,&s_Maintain.OldHandler,NULL))
	{
		perror("Unregister sigaction fail");
	}
}


/*
*/
void RegisterExceptionHandler(IN PEXCEPTION_ENTRY pEntry)
{
	if(!s_Maintain.Initialized)
	{
		StartSEHService();
	}
	
#ifdef _DEBUG
	pEntry->ID=g_iID++;
#endif //_DEBUG
	pEntry->Prev=pEntry->Next=NULL;
#ifdef _DEBUG
	SEH_LOG(("  ++Register exception handler %p for thread %llXH, id %d\n",pEntry,GetCurrentThreadID(),pEntry->ID));
#else  //_DEBUG
	SEH_LOG(("  ++Register exception handler %p for thread %llXH\n",pEntry,GetCurrentThreadID()));
#endif //_DEBUG
	//LOCK(&s_Maintain.Lock);
	
	SEH_LOG(("%s %d\n",__FILE__,__LINE__));
	INSERT_HANDLER_NODE(&s_Maintain.ExceptionHandlerChain,pEntry);
	SEH_LOG(("%s %d\n",__FILE__,__LINE__));
#ifdef _DEBUG
	ShowThreadExceptionChain();
#endif //_DEBUG
	//UNLOCK(&s_Maintain.Lock);
}


/*
*/
void UnregisterExceptionHandler(IN PEXCEPTION_ENTRY pEntry)
{
#ifdef _DEBUG
	SEH_LOG(("  --Unregister handler %p of thread %llXH, id %X\n",pEntry,pEntry->ThreadID,pEntry->ID));
#else //_DEBUG
	SEH_LOG(("  --Unregister handler %p of thread %llXH\n",pEntry,pEntry->ThreadID));
#endif //_DEBUG
	//LOCK(&s_Maintain.Lock);
#ifdef _DEBUG
	ShowThreadExceptionChain();
#endif //_DEBUG
	REMOVE_HANDLER_NODE(pEntry);
	SEH_LOG(("Handler %p removed\n",pEntry));
#ifdef _DEBUG
	ShowThreadExceptionChain();
#endif //_DEBUG
	//UNLOCK(&s_Maintain.Lock);
}
#endif //defined(_WIN32)

