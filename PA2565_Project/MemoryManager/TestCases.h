#ifndef TEST_CASES
#define TEST_CASES


class TestCases {
private:

public:
	TestCases();
	~TestCases();

	void timingTest(unsigned int sizePerAlloc, unsigned int numAlloc, unsigned int numIter);

	void poolAllocDealloc();

};

#endif //TEST_CASES
