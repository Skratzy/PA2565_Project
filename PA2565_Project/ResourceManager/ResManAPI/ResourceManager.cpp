#include <experimental/filesystem>
#include <new>

#include "ResourceManager.h"
#include "../Defines.h"
#include "Resources/Resource.h"
#include "FormatLoaders/FormatLoader.h"
#include <fstream>
#include <ziplib/zip.h>

// Only a single thread will ever run this function
void ResourceManager::asyncLoadStart()
{
	while (m_running) {
		std::unique_lock<std::mutex> lock(m_asyncMutex);
		if(m_asyncResJobs.size() == 0 && m_running)
			m_cond.wait(lock);
		{
			// Critical region (If jobs are getting cleared)
			std::lock_guard<std::mutex> jobsClearingLock(m_clearJobsMutex);

			if (m_asyncResJobs.size() > 0 && m_running) {
				std::cout << "Started Async Loading" << std::endl;
				// Get the GUID for the next resource job
				long GUID = m_asyncJobQueue.front();
				m_asyncJobQueue.pop();
				// Find the job
				auto currJob = m_asyncResJobs.find(GUID);
				Resource* res = load(currJob->second.filepath);
				{
					std::lock_guard<std::mutex> critLock(m_asyncLoadMutex);
					auto size = currJob->second.callbacks.size();
					for (int i = 0; i < size; i++) {
						// If the job should still be performed, run the job
						if (currJob->second.callbacks[i].run) {
							// Increase the reference count of the resource
							res->refer();
							currJob->second.callbacks[i].callback(res);
						}
					}
					/*// Runs through all callbacks for the current job
					for (auto callback : currJob->second.callbacks) {

						// If the job should still be performed, run the job
						if (callback.run) {
							// Increase the reference count of the resource
							res->refer();
							callback.callback(res);
						}
					}*/
					// Decreases the reference from the resource (the initial load)
					decrementReference(res->getGUID());
					//res->derefer();
					// Remove the finished job from the map
					m_asyncResJobs.erase(currJob);
					RM_DEBUG_MESSAGE("Done with async job", 0);
				}
			}

		}
	}
}

ResourceManager::ResourceManager()
{
	m_capacity = 0;
	m_memUsage = 0;
	m_initialized = false;
	m_running = true;

	// Start the async loading thread
	m_asyncLoadThread = std::thread(std::bind(&ResourceManager::asyncLoadStart, this));
}


ResourceManager::~ResourceManager()
{
}

void ResourceManager::cleanup()
{
	m_running = false;
	m_cond.notify_all();
	m_asyncLoadThread.join();
	for (auto FL : m_formatLoaders) {
		if (FL) {
			delete FL;
			FL = nullptr;
		}
	}
	m_formatLoaders.clear();
	m_formatLoaders.resize(0);
	for (auto RES : m_resources) {
		RES.second->~Resource();
		//RM_FREE(RES.second);
	}
}

void ResourceManager::clearResourceManager()
{
	std::lock_guard<std::mutex> lock(m_clearJobsMutex);

	while (m_asyncJobQueue.size() > 0)
		m_asyncJobQueue.pop();

	m_asyncResJobs.clear();

	for (auto res : m_resources) {
		res.second->~Resource();
	}
	m_resources.clear();
}


void ResourceManager::init(const unsigned int capacity) {
	if (!m_initialized) {
		m_capacity = capacity;
		m_initialized = true;
	}
}

