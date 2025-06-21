#pragma once


#ifndef _MINGW //For debug only
#define _REGISTER_SEH_NODE \
	_asm{mov eax,fs:[0]}; \
	_asm{mov [__weLees_Pattern_SEH_Section.Prev],eax}; \
	_asm{lea eax, [__weLees_Pattern_SEH_Section]}; \
	_asm{mov fs:[0],eax};


#define _UNREGISTER_SEH_NODE \
	_asm{mov eax,__weLees_Pattern_SEH_Section.Prev}; \
	_asm{mov fs:[0],eax}; \
	__weLees_Pattern_SEH_Section.Prev=NULL;

#else //_MINGW

#define _REGISTER_SEH_NODE \
	asm volatile \
	( \
		"mov %%fs:0, %%eax\n\t" \
		"mov %%eax, %[except_node_next]\n\t" \
		"lea %[except_node], %%eax\n\t" \
		"mov %%eax, %%fs:0" \
		: /* no outputs */ \
		: [except_node_next] "m" (__weLees_Pattern_SEH_Section.Prev), \
		  [except_node] "m" (__weLees_Pattern_SEH_Section) \
		: "%eax", "memory" \
	);


#define _UNREGISTER_SEH_NODE \
	asm volatile \
	( \
		"mov %[except_node_next], %%eax\n\t" \
		"mov %%eax, %%fs:0" \
		: /* no outputs */ \
		: [except_node_next] "m" (__weLees_Pattern_SEH_Section.Prev) \
		: "%eax", "memory" \
	); \
	__weLees_Pattern_SEH_Section.Prev=NULL;
#endif //_MINGW


typedef struct _EXCEPTION_NODE
{
	struct _EXCEPTION_NODE *Prev;
	FARPROC                HandlerEntry;
	int                    (*CustomHandler)(IN PEXCEPTION_POINTERS pContext);
	jmp_buf                SectionEntry;
	UINT                   RunStatus;
#if defined(__cplusplus) //for cpp, unregister exception section when call break/return instruction
	_EXCEPTION_NODE(void)
	{
		if(Prev)
		{
#ifndef _MINGW //For debug only
			_asm{mov eax,Prev};
			_asm{mov fs:[0],eax};
			Prev=NULL;
#else //_MINGW
			asm volatile
			(
				"mov %[except_node_next], %%eax\n\t"
				"mov %%eax, %%fs:0"
				: /* no outputs */
				: [except_node_next] "m" (Prev)
				: "%eax", "memory"
			);
			Prev=NULL;
#endif //_MINGW
		}
	}
#endif //defined(__cplusplus)
}EXCEPTION_NODE,*PEXCEPTION_NODE;


EXCEPTION_DISPOSITION __stdcall __welees_ExceptionPreHandler(IN PEXCEPTION_RECORD pER,IN PEXCEPTION_NODE pNode,IN PCONTEXT pContext,IN PVOID pDC);


#define TRY_START \
	{ \
		EXCEPTION_NODE __weLees_Pattern_SEH_Section; \
		/*Save local exception node*/ \
		 \
		__weLees_Pattern_SEH_Section.HandlerEntry=(FARPROC)__welees_ExceptionPreHandler; \
		__weLees_Pattern_SEH_Section.RunStatus=setjmp(__weLees_Pattern_SEH_Section.SectionEntry); \
		if(2==__weLees_Pattern_SEH_Section.RunStatus) \
		{/*Init exception node*/ \
			SEH_LOG(("%s %d Register exception Node %pH\n",__FILE__,__LINE__,&__weLees_Pattern_SEH_Section)); \
			_REGISTER_SEH_NODE \
			/*Run custom code*/

#define TRY_EXCEPT(filter) \
			SEH_LOG(("%s %d Unregister exception Node %pH\n",__FILE__,__LINE__,&__weLees_Pattern_SEH_Section)); \
			_UNREGISTER_SEH_NODE \
		} \
		else if(!__weLees_Pattern_SEH_Section.RunStatus) \
		{/*Store the filter into exception node*/ \
			__weLees_Pattern_SEH_Section.CustomHandler=filter; \
			longjmp(__weLees_Pattern_SEH_Section.SectionEntry,2); \
		} \
		else

#define TRY_END }


#if defined(__cplusplus)

#define TRY_BREAK   _UNREGISTER_SEH_NODE;break
#define TRY_BREAK2  _UNREGISTER_SEH_NODE;_UNREGISTER_SEH_NODE;break
#define TRY_BREAK3  _UNREGISTER_SEH_NODE;_UNREGISTER_SEH_NODE;_UNREGISTER_SEH_NODE;break


#define TRY_RETURN  _UNREGISTER_SEH_NODE;return
#define TRY_RETURN2 _UNREGISTER_SEH_NODE;_UNREGISTER_SEH_NODE;return
#define TRY_RETURN3 _UNREGISTER_SEH_NODE;_UNREGISTER_SEH_NODE;_UNREGISTER_SEH_NODE;return

#else //defined(__cplusplus)

#define TRY_BREAK   break
#define TRY_BREAK2  break
#define TRY_BREAK3  break


#define TRY_RETURN  return
#define TRY_RETURN2 return
#define TRY_RETURN3 return

#endif //defined(__cplusplus)
