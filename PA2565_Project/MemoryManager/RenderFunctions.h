#pragma once
#include "GlutIncludes.h"

void renderSquare(double size, double startX, double startY);
void renderRectangle(double sizeX, double sizeY, double startX, double startY);
void renderBar(double startX, double startY, std::vector<bool> entries, GLfloat entryColor[]);
void renderVector(std::vector<std::vector<bool>> vector, GLfloat entryColor[3], int* barCount);
std::vector<std::vector<bool>> createFauxTestData();
