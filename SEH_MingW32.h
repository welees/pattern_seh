#pragma once


#ifndef _MINGW //For debug only
#define _REGISTER_SEH_NODE \
	_asm{mov eax,fs:[0]}; \
	_asm{mov [__weLees__ExceptNode.Next],eax}; \
	_asm{lea eax, [__weLees__ExceptNode]}; \
	_asm{mov fs:[0],eax};


#define _UNREGISTER_SEH_NODE \
	_asm{mov eax,__weLees__ExceptNode.Next}; \
	_asm{mov fs:[0],eax};

#else //_MINGW

#define _REGISTER_SEH_NODE \
	asm volatile \
	( \
		"mov %%fs:0, %%eax\n\t" \
		"mov %%eax, %[except_node_next]\n\t" \
		"lea %[except_node], %%eax\n\t" \
		"mov %%eax, %%fs:0" \
		: /* no outputs */ \
		: [except_node_next] "m" (__weLees__ExceptNode.Next), \
		  [except_node] "m" (__weLees__ExceptNode) \
		: "%eax", "memory" \
	);


#define _UNREGISTER_SEH_NODE \
	asm volatile \
	( \
		"mov %[except_node_next], %%eax\n\t" \
		"mov %%eax, %%fs:0" \
		: /* no outputs */ \
		: [except_node_next] "m" (__weLees__ExceptNode.Next) \
		: "%eax", "memory" \
	);
#endif //_MINGW


typedef struct _EXCEPTION_HANDLER_NODE
{
	struct _EXCEPTION_HANDLER_NODE *Next;
	FARPROC                        HandlerEntry;
	int (*CustomHandler)(IN PEXCEPTION_POINTERS pContext);
	jmp_buf                        Final;
}EXCEPTION_HANDLER_NODE,*PEXCEPTION_HANDLER_NODE;

EXCEPTION_DISPOSITION __stdcall __welees_ExceptionPreHandler(IN PEXCEPTION_RECORD pER,IN PEXCEPTION_HANDLER_NODE pNode,IN PCONTEXT pContext,IN PVOID pDC);



#define TRY_START \
	{ \
		int                    __weLees__i; \
		EXCEPTION_HANDLER_NODE __weLees__ExceptNode; \
		/*Save local exception node*/ \
		 \
		__weLees__ExceptNode.HandlerEntry=(FARPROC)__welees_ExceptionPreHandler; \
		__weLees__i=setjmp(__weLees__ExceptNode.Final); \
		if(2==__weLees__i) \
		{/*Init exception node*/ \
			SEH_LOG(("%s %d Register exception Node %pH\n",__FILE__,__LINE__,&__weLees__ExceptNode)); \
			_REGISTER_SEH_NODE \
			/*Run custom code*/

#define TRY_EXCEPT(filter) \
			SEH_LOG(("%s %d Unregister exception Node %pH\n",__FILE__,__LINE__,&__weLees__ExceptNode)); \
			_UNREGISTER_SEH_NODE \
		} \
		else if(!__weLees__i) \
		{/*Store the filter into exception node*/ \
			__weLees__ExceptNode.CustomHandler=filter; \
			longjmp(__weLees__ExceptNode.Final,2); \
		} \
		else

#define TRY_END }

#define TRY_BREAK _UNREGISTER_SEH_NODE;break
