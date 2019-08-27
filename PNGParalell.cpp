#include "PNGParalell.h"

/**
 * Function used internally in libpng to write to an output file
 *
 * @param	pngPtr
 * @param	data
 * @param	length
 */

void pngWrite(png_structp pngPtr, png_bytep data, png_size_t length)
{
	ofstream* file = reinterpret_cast<ofstream*>(png_get_io_ptr(pngPtr));
	file->write(reinterpret_cast<char*>(data), length);
	if (file->bad()) {
		cout << reinterpret_cast<char*>(data) << endl << endl << length << endl << file->badbit << endl;
		png_error(pngPtr, "Write error");
	}
}


/**
 * Function used internally in libpng to flush the output buffer to a file
 *
 * @param	pngPtr
 */

void pngFlush(png_structp pngPtr)
{
	ofstream* file = reinterpret_cast<ofstream*>(png_get_io_ptr(pngPtr));
	file->flush();
}

/**
 * Compress the loaded image to the specified output file
 *
 * @param	outputFile	output file for the generated PNG file
 */

void writePNG_Parallel(Image_t* image, const char* path, int numThreads, int compressionLevel)
{
	ofstream outputFile;
	outputFile.open(path);
	if (!outputFile.good()) {
		cout << "Output file could not be opened" << endl;
		return;
	}

	//Init PNG write struct
	png_structp pngPtr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!pngPtr) {
		cout << "PNG write struct could not be initialized" << endl;
		return;
	}

	//Init PNG info struct
	png_infop infoPtr = png_create_info_struct(pngPtr);
	if (!infoPtr) {
		png_destroy_write_struct(&pngPtr, (png_infopp)NULL);
		cout << "PNG info struct could not be initialized" << endl;
		return;
	}

	//Error handling
	if (setjmp(png_jmpbuf(pngPtr))) abort();

	//Tell the pnglib where to save the image
	png_set_write_fn(pngPtr, &outputFile, pngWrite, pngFlush);

	//For the sake of simplicity we do not apply any filters to a scanline
	png_set_filter(pngPtr, 0, PNG_FILTER_NONE);

	// Write IHDR chunk
	png_set_IHDR(
		pngPtr,
		infoPtr,
		image->width, image->height,
		8,
		PNG_COLOR_TYPE_RGBA,
		PNG_INTERLACE_NONE,
		PNG_COMPRESSION_TYPE_DEFAULT,
		PNG_FILTER_TYPE_BASE
	);

	//Write the file header information.
	png_write_info(pngPtr, infoPtr);

	// To remove the alpha channel for PNG_COLOR_TYPE_RGB format,
	// Use png_set_filler().
	//png_set_filler(png, 0, PNG_FILLER_AFTER);

	//Init vars used for compression
	int totalDeflateOutputSize = 0;
	uint32_t adler32Combined = 0L;
	z_stream* zStreams = (z_stream*)malloc(sizeof(z_stream)* numThreads);
	size_t* deflateOutputSize = (size_t*)malloc(sizeof(size_t)* numThreads);
	char **deflateOutput = (char**)malloc(sizeof(char*)* numThreads);
	uint32_t* adlerSums = (uint32_t*)malloc(sizeof(uint32_t)* numThreads);
	char **filteredData = (char**)malloc(sizeof(char*)* numThreads);
	work_package_t* work_packages = NULL;
	char** filteredChunks = (char**)malloc(sizeof(char*)* numThreads);
	
	//Prepare for compression
	work_packages = createWorkPackages(image->height, numThreads);
	filteredData = filterRows(image->row_pointers, image->height, image->width);

	// Start threads
	thread** th1;
	th1 = (thread**)malloc(sizeof(thread*)*numThreads);
	for (int i = 0; i < numThreads; i++)
	{
		th1[i] = new thread(thread_compress, image, work_packages, filteredData, zStreams, adlerSums, deflateOutput, deflateOutputSize, filteredChunks, i);
	}
	for (int i = 0; i < numThreads; i++)
	{
		th1[i]->join();
		delete th1[i];
	}

	// These must run in order for safety
	adler32Combined = adler32(0L, NULL, 0);
	for (int threadNum = 0; threadNum < numThreads; threadNum++) {
		totalDeflateOutputSize += deflateOutputSize[threadNum];

		//Calculate the combined adler32 checksum
		int input_length = (work_packages[threadNum].i_final - work_packages[threadNum].i_start) * (4 * image->width + 1);
		adler32Combined = adler32_combine(adler32Combined, adlerSums[threadNum], input_length);
	}

	//Concatenate the zStreams
	png_byte *idatData = new png_byte[totalDeflateOutputSize];
	for (int i = 0; i < numThreads; i++) {
		if (i == 0) {
			memcpy(idatData, deflateOutput[i], deflateOutputSize[i]);
			idatData += deflateOutputSize[i];
		}
		else {
			//strip the zlib stream header
			// 2 bytes regular
			// 4 more for the "dictionary"
			const size_t hdr = 2 + 4;
			memcpy(idatData, deflateOutput[i] + hdr, deflateOutputSize[i] - hdr);
			idatData += (deflateOutputSize[i] - hdr);
			totalDeflateOutputSize -= hdr;
		}
	}

	//Add the combined adler32 checksum
	//idatData -= sizeof(adler32Combined);
	//memcpy(idatData, &adler32Combined, sizeof(adler32Combined));
	//idatData -= (totalDeflateOutputSize - sizeof(adler32Combined));
	idatData -= 4;
	*idatData++ = (adler32Combined >> 24) & 0xff;
	*idatData++ = (adler32Combined >> 16) & 0xff;
	*idatData++ = (adler32Combined >> 8) & 0xff;
	*idatData++ = (adler32Combined >> 0) & 0xff;
	idatData -= (totalDeflateOutputSize);

	//We have to tell libpng that an IDAT was written to the file
	pngPtr->mode |= PNG_HAVE_IDAT;

	//Create an IDAT chunk
	png_unknown_chunk idatChunks[1];
	strcpy((png_charp)idatChunks[0].name, "IDAT");
	idatChunks[0].data = idatData;
	idatChunks[0].size = totalDeflateOutputSize;
	idatChunks[0].location = PNG_AFTER_IDAT;
	//pngPtr->flags |= 0x10000L; //PNG_FLAG_KEEP_UNSAFE_CHUNKS
	png_set_keep_unknown_chunks(pngPtr, 3, idatChunks[0].name, 1);
	png_set_unknown_chunks(pngPtr, infoPtr, idatChunks, 1);
	png_set_unknown_chunk_location(pngPtr, infoPtr, 0, PNG_AFTER_IDAT);

	//Write the rest of the file
	png_write_end(pngPtr, infoPtr);

	//Cleanup memory
	png_destroy_write_struct(&pngPtr, &infoPtr);
	delete(idatData);
	// TODO: Liberar variables pendientes
	free(work_packages);
	free(th1);
	for (int y = 0; y < image->height; y++) {
		free(image->row_pointers[y]);
		free(filteredData[y]);
	}
	free(image->row_pointers);
	free(filteredData);
	delete image;
}

