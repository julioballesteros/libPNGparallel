#pragma once
#include <png.h>
#include <stdio.h>
#include <stdlib.h>

typedef  unsigned char uint8;
typedef  unsigned short uint16;
typedef  unsigned int uint32;

typedef struct work_package_t
{
	int i_start;
	int i_final;

}work_package_t;

typedef struct pixel32bpp
{
	unsigned char r;
	unsigned char g;
	unsigned char b;
	unsigned char a;
}pixel32bpp;

typedef struct Image_t
{
	int width, height;
	char color_type;
	char bit_depth;
	char** row_pointers;

}Image_t;

Image_t* loadPNG(const char* path);
void writePNG(Image_t* image, const char* path);
bool comparePNG(Image_t* image1, Image_t* image2);