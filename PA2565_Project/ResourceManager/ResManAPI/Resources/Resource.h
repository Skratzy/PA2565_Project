#ifndef _RM_RESOURCE_
#define _RM_RESOURCE_

class Resource
{
private:
	const long m_GUID;
	std::string m_path;
	unsigned int m_refCount;
	unsigned int m_size;	// Initialize at inheritors constructor!

public:
	Resource(const long GUID)
		: m_GUID(GUID) {
		m_size = 0;
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

	void setSize(int size) {
		this->m_size = size;
	}
	const unsigned int getSize() const {
		return m_size;
	}

	const char* getPath() const {
		return m_path.c_str();
	}
	void setPath(const char* path) {
		m_path = path;
	}

};

#endif //_RM_RESOURCE_