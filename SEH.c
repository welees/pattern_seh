#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
//#include <errno.h>
#include <setjmp.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <stdbool.h>

#include "SEH.h"

#ifdef Darwin
#define SPINLOCK             pthread_mutex_t
#define INIT_SPINLOCK(lock)  pthread_mutex_init(lock,NULL)
#define LOCK_SPINLOCK        pthread_mutex_lock
#define UNLOCK_SPINLOCK      pthread_mutex_unlock
#define GetCurrentThreadID   pthread_self
#endif //Darwin
#ifdef Linux
#define SPINLOCK             pthread_spinlock_t
#define INIT_SPINLOCK(lock)  pthread_spin_init(lock,PTHREAD_PROCESS_PRIVATE)
#define LOCK_SPINLOCK        pthread_spin_lock
#define UNLOCK_SPINLOCK      pthread_spin_unlock
#define GetCurrentThreadID() syscall(SYS_gettid)
#endif //Linux


#if !(defined(Darwin)||defined(Linux))
#error This project is for Linux or OSX platform only
#endif //!(defined(Darwin)||defined(Linux))




#ifdef _DEBUG
int g_iID=1;
#endif //_DEBUG


#if defined(LOCK_MONITOR)
#define LOCK(lock)   printf("  >>>Lock @ %s %d\n",__FILE__,__LINE__);LOCK_SPINLOCK(lock)
#define UNLOCK(lock) printf("  <<<Unlock @ %s %d\n",__FILE__,__LINE__);UNLOCK_SPINLOCK(lock)
#else //defined(LOCK_MONITOR)
#define LOCK(lock)   LOCK_SPINLOCK(lock)
#define UNLOCK(lock) UNLOCK_SPINLOCK(lock)
#endif //defined(LOCK_MONITOR)


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


#define INSERT_THREAD_NODE(Prev,Next)  _INSERT_NODE(Prev,Next,ThreadPrev,ThreadNext)
#define REMOVE_THREAD_NODE(Node)       _REMOVE_NODE(Node,ThreadPrev,ThreadNext)

#define INSERT_HANDLER_NODE(prev,next) _INSERT_NODE(prev,next,Prev,Next)
#define REMOVE_HANDLER_NODE(Node)      _REMOVE_NODE(Node,Prev,Next)

#define GOTO_HEADER(Node) \
while(Node->Prev) \
{ \
	printf("%s %d %p %p %p\n",__FILE__,__LINE__,&s_Maintain.ExceptionHandlerChain,Node,Node->Prev); \
	Node=Node->Prev; \
}


static struct
{
	EXCEPTION_ENTRY  ExceptionHandlerChain;
	struct sigaction SignalHandler,OldHandler;
	SPINLOCK         Lock;
}s_Maintain;


PEXCEPTION_ENTRY SearchThreadExceptionHandlerList(IN PEXCEPTION_ENTRY pChain,IN THREAD_ID pidThreadID)
{
	SEH_LOG("  Search exception handle chain for thread %X\n",pidThreadID);
	while(pChain)
	{
		if(pChain->ThreadID==pidThreadID)
		{
			SEH_LOG("    Exception chain for current thread was found\n");
			break;
		}
		pChain=pChain->ThreadNext;
	}
	
	return pChain;
}


static void _ExceptionHandler(IN int iSignal,IN siginfo_t *pSignalInfo,IN void *pContext)
{
	int              iFilter;
	PEXCEPTION_ENTRY pEntry;
	
	//printf("Signal %d\n",iSignal);
	LOCK(&s_Maintain.Lock);
	SEH_LOG("    Got SIGSEGV at address: %lXH, Thread %lXH %p\n",(long) pSignalInfo->si_addr,GetCurrentThreadID(),pContext);
	pEntry=SearchThreadExceptionHandlerList(&s_Maintain.ExceptionHandlerChain,GetCurrentThreadID());
	do
	{
#ifdef _DEBUG
		SEH_LOG("      Try handler %p for thread %X, id %d\n",pEntry,pEntry->ThreadID,pEntry->ID);
#else //_DEBUG
		SEH_LOG("      Try handler %p for thread %X\n",pEntry,pEntry->ThreadID);
#endif //_DEBUG
		iFilter=pEntry->FilterRoutine(iSignal,pSignalInfo,pContext);
		switch(iFilter)
		{
			case EXCEPTION_EXECUTE_HANDLER://Fail to handle exception
				UNLOCK(&s_Maintain.Lock);
				siglongjmp(pEntry->SectionHandler,1);
				break;
			case EXCEPTION_CONTINUE_SEARCH://Search upper handler
				REMOVE_THREAD_NODE(pEntry);
				pEntry=pEntry->Next;
				if(pEntry)
				{
					INSERT_THREAD_NODE(&s_Maintain.ExceptionHandlerChain,pEntry);
					SEH_LOG("      Pass, to previous handle %p\n",pEntry);
				}
				break;
			case EXCEPTION_CONTINUE_EXECUTION://Exception was handled, retry
				UNLOCK(&s_Maintain.Lock);
				return;
				//siglongjmp(pEntry->SectionEntry,1);
				break;
			default:
				SEH_LOG("Bad exception filter result %d\n",iFilter);
				break;
		}
	}while(pEntry);
	UNLOCK(&s_Maintain.Lock);
	
	SEH_LOG("    No more handler\n");
	sigaction(SIGSEGV,&s_Maintain.OldHandler,NULL);
}


