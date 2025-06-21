#include <windows.h>
#include <stdio.h>
#include <setjmp.h>
#include <crtdbg.h>

typedef struct _EXCEPTION_NODE
{
	struct _EXCEPTION_NODE *Prev;
	int                    (*ExceptionFilter)(PEXCEPTION_POINTERS pExceptionInfo);
	jmp_buf                Entry;
	UINT                   RunStatus;
}EXCEPTION_NODE,*PEXCEPTION_NODE;

__thread struct _SEH_SET
{
	PEXCEPTION_NODE Chain;
	int             Initialized;
}s_Maintain={0};

#define TRY_START \
	{ \
		EXCEPTION_NODE __weLees_ExceptionNode; \
		if(!s_Maintain.Initialized) \
		{/*注册 VEH（优先级高于未处理过滤器）*/ \
			AddVectoredExceptionHandler(1,DefVEHHandler); \
			s_Maintain.Initialized=1; \
		} \
		__weLees_ExceptionNode.Prev=s_Maintain.Chain; \
		s_Maintain.Chain=&__weLees_ExceptionNode; \
		__weLees_ExceptionNode.RunStatus=setjmp(__weLees_ExceptionNode.Entry); \
		if(2==__weLees_ExceptionNode.RunStatus) \
		{

#define TRY_EXCEPT(filter) \
			s_Maintain.Chain=__weLees_ExceptionNode.Prev; \
		} \
		else if(!__weLees_ExceptionNode.RunStatus) \
		{ \
			__weLees_ExceptionNode.ExceptionFilter=filter; \
			longjmp(__weLees_ExceptionNode.Entry, 2); \
		} \
		else

#define TRY_END }

LONG WINAPI DefVEHHandler(PEXCEPTION_POINTERS pInfo)
{
	int             iResult;
	PEXCEPTION_NODE pNode;
	
	printf("VEH caught exception: 0x%08X\n",pInfo->ExceptionRecord->ExceptionCode);
	while(s_Maintain.Chain)
	{
		pNode=s_Maintain.Chain;
		iResult=s_Maintain.Chain->ExceptionFilter(pInfo);
		switch(iResult)
		{
			case EXCEPTION_EXECUTE_HANDLER:
				s_Maintain.Chain=s_Maintain.Chain->Prev;
				longjmp(pNode->Entry,1);
			case EXCEPTION_CONTINUE_EXECUTION:
				return iResult;
			case EXCEPTION_CONTINUE_SEARCH:
				s_Maintain.Chain=s_Maintain.Chain->Prev;
				break;
			default:
				_ASSERT(FALSE);
				break;
		}
	}
	return -1;
}

int g_iTest;

int MyFilter(PEXCEPTION_POINTERS pExceptionInfo)
{
	//pExceptionInfo->ContextRecord->Rax=(UINT64)&g_iTest;
	printf("  Inner exception clean handler, we cannot handle it, try outer exception handler\n");
	return EXCEPTION_CONTINUE_SEARCH;
}

int MyFilter2(PEXCEPTION_POINTERS pExceptionInfo)
{
	//pExceptionInfo->ContextRecord->Rax=(UINT64)&g_iTest;
	printf("Outer exception clean handler, do clean work\n");
	return EXCEPTION_EXECUTE_HANDLER;
}

int main(void)
{
	TRY_START
	{
		printf("Enter outer exception monitor section\n");
		TRY_START
		{
			printf("  Enter inner exception monitor section\n");
			// 触发异常
			int* ptr=NULL;
			printf("    Let's do bad thing.\n");
			*ptr=42;
			s_Maintain.Chain=__weLees_ExceptionNode.Prev;
		}
		TRY_EXCEPT(MyFilter)
		{
			printf("WTF\n");
		}
		TRY_END
	}
	TRY_EXCEPT(MyFilter2)
	{
		printf("WTF2\n");
	}
	TRY_END

	return 0;
}
