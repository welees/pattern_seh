#if defined(_WIN32)&&defined(_MINGW)&&(!defined(_WIN64))
#include <windows.h>
#include <setjmp.h>
#include <stdio.h>
#include "SEH.h"


int StartSEHService(void)
{
	return TRUE;
}


void StopSEHService(void)
{
}


//MINGW32 exception handler stub
EXCEPTION_DISPOSITION __stdcall __welees_ExceptionPreHandler(IN PEXCEPTION_RECORD pER,IN PEXCEPTION_NODE pNode,IN PCONTEXT pContext,IN PVOID pDC)
{
	int                   i;
	EXCEPTION_POINTERS    Param;
	EXCEPTION_DISPOSITION edResult;
	
	printf("Current exception node %p\n",pNode);
	Param.ContextRecord=pContext;
	Param.ExceptionRecord=pER;
	i=pNode->CustomHandler(&Param);
	printf("Exception filter result %d\n",i);
	switch(i)
	{
		case EXCEPTION_CONTINUE_EXECUTION:
			printf("%s %d\n",__FILE__,__LINE__);
			return ExceptionContinueExecution;
		case EXCEPTION_EXECUTE_HANDLER:
			printf("%s %d\n",__FILE__,__LINE__);
#ifndef _MINGW //For debug only
			{
				PEXCEPTION_NODE pNext=pNode->Prev;
				__asm
				{
					mov eax,pNext
					mov fs:[0],eax
				}
			}
#else //_MINGW
			asm volatile("mov %0, %%fs:0" : : "r" (pNode->Prev));
#endif //_MINGW
			pNode->Prev=NULL;
			longjmp(pNode->SectionEntry,1);
		case EXCEPTION_CONTINUE_SEARCH:
			printf("%s %d\n",__FILE__,__LINE__);
			return ExceptionContinueSearch;
	}
}
#endif //defined(_WIN32)&&defined(_MINGW)&&(!defined(_WIN64))
