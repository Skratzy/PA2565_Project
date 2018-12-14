#include <iostream>

#include "GlutManager.h"
#include "GlutIncludes.h"
#include "RenderFunctions.h"

GlutManager& asdf = GlutManager::getInstance();
GlutManager* ptr = &asdf;

int frameCount = 0;

void renderScene()
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Create faux input
	int* renderedBarCount = new int(0);
	std::vector<std::vector<bool>> vectors = createFauxTestData();

	// Render vectors 
	std::vector<std::vector<std::vector<bool>>> stacksAndPools = ptr->getVectors();
	std::vector<std::vector<bool>> stacks = stacksAndPools.at(0);
	std::vector<std::vector<bool>> pools = stacksAndPools.at(1);

	GLfloat stackEntryColor[] = { 0.0f, 1.0f, 0.0f };
	renderVector(stacks, stackEntryColor, renderedBarCount);

	GLfloat poolEntryColor[] = { 0.0f, 0.0f, 1.0f };
	renderVector(pools, poolEntryColor, renderedBarCount);

	delete renderedBarCount;

	//frameCount++;
	//std::cout << "Frames that has been rendered: " << frameCount << std::endl;
	glutSwapBuffers();
}

GlutManager::GlutManager()
{
	// Fake input so that glut doesn't REEEEEEEEEEE
	int argc = 1; 
	char *temp = new char('a');
	char **argv = &temp;

	// Initialize GLUT & vectors
	this->initialize(argc, argv);

	// Clean
	delete temp;
}

GlutManager::~GlutManager()
{
}

void GlutManager::EnterMainLoop()
{
	// Enter the mainloop
	glutMainLoop();
}

std::vector<std::vector<std::vector<bool>>> GlutManager::getVectors()
{
	std::vector<std::vector<std::vector<bool>>> stacksAndPools;
	stacksAndPools.push_back(m_stacks);
	stacksAndPools.push_back(m_pools);
	return stacksAndPools;
}

void GlutManager::updateVectors(std::vector<std::vector<bool>>& stacks, std::vector<std::vector<bool>>& pools)
{
	// Convert pointerd vector to non-pointered so if threads run out
	// the vectors won't be affected.
	std::vector<std::vector<bool>> newStacks;
	for (unsigned int i = 0; i < stacks.size(); i++) {
		newStacks.push_back(stacks.at(i));
	}
	std::vector<std::vector<bool>> newPools;
	for (unsigned int i = 0; i < pools.size(); i++) {
		newPools.push_back(pools.at(i));
	}
	m_stacks = newStacks;
	m_pools = newPools;
}

void GlutManager::addStack(std::vector<bool> stack)
{
	m_stacks.push_back(stack);
}

void GlutManager::addPool(std::vector<bool> pool)
{
	m_pools.push_back(pool);
}

/*
'glutTimerFunc' calls the function 'timerEvent' after X milliseconds, where
X is the first parameter. By having 'timerEvent' call 'glutTimerFunc' at 
the end of itself, it practically acts like an update loop which runs
every X milliseconds.
*/
void timerEvent(int t)
{
	glutPostRedisplay();				// Render the scene one more time.
	glutTimerFunc(10, timerEvent, 1);	// Call this function in 100ms.
}

void GlutManager::initialize(int argc, char **argv)
{
	// Init GLUT and create the window
	glutInit(&argc, argv);		// - Initializes GLUT itself
	glutInitWindowPosition(-1, -1);	// -1, -1 leaves the window position to the OS
	glutInitWindowSize(600, 600);	//
	glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE);
	glutCreateWindow("Render Window");

	// register callbacks
	glutDisplayFunc(renderScene);

	// See the definition of 'timerEvent' above
	glutTimerFunc(10, timerEvent, 1);

	// Fill vectors with testData so that it's visible if we haven't done anything
	m_stacks = createFauxTestData();
	m_pools = createFauxTestData();
}

std::vector<std::vector<bool>> GlutManager::createFauxTestData()
{
	std::vector<std::vector<bool>> vectors;
	std::vector<bool> inputEveryOther;
	std::vector<bool> inputHalf;
	std::vector<bool> inputOtherHalf;

	for (int i = 0; i < 150; i++) {
		if (i % 2 == 0) {
			inputEveryOther.push_back(true);
		}
		else {
			inputEveryOther.push_back(false);
		}
	}
	vectors.push_back(inputEveryOther);


	for (int i = 0; i < 100; i++) {
		if (i < 50) {
			inputHalf.push_back(true);
		}
		else {
			inputHalf.push_back(false);
		}
	}
	vectors.push_back(inputHalf);


	for (int i = 0; i < 100; i++) {
		if (i < 50) {
			inputOtherHalf.push_back(false);
		}
		else {
			inputOtherHalf.push_back(true);
		}
	}
	vectors.push_back(inputOtherHalf);

	return vectors;
}

