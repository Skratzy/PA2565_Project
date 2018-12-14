#include "TestCases.h"

#include <chrono>
#include <future>
#include <random>
#include <iostream>

#include "Defines.h"
#include "MemoryManager.h"
#include "GlutManager.h"

TestCases::TestCases()
{
}

TestCases::~TestCases()
{
}

void TestCases::timingTest(unsigned int sizePerAlloc, unsigned int numAllocs, unsigned int numIter) 
{
	double averages[3];
	averages[0] = 0;
	averages[1] = 0;
	averages[2] = 0;
	MemoryManager& memMgr = MemoryManager::getInstance();
	memMgr.cleanUp();

 	auto testFunc = [&sizePerAlloc, &numAllocs, &averages]() {
		MemoryManager& memMgr = MemoryManager::getInstance();
		std::vector<PoolInstance> pi;

		// Make sure the memory manager is cleared of everything
		memMgr.cleanUp();

		unsigned int size = sizePerAlloc;
		unsigned int numAllocations = numAllocs;
		pi.push_back(PoolInstance{ size, numAllocations, 4 });

		memMgr.init(size * numAllocations, pi);

		// Pool allocation
		auto start = std::chrono::high_resolution_clock::now();
		for (unsigned int i = 0; i < numAllocations; i++) {
			void* ptr = memMgr.poolAllocate(size);
		}
		auto end = std::chrono::high_resolution_clock::now();
		auto timeSpan = std::chrono::duration_cast<std::chrono::duration<double>>(end - start);
		averages[0] += timeSpan.count();

		// Stack allocation
		start = std::chrono::high_resolution_clock::now();
		for (unsigned int i = 0; i < numAllocations; i++) {
			void* ptr = memMgr.singleFrameAllocate(size);
		}
		end = std::chrono::high_resolution_clock::now();
		timeSpan = std::chrono::duration_cast<std::chrono::duration<double>>(end - start);
		averages[1] += timeSpan.count();

		// Malloc
		start = std::chrono::high_resolution_clock::now();
		for (unsigned int i = 0; i < numAllocations; i++)
			void* ptr = malloc(size);
		end = std::chrono::high_resolution_clock::now();
		timeSpan = std::chrono::duration_cast<std::chrono::duration<double>>(end - start);
		averages[2] += timeSpan.count();
	};

	for (unsigned int i = 0; i < numIter; i++) {
		std::thread t1(testFunc);

		t1.join();
	}

	for(unsigned int i = 0; i < 3; i++)
		averages[i] /= static_cast<double>(numIter);

	std::cout << "Size of each allocation [" << sizePerAlloc << " bytes]. Number of allocations [" << numAllocs << "]. Number of iterations: "  << numIter << "." << std::endl;
	std::cout << "Our pool allocation took on average: \t" << averages[0] << " seconds." << std::endl;
	std::cout << "Our stack allocation took on average: \t" << averages[1] << " seconds." << std::endl;
	std::cout << "Native malloc took on average: \t\t" << averages[2] << " seconds." << std::endl;

}

void TestCases::poolAllocDealloc()
{
	MemoryManager& memMgr = MemoryManager::getInstance();
	memMgr.cleanUp();

	std::vector<PoolInstance> pi;

	unsigned int size = 64; // The size of a 4x4 matrix of floats
	unsigned int numAssignments = ARCH_BYTESIZE * 50;
	unsigned int maxSizeBytes = size * numAssignments;
	pi.push_back(PoolInstance{ size, numAssignments, 4 });

	memMgr.init(size * numAssignments, pi);

	auto testFunc = [&numAssignments, &size]() {
		MemoryManager& memMgr = MemoryManager::getInstance();

		srand(static_cast<unsigned int>(time(0)));
		std::vector<void*> poolPointers;
		while (true) {
			// Try-catch clause to catch any overflow-throws
			try {
				// "40%" chance of allocating from the pool
				if (rand() % 10 < 4 || poolPointers.size() < numAssignments / 8) {
					poolPointers.push_back(memMgr.poolAllocate(size));
				}
				// "60%" chance of deallocating from the pool
				else {
					if (poolPointers.size() > 0) {
						unsigned int index = rand() % poolPointers.size();
						memMgr.deallocateSinglePool(poolPointers[index], size);
						std::swap(poolPointers[index], poolPointers.back());
						poolPointers.pop_back();
					}
				}

				// "90%" chance of allocating from the stack
				if (rand() % 100 < 90) {
					memMgr.singleFrameAllocate(rand() % 256);
				}
				// "10%" chance of deallocating everything from the stack
				else {
					memMgr.deallocateSingleFrameStack();
				}

			}
			catch (std::exception e) {
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(40));
		}

	};
	std::thread t1(testFunc);
	std::thread t2(testFunc);
	std::thread t3(testFunc);
	std::thread t4(testFunc);

	auto allocatedSpaceUpdateFunction = []() {
		GlutManager& glutMngr = GlutManager::getInstance();
		MemoryManager& memMngr = MemoryManager::getInstance();

		auto allocatedSpace = memMngr.getAllocatedSpace();
		std::vector<std::vector<bool>> stacks(1);
		std::this_thread::sleep_for(std::chrono::milliseconds(500));

		// Update the vectors of the GUI
		while (true) {
			auto allocatedSpace = memMngr.getAllocatedSpace();
			stacks.at(0) = allocatedSpace.stacks;
			glutMngr.updateVectors(stacks, allocatedSpace.pools);
			memMngr.updateAllocatedSpace();
			std::this_thread::sleep_for(std::chrono::milliseconds(20));
		}
	};
	std::this_thread::sleep_for(std::chrono::seconds(1));
	std::thread t5(allocatedSpaceUpdateFunction);
	GlutManager& glutMngr = GlutManager::getInstance();
	glutMngr.EnterMainLoop();

	t1.join();
	t2.join();
	t3.join();
	t4.join();
	t5.join();
}