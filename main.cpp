#include <stdio.h>
#include <png.h>
#include <iostream>
#include <stdlib.h>
#include "PNGSecuential.h"
#include "PNGParalell.h"

using namespace std;

int main(int argc, char *argv[]) {
	//Obtenemos el numero de threads y tamaño de paquete que vamos a utilizar
	int numThreads = 8;
	int packageSize = 1230;
	if (argc > 2) {
		numThreads = atoi(argv[1]);
		packageSize = atoi(argv[2]);
	}

	//Cargamos la imagen
	Image_t* im = loadPNG("sunHD4k.png");

	//Escribimos el resultado
	writePNG(im, "sunHD4k2.png");
	//writePNG_Parallel(im2, "sunHD4k2.png", numThreads, 9);

	return 0;
}