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


static THREAD_CONTEXT struct
{
	PEXCEPTION_NODE  Chain;
	struct sigaction SignalHandler,OldHandler;
	int              Initialized;
}s_weLees_SEH_Maintain={0};


#define _MAX_STACK_COUNT 16


static int PipeRunExA(IN char const *pCmd,OUT char *pResult,IN int iBufLength)
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
Default exception handler, it enumerates can call the registered exception handlers
*/
static void __weLees_NonWinExceptionPreHandler(IN int iSignal,IN siginfo_t *pSignalInfo,IN void *pContext)
{
	int             iFilter;
	PEXCEPTION_NODE pEntry,pPrev;
	
	SEH_LOG(("    Got SIGSEGV at address: %lXH, Thread %llXH %p\n",(long)pSignalInfo->si_addr,GetCurrentThreadID(),pContext));
	pEntry=s_weLees_SEH_Maintain.Chain;
	do
	{
		SEH_LOG(("      Try handler %p for thread %llXH, id %d\n",pEntry,GetCurrentThreadID(),pEntry->ID));
		iFilter=pEntry->FilterRoutine(iSignal,pSignalInfo,pContext);
		switch(iFilter)
		{
			case EXCEPTION_EXECUTE_HANDLER://Fail to handle exception, do clean work
				UnregisterExceptionHandler();
				siglongjmp(pEntry->SectionEntry,1);
				break;
			case EXCEPTION_CONTINUE_SEARCH://Search older handler
				pPrev=pEntry->Prev;
				UnregisterExceptionHandler();
				pEntry=pPrev;
				break;
			case EXCEPTION_CONTINUE_EXECUTION://Exception was handled, retry
				return;
				break;
			default:
				SEH_LOG(("Bad exception filter result %d\n",iFilter));
				break;
		}
	}while(pEntry);
	
	SEH_LOG(("    No more handler\n"));
	sigaction(SIGSEGV,&s_weLees_SEH_Maintain.OldHandler,NULL);
}


/*
Initialize SEH service, register signal handler and prepare the exception chain
*/
int StartSEHService(void)
{
	s_weLees_SEH_Maintain.SignalHandler.sa_flags=SA_SIGINFO;
	sigemptyset(&s_weLees_SEH_Maintain.SignalHandler.sa_mask);
	s_weLees_SEH_Maintain.SignalHandler.sa_sigaction=__weLees_NonWinExceptionPreHandler;
	s_weLees_SEH_Maintain.Initialized=TRUE;
	if(-1==sigaction(SIGSEGV,&s_weLees_SEH_Maintain.SignalHandler,&s_weLees_SEH_Maintain.OldHandler))
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
	int             i,iCount=0;
	PEXCEPTION_NODE pEntry=s_weLees_SEH_Maintain.Chain;
	
	while(pEntry)
	{
		SEH_LOG(("  chain %p for thread %llXH. ID %d\n",pEntry,GetCurrentThreadID(),pEntry->ID));
		pEntry=pEntry->Prev;
		iCount++;
		if((pEntry==s_weLees_SEH_Maintain.Chain)||(iCount>20))
		{
			printf("Loop chain!\n");
			exit(EXIT_FAILURE);
		}
	}
}


/*
Stop SEH service. Recover the signal handler
*/
void StopSEHService(void)
{
	SEH_LOG(("Stop SEH service\n"));
	if(-1==sigaction(SIGSEGV,&s_weLees_SEH_Maintain.OldHandler,NULL))
	{
		perror("Unregister sigaction fail");
	}
}


/*
*/
void RegisterExceptionHandler(IN void *pEntry)
{
	if(!s_weLees_SEH_Maintain.Initialized)
	{
		StartSEHService();
	}
	
#ifdef _DEBUG
	((PEXCEPTION_NODE)pEntry)->ID=g_iID++;
#endif //_DEBUG
	SEH_LOG(("  ++Register exception handler %p for thread %llXH, id %d\n",pEntry,GetCurrentThreadID(),((PEXCEPTION_NODE)pEntry)->ID));
	
	SEH_LOG(("%s %d\n",__FILE__,__LINE__));
	((PEXCEPTION_NODE)pEntry)->Prev=s_weLees_SEH_Maintain.Chain;
	s_weLees_SEH_Maintain.Chain=(PEXCEPTION_NODE)pEntry;
	SEH_LOG(("%s %d\n",__FILE__,__LINE__));
#ifdef _DEBUG
	ShowThreadExceptionChain();
#endif //_DEBUG
}


/*
*/
void UnregisterExceptionHandler(void)
{
	PEXCEPTION_NODE pEntry=s_weLees_SEH_Maintain.Chain;
	SEH_LOG(("  --Unregister handler %p, id %X\n",pEntry,pEntry->ID));
#ifdef _DEBUG
	ShowThreadExceptionChain();
#endif //_DEBUG
	s_weLees_SEH_Maintain.Chain=s_weLees_SEH_Maintain.Chain->Prev;
	pEntry->Prev=NULL;
	SEH_LOG(("Handler %p removed\n",pEntry));
#ifdef _DEBUG
	ShowThreadExceptionChain();
#endif //_DEBUG
}
#endif //defined(_WIN32)
