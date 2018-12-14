#pragma once
#include <vector>

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

public:
	GlutManager();
	~GlutManager();
	static GlutManager& getInstance()
	{
		static GlutManager instance;
		return instance;
	}

	void EnterMainLoop();
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
};