work_package_t* createWorkPackages(int rows, int numThreads)
{
	//Declarar paquetes
	work_package_t* packages = (work_package_t*)malloc(sizeof(work_package_t)*numThreads);

	//Dividir filas entre paquetes
	int rows_per_package = (rows - 2) / numThreads;
	int rest_rows = (rows - 2) % numThreads;

	int row = 1;
	for (int i = 0; i < numThreads; i++) {
		packages[i].i_start = row;
		row += rows_per_package;
		if (i < rest_rows)
		{
			row++;
		}
		packages[i].i_final = row;
	}
	packages[numThreads - 1].i_final = rows - 1;

	return packages;
}

void thread_compress(Image_t* image, work_package_t* work_packages, char **filteredData, z_stream* zStreams, uint32_t* adlerSums, char **deflateOutput, size_t* deflateOutputSize, char** filteredChunks, int threadID)
{
	int ret, flush, row, stopAtRow;
	unsigned int have;
	const int chunkSize = 16384;
	unsigned char output_buffer[chunkSize];

	row = work_packages[threadID].i_start;
	stopAtRow = work_packages[threadID].i_final;

	filteredChunks[threadID] = (char*)malloc((stopAtRow - row) * (4 * image->width + 1));
	char * chunkPtr = filteredChunks[threadID];
	for (int i = row; i < stopAtRow; i++)
	{
		//Copy all data managed by the thread to its chunk
		memcpy(chunkPtr, filteredData[row], 4 * image->width + 1);
		chunkPtr += (4 * image->width + 1);
	}

	//Allocate deflate state
	zStreams[threadID].zalloc = Z_NULL;
	zStreams[threadID].zfree = Z_NULL;
	zStreams[threadID].opaque = Z_NULL;
	if (deflateInit(&zStreams[threadID], 9) != Z_OK) {
		cout << "Not enough memory for compression" << endl;
	}

	// Initialize dictionary with the last 32kb of the previous chunk
	if (threadID > 0) {
		int lastThread = threadID - 1;
		size_t maxDict = 32768;
		size_t lastChunkSize = (work_packages[lastThread].i_final - work_packages[lastThread].i_start) * (4 * image->width + 1);
		Bytef* lastData = (Bytef*)(filteredChunks[lastThread]);
		size_t dictSize = (lastChunkSize > maxDict) ? maxDict : lastChunkSize;
		deflateSetDictionary(&zStreams[threadID],
			lastData + lastChunkSize - dictSize,
			dictSize);
	}

	//Let's compress line by line so the input buffer is the number of bytes of one pixel row plus the filter byte
	zStreams[threadID].avail_in = (4 * image->width + 1) * (stopAtRow - row);
	//zStreams[threadNum].avail_in += stopAtRow == height ? 0 : 1;

	//Finish the stream if it's the last pixel row
	flush = stopAtRow == image->height ? Z_FINISH : Z_SYNC_FLUSH;
	zStreams[threadID].next_in = (Bytef*)(filteredChunks[threadID]);
	zStreams[threadID].adler = adler32(0L, NULL, 0);

	//Pasar a puntero y utilizar memcpy en vez de fwrite
	//char* deflate_stream_start = (char*)malloc(chunkSize * 30);
	//char *deflate_stream = deflate_stream_start;
    FILE *deflate_stream = open_memstream(&deflateOutput[threadID], &deflateOutputSize[threadID]);

	//Compress the image data with deflate
	deflateOutputSize[threadID] = 0;
	do {
		zStreams[threadID].avail_out = chunkSize;
		zStreams[threadID].next_out = output_buffer;
		ret = deflate(&zStreams[threadID], flush);
		have = chunkSize - zStreams[threadID].avail_out;
		fwrite(&output_buffer, 1, have, deflate_stream);
	} while (zStreams[threadID].avail_out == 0);

	fclose(deflate_stream);
	deflateOutput[threadID] = (char*)malloc(deflateOutputSize[threadID]);
	memcpy(deflateOutput[threadID], deflate_stream_start, deflateOutputSize[threadID]);

	adlerSums[threadID] = zStreams[threadID].adler;

	//Finish deflate process
	(void)deflateEnd(&zStreams[threadID]);
}


/**
 * Filter a pixel row to optimize deflate compression
 * Note: Filtering is disabled in this implementation.
 *
 * @param	pixels		pointer to the pixels in memory
 * @param	width	amount of pixels per row
 * @return	filtered row
 */

char* filterRow(char* pixels, int width)
{
	char filterByte = 0;

	//Add filter byte 0 to disable row filtering
	char* filteredRow = (char*)malloc(width * sizeof(char) * 4 + 1);
	memcpy(filteredRow, &filterByte, 1);
	memcpy(&(filteredRow[1]), pixels, sizeof(char) * 4 * width);

	return filteredRow;
}


/**
 * Filter all pixel rows to optimize the deflate compression
 * Note: Filtering is disabled in this implementation.
 *
 * @param	row_pointers	pointer to the rows of pixels in memory
 * @param	width			amount of pixels per row
 * @return	filtered rows
 */

char** filterRows(char** row_pointers, int rows, int width)
{
	int bytesPerRow = (sizeof(char) * 4 * width + 1);
	char** filteredRows = (char**)malloc(sizeof(char*) * rows);

	for (int row = 0; row < rows; row++) {
		filteredRows[row] = filterRow(row_pointers[row], width);
	}

	return filteredRows;
}