#if defined(_WIN32)&&defined(_MINGW)&&defined(_WIN64)
#include <windows.h>
#include <setjmp.h>
#include <stdio.h>
#include <crtdbg.h>
#include "SEH.h"


__declspec(thread) WELEES_SEH_SET s_weLees_SEH_Maintain={0};


#if defined(__cplusplus)
EXCEPTION_NODE::~EXCEPTION_NODE(void)
{
	if(Prev)
	{
		s_weLees_SEH_Maintain.Chain=s_weLees_SEH_Maintain.Chain->Prev;
	}
}
#endif //defined(__cplusplus)


int StartSEHService(void)
{
	return TRUE;
}


void StopSEHService(void)
{
}


//MINGW64 exception handler stub
LONG WINAPI DefVEHHandler(PEXCEPTION_POINTERS pInfo)
{
	int             iResult;
	PEXCEPTION_NODE pNode;
	
	printf("VEH caught exception: 0x%08X\n",pInfo->ExceptionRecord->ExceptionCode);
	while(s_weLees_SEH_Maintain.Chain)
	{
		pNode=s_weLees_SEH_Maintain.Chain;
		iResult=s_weLees_SEH_Maintain.Chain->ExceptionFilter(pInfo);
		switch(iResult)
		{
			case EXCEPTION_EXECUTE_HANDLER:
				s_weLees_SEH_Maintain.Chain=s_weLees_SEH_Maintain.Chain->Prev;//Unlink exception block
				pNode->Prev=NULL;
				longjmp(pNode->Entry,1);
			case EXCEPTION_CONTINUE_EXECUTION:
				return iResult;
			case EXCEPTION_CONTINUE_SEARCH:
				s_weLees_SEH_Maintain.Chain=s_weLees_SEH_Maintain.Chain->Prev;//Unlink exception block
				pNode->Prev=NULL;
				break;
			default:
				_ASSERT(FALSE);
				break;
		}
	}
	return -1;
}
#endif //defined(_WIN32)&&defined(_MINGW)&&defined(_WIN64)
