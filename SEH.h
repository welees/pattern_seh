#pragma once

#ifdef Darwin
#define THREAD_ID pthread_t
#endif //Darwin
#ifdef Linux
#define THREAD_ID pid_t
#endif //Linux


#define IN
#define TRUE 1
#define FALSE 0


#ifdef _DEBUG
#define SEH_LOG printf
#else //_DEBUG
#define SEH_LOG
#endif //_DEBUG


#if !(defined(Darwin)||defined(Linux))
#error This project is for Linux or OSX platform only
#endif //!(defined(Darwin)||defined(Linux))


typedef struct _EXCEPTION_ENTRY
{
	struct _EXCEPTION_ENTRY *ThreadPrev,*ThreadNext;//For thread chain
	struct _EXCEPTION_ENTRY *Prev,*Next;//For handler chain in the same thread
	jmp_buf                 SectionEntry,SectionHandler;
	THREAD_ID               ThreadID;
#ifdef _DEBUG
	int                     ID;
#endif //_DEBUG
	int (*FilterRoutine)(IN int iSignal,IN siginfo_t *pSignalInfo,IN void *pContext);
}EXCEPTION_ENTRY,*PEXCEPTION_ENTRY;


#define EXCEPTION_EXECUTE_HANDLER       1
#define EXCEPTION_CONTINUE_SEARCH       0
#define EXCEPTION_CONTINUE_EXECUTION    -1


bool StartSEHService(void);


void StopSEHService(void);


void RegisterExceptionHandler(IN PEXCEPTION_ENTRY pEntry);


void UnregisterExceptionHandler(IN PEXCEPTION_ENTRY pEntry);


#ifdef _DEBUG
#define TRY_START \
{ \
	struct \
	{ \
		jmp_buf         Entry; \
		EXCEPTION_ENTRY Node; \
	}__weLees_Pattern_SEH_Section; \
	SEH_LOG("  Enter critical section @ %s %d\n",__FILE__,__LINE__); \
	RegisterExceptionHandler(&__weLees_Pattern_SEH_Section.Node); \
	if(sigsetjmp(__weLees_Pattern_SEH_Section.Entry,1)) /*0 means entry was set, go to exception section to do init work*/
#else //_DEBUG
#define TRY_START \
{ \
	struct \
	{ \
		jmp_buf         Entry; \
		EXCEPTION_ENTRY Node; \
	}__weLees_Pattern_SEH_Section; \
	RegisterExceptionHandler(&__weLees_Pattern_SEH_Section.Node); \
	if(sigsetjmp(__weLees_Pattern_SEH_Section.Entry,1)) /*0 means entry was set, go to exception section to do init work*/
#endif //_DEBUG

#define TRY_EXCEPT(Handler) \
else \
{/*exception handling*/ \
	__weLees_Pattern_SEH_Section.Node.FilterRoutine=Handler; \
	if(!sigsetjmp(__weLees_Pattern_SEH_Section.Node.SectionHandler,1)) \
	{ \
		siglongjmp(__weLees_Pattern_SEH_Section.Entry,1);/*Start protected section*/ \
	} \
	else

#ifdef _DEBUG
#define TRY_END \
	} \
	SEH_LOG("  Leave critical section @ %s %d\n",__FILE__,__LINE__); \
	UnregisterExceptionHandler(&__weLees_Pattern_SEH_Section.Node); \
}
#else //_DEBUG
#define TRY_END \
	} \
	UnregisterExceptionHandler(&__weLees_Pattern_SEH_Section.Node); \
}
#endif //_DEBUG
