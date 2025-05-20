#pragma once


#define THREAD_CONTEXT __thread


int ShowStackFrame(void);


typedef struct _EXCEPTION_ENTRY
{
	//struct _EXCEPTION_ENTRY *ThreadPrev,*ThreadNext;//For thread chain
	struct _EXCEPTION_ENTRY *Prev,*Next;//For handler chain in the same thread
	jmp_buf                 SectionEntry,SectionHandler;
	//unsigned long long      ThreadID;
#ifdef _DEBUG
	int                     ID;
#endif //_DEBUG
	int (*FilterRoutine)(IN int iSignal,IN siginfo_t *pSignalInfo,IN void *pContext);
}EXCEPTION_ENTRY,*PEXCEPTION_ENTRY;


#define EXCEPTION_EXECUTE_HANDLER       1
#define EXCEPTION_CONTINUE_SEARCH       0
#define EXCEPTION_CONTINUE_EXECUTION    -1


void RegisterExceptionHandler(IN PEXCEPTION_ENTRY pEntry);


void UnregisterExceptionHandler(IN PEXCEPTION_ENTRY pEntry);


#define TRY_START \
	{ \
		EXCEPTION_ENTRY __weLees_Pattern_SEH_Section; \
		SEH_LOG(("  Enter critical section @ %s %d\n",__FILE__,__LINE__)); \
		RegisterExceptionHandler(&__weLees_Pattern_SEH_Section); \
		if(sigsetjmp(__weLees_Pattern_SEH_Section.SectionEntry,1)) /*0 means entry was set, go to exception section to do init work*/ \
		{

#define TRY_EXCEPT(Handler) \
			SEH_LOG(("  Leave critical section @ %s %d\n",__FILE__,__LINE__)); \
			UnregisterExceptionHandler(&__weLees_Pattern_SEH_Section); \
		} \
		else \
		{/*exception handling*/ \
			__weLees_Pattern_SEH_Section.FilterRoutine=Handler; \
			if(!sigsetjmp(__weLees_Pattern_SEH_Section.SectionHandler,1)) \
			{ \
				siglongjmp(__weLees_Pattern_SEH_Section.SectionEntry,1);/*Start protected section*/ \
			} \
			else \
			{ \
				SEH_LOG(("  Leave critical section @ %s %d\n",__FILE__,__LINE__)); \
				UnregisterExceptionHandler(&__weLees_Pattern_SEH_Section);

#define TRY_END \
			} \
		} \
	}


#define TRY_BREAK UnregisterExceptionHandler(&__weLees_Pattern_SEH_Section);break
