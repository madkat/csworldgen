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
#ifdef _WIN32
#include <direct.h>
#endif

#include "simplexnoise.h"

char version[] = "0.9";

char help[] = "\ncsworldgen %s\n\noutput options\n\
\n\
-o     output directory - no default, mandatory\n\
-pgm   write pgm files - default: 0\n\
-info  write info file - default: 1\n\
\n\
noise function parameters for island outline\n\
\n\
-i     seed - default: random\n\
-is    scale - default: 8.0\n\
-io    number of octaves - default: 3\n\
-ios   scale factor for each octave - default: 0.5\n\
-iop   persistence factor for each octave - default: 0.5\n\
\n\
other island outline parameters\n\
\n\
-ie    width of edge - default: 0.25\n\
-iz    size - default: 0.65\n\
-id    density - default: 0.5\n\
\n\
noise function parameters for terrain height\n\
\n\
-h     seed - default: random\n\
-hs    scale - default: 8.0\n\
-ho    number of octaves - default: 3\n\
-hos   scale factor for each octave - default: 0.5\n\
-hop   persistence factor for each octave - default: 0.5\n\
\n\
other terrain height parameters\n\
\n\
-hb    base level - default: 128.0\n\
-ht    top level - default: 192.0\n\
-he    exponent - default: 4.0\n\
-hi    invert noise value - default: 0\n\
-hf    falloff to the outside - default: 0\n\
\n\
terrain bottom parameters\n\
\n\
-b     seed - default: random\n\
-ba    additional random part - default: 1.0\n\
-bm    minimal thickness - default: 3\n\
\n\
noise function parameters for tree distribution\n\
\n\
-t     seed - default: random\n\
-ts    scale - default: 32.0\n\
-to    number of octaves - default: 1\n\
-tos   scale factor for each octave - default: 0.5\n\
-top   persistence factor for each octave - default: 0.5\n\
\n\
other tree distribution parameters\n\
\n\
-tp    position seed - default: random\n\
-tn    number - default: 1536\n\
-td    density - default: 0.6\n\
-ti    invert noise value - default: 0\n\
-tf    falloff to the outside - default: 0\n\
\n\
crystal distribution parameters\n\
\n\
-c     position seed - default: random\n\
-cr    radius that needs to be free - default: 16\n\
-cn    number - default: 4\n\
-cd    distance crystal to crystal - default: 128\n\
-cs    allowed slope - default: 0.13\n\
-csd   distance of start point - default: 8.0\n\
\n\
\n\
\n\
example call: csworldgen -o OutDir -i 5 -h 3 -ht 224.0 -t 7\n\
everything but output directory is optional\n";

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

char   outputDir[512] = "";

int    islandSeed;
double islandScale = 8.0;
int    islandOctaves = 3;
double islandOctaveScale = 0.5;
double islandOctavePersistence = 0.5;
double islandEdge = 0.25;
double islandSize = 0.65;
double islandDensity = 0.5;

int    heightSeed;
double heightScale = 8.0;
int    heightOctaves = 3;
double heightOctaveScale = 0.5;
double heightOctavePersistence = 0.5;
double heightBase = 128.0;
double heightTop = 192.0;
double heightExponent = 4.0;
int    heightValueInvert = 0;
int    heightFalloff = 0;

int    bottomSeed;
double bottomAdd = 1.0;
int    bottomMinThick = 3;

int    treeSeed;
double treeScale = 32.0;
int    treeOctaves = 1;
double treeOctaveScale = 0.5;
double treeOctavePersistence = 0.5;
int    treeSeedPos;
int    treeNumber = 1536;
double treeDensity = 0.6;
int    treeValueInvert = 0;
int    treeFalloff = 0;

int    crystalSeed;
int    crystalGrassRadius = 16;
int    crystalNumber = 4;
int    crystalDistance = 128;
double crystalMaxSlope = 0.13;
double crystalStartPointDistance = 8.0;

int    pgmOut = 0;
int    infoOut = 1;


