/*

  Copyright (c) 2015, Intel Corporation
  All rights reserved.

*/

#pragma once

#include <windows.h>
#include <dxgiformat.h>
#include "ispc_texcomp.h"


typedef void (CompressionFunc)(const rgba_surface* input, BYTE* output);

enum EThreadState {
	eThreadState_WaitForData,
	eThreadState_DataLoaded,
	eThreadState_Running,
	eThreadState_Done
};

struct WinThreadData {
	EThreadState state;
	int threadIdx;
	CompressionFunc* cmpFunc;
	rgba_surface input;
	BYTE *output;

	// Defaults..
	WinThreadData() :
		state(eThreadState_Done),
		threadIdx(-1),
		input(),
		output(NULL),
		cmpFunc(NULL)
	{ }

};

// Win32 thread API
int GetProcessorCount();	// 1 or more

void DestroyThreads();
void InitWin32Threads();

bool CompressImageMT(const rgba_surface* input, BYTE* output, CompressionFunc* cmpFunc, DXGI_FORMAT compformat);
bool CompressImageST(const rgba_surface* input, uint8_t* output, CompressionFunc* cmpFunc, DXGI_FORMAT compformat);



/////////////////////////////////////////////////////////////////////////////////////////////////////
//CALL IN TO ISPC CODE
void CompressImageBC1(const rgba_surface* input, BYTE* output);
void CompressImageBC3(const rgba_surface* input, BYTE* output);
void CompressImageBC7_ultrafast(const rgba_surface* input, BYTE* output);
void CompressImageBC7_veryfast(const rgba_surface* input, BYTE* output);
void CompressImageBC7_fast(const rgba_surface* input, BYTE* output);
void CompressImageBC7_basic(const rgba_surface* input, BYTE* output);
void CompressImageBC7_slow(const rgba_surface* input, BYTE* output);
void CompressImageBC7_alpha_ultrafast(const rgba_surface* input, BYTE* output);
void CompressImageBC7_alpha_veryfast(const rgba_surface* input, BYTE* output);
void CompressImageBC7_alpha_fast(const rgba_surface* input, BYTE* output);
void CompressImageBC7_alpha_basic(const rgba_surface* input, BYTE* output);
void CompressImageBC7_alpha_slow(const rgba_surface* input, BYTE* output);
void CompressImageBC6H_veryfast(const rgba_surface* input, BYTE* output);
void CompressImageBC6H_fast(const rgba_surface* input, BYTE* output);
void CompressImageBC6H_basic(const rgba_surface* input, BYTE* output);
void CompressImageBC6H_slow(const rgba_surface* input, BYTE* output);
void CompressImageBC6H_veryslow(const rgba_surface* input, BYTE* output);

