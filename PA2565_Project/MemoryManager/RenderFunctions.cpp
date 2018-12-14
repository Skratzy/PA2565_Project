#include "RenderFunctions.h"

#define BARSIZE_X 1.5f
#define BARSIZE_Y 0.2f
#define OUTER_PADDING 0.05f
#define INNER_PADDING 0.03f

void renderSquare(double size, double startX, double startY)
{
	// Build vertices
	double botLeft_x = startX;
	double botLeft_y = startY;
	double topLeft_x = startX;
	double topLeft_y = startY + size;
	double topRight_x = startX + size;
	double topRight_y = startY + size;
	double botRight_x = startX + size;
	double botRight_y = startY;

	// Render square according to vertices
	glColor3f(0.0f, 1.0f, 0.0f);			//sets color
	glBegin(GL_POLYGON);
	glVertex2f(botLeft_x, botLeft_y);		// x1,y1
	glVertex2f(topLeft_x, topLeft_y);		// x2,y1
	glVertex2f(topRight_x, topRight_y);		// x2,y2
	glVertex2f(botRight_x, botRight_y);		// x1,y2
	glEnd();
}

void renderRectangle(double sizeX, double sizeY, double startX, double startY)
{
	// Build vertices
	double botLeft_x = startX;
	double botLeft_y = startY;
	double topLeft_x = startX;
	double topLeft_y = startY + sizeY;
	double topRight_x = startX + sizeX;
	double topRight_y = startY + sizeY;
	double botRight_x = startX + sizeX;
	double botRight_y = startY;

	// Render square according to vertices
	glBegin(GL_POLYGON);
	glVertex2f(botLeft_x, botLeft_y);		// x1,y1
	glVertex2f(topLeft_x, topLeft_y);		// x2,y1
	glVertex2f(topRight_x, topRight_y);		// x2,y2
	glVertex2f(botRight_x, botRight_y);		// x1,y2
	glEnd();
}

void renderBar(double startX, double startY, std::vector<bool> entries, GLfloat entryColor[])
{
	// Calculate the neccessary data
	double outerPadding = (BARSIZE_X) / 200;
	double rectSizeX = (BARSIZE_X - outerPadding * 2) / entries.size();
	double rectSizeY = BARSIZE_Y - 2 * outerPadding;
	double innerStartX = startX + outerPadding;
	double innerStartY = startY + outerPadding;

	// Render large rectangle
	glColor3f(1.0f, 1.0f, 1.0f);				// Set white Bar-color
	renderRectangle(BARSIZE_X, BARSIZE_Y, startX, startY);

	// Render squares in terms of entries
	for (unsigned int x = 0; x < entries.size(); x++) {
		if (entries.at(x) == true) {			// If an entry exists,
			glColor3f(entryColor[0], entryColor[1], entryColor[2]);	// Set chosen entry-color
			renderRectangle(										// Render it
				rectSizeX, rectSizeY,
				innerStartX + rectSizeX * x,
				innerStartY
			);
		}
		else {									// If it doesn't exist,
			glColor3f(0.0f, 0.0f, 0.0f);							// Set 'empty'-color
			renderRectangle(										// Render it
				rectSizeX, rectSizeY,
				innerStartX + rectSizeX * x,
				innerStartY
			);
		}
	}
}

void renderVector(std::vector<std::vector<bool>> vector, GLfloat entryColor[3], int* barCount)
{
	for (unsigned int i = 0; i < vector.size(); i++) {
		double startX = ((-1) + OUTER_PADDING);												// Starts from the left side of the window
		double startY = 1 - (OUTER_PADDING + (BARSIZE_Y + INNER_PADDING) * ((*barCount) + 1));	// Starts from the top of the window
		renderBar(startX, startY, vector.at((i)), entryColor);
		(*barCount)++;
	}
}

std::vector<std::vector<bool>> createFauxTestData()
{
	std::vector<std::vector<bool>> vectors;
	std::vector<bool> inputEveryOther;
	for (int i = 0; i < 150; i++) {
		if (i % 2 == 0) {
			inputEveryOther.push_back(true);
		}
		else {
			inputEveryOther.push_back(false);
		}
	}
	vectors.push_back(inputEveryOther);

	std::vector<bool> inputHalf;
	for (int i = 0; i < 100; i++) {
		if (i < 50) {
			inputHalf.push_back(true);
		}
		else {
			inputHalf.push_back(false);
		}
	}
	vectors.push_back(inputHalf);

	std::vector<bool> inputOtherHalf;
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