Resource * ResourceManager::load(const char* path)
{

	Resource* res = nullptr;
	namespace fs = std::experimental::filesystem;
	long hashedPath = m_pathHasher(path);

	// Check if the resource already exists in the system
	auto it = m_resources.find(hashedPath);
	if (it != m_resources.end()) {
		// Found the resource
		res = it->second;
		res->refer();
	}
	// Else load it
	else {
		// Only one thread can create and load new resources to the resource manager

		std::lock_guard<std::mutex> lock(m_mutex);

		// Additional check if several threads tries to load the same asset it does
		// not create the same resource more than once.
		auto it = m_resources.find(hashedPath);
		if (it != m_resources.end()) {
			// Found the resource
			res = it->second;
			res->refer();
		}
		else {
			std::string ext = fs::path(path).extension().generic_string();

			// Find the format loader corresponding to the extension
			for (auto FL : m_formatLoaders) {
				// Check if the format loader supports the extension
				if (FL->extensionSupported(ext)) {

					// Load the resource and return it
					res = FL->load(path, hashedPath);

					res->setPath(path);

					// Update memory usage
					m_memUsage += res->getSize();
					if (m_memUsage > m_capacity) {
#ifdef _DEBUG
						RM_DEBUG_MESSAGE(("ResourceManager::load() - Memory usage exceeds the memory limit. (" + std::to_string(m_memUsage / (1024)) + "KB / " + std::to_string(m_capacity / (1024)) + "KB) (Usage / Capacity)"), 0);
#else
						RM_DEBUG_MESSAGE(("ResourceManager::load() - Memory usage exceeds the memory limit. (" + std::to_string(m_memUsage / (1024)) + "KB / " + std::to_string(m_capacity / (1024)) + "KB) (Usage / Capacity)"), 0);
						RM_DEBUG_MESSAGE("Resource in memory:", 0);
						for (auto res : m_resources)
							RM_DEBUG_MESSAGE("Resource GUID: (" + std::to_string(res.second->getGUID()) + ")  Path: ("+ res.second->getPath() +")  Size: (" + std::to_string(res.second->getSize()) + " byte)", 0);
#endif
					}
					// Increase the reference count of the resource
					res->refer();
					// Add the loaded resource to the map
					m_resources.emplace(hashedPath, res);

				}
			}
		}
	}

	return res;
}

ResourceManager::AsyncJobIndex ResourceManager::asyncLoad(const char * path, std::function<void(Resource*)> callback)
{
	long hashedPath = m_pathHasher(path);
	Resource* res = nullptr;

	// Check if the resource already exists in the system (avoid getting locked in a mutex)
	auto it = m_resources.find(hashedPath);
	if (it != m_resources.end()) {
		// Found the resource
		res = it->second;
		res->refer();
		callback(res);
		return AsyncJobIndex{0, 0};
	}
	else {
		// Critical region
		std::lock_guard<std::mutex> lock(m_asyncLoadMutex);

		// Check if the resource already exists in the system (Avoid assigning the job if it was done
		//	during the lock)
		it = m_resources.find(hashedPath);
		if (it != m_resources.end()) {
			// Found the resource
			res = it->second;
			res->refer();
			callback(res);
			return AsyncJobIndex{ 0, 0 };
		}
		
		// Find out if the job is already queued
		auto asyncJobsIt = m_asyncResJobs.find(hashedPath);
		// The job already exists, push back another callback
		if (asyncJobsIt != m_asyncResJobs.end()) {
			asyncJobsIt->second.callbacks.push_back(AsyncJobCallback{ true, callback });
			return AsyncJobIndex{ hashedPath, asyncJobsIt->second.callbacks.size() - 1 };
		}
		else {
			std::vector<AsyncJobCallback> callbacks;
			callbacks.push_back(AsyncJobCallback{ true, callback });
			m_asyncResJobs.emplace(hashedPath, AsyncJob{ path, callbacks });
			m_asyncJobQueue.push(hashedPath);
			m_cond.notify_one();
			return AsyncJobIndex{ hashedPath, 0 };
		}
	}
}

void ResourceManager::removeAsyncJob(AsyncJobIndex index)
{
	std::lock_guard<std::mutex> critLock(m_asyncLoadMutex);
	auto job = m_asyncResJobs.find(index.GUID);
	// if the job was found, make sure the job isn't ran
	if (job != m_asyncResJobs.end())
		job->second.callbacks.at(index.IndexOfCallback).run = false;
}

void ResourceManager::removeAllAsyncJobs()
{
	std::lock_guard<std::mutex> critLock(m_asyncLoadMutex);
	for (auto& job : m_asyncResJobs)
		for (auto& callback : job.second.callbacks)
			callback.run = false;
}

void ResourceManager::decrementReference(long key)
{
	auto resource = m_resources.find(key);
	if (resource != m_resources.end()) {
		if (resource->second->derefer() == 0)
		{
			// Only when not using level-based handling of the memory
			//m_memUsage -= resource->second->getSize();
			resource->second->~Resource();
			// Should no longer be required
			//RM_FREE(m_resources.at(key));
			// Remove the resource from the map
			m_resources.erase(resource);
		}
	}
}

void ResourceManager::registerFormatLoader(FormatLoader* formatLoader)
{
	// Put the new format loader in the vector
	m_formatLoaders.emplace_back(formatLoader);
}

const std::map<long, Resource*>& ResourceManager::getResources() const
{
	return m_resources;
}
