#include "Allocator.h"

Allocator::Allocator(void* memPtr, unsigned int sizeBytes)
{
	m_memPtr = memPtr;
	m_sizeBytes = sizeBytes;
}

Allocator::~Allocator() 
{
}