#ifndef MEMORY_MANAGER
#define MEMORY_MANAGER

#include <vector>

#include "PoolAllocator.h"
#include "StackAllocator.h"
#include <thread>

// DEFINITIONS
// --------------------------------------
struct MemoryUsage { // All vectors of bools are visually depicted by GLUT
	std::vector<bool> stacks;
	std::vector<std::vector<bool>> pools;
};

struct PoolInstance {
	unsigned int sizeBytesEachEntry;
	unsigned int numEntries;
	unsigned int numQuadrants;
};
// ---------------------------------------

class MemoryManager
{
private: /// VARIABLES
	std::vector<PoolAllocator*> m_pools;
	StackAllocator* m_stack;

	MemoryUsage m_currMemUsage;

private: /// FUNCTIONS
	void* getMem(unsigned int sizeBytes);

	// Singleton class shouldn't be able to be copied
	MemoryManager(MemoryManager const&) = delete;
	void operator=(MemoryManager const&) = delete;

	void addPool(unsigned int sizeBytesEachEntry, unsigned int numEntries, unsigned int numQuadrants);
	void addStack(unsigned int sizeBytes);

public: /// FUNCTIONS

	static MemoryManager& getInstance()
	{
		static MemoryManager instance;

		return instance;
	}

	MemoryManager();
	~MemoryManager();

	void init(unsigned int stackSizeBytes, std::vector<PoolInstance> poolInstances);

	void* singleFrameAllocate(unsigned int sizeBytes);
	void* poolAllocate(unsigned int sizeBytes);

	void deallocateSinglePool(void* ptr, unsigned int sizeOfAlloc);
	void deallocateAllPools();
	void deallocateSingleFrameStack();

	void updateAllocatedSpace();
	MemoryUsage& getAllocatedSpace();

	void cleanUp();
};

#endif //MEMORY_MANAGER
