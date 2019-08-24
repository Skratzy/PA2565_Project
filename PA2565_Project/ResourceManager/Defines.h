#ifndef _RM_DEFINES_
#define _RM_DEFINES_

#include <assert.h>
#include <malloc.h>
#include <iostream>
#include <new>
#include <windows.h>

#include "../MemoryManager/MemoryManager.h"

#ifdef _DEBUG
#define RM_DEBUGGING true
#else
#define RM_DEBUGGING false
#endif

#define RM_ASSERT(s) assert(s)
/*
	s: Size in bytes
*/
#define RM_MALLOC(s) malloc(s)
/*
	s: Size in bytes
*/
#define RM_MALLOC_POOL_ALLOC(s)
constexpr auto PERSISTENT_STACK_INDEX = 0;
constexpr auto FUNCTION_STACK_INDEX = 1;

/*
	Allocates memory for a single frame
	s: Size in bytes
*/
#define RM_MALLOC_SINGLE_FRAME(s) MemoryManager::getInstance().stackAllocate(s, SINGLE_FRAME_STACK_INDEX)
/*
	Allocates memory from the persistent stack allocator
	s: Size in bytes
*/
#define RM_MALLOC_PERSISTENT(s) MemoryManager::getInstance().stackAllocate(s, PERSISTENT_STACK_INDEX)
/*
	Allocates memory from the function stack allocator
	s: Size in bytes
*/
#define RM_MALLOC_FUNCTION(s) MemoryManager::getInstance().stackAllocate(s, FUNCTION_STACK_INDEX)

/*
	TYPE: Type
*/
#define RM_NEW_PERSISTENT(TYPE) new(RM_MALLOC_PERSISTENT(sizeof(TYPE))) TYPE()
#define RM_NEW_SINGLE_FRAME(TYPE) new(RM_MALLOC_SINGLE_FRAME(sizeof(TYPE))) TYPE()
#define RM_NEW_FUNCTION(TYPE) new(RM_MALLOC_FUNCTION(sizeof(TYPE))) TYPE()
#define RM_FREE(s) free(s)
#define RM_DELETE(s) delete s
/*
	s: Debug message - to be printed
	l: Severity level
		0 -> print the message
		1 -> print the message and abort
*/
#define RM_DEBUG_MESSAGE(s, l) { \
	HANDLE stdHandle = GetStdHandle(STD_OUTPUT_HANDLE); \
	CONSOLE_SCREEN_BUFFER_INFO conScrBufInf; \
	GetConsoleScreenBufferInfo(stdHandle, &conScrBufInf); \
	switch(l) { \
	case 0: \
		SetConsoleTextAttribute(stdHandle, BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_INTENSITY); \
		std::clog << "WARNING: " << s << std::endl; \
		SetConsoleTextAttribute(stdHandle, conScrBufInf.wAttributes); \
		break; \
	case 1:\
		SetConsoleTextAttribute(stdHandle, BACKGROUND_RED | BACKGROUND_INTENSITY);\
		std::clog << "ERROR: " << s << std::endl; \
		SetConsoleTextAttribute(stdHandle, conScrBufInf.wAttributes);\
		abort();\
		break;\
	default: break;\
}}

#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE

#define SOKOL_LOG(s) RM_DEBUG_MESSAGE(s, 0)

#ifdef __cplusplus
extern "C" {
#include "Sokol/HandmadeMath.h"
#include "Sokol/sokol_gfx.h"
}
#endif

/* a uniform block with a model-view-projection matrix */
typedef struct {
	hmm_mat4 m;
	hmm_mat4 vp;
	hmm_vec4 sunDir;
} vs_params_t;

#endif //_RM_DEFINES_