void checkHelp(int argc, char** argv)
{
	if (  (argc == 1)
	   || (  (argc == 2)
	      && (  (strcmp(argv[1], "-h") == 0)
	         || (strcmp(argv[1], "--help") == 0)
	         || (strcmp(argv[1], "/?") == 0)
	         )
	      )
	   )
	{
		printf(help, version);
		exit(EXIT_SUCCESS);
	}
}

void initialize()
{
	srand(time(0));
	islandSeed = rand() % 0x8000;
	heightSeed = rand() % 0x8000;
	bottomSeed = rand() % 0x8000;
	treeSeed = rand() % 0x8000;
	treeSeedPos = rand() % 0x8000;
	crystalSeed = rand() % 0x8000;
}

void readParameters(int argc, char** argv)
{
	for(int i = 1; i < argc; ++i)
	{
		if(!strcmp(argv[i], "-o")) strcpy(outputDir, argv[++i]);
		else if(!strcmp(argv[i], "-pgm")) pgmOut = atoi(argv[++i]);
		else if(!strcmp(argv[i], "-info")) infoOut = atoi(argv[++i]);

		else if(!strcmp(argv[i], "-i")) islandSeed = atoi(argv[++i]);
		else if(!strcmp(argv[i], "-is")) islandScale = atof(argv[++i]);
		else if(!strcmp(argv[i], "-io")) islandOctaves = atoi(argv[++i]);
		else if(!strcmp(argv[i], "-ios")) islandOctaveScale = atof(argv[++i]);
		else if(!strcmp(argv[i], "-iop")) islandOctavePersistence = atof(argv[++i]);

		else if(!strcmp(argv[i], "-ie")) islandEdge = atof(argv[++i]);
		else if(!strcmp(argv[i], "-iz")) islandSize = atof(argv[++i]);
		else if(!strcmp(argv[i], "-id")) islandDensity = atof(argv[++i]);

		else if(!strcmp(argv[i], "-h")) heightSeed = atoi(argv[++i]);
		else if(!strcmp(argv[i], "-hs")) heightScale = atof(argv[++i]);
		else if(!strcmp(argv[i], "-ho")) heightOctaves = atoi(argv[++i]);
		else if(!strcmp(argv[i], "-hos")) heightOctaveScale = atof(argv[++i]);
		else if(!strcmp(argv[i], "-hop")) heightOctavePersistence = atof(argv[++i]);

		else if(!strcmp(argv[i], "-hb")) heightBase = atof(argv[++i]);
		else if(!strcmp(argv[i], "-ht")) heightTop = atof(argv[++i]);
		else if(!strcmp(argv[i], "-he")) heightExponent = atof(argv[++i]);
		else if(!strcmp(argv[i], "-hi")) heightValueInvert = atoi(argv[++i]);
		else if(!strcmp(argv[i], "-hf")) heightFalloff = atoi(argv[++i]);

		else if(!strcmp(argv[i], "-b")) bottomSeed = atof(argv[++i]);
		else if(!strcmp(argv[i], "-ba")) bottomAdd = atof(argv[++i]);
		else if(!strcmp(argv[i], "-bm")) bottomMinThick = atof(argv[++i]);

		else if(!strcmp(argv[i], "-t")) treeSeed = atoi(argv[++i]);
		else if(!strcmp(argv[i], "-ts")) treeScale = atof(argv[++i]);
		else if(!strcmp(argv[i], "-to")) treeOctaves = atoi(argv[++i]);
		else if(!strcmp(argv[i], "-tos")) treeOctaveScale = atof(argv[++i]);
		else if(!strcmp(argv[i], "-top")) treeOctavePersistence = atof(argv[++i]);

		else if(!strcmp(argv[i], "-tp")) treeSeedPos = atoi(argv[++i]);
		else if(!strcmp(argv[i], "-tn")) treeNumber = atoi(argv[++i]);
		else if(!strcmp(argv[i], "-td")) treeDensity = atof(argv[++i]);
		else if(!strcmp(argv[i], "-ti")) treeValueInvert = atoi(argv[++i]);
		else if(!strcmp(argv[i], "-tf")) treeFalloff = atoi(argv[++i]);

		else if(!strcmp(argv[i], "-c")) crystalSeed = atoi(argv[++i]);
		else if(!strcmp(argv[i], "-cr")) crystalGrassRadius = atoi(argv[++i]);
		else if(!strcmp(argv[i], "-cn")) crystalNumber = atoi(argv[++i]);
		else if(!strcmp(argv[i], "-cd")) crystalDistance = atoi(argv[++i]);
		else if(!strcmp(argv[i], "-cs")) crystalMaxSlope = atof(argv[++i]);
		else if(!strcmp(argv[i], "-csd")) crystalStartPointDistance = atof(argv[++i]);

		else
		{
			printf("error at or before commandline parameter %d: %s\n", i, argv[i]);
			exit(EXIT_FAILURE);
		}
	}
	if(!strcmp(outputDir, ""))
	{
		printf("output directory is mandatory and can't be empty.\n");
		exit(EXIT_FAILURE);
	}
}

