#pragma once


#define THREAD_CONTEXT __thread


int ShowStackFrame(void);


void RegisterExceptionHandler(IN void *pEntry);


void UnregisterExceptionHandler(void);


typedef struct _EXCEPTION_NODE
{
	struct _EXCEPTION_NODE *Prev;//For handler chain in the same thread
	int                    (*FilterRoutine)(IN int iSignal,IN siginfo_t *pSignalInfo,IN void *pContext);
	jmp_buf                SectionEntry;
	int                    RunStatus;
#ifdef _DEBUG
	int                    ID;
#endif //_DEBUG
#if defined(__cplusplus)
	~_EXCEPTION_NODE(void)
	{
		if(Prev)
		{
			UnregisterExceptionHandler();
		}
	}
#endif //defined(__cplusplus)
}EXCEPTION_NODE,*PEXCEPTION_NODE;


#define EXCEPTION_EXECUTE_HANDLER       1
#define EXCEPTION_CONTINUE_SEARCH       0
#define EXCEPTION_CONTINUE_EXECUTION    -1


#define TRY_START \
	{ \
		EXCEPTION_NODE __weLees_Pattern_SEH_Section; \
		SEH_LOG(("  Enter critical section @ %s %d\n",__FILE__,__LINE__)); \
		RegisterExceptionHandler(&__weLees_Pattern_SEH_Section); \
		__weLees_Pattern_SEH_Section.RunStatus=sigsetjmp(__weLees_Pattern_SEH_Section.SectionEntry,1); \
		if(2==__weLees_Pattern_SEH_Section.RunStatus) /*0 means entry was set, go to exception section to do init work*/ \
		{


#define TRY_EXCEPT(filter) \
			SEH_LOG(("  Leave critical section @ %s %d\n",__FILE__,__LINE__)); \
			UnregisterExceptionHandler(); \
		} \
		else if(!__weLees_Pattern_SEH_Section.RunStatus)\
		{/*exception handling*/ \
			__weLees_Pattern_SEH_Section.FilterRoutine=filter; \
			siglongjmp(__weLees_Pattern_SEH_Section.SectionEntry,2);/*Start protected section*/ \
		} \
		else


#define TRY_END }


#if !defined(__cplusplus)

#define TRY_BREAK   UnregisterExceptionHandler();break
#define TRY_BREAK2  UnregisterExceptionHandler();UnregisterExceptionHandler();break
#define TRY_BREAK3  UnregisterExceptionHandler();UnregisterExceptionHandler();UnregisterExceptionHandler();break


#define TRY_RETURN  UnregisterExceptionHandler();return
#define TRY_RETURN2 UnregisterExceptionHandler();UnregisterExceptionHandler();return
#define TRY_RETURN3 UnregisterExceptionHandler();UnregisterExceptionHandler();UnregisterExceptionHandler();return

#else //!defined(__cplusplus)

#define TRY_BREAK   break
#define TRY_BREAK2  break
#define TRY_BREAK3  break


#define TRY_RETURN  return
#define TRY_RETURN2 return
#define TRY_RETURN3 return

#endif //!defined(__cplusplus)
