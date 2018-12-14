#ifndef STACK_ALLOCATOR
#define STACK_ALLOCATOR

#include <atomic>

#include "Allocator.h"

// DEFINITIONS
// -------------------------
typedef unsigned int Marker;
// -------------------------

class StackAllocator : private Allocator 
{
private: /// VARIABLES
	std::atomic<Marker> m_marker;

private: /// FUNCTIONS
	unsigned int padMemory(unsigned int sizeBytes);
	void cleanUp();

public: /// FUNCTIONS
	StackAllocator(void* memPtr, unsigned int sizeBytes);
	virtual ~StackAllocator();

	void* allocate(unsigned int sizeBytes);
	virtual void deallocateAll();

	Marker getMarker();
	void clearToMarker(Marker marker);

	// Memory tracking for our GLUT-Manager (Drawing our stack)
	virtual std::vector<bool> getUsedMemory();
};

#endif //STACK_ALLOCATOR
