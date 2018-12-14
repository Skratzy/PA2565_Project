#include "MemoryManager.h"

#include <string>

/*+-+-+-+-+-+-+-+-+-+-+-+
	PRIVATE FUNCTIONS
+-+-+-+-+-+-+-+-+-+-+-+*/

void* MemoryManager::getMem(unsigned int sizeBytes)
{
	return calloc(1, sizeBytes);
}

void MemoryManager::addPool(unsigned int sizeBytesEachEntry, unsigned int numEntries, unsigned int numQuadrants)
{
	PoolAllocator* temp = new PoolAllocator(getMem(sizeBytesEachEntry * numEntries), sizeBytesEachEntry, numEntries, numQuadrants);
	std::vector<PoolAllocator*>::iterator it = m_pools.begin();
	int pos = 0;
	bool largerFound = false;

	// Check for pools with entires larger than the one being added
	for (it; it != m_pools.end(); it++) {
		if (sizeBytesEachEntry < (*it)->getEntrySize()) {
			// If found a pool with larger entries, insert the new pool before the larger pool
			m_pools.insert(it, temp);
			largerFound = true;
			return;
		}
	}
	if (!largerFound) {
		// No pool found, add it to the back of the vector
		m_pools.push_back(temp);
	}
}

void MemoryManager::addStack(unsigned int sizeBytes)
{
	if (m_stack == nullptr)
		m_stack = new StackAllocator(getMem(sizeBytes), sizeBytes);
	else
		throw std::exception("MemoryManager::addStack : Stack already created");


}





/*+-+-+-+-+-+-+-+-+-+-+-+
	 PUBLIC FUNCTIONS
+-+-+-+-+-+-+-+-+-+-+-+*/

MemoryManager::MemoryManager()
{
	m_stack = nullptr;
}
MemoryManager::~MemoryManager()
{
	cleanUp();
}

void MemoryManager::init(unsigned int stackSizeBytes, std::vector<PoolInstance> poolInstances)
{
	// Currently we only have one stack (single-frame stack)
	addStack(stackSizeBytes);
	// Creating 'vector<bool>' for memory tacking purposes
	m_currMemUsage.stacks = m_stack->getUsedMemory();

	int currIndex = 0;
	// Initializing each 'PoolInstance' into an actual pool in the memory manager
	for (PoolInstance PI : poolInstances) {
		if (PI.numEntries % PI.numQuadrants != 0)
			throw std::exception(("The number of entries in PoolInstance " + std::to_string(currIndex) + " was not divisible with the number of quadrants.").c_str());
		// Inserting the pool into the 'm_pools' vector
		addPool(PI.sizeBytesEachEntry, PI.numEntries, PI.numQuadrants);
		// Creating 'vector<bool>' for memory tacking purposes
		m_currMemUsage.pools.push_back(m_pools.at(currIndex++)->getUsedMemory());
	}
}

void* MemoryManager::singleFrameAllocate(unsigned int sizeBytes) {
	return m_stack->allocate(sizeBytes);
}

void* MemoryManager::poolAllocate(unsigned int sizeBytes) {
	void* ptrToAllocation = nullptr;
	// Check for an appropriate sized pool; if it already exists
	for (auto it = m_pools.begin(); it != m_pools.end(); ++it) {
		if (sizeBytes <= (*it)->getEntrySize()) {
			ptrToAllocation = (*it)->allocate();
			break;
		}
	}

	if (ptrToAllocation == nullptr)
		throw std::exception("MemoryManager::poolAllocate : No pool allocated for size " + sizeBytes);

	return ptrToAllocation;
}

void MemoryManager::deallocateSinglePool(void* ptr, unsigned int sizeOfAlloc)
{
	// We look for which pool the object exists in (ordered: smallest pool -> biggest pool)
	for (unsigned int i = 0; i < m_pools.size(); i++)
		if (sizeOfAlloc <= m_pools.at(i)->getEntrySize())
			m_pools.at(i)->deallocateSingle(ptr); // We then deallocate it
}

void MemoryManager::deallocateAllPools()
{
	for (unsigned int i = 0; i < m_pools.size(); i++)
		m_pools.at(i)->deallocateAll();
}

void MemoryManager::deallocateSingleFrameStack() {
	m_stack->deallocateAll();
}

void MemoryManager::updateAllocatedSpace()
{
	// Updates the 'vector<bool>' for the SF-stack for GLUT (used to visualize memory consumption)
	m_currMemUsage.stacks = m_stack->getUsedMemory();

	for (unsigned int i = 0; i < m_pools.size(); i++) {
		// Updates the multiple 'vector<bool>' for GLUT (used to visualize memory consumption)
		m_currMemUsage.pools.at(i) = m_pools.at(i)->getUsedMemory();
	}
}

MemoryUsage& MemoryManager::getAllocatedSpace()
{
	return m_currMemUsage;
}

void MemoryManager::cleanUp()
{
	size_t loopCount;

	loopCount = m_pools.size();
	for (unsigned int i = 0; i < loopCount; i++)
		if (m_pools.at(i) != nullptr)
			delete m_pools.at(i);
	m_pools.clear();
	m_pools.resize(0);

	delete m_stack;
	m_stack = nullptr;

	m_currMemUsage.pools.clear();
	m_currMemUsage.stacks.clear();
}
