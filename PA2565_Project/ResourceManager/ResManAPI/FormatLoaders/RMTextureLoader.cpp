#include "RMTextureLoader.h"
#include <fstream>
#include <sstream>
#include "../Resources/TextureResource.h"
#include "../Resources/Resource.h"
#include "../../Defines.h"

RMTextureLoader::RMTextureLoader()
{
	this->m_supportedExtensions.push_back(".rmtex");
}

RMTextureLoader::~RMTextureLoader()
{
}

Resource * RMTextureLoader::load(const char * path, const long GUID)
{
	using namespace std;

	// If it's in a zip, deal with it appropriately
	std::string filePath = path;
	size_t check = 0;
	check = filePath.find(".zip");
	bool loadZipped = false;
	if (check < filePath.length()) {
		loadZipped = true;
	}
	unsigned int width;
	unsigned int height;
	string lineData;
	std::vector<unsigned char> imageData;
	unsigned char* imageDataPtr;

	string colourData[4];
	enum COLOR { RED, BLUE, GREEN, ALPHA, COUNT };
	unsigned int lineIndex = 0;

	auto marker = MemoryManager::getInstance().getStackMarker(FUNCTION_STACK_INDEX);
	unsigned int imageDataIndex = 0;
	// Start the loading process with the correct filepath
	if (!loadZipped) {
		ifstream inputStream(path, std::ios_base::in | std::ios_base::binary);

		std::string sWidth;
		std::string sHeight;
		getline(inputStream, sWidth);			// Load Width
		getline(inputStream, sHeight);			// Load Height
		width = std::stoi(sWidth);
		height = std::stoi(sHeight);

		imageDataPtr = new (RM_MALLOC_FUNCTION(sizeof(unsigned char) * width * height * 4)) unsigned char;

	// Read the filestream one line at a time and input the data into
	// the imageData
		while (inputStream >> lineData) {
			lineIndex = 0;
			// Per color, read the color until a comma is met, last element should be a newline
			for (int currentColor = COLOR::RED; currentColor < COLOR::COUNT; currentColor++) {
				// Fetch all the data until a newline is met

				for (; lineIndex < lineData.size();) {
					string color;
					if (lineData.at(lineIndex) != ',') {
						unsigned char character = lineData.at(lineIndex);
						colourData[currentColor].push_back(character);
						lineIndex++;
					}
					else {
						lineIndex++;
						break;
					}

				}

			}

			// Finish it up

			for (int currentColor = COLOR::RED; currentColor < COLOR::COUNT; currentColor++) {
				int castedInt = std::stoi(colourData[currentColor]);
				unsigned char castedChar = (unsigned char)castedInt;
				//imageData.push_back(castedChar);
				imageDataPtr[imageDataIndex++] = castedChar;

				// ... and clear contents in preparation of next iteration
				colourData[currentColor].clear();
			}
		}
	}
	else
	{
		void* ptr = readFile(path, check);
		
		std::string tempString = (char*)ptr;
		std::stringstream inputStream;
		inputStream << tempString;

		std::string sWidth;
		std::string sHeight;
		getline(inputStream, sWidth);			// Load Width
		getline(inputStream, sHeight);			// Load Height
		width = std::stoi(sWidth);
		height = std::stoi(sHeight);

		imageDataPtr = new (RM_MALLOC_FUNCTION(sizeof(unsigned char) * width * height * 4)) unsigned char;

		// Read the filestream one line at a time and input the data into
		// the imageData
		while (inputStream >> lineData) {
			lineIndex = 0;
			// Per color, read the color until a comma is met, last element should be a newline
			for (int currentColor = COLOR::RED; currentColor < COLOR::COUNT; currentColor++) {
				// Fetch all the data until a newline is met
				unsigned char character;

				for (; lineIndex < lineData.size();) {
					string color;
					if (lineData.at(lineIndex) != ',') {
						unsigned char character = lineData.at(lineIndex);
						colourData[currentColor].push_back(character);
						lineIndex++;
					}
					else {
						lineIndex++;
						break;
					}

				}

			}

			// Finish it up

			for (int currentColor = COLOR::RED; currentColor < COLOR::COUNT; currentColor++) {
				int castedInt = std::stoi(colourData[currentColor]);
				unsigned char castedChar = (unsigned char)castedInt;
				//imageData.push_back(castedChar);
				imageDataPtr[imageDataIndex++] = castedChar;

				// ... and clear contents in preparation of next iteration
				colourData[currentColor].clear();
			}
		}
		free(ptr);
	}

	// Fix size (VRAM vs RAM)
	unsigned int size = sizeof(TextureResource);// +sizeof(unsigned int) * image.size();
	// Attach the formatted image to a textureresource
	Resource* res = new (RM_MALLOC_PERSISTENT(size)) TextureResource(width, height, imageDataPtr, GUID);
	res->setSize(size);

	MemoryManager::getInstance().deallocateStack(FUNCTION_STACK_INDEX, marker);
	
	// Return it out!
	return res;
}
