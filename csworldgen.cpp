/* Copyright (c) 2012 Manuel Kasten
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <sys/stat.h>
#include <direct.h>

#include "simplexnoise.h"

unsigned char top[1024][1024];
unsigned char bottom[1024][1024];
unsigned char material[1024][1024];
unsigned char fraction[1024][1024];
unsigned char temp[1024][1024];
unsigned int  trees[32768];
unsigned int  crystals[512];
unsigned int  startPoint;

#define GRASS 2
#define DIRT 4

#define MIN(a, b) ((a < b) ? (a) : (b))
#define LIMIT(a, min, max) ((a < min) ? (min) : ((a > max) ? (max) : (a)))

char  outputDir[512] = ".";

int   islandSeed;
float islandScale = 8.0f;
int   islandOctaves = 3;
float islandOctaveScale = 0.5f;
float islandOctavePersistence = 0.5f;
float islandEdge = 0.25f;
float islandSize = 0.65f;
float islandDensity = 0.5f;

int   heightSeed;
float heightScale = 8.0f;
int   heightOctaves = 3;
float heightOctaveScale = 0.5f;
float heightOctavePersistence = 0.5f;
int   heightValueInvert = 0;
float heightExponent = 4.0f;
float heightBase = 128.0f;
float heightTop = 192.0f;
int   heightFalloff = 0;

int   bottomSeed;
float bottomAdd = 1.0f;
int   bottomMinThick = 3;

int   treeSeed;
float treeScale = 16.0f;
int   treeOctaves = 1;
float treeOctaveScale = 0.5f;
float treeOctavePersistence = 0.5f;
int   treeValueInvert = 0;
int   treeSeedPos;
int   treeNumber = 1536;
float treeDensity = 0.6f;
int   treeFalloff = 0;

int   crystalSeed;
int   crystalGrassRadius = 16;
int   crystalNumber = 4;
int   crystalDistance = 128;
float crystalMaxSlope = 0.13f;
int   crystalStartPointDistance = 8;

int   pgmOut = 0;

float falloff(int x, int y)
{
	float fx = ((float)x - 512) / 512;
	float fy = ((float)y - 512) / 512;
	float f = ((islandSize - islandEdge) - sqrt(fx * fx + fy * fy)) / islandEdge;
	f = pow(f, 3.0f) + 1.0f;
	f = LIMIT(f, 0.0f, 1.0f);
	return f;
}

void initialize()
{
	srand(time(0));
	islandSeed = rand();
	heightSeed = rand();
	bottomSeed = rand();
	treeSeed = rand();
	treeSeedPos = rand();
	crystalSeed = rand();
}

void generateIsland()
{
	printf("generating island outline: ");
	init_noise(islandSeed);
	for(int y = 0; y < 1024; ++y)
	{
		for(int x = 0; x < 1024; ++x)
		{
			float val = scaled_octave_noise_2d(islandOctaves, islandOctavePersistence, islandOctaveScale, 0.0, 1.0, x * islandScale / 1024, y * islandScale / 1024);
			val *= falloff(x, y);
			if(val > (1.0 - islandDensity)) material[y][x] = GRASS;
		}
	}
	printf(" done.\n");
}

void generateTop()
{
	printf("generating island top layer: ");
	init_noise(heightSeed);
	for(int y = 0; y < 1024; ++y)
	{
		for(int x = 0; x < 1024; ++x)
		{
			float val = scaled_octave_noise_2d(heightOctaves, heightOctavePersistence, heightOctaveScale, 0.0, 1.0, x * heightScale / 1024, y * heightScale / 1024);
			if(heightFalloff) val *= falloff(x, y);
			if(heightValueInvert) val = 1.0f - val;
			float height = pow(val, heightExponent);
			height = heightBase + (heightTop - heightBase) * height;
			if(material[y][x] != 0)
			{
				top[y][x] = (int)height;
				fraction[y][x] = (int)((height - (int)height) * 3 + 1);
				bottom[y][x] = top[y][x] - bottomMinThick;
			}
		}
	}
	printf(" done.\n");
}

void erode()
{
	for (int y = 1; y < 1023; ++y)
	{
		for (int x = 1; x < 1023; ++x)
		{
			if (  (material[y][x] == GRASS)
			   && (material[y-1][x] == GRASS)
			   && (material[y+1][x] == GRASS)
			   && (material[y][x-1] == GRASS)
			   && (material[y][x+1] == GRASS)
			   )
			{
				temp[y][x] = 1;
			}
			else
			{
				temp[y][x] = 0;
			}
		}
	}
}

void roundEdges()
{
	printf("rounding edges: ");
	erode();
	for(int y = 0; y < 1024; ++y)
	{
		for(int x = 0; x < 1024; ++x)
		{
			if(material[y][x] == GRASS && temp[y][x] == 0)
			{
				material[y][x] = DIRT;
				top[y][x] -= 1;
				bottom[y][x] = top[y][x] - bottomMinThick;
			}
		}
	}

	erode();
	for(int y = 0; y < 1024; ++y)
	{
		for(int x = 0; x < 1024; ++x)
		{
			if(material[y][x] == GRASS && temp[y][x] == 0)
			{
				material[y][x] = DIRT;
				fraction[y][x] -=1;
				if(fraction[y][x] == 0)
				{
					fraction[y][x] = 3;
					top[y][x] -= 1;
					bottom[y][x] = top[y][x] - bottomMinThick;
				}
			}
		}
	}
	printf(" done.\n");
}

void generateBottom()
{
	printf("generating bottom: ");
	srand(bottomSeed);
	for(;;)
	{
		int i = 0;
		for(int y = 0; y < 1024; ++y)
		{
			for(int x = 0; x < 1024; ++x)
			{
				unsigned char max = 0;
				if((unsigned char)(bottom[y-1][x] - 1) > max) max = (unsigned char)(bottom[y-1][x] - 1);
				if((unsigned char)(bottom[y+1][x] - 1) > max) max = (unsigned char)(bottom[y+1][x] - 1);
				if((unsigned char)(bottom[y][x-1] - 1) > max) max = (unsigned char)(bottom[y][x-1] - 1);
				if((unsigned char)(bottom[y][x+1] - 1) > max) max = (unsigned char)(bottom[y][x+1] - 1);
				
				if(max < bottom[y][x])
				{
					temp[y][x] = max;
					temp[y][x] -= (int)((1.0 + bottomAdd) * rand() / (RAND_MAX + 1));
					i++;
				}
				else
				{
					temp[y][x] = bottom[y][x];
					if(bottom[y][x] != 0)
					{
						unsigned char thickness = top[y][x] - bottom[y][x];
						unsigned char thicknessConstant = 1;
						if(top[y-1][x] - bottom[y-1][x] != thickness) thicknessConstant = 0;
						if(top[y+1][x] - bottom[y+1][x] != thickness) thicknessConstant = 0;
						if(top[y][x-1] - bottom[y][x-1] != thickness) thicknessConstant = 0;
						if(top[y][x+1] - bottom[y][x+1] != thickness) thicknessConstant = 0;
						if(thicknessConstant)
						{
							temp[y][x] -= (int)((1.0 + bottomAdd) * rand() / (RAND_MAX + 1));
							i++;
						}
					}
				}
			}
		}
		memcpy(bottom, temp, 1024 * 1024);
		if(i == 0) break;
	}
	printf(" done.\n");
}

void plantTrees()
{
	printf("planting trees: ");
	init_noise(treeSeed);
	srand(treeSeedPos);
	int i = 0;
	int j = 0;
	while(i < treeNumber)
	{
		if(j++ > 1024 * 1024 * 8) break;
		
		int x = rand() % 1024;
		int y = rand() % 1024;

		if(material[y][x] != GRASS) continue;
		
		float val = scaled_octave_noise_2d(treeOctaves, treeOctavePersistence, treeOctaveScale, 0.0, 1.0, x * treeScale / 1024, y * treeScale / 1024);
		if(treeFalloff) val *= falloff(x, y);
		if(treeValueInvert) val = 1.0f - val;
		if(val > treeDensity)
		{
			trees[i++] = ((top[y][x] - 1) << 20) + (y << 10) + x;
			material[y][x] = DIRT;
		}
	}
	if(i < treeNumber)
	{
		printf("\ncould only plant %d trees\n", i);
		treeNumber = i;
	}
	printf(" done.\n");
}

void growCrystals()
{
	printf("growing crystals: ");
	srand(crystalSeed);
	int i = 0;
	int j = 0;
	while(i < crystalNumber)
	{
		if(j++ > 1024 * 1024 * 8) break;
		int x = (rand() % (1022 - 2 * crystalGrassRadius)) + crystalGrassRadius + 1;
		int y = (rand() % (1022 - 2 * crystalGrassRadius)) + crystalGrassRadius + 1;

		if(material[y][x] != GRASS) continue;

		unsigned char isOK = 1;
		for(int j = 0; isOK && j < i; j++)
		{
			int dx = crystals[j] % 1024 - x;
			int dy = (crystals[j] >> 10) % 1024 - y;
			if(dx * dx + dy * dy < crystalDistance * crystalDistance) isOK = 0;
		}
		if(!isOK) continue;

		if(fabs((float)(top[y][x] - top[y - crystalGrassRadius][x]) / crystalGrassRadius) > crystalMaxSlope) isOK = 0;
		if(fabs((float)(top[y][x] - top[y + crystalGrassRadius][x]) / crystalGrassRadius) > crystalMaxSlope) isOK = 0;
		if(fabs((float)(top[y][x] - top[y][x - crystalGrassRadius]) / crystalGrassRadius) > crystalMaxSlope) isOK = 0;
		if(fabs((float)(top[y][x] - top[y][x + crystalGrassRadius]) / crystalGrassRadius) > crystalMaxSlope) isOK = 0;
		if(fabs((float)(top[y][x] - top[y - crystalGrassRadius / 2][x]) / (crystalGrassRadius / 2)) > crystalMaxSlope) isOK = 0;
		if(fabs((float)(top[y][x] - top[y + crystalGrassRadius / 2][x]) / (crystalGrassRadius / 2)) > crystalMaxSlope) isOK = 0;
		if(fabs((float)(top[y][x] - top[y][x - crystalGrassRadius / 2]) / (crystalGrassRadius / 2)) > crystalMaxSlope) isOK = 0;
		if(fabs((float)(top[y][x] - top[y][x + crystalGrassRadius / 2]) / (crystalGrassRadius / 2)) > crystalMaxSlope) isOK = 0;
		if(!isOK) continue;

		for(int ty = y - crystalGrassRadius; isOK && ty < y + crystalGrassRadius; ++ty)
		{
			for(int tx = x - crystalGrassRadius; isOK && tx < x + crystalGrassRadius; ++tx)
			{
				int dx = tx - x;
				int dy = ty - y;
				if(dx * dx + dy * dy <= crystalGrassRadius * crystalGrassRadius && material[ty][tx] != GRASS) isOK = 0;
			}
		}
		if(!isOK) continue;

		crystals[i] = ((top[y][x] - 1) << 20) + (y << 10) + x;
		i++;
	}

	if(i < crystalNumber)
	{
		printf("\ncould only grow %d crystals\n", i);
		crystalNumber = i;
	}

	if(crystalNumber)
	{
		float angle = 2 * (float)M_PI * rand() / (RAND_MAX + 1);
		int dx = (int)(cos(angle) * crystalStartPointDistance);
		int dy = (int)(sin(angle) * crystalStartPointDistance);
		int x = crystals[0] % 1024 + dx;
		int y = (crystals[0] >> 10) % 1024 + dy;
		startPoint = ((top[y][x] - 1) << 20) + (y << 10) + x;
	}

	printf(" done.\n");
}

void writePGM(char* file, unsigned char* data, int width, int height)
{
	FILE* out = fopen(file, "wb");
	fprintf(out, "P5\n%d %d\n255\n", width, height);
	fwrite(data, 1, width*height, out);
	fclose(out);
}

void writePGMs()
{
	char fname[512];
	sprintf(fname, "%s/mat.pgm", outputDir);
	writePGM(fname, &material[0][0], 1024, 1024);
	sprintf(fname, "%s/top.pgm", outputDir);
	writePGM(fname, &top[0][0], 1024, 1024);
	sprintf(fname, "%s/fra.pgm", outputDir);
	writePGM(fname, &fraction[0][0], 1024, 1024);
	sprintf(fname, "%s/bot.pgm", outputDir);
	writePGM(fname, &bottom[0][0], 1024, 1024);
}

int fileExists(const char* name)
{
	struct stat info;
	return !stat(name, &info);
}

void writeFiles()
{
	char fname[512];
	FILE* out;

	printf("writing files: ");

	if(!fileExists(outputDir))
	{
		mkdir(outputDir);
	}

	for(int i = 0; i < 28; ++i)
	{
		if(i % 8 > 3) continue;

		sprintf(fname, "%s/Monde_%d", outputDir, i);

		int isEmpty = 1;
		for(int y = (i / 8) * 256; isEmpty && y < ((i / 8) + 1) * 256; ++y)
		{
			for(int x = (i % 8) * 256; isEmpty && x < ((i % 8) + 1) * 256; ++x)
			{
				if(material[y][x] != 0) isEmpty = 0;
			}
		}
		if(isEmpty){
			if(fileExists(fname)) remove(fname);
			continue;
		}

		out = fopen(fname, "wb");

		fwrite("\0\0", 1, 2, out);
		for(int y = (i / 8) * 256; y < ((i / 8) + 1) * 256; ++y)
		{
			for(int x = (i % 8) * 256; x < ((i % 8) + 1) * 256; ++x)
			{
				fwrite(&bottom[y][x], 1, 1, out);
				fwrite(&top[y][x], 1, 1, out);
				fwrite(&material[y][x], 1, 1, out);
				fwrite(&fraction[y][x], 1, 1, out);
			}
		}
		fclose(out);
	}

	sprintf(fname, "%s/Monde_Arbre", outputDir);
	out = fopen(fname, "wb");
	fwrite(&trees[0], 4, treeNumber, out);
	fclose(out);

	sprintf(fname, "%s/Monde_Doodads", outputDir);
	out = fopen(fname, "wb");
	fprintf(out, "StartingPoint %d ", startPoint);
	for(int i = 0; i < crystalNumber; ++i)
	{
		fprintf(out, "Crystal %d ", crystals[i]);
	}
	fclose(out);

	if(pgmOut) writePGMs();
	printf(" done.\n");
}

int main(int argc, char** argv)
{
	initialize();
	generateIsland();
	generateTop();
	roundEdges();
	generateBottom();
	plantTrees();
	growCrystals();
	writeFiles();

	system("pause");
}

