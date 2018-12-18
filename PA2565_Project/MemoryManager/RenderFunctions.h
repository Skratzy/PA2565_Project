#pragma once
#include "GlutIncludes.h"

void renderSquare(double size, double startX, double startY);
void renderRectangle(double sizeX, double sizeY, double startX, double startY);
void renderBar(double startX, double startY, std::vector<bool> entries, GLfloat entryColor[]);
void renderAsyncBar(double startX, double startY, std::vector<bool> entries, GLfloat entryColor[], double asyncBarY);
void renderVector(std::vector<std::vector<bool>> vector, GLfloat entryColor[3], int* barCount);
void renderAsyncVector(std::vector<std::vector<bool>> vector, GLfloat entryColor[3], int* barCount);
std::vector<std::vector<bool>> createFauxTestData();
