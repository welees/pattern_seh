#pragma once


#ifdef _DEBUG
#define SEH_LOG(x) printf x
#else //_DEBUG
#define SEH_LOG(x)
#endif //_DEBUG


#define IN
#define OUT


#define EXCEPTION_EXECUTE_HANDLER    1
#define EXCEPTION_CONTINUE_SEARCH    0
#define EXCEPTION_CONTINUE_EXECUTION -1


#if defined(_WIN32)


#define THREAD_CONTEXT __declspec(thread)

#if defined(_MINGW)
#if defined(_WIN64)
#include "SEH_MingW64.h"
#else //defined(_WIN64)
#include "SEH_MingW32.h"
#endif //defined(_WIN64)
#else //defined(_MINGW)

#define TRY_START          __try
#define TRY_EXCEPT(filter) __except(filter(GetExceptionInformation()))
#define TRY_END
#define TRY_BREAK          break
#define TRY_BREAK2         break
#define TRY_BREAK3         break
#define TRY_RETURN         return
#define TRY_RETURN2        return
#define TRY_RETURN3        return

#endif //defined(_MINGW)
#else //defined(_WIN32)
#include "SEH_NonWin.h"
#endif //defined(_WIN32)


int StartSEHService(void);


void StopSEHService(void);