double falloff(int x, int y)
{
	double fx = (x - 512.0) / 512.0;
	double fy = (y - 512.0) / 512.0;
	double f = ((islandSize - islandEdge) - sqrt(fx * fx + fy * fy)) / islandEdge;
	f = pow(f, 3.0) + 1.0;
	f = LIMIT(f, 0.0, 1.0);
	return f;
}

void generateIsland()
{
	printf("generating island outline: ");
	init_noise(islandSeed);
	for(int y = 0; y < 1024; ++y)
	{
		for(int x = 0; x < 1024; ++x)
		{
			double val = scaled_octave_noise_2d(islandOctaves, islandOctavePersistence, islandOctaveScale, 0.0, 1.0, (x - 512) * islandScale / 1024, (y - 512) * islandScale / 1024);
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
			double val = scaled_octave_noise_2d(heightOctaves, heightOctavePersistence, heightOctaveScale, 0.0, 1.0, (x - 512) * heightScale / 1024, (y - 512) * heightScale / 1024);
			if(heightFalloff) val *= falloff(x, y);
			if(heightValueInvert) val = 1.0f - val;
			double height = pow(val, heightExponent);
			height = heightBase + (heightTop - heightBase) * height;
			if(material[y][x] != 0)
			{
				top[y][x] = height;
				fraction[y][x] = (height - top[y][x]) * 3.0 + 1.0;
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
					temp[y][x] -= (1.0 + bottomAdd) * rand() / ((long)RAND_MAX + 1);
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
							temp[y][x] -= (1.0 + bottomAdd) * rand() / ((long)RAND_MAX + 1);
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

		double val = scaled_octave_noise_2d(treeOctaves, treeOctavePersistence, treeOctaveScale, 0.0, 1.0, (x - 512) * treeScale / 1024, (y - 512) * treeScale / 1024);
		if(treeFalloff) val *= falloff(x, y);
		if(treeValueInvert) val = 1.0 - val;
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

		if(fabs((double)(top[y][x] - top[y - crystalGrassRadius][x]) / crystalGrassRadius) > crystalMaxSlope) isOK = 0;
		if(fabs((double)(top[y][x] - top[y + crystalGrassRadius][x]) / crystalGrassRadius) > crystalMaxSlope) isOK = 0;
		if(fabs((double)(top[y][x] - top[y][x - crystalGrassRadius]) / crystalGrassRadius) > crystalMaxSlope) isOK = 0;
		if(fabs((double)(top[y][x] - top[y][x + crystalGrassRadius]) / crystalGrassRadius) > crystalMaxSlope) isOK = 0;
		if(fabs((double)(top[y][x] - top[y - crystalGrassRadius / 2][x]) / (crystalGrassRadius / 2)) > crystalMaxSlope) isOK = 0;
		if(fabs((double)(top[y][x] - top[y + crystalGrassRadius / 2][x]) / (crystalGrassRadius / 2)) > crystalMaxSlope) isOK = 0;
		if(fabs((double)(top[y][x] - top[y][x - crystalGrassRadius / 2]) / (crystalGrassRadius / 2)) > crystalMaxSlope) isOK = 0;
		if(fabs((double)(top[y][x] - top[y][x + crystalGrassRadius / 2]) / (crystalGrassRadius / 2)) > crystalMaxSlope) isOK = 0;
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
		double angle = 2 * M_PI * rand() / ((long)RAND_MAX + 1);
		int dx = cos(angle) * crystalStartPointDistance;
		int dy = sin(angle) * crystalStartPointDistance;
		int x = crystals[0] % 1024 + dx;
		int y = (crystals[0] >> 10) % 1024 + dy;
		startPoint = ((top[y][x] - 1) << 20) + (y << 10) + x;
	}
	else
	{
		printf("warning: no crystals, start point will be invalid\n");
	}

	printf(" done.\n");
}

void checkError(FILE* out, char* fname)
{
	if(!out)
	{
		printf("Could not open %s, aborting", fname);
		exit(EXIT_FAILURE);
	}
}

void writePGM(char* fname, unsigned char* data, int width, int height)
{
	FILE* out = fopen(fname, "wb");
	checkError(out, fname);
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

void writeInfoFile()
{
	char fname[512];
	sprintf(fname, "%s/csworldgen.info", outputDir);
	FILE* out = fopen(fname, "w");
	checkError(out, fname);
	fprintf(out, "Generated with csworldgen %s (https://github.com/Draradech/csworldgen)\n", version);
	fprintf(out, "\n");
	fprintf(out, "generation parameters:\n");
	fprintf(out, "-i %d -is %lf -io %d -ios %lf -iop %lf ", islandSeed, islandScale, islandOctaves, islandOctaveScale, islandOctavePersistence);
	fprintf(out, "-ie %lf -iz %lf -id %lf ", islandEdge, islandSize, islandDensity);
	fprintf(out, "-h %d -hs %lf -ho %d -hos %lf -hop %lf ", heightSeed, heightScale, heightOctaves, heightOctaveScale, heightOctavePersistence);
	fprintf(out, "-hb %lf -ht %lf -he %lf -hi %d -hf %d ", heightBase, heightTop, heightExponent, heightValueInvert, heightFalloff);
	fprintf(out, "-b %d -ba %lf -bm %d ", bottomSeed, bottomAdd, bottomMinThick);
	fprintf(out, "-t %d -ts %lf -to %d -tos %lf -top %lf ", treeSeed, treeScale, treeOctaves, treeOctaveScale, treeOctavePersistence);
	fprintf(out, "-tp %d -tn %d -td %lf -hi %d -hf %d ", treeSeedPos, treeNumber, treeDensity, treeValueInvert, treeFalloff);
	fprintf(out, "-c %d -cr %d -cn %d -cd %d -cs %lf -csd %lf\n", crystalSeed, crystalGrassRadius, crystalNumber, crystalDistance, crystalMaxSlope, crystalStartPointDistance);
	fclose(out);
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
		int status;
		#ifdef _WIN32
		status = mkdir(outputDir);
		#else
		status = mkdir(outputDir, 0777);
		#endif
		if(status)
		{
			printf("Could not create output directory %s, aborting\n", outputDir);
			exit(EXIT_FAILURE);
		}
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
		checkError(out, fname);

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
	checkError(out, fname);
	fwrite(&trees[0], 4, treeNumber, out);
	fclose(out);

	sprintf(fname, "%s/Monde_Doodads", outputDir);
	out = fopen(fname, "wb");
	checkError(out, fname);
	fprintf(out, "StartingPoint %d ", startPoint);
	for(int i = 0; i < crystalNumber; ++i)
	{
		fprintf(out, "Crystal %d ", crystals[i]);
	}
	fclose(out);

	if(pgmOut) writePGMs();
	if(infoOut) writeInfoFile();
	printf(" done.\n");
}

int main(int argc, char** argv)
{
	checkHelp(argc, argv);
	initialize();
	readParameters(argc, argv);
	generateIsland();
	generateTop();
	roundEdges();
	generateBottom();
	plantTrees();
	growCrystals();
	writeFiles();
}
