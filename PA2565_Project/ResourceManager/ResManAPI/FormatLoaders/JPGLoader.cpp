#include "JPGLoader.h"
#include "../Resources/TextureResource.h"
#include <experimental/filesystem>

#define cimg_use_jpeg
#include "CImg.h"


Resource * JPGLoader::load(const char* path, const long GUID)
{
	using namespace cimg_library;

	// Checking if the asset is in a package
	std::string filePath = path;
	size_t check = 0;
	check = filePath.find(".zip");
	bool loadZipped = false;
	if (check < filePath.length()) {
		loadZipped = true;
		filePath = extractFile(path, check);
	}

	CImg<unsigned char> source(filePath.c_str());
	unsigned int width = source.width();
	unsigned int height = source.height();
	Resource* resource = nullptr;

	auto marker = MemoryManager::getInstance().getStackMarker(FUNCTION_STACK_INDEX);
	unsigned char* imageDataPtr = new (RM_MALLOC_FUNCTION(sizeof(unsigned char) * width * height * 4)) unsigned char;

	if (source.size() == 0) {
		std::string debugMessage = (filePath.c_str() + std::string(" did not load succesfully!"));
		RM_DEBUG_MESSAGE(debugMessage, 0);
	}
	else {
		unsigned int index = 0;
		// Conversion of a single char array into the vector
		for (unsigned int y = 0; y < height; y++) {
			for (unsigned int x = 0; x < width; x++) {
				imageDataPtr[index++] = *source.data(x, y, 0, 0);
				imageDataPtr[index++] = *source.data(x, y, 0, 1);
				imageDataPtr[index++] = *source.data(x, y, 0, 2);
				imageDataPtr[index++] = unsigned char(255);
			}
		}

		// Fix size (VRAM vs RAM)
		unsigned int size = sizeof(TextureResource);// +sizeof(unsigned int) * image.size();
		// Attach the formatted image to a textureresource
		resource = new (RM_MALLOC_PERSISTENT(size)) TextureResource(width, height, imageDataPtr, GUID);
		// Size on DRAM
		resource->setSizeCPU(size);
		// Size on VRAM
		resource->setSizeGPU(width * height * sizeof(unsigned char) * 4);
	}

	MemoryManager::getInstance().deallocateStack(FUNCTION_STACK_INDEX, marker);

	if (loadZipped) {
		// Deleting extracted file once loaded in to memory
		std::experimental::filesystem::remove(filePath.c_str());
	}

	return resource;
}
