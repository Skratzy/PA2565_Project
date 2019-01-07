#ifndef _RM_RESOURCE_
#define _RM_RESOURCE_

class Resource
{
private:
	const long m_GUID;
	std::string m_path;
	unsigned int m_refCount;
	unsigned int m_sizeCPU;	// Initialize at inheritors constructor!
	unsigned int m_sizeGPU;	// Initialize at inheritors constructor!

public:
	Resource(const long GUID)
		: m_GUID(GUID) {
		m_sizeCPU = 0;
		m_sizeGPU = 0;
		m_refCount = 0;
	}
	virtual ~Resource() {}

	const long getGUID() const {
		return m_GUID;
	}

	unsigned int refer() {
		m_refCount++;
		return m_refCount;
	}
	unsigned int derefer() {
		m_refCount--;
		return m_refCount;
	}

	void setSizeCPU(int size) {
		this->m_sizeCPU = size;
	}
	void setSizeGPU(int size) {
		this->m_sizeGPU = size;
	}
	const unsigned int getSizeCPU() const {
		return m_sizeCPU;
	}
	const unsigned int getSizeGPU() const {
		return m_sizeGPU;
	}

	const char* getPath() const {
		return m_path.c_str();
	}
	void setPath(const char* path) {
		m_path = path;
	}

};

#endif //_RM_RESOURCE_