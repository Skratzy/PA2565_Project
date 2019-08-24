#include "FormatLoader.h"
//#include <ziplib/zip.h>
#include <fstream>
#include <experimental/filesystem>

#include "../../../lib/miniz/miniz.h"


std::string FormatLoader::extractFile(const char* path, size_t check) {
	// substrings for parts of the path
	std::experimental::filesystem::path filePath(path);
	std::string zipPath = filePath.string();
	// Convert from double back-slash to forward-slash (damnit windows...)
	std::replace(zipPath.begin(), zipPath.end(), '\\', '/');
	std::string zipLocation = zipPath.substr(0, check + 4);
	std::string pathInPackage = zipPath.substr(check + 5, zipPath.length());
	std::string fileName = filePath.filename().string();
	// Opening and extracting asset from package
	//int error = 0;
	mz_zip_archive archive;
	memset(&archive, 0, sizeof(archive));
	auto status = mz_zip_reader_init_file(&archive, zipLocation.c_str(), 0);
	if (!status) 
		RM_DEBUG_MESSAGE("Error while trying to open zip archive: " + zipLocation, 1);


	auto index = mz_zip_reader_locate_file(&archive, pathInPackage.c_str(), "", 0);
	mz_zip_archive_file_stat fileStat;
	mz_zip_reader_file_stat(&archive, index, &fileStat);
	void* buffer = malloc(static_cast<size_t>(fileStat.m_uncomp_size));
	// TODO: Replace with extract_to_heap
	mz_zip_reader_extract_to_mem(&archive, index, buffer, fileStat.m_uncomp_size, 0);

	std::ofstream aFile;
	aFile.open(fileName.c_str(), std::ios::out | std::ios::binary);
	aFile.write((char*)buffer, fileStat.m_uncomp_size);
	aFile.close();
	mz_zip_reader_end(&archive);
	free(buffer);

	zipPath = fileName;
	return zipPath;
}

void* FormatLoader::readFile(const char* path, size_t check) {
	// substrings for parts of the path
	std::experimental::filesystem::path filePath(path);
	std::string zipPath = filePath.string();
	// Convert from double back-slash to forward-slash (damnit windows...)
	std::replace(zipPath.begin(), zipPath.end(), '\\', '/');
	std::string zipLocation = zipPath.substr(0, check + 4);
	std::string pathInPackage = zipPath.substr(check + 5, zipPath.length());
	// Opening and extracting asset from package
	mz_zip_archive archive;
	memset(&archive, 0, sizeof(archive));
	auto status = mz_zip_reader_init_file(&archive, zipLocation.c_str(), 0);
	if (!status)
		RM_DEBUG_MESSAGE("Error while trying to open zip archive: " + zipLocation, 1);
	
	auto index = mz_zip_reader_locate_file(&archive, pathInPackage.c_str(), "", 0);
	mz_zip_archive_file_stat fileStat;
	mz_zip_reader_file_stat(&archive, index, &fileStat);
	void* buffer = malloc(static_cast<size_t>(fileStat.m_uncomp_size));
	// TODO: Replace with extract_to_heap
	mz_zip_reader_extract_to_mem(&archive, index, buffer, fileStat.m_uncomp_size, 0);

	mz_zip_reader_end(&archive);

	return buffer;
}