#include "PNGSecuential.h"


Image_t* loadPNG(const char* path)
{
	FILE *fp = fopen(path, "rb");
	Image_t* newPNG = new Image_t;

	png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png) abort();

	png_infop info = png_create_info_struct(png);
	if (!info) abort();

	if (setjmp(png_jmpbuf(png))) abort();

	png_init_io(png, fp);

	png_read_info(png, info);

	newPNG->width = png_get_image_width(png, info);
	newPNG->height = png_get_image_height(png, info);
	newPNG->color_type = png_get_color_type(png, info);
	newPNG->bit_depth = png_get_bit_depth(png, info);

	// Read any color_type into 8bit depth, RGBA format.
	// See http://www.libpng.org/pub/png/libpng-manual.txt

	if (newPNG->bit_depth == 16)
		png_set_strip_16(png);

	if (newPNG->color_type == PNG_COLOR_TYPE_PALETTE)
		png_set_palette_to_rgb(png);

	// PNG_COLOR_TYPE_GRAY_ALPHA is always 8 or 16bit depth.
	if (newPNG->color_type == PNG_COLOR_TYPE_GRAY && newPNG->bit_depth < 8)
		png_set_expand_gray_1_2_4_to_8(png);

	if (png_get_valid(png, info, PNG_INFO_tRNS))
		png_set_tRNS_to_alpha(png);

	// These color_type don't have an alpha channel then fill it with 0xff.
	if (newPNG->color_type == PNG_COLOR_TYPE_RGB ||
		newPNG->color_type == PNG_COLOR_TYPE_GRAY ||
		newPNG->color_type == PNG_COLOR_TYPE_PALETTE)
		png_set_filler(png, 0xFF, PNG_FILLER_AFTER);

	if (newPNG->color_type == PNG_COLOR_TYPE_GRAY ||
		newPNG->color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
		png_set_gray_to_rgb(png);

	png_read_update_info(png, info);

	newPNG->row_pointers = (char**)malloc(sizeof(png_bytep) * newPNG->height);
	for (int y = 0; y < newPNG->height; y++) {
		newPNG->row_pointers[y] = (char*)malloc(png_get_rowbytes(png, info));
	}

	png_read_image(png, (png_bytepp)(newPNG->row_pointers));

	fclose(fp);
	return newPNG;
}


void writePNG(Image_t* image, const char* path) {

	FILE *fp = fopen(path, "wb");
	if (!fp) abort();

	png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png) abort();

	png_infop info = png_create_info_struct(png);
	if (!info) abort();

	if (setjmp(png_jmpbuf(png))) abort();

	png_init_io(png, fp);

	// Output is 8bit depth, RGBA format.
	png_set_IHDR(
		png,
		info,
		image->width, image->height,
		8,
		PNG_COLOR_TYPE_RGBA,
		PNG_INTERLACE_NONE,
		PNG_COMPRESSION_TYPE_DEFAULT,
		PNG_FILTER_TYPE_DEFAULT
	);
	png_write_info(png, info);

	// To remove the alpha channel for PNG_COLOR_TYPE_RGB format,
	// Use png_set_filler().
	//png_set_filler(png, 0, PNG_FILLER_AFTER);

	png_write_image(png, (png_bytepp)(image->row_pointers));
	png_write_end(png, NULL);

	for (int y = 0; y < image->height; y++) {
		free(image->row_pointers[y]);
	}
	free(image->row_pointers);
	delete image;
	fclose(fp);
}

bool comparePNG(Image_t* image1, Image_t* image2)
{
	bool igual = true;

	if (image1->height != image2->height)	return false;
	if (image1->width != image2->width)	return false;

	for (int i = 0; i < image1->height; i++)
		for (int j = 0; j < image1->width * 4; j++) {
			if (image1->row_pointers[i][j] != image2->row_pointers[i][j]) {
				igual = false;
			}
		}

	return igual;
}