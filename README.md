# This project was developed by weLees group.
# mail to support@welees.com for any advise or question

# pattern_seh
Approximate Implementation of Windows SEH which works on MAC/Linux/Windows Platform

# RELEASE INFO
  2025.6.20 release ver 1.0
  
      - Support MingW64
      - Optimize Non-Windows platform code
      - Fix nest exception section maintaining bug
      
  2025.5.20 release ver 0.99
  
      - Support MingW32
      - Support MACOS
      - Fixed the problem that raising an exception in the exception handling block would generate an exception repeatedly in a loop.
      - Using thread-independent exception handler linking table

  2024.11.16 release ver 0.9
  
    First release.
      
      - Support nesting exception handler
      
      - Do not support __finally
      - Do not support un-handled exception handler for whole process

# Project Description
This project is an imitation of the SEH mechanism of Windows on the MAC/Linux platform, in order to capture and repair exceptions in the simplest way possible, and to allow cross-platform code to use the same code and exception handling logic as much as possible.
This project can work on the MAC/Linux/Windows platform. If you need to run on  other platforms, please refer to the differences in MAC/Linux implementations and make corresponding modifications.

Users should use pattern seh in the following way:

    TRY_START
    
    {
      
      Code that may cause exceptions
      
    }
    
    TRY_EXCEPT(ExceptionFilter)
    
    {
    
      Exception detection/repair code
      
    }
    
    TRY_END

Since the exception capture mechanism of Linux/MAC is different from that of Windows, and supporting multiple platforms is a very labor-intensive task, this project does not pursue complete consistency with Windows SEH. If the user wants to use the same set of codes on all platforms, please define ExceptionFilter as a platform-related macro.

On MAC/Linux platforms, the declaration of the ExceptionFilter function is

typedef int (*ExceptionFilter_MAC_LINUX)(int iSignal,siginfo_t *pSignalInfo,void *pContext)

On Windows platforms, it should be

typedef int (*Filter_WINDOW)(IN PEXCEPTION_POINTERS pExceptionPointer)

When an exception occurs, pattern SEH will traverse and call the exception filter registered through TRY_START. In the filter, the user needs to handle the exception & instruct pattern SEH how to perform the next action according to the actual situation.

The exception filter should return one of followed 3 values, which are

#define EXCEPTION_EXECUTE_HANDLER 1

#define EXCEPTION_CONTINUE_SEARCH 0

#define EXCEPTION_CONTINUE_EXECUTION -1

EXCEPTION_EXECUTE_HANDLER
    means that the current filter cannot fix the exception, but can perform the clean up work. Pattern SEH will execute the code in the TRY_EXCEPT{} block.

EXCEPTION_CONTINUE_SEARCH
    means that the current filter cannot fix the exception, and cannot even perform the clean up of current  environment. Pattern SEH will try to traverse all nested outer exception blocks in the current thread, switch to their stacks and run exception filters. When all nested exception blocks are traversed, it will report the exception and end the program as the OS default behavor.

EXCEPTION_CONTINUE_EXECUTION
    indicates that the exception has been fixed, and pattern SEH can try to return to the location where the exception occurred and re-run the code which triggered the exception.

NOTE: like Windows SEH, the nesting of exception blocks is thread-related, and exception blocks in different threads will not be nested with each other.

If the user needs to exit the loop from an exception section, use TRY_BREAK instead of the break instruction to ensure that the exception handling chain is properly maintained.

Users should not use the return/goto instruction to leave an exception section from within an exception block, as this will cause a crash!
