#pragma once
#include <iostream>
#include <iterator>
#include <fstream>
#include <png.h>
#include <time.h>
#include <zlib.h>
#include <math.h>
#include <thread>
#include <cstring>
#include  "PNGSecuential.h"
#include "pngpriv.h"

using namespace std;


void writePNG_Parallel(Image_t* image, const char* path, int numThreads, int compressionLevel);
void thread_compress(Image_t* image, work_package_t* work_packages, char **filteredData, z_stream* zStreams, uint32_t* adlerSums, char **deflateOutput, size_t* deflateOutputSize, char** filteredChunks, int threadID);
work_package_t* createWorkPackages(int rows, int numThreads);
char* filterRow(char* pixels, int width);
char** filterRows(char** row_pointers, int rows, int width);
