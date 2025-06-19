#pragma once


typedef struct _EXCEPTION_NODE
{
	struct _EXCEPTION_NODE *Prev;
	int                   (*ExceptionFilter)(PEXCEPTION_POINTERS pExceptionInfo);
	jmp_buf                Entry;
	UINT                   RunStatus;
#if defined(__cplusplus)
	~_EXCEPTION_NODE(void);
#endif //defined(__cplusplus)
}EXCEPTION_NODE,*PEXCEPTION_NODE;


typedef struct _WELEES_SEH_SET
{
	PEXCEPTION_NODE Chain;
	int             Initialized;
}WELEES_SEH_SET,*PWELEES_SEH_SET;


extern WELEES_SEH_SET s_weLees_SEH_Maintain;


LONG WINAPI DefVEHHandler(PEXCEPTION_POINTERS pInfo);


#define TRY_START \
	{ \
		EXCEPTION_NODE __weLees_ExceptionNode; \
		if(!s_weLees_SEH_Maintain.Initialized) \
		{/*注册 VEH（优先级高于未处理过滤器）*/ \
			AddVectoredExceptionHandler(1,DefVEHHandler); \
			s_weLees_SEH_Maintain.Initialized=1; \
		} \
		__weLees_ExceptionNode.Prev=s_weLees_SEH_Maintain.Chain; \
		s_weLees_SEH_Maintain.Chain=&__weLees_ExceptionNode; \
		__weLees_ExceptionNode.RunStatus=setjmp(__weLees_ExceptionNode.Entry); \
		if(2==__weLees_ExceptionNode.RunStatus) \
		{


#define TRY_EXCEPT(filter) \
			s_weLees_SEH_Maintain.Chain=__weLees_ExceptionNode.Prev; \
			__weLees_ExceptionNode.Prev=NULL; \
		} \
		else if(!__weLees_ExceptionNode.RunStatus) \
		{ \
			__weLees_ExceptionNode.ExceptionFilter=filter; \
			longjmp(__weLees_ExceptionNode.Entry, 2); \
		} \
		else


#define TRY_END }

#define TRY_BREAK _UNREGISTER_SEH_NODE;break