bool StartSEHService(void)
{
	INIT_SPINLOCK(&s_Maintain.Lock);
	
	memset(&s_Maintain.ExceptionHandlerChain,0,sizeof(s_Maintain.ExceptionHandlerChain));
	s_Maintain.SignalHandler.sa_flags=SA_SIGINFO;
	sigemptyset(&s_Maintain.SignalHandler.sa_mask);
	s_Maintain.SignalHandler.sa_sigaction=_ExceptionHandler;
	printf("Start SEH service\n");
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

void ShowThreadExceptionChain(void)
{
	int              i,iCount=0;
	PEXCEPTION_ENTRY pEntry=&s_Maintain.ExceptionHandlerChain,pChild;
	
	while(pEntry)
	{
#ifdef _DEBUG
		printf("Thread chain %p for thread %XH. ID %d\n",pEntry,pEntry->ThreadID,pEntry->ID);
#else //_DEBUG
		printf("Thread chain %p for thread %XH.\n",pEntry,pEntry->ThreadID);
#endif //_DEBUG
		printf("  Global node %p,%p\n",pEntry->ThreadPrev,pEntry->ThreadNext);
		printf("  Thread node %p,%p\n",pEntry->Prev,pEntry->Next);
		pChild=pEntry->Next;
		while(pChild)
		{
#ifdef _DEBUG
			printf("  chain %p for thread %XH. ID %d\n",pChild,pChild->ThreadID,pChild->ID);
#else //_DEBUG
			printf("  chain %p for thread %XH.\n",pChild,pChild->ThreadID);
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

void StopSEHService(void)
{
	SEH_LOG("Stop SEH service\n");
	if(-1==sigaction(SIGSEGV,&s_Maintain.OldHandler,NULL))
	{
		perror("Unregister sigaction fail");
	}
}


void RegisterExceptionHandler(IN PEXCEPTION_ENTRY pEntry)
{
	PEXCEPTION_ENTRY pList;
	
	pEntry->ThreadID=GetCurrentThreadID();
#ifdef _DEBUG
	pEntry->ID=g_iID++;
#endif //_DEBUG
	pEntry->ThreadNext=pEntry->ThreadPrev=NULL;
	pEntry->Prev=pEntry->Next=NULL;
#ifdef _DEBUG
	SEH_LOG("  ++Register exception handler %p for thread %X, id %d\n",pEntry,pEntry->ThreadID,pEntry->ID);
#else  //_DEBUG
	SEH_LOG("  ++Register exception handler %p for thread %X\n",pEntry,pEntry->ThreadID);
#endif //_DEBUG
	LOCK(&s_Maintain.Lock);
	
	pList=SearchThreadExceptionHandlerList(&s_Maintain.ExceptionHandlerChain,pEntry->ThreadID);
	if(!pList)
	{//There is no handler for current thread
		SEH_LOG("    New handler for current thread\n");
		INSERT_THREAD_NODE(&s_Maintain.ExceptionHandlerChain,pEntry);
	}
	else
	{
		REMOVE_THREAD_NODE(pList);
		INSERT_THREAD_NODE(&s_Maintain.ExceptionHandlerChain,pEntry);
		INSERT_HANDLER_NODE(pEntry,pList);
	}
#ifdef _DEBUG
	ShowThreadExceptionChain();
#endif //_DEBUG
	UNLOCK(&s_Maintain.Lock);
}


void UnregisterExceptionHandler(IN PEXCEPTION_ENTRY pEntry)
{
	PEXCEPTION_ENTRY pList;
	
#ifdef _DEBUG
	SEH_LOG("  --Unregister handler %p of thread %X, id %X\n",pEntry,pEntry->ThreadID,pEntry->ID);
#else //_DEBUG
	SEH_LOG("  --Unregister handler %p of thread %X\n",pEntry,pEntry->ThreadID);
#endif //_DEBUG
	LOCK(&s_Maintain.Lock);
#ifdef _DEBUG
	ShowThreadExceptionChain();
#endif //_DEBUG
	REMOVE_THREAD_NODE(pEntry);
	if(pEntry->Next)
	{
		pEntry->Next->Prev=NULL;
		INSERT_THREAD_NODE(&s_Maintain.ExceptionHandlerChain,pEntry->Next);
	}
	(SEH_LOG("Handler removed\n"));
#ifdef _DEBUG
	ShowThreadExceptionChain();
#endif //_DEBUG
	UNLOCK(&s_Maintain.Lock);
}
