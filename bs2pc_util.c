#include "bs2pc.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "zlib.h"
#ifdef _WIN32
#include <Windows.h>
#endif

void *BS2PC_Alloc(size_t size, bool zeroed) {
	void *memory = malloc(size);
	if (memory == NULL) {
		fprintf(stderr, "Couldn't allocate %zu bytes.\n", size);
		exit(EXIT_FAILURE);
	}
	if (zeroed) {
		memset(memory, 0, size);
	}
	return memory;
}

void *BS2PC_LoadFile(const char *fileName, unsigned int *fileSize) {
	FILE *file;
	long fileEnd;
	void *data;

	file = fopen(fileName, "rb");
	if (file == NULL) {
		fprintf(stderr, "Couldn't open %s.\n", fileName);
		exit(EXIT_FAILURE);
	}

	if (fseek(file, 0, SEEK_END) != 0) {
		fprintf(stderr, "Couldn't seek to the end of %s.\n", fileName);
		exit(EXIT_FAILURE);
	}

	fileEnd = ftell(file);
	if (fileEnd <= 0) {
		fprintf(stderr, "Couldn't get the length of %s or it's empty.\n", fileName);
		exit(EXIT_FAILURE);
	}

	if (fseek(file, 0, SEEK_SET) != 0) {
		fprintf(stderr, "Couldn't seek to the beginning of %s.\n", fileName);
		exit(EXIT_FAILURE);
	}

	data = BS2PC_Alloc(fileEnd, false);
	if (fread(data, fileEnd, 1, file) == 0) {
		fprintf(stderr, "Couldn't read %s.\n", fileName);
		exit(EXIT_FAILURE);
	}

	fclose(file);

	if (fileSize != NULL) {
		*fileSize = fileEnd;
	}
	return data;
}

void BS2PC_WriteFile(const char *fileName, void *data, unsigned int size) {
	FILE *file;

	file = fopen(fileName, "wb");
	if (file == NULL) {
		fprintf(stderr, "Couldn't open %s.\n", fileName);
		exit(EXIT_FAILURE);
	}

	if (fwrite(data, size, 1, file) == 0) {
		fprintf(stderr, "Couldn't write to %s.\n", fileName);
		exit(EXIT_FAILURE);
	}

	fclose(file);
}

// Zlib compression.

static bool bs2pc_zlib_initialized = false;

#ifdef _WIN32
#define BS2PC_ZLIB_IMPORT WINAPI
#else
#define BS2PC_ZLIB_IMPORT
#endif
typedef int (BS2PC_ZLIB_IMPORT *bs2pc_zlib_inflateInit__t)(z_streamp strm, const char *version, int stream_size);
static bs2pc_zlib_inflateInit__t bs2pc_zlib_inflateInit_;
typedef int (BS2PC_ZLIB_IMPORT *bs2pc_zlib_inflate_t)(z_streamp strm, int flush);
static bs2pc_zlib_inflate_t bs2pc_zlib_inflate;
typedef int (BS2PC_ZLIB_IMPORT *bs2pc_zlib_inflateEnd_t)(z_streamp strm);
static bs2pc_zlib_inflateEnd_t bs2pc_zlib_inflateEnd;

void BS2PC_InitializeZlib() {
	#ifdef _WIN32
	HMODULE module;
	#endif

	if (bs2pc_zlib_initialized) {
		return;
	}
	
	#ifdef _WIN32
	module = LoadLibrary(TEXT("zlibwapi.dll"));
	if (module == NULL) {
		fputs("Couldn't open zlibwapi.dll.\n", stderr);
		exit(EXIT_FAILURE);
	}
	bs2pc_zlib_inflateInit_ = (bs2pc_zlib_inflateInit__t) GetProcAddress(module, "inflateInit_");
	bs2pc_zlib_inflate = (bs2pc_zlib_inflate_t) GetProcAddress(module, "inflate");
	bs2pc_zlib_inflateEnd = (bs2pc_zlib_inflateEnd_t) GetProcAddress(module, "inflateEnd");
	#else
	#error No zlib loading code for this platform.
	#endif

	if (bs2pc_zlib_inflateInit_ == NULL || bs2pc_zlib_inflate == NULL || bs2pc_zlib_inflateEnd == NULL) {
		fputs("Couldn't get a zlib function.\n", stderr);
		exit(EXIT_FAILURE);
	}

	bs2pc_zlib_initialized = true;
}

void BS2PC_Decompress(const void *source, unsigned int sourceSize, void *target, unsigned int targetSize) {
	z_stream stream;

	BS2PC_InitializeZlib();

	stream.zalloc = Z_NULL;
	stream.zfree = Z_NULL;
	stream.opaque = Z_NULL;
	stream.avail_in = sourceSize;
	stream.next_in = (Bytef *) source;
	stream.avail_out = targetSize;
	stream.next_out = (Bytef *) target;
	if (bs2pc_zlib_inflateInit_(&stream, ZLIB_VERSION, sizeof(stream)) != Z_OK) {
		fputs("Couldn't initialize decompression.\n", stderr);
		exit(EXIT_FAILURE);
	}
	if (bs2pc_zlib_inflate(&stream, Z_NO_FLUSH) != Z_STREAM_END) {
		fputs("Couldn't decompress the data.\n", stderr);
		exit(EXIT_FAILURE);
	}
	bs2pc_zlib_inflateEnd(&stream);
}