#include "../../Defines.h"
#include "RMMeshLoader.h"

#include "../Resources/MeshResource.h"
#include <fstream>
#include <sstream>

#include <map>
#include <unordered_map>

Resource* RMMeshLoader::load(const char* path, const long GUID)
{
	// MeshResource Variables
	/// ----------------------------------------
	int numberOfVertices;
	int numberOfIndices;
	float* verticesDataPtr;
	uint32_t* indicesPtr;
	/// ----------------------------------------

	//Check if asset is in package
	std::string filePath = path;
	size_t check = 0;
	check = filePath.find(".zip");
	bool loadZipped = false;
	if (check < filePath.length()) {
		loadZipped = true;
	}

	auto marker = MemoryManager::getInstance().getStackMarker(FUNCTION_STACK_INDEX);

	// STEP 1: LOAD THE 'RMMesh' FILE
	if (!loadZipped) {
		std::ifstream inputFile;
		inputFile.open(path);


		if (!inputFile.is_open())
			RM_DEBUG_MESSAGE("ERROR: File could not be opened: " + std::string(path), 1);

		std::string tempString;
		// # OF VERTICES
		std::getline(inputFile, tempString);
		numberOfVertices = std::stoi(tempString);
		// # OF INDICES
		std::getline(inputFile, tempString);
		numberOfIndices = std::stoi(tempString);

		//verticesData.resize(numberOfVertices * 8);
		verticesDataPtr = new (RM_MALLOC_FUNCTION(numberOfVertices * 8 * sizeof(float))) float;
		for (int i = 0; i < numberOfVertices; i++) // ORDER: p.X, p.Y, p.Z, n.X, n.Y, n.Z, U, V
		{
			std::getline(inputFile, tempString, ',');
			verticesDataPtr[i] = std::stof(tempString);
		}

		indicesPtr = new (RM_MALLOC_FUNCTION(numberOfIndices * sizeof(uint32_t))) uint32_t;
		for (int i = 0; i < numberOfIndices; i++)
		{
			std::getline(inputFile, tempString, ',');
			indicesPtr[i] = static_cast<uint32_t>(std::stoi(tempString));
		}
	}
	else {
		void* ptr = readFile(path, check);
		std::string tempString = (char*)ptr;
		std::stringstream inputFile;
		inputFile << tempString;
		// # OF VERTICES
		std::getline(inputFile, tempString);
		numberOfVertices = std::stoi(tempString);
		// # OF INDICES
		std::getline(inputFile, tempString);
		numberOfIndices = std::stoi(tempString);

		verticesDataPtr = new (RM_MALLOC_FUNCTION(numberOfVertices * sizeof(float))) float;
		for (int i = 0; i < numberOfVertices; i++) // ORDER: p.X, p.Y, p.Z, n.X, n.Y, n.Z, U, V
		{
			std::getline(inputFile, tempString, ',');
			verticesDataPtr[i] = std::stof(tempString);
		}

		indicesPtr = new (RM_MALLOC_FUNCTION(numberOfIndices * sizeof(uint32_t))) uint32_t;
		for (int i = 0; i < numberOfIndices; i++)
		{
			std::getline(inputFile, tempString, ',');
			indicesPtr[i] = static_cast<uint32_t>(std::stoi(tempString));
		}
		free(ptr);
	}


	unsigned int sizeOnRam = sizeof(MeshResource);
	MeshResource* meshToBeReturned = new (RM_MALLOC_PERSISTENT(sizeOnRam)) MeshResource(verticesDataPtr, indicesPtr, numberOfVertices, numberOfIndices, GUID);
	meshToBeReturned->setSize(sizeOnRam);

	// Clear everything we've allocated during the function
	MemoryManager::getInstance().deallocateStack(FUNCTION_STACK_INDEX, marker);

	/// ----------------------------------------------------
	return meshToBeReturned;
}