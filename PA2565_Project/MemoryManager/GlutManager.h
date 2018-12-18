#pragma once
#include <vector>
#include <mutex>
/*
HOW TO TRACK THE MEMORY:
- #include "GlutManager.hpp"
- GlutManager& instance = GlutManager::getInstance();
- GlutManager* ptr = &instance;

- auto function = [&ptr, etc, etc] {
	- do some shit with memory, then get it as two vectors of stacks/pools
	- std::vector<std::vector<bool>> stacks;
	- std::vector<std::vector<bool>> pools;
	- ptr->updateVectors(stacks, pools);
}
- std::thread threadname(function);

NOTE: GlutManager is only functioning from the main thread, and therefore
every other part of the program needs to work as a thread
*/
class GlutManager {
private:
	void initialize(int argc, char **argv);
	std::vector<std::vector<bool>> createFauxTestData();
	std::vector<std::vector<bool>> m_stacks;
	std::vector<std::vector<bool>> m_pools;
	std::vector<bool> m_loadingStack;
	std::vector<bool> m_asyncStack;
	std::vector<std::vector<bool>> m_asyncArrays;
	std::mutex m_asyncMutex;

public:
	GlutManager();
	~GlutManager();
	static GlutManager& getInstance()
	{
		static GlutManager instance;
		return instance;
	}

	void EnterMainLoop();
	void LeaveMainLoop();
	/*
	Vector: Stacks, Pools
		Vector: Number of stacks
			Vector: bools of entries per stack
		Vector: Number of pools
			Vector: bools of entries per pool
	*/
	std::vector<std::vector<std::vector<bool>>> getVectors();
	void updateVectors(std::vector<std::vector<bool>>& stacks, std::vector<std::vector<bool>>& pools);
	void addStack(std::vector<bool> stack);
	void addPool(std::vector<bool> pool);

	std::vector<std::vector<bool>> getLoadingVectors();
	void updateLoadingVector();
	void cleanLoadingVector();
	void updateAsyncVector();
	void cleanAsyncVector();

	std::vector<std::vector<bool>> getAsync() {
		return m_asyncArrays;
	}
	int addAsyncArray(std::vector<bool> vector) {
		std::unique_lock<std::mutex> lock(m_asyncMutex);
		this->m_asyncArrays.clear();
		this->m_asyncArrays.push_back(vector);
		int returnIndex = (this->m_asyncArrays.size() - 1);
		lock.unlock();

		return returnIndex;
	}
	void cleanAsyncArrays() {
		m_asyncArrays.clear();
		std::vector<bool> tempVector;
		tempVector.push_back(false);
		m_asyncArrays.push_back(tempVector);
	}
	void alterAsyncArray(int arrayIndex, int boolIndex, bool value) {
		this->m_asyncArrays[arrayIndex][boolIndex] = value;
	}
};
