////////////////////////////////////////////////////////////////////////////////
// Copyright 2017 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License.  You may obtain a copy
// of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
// License for the specific language governing permissions and limitations
// under the License.
////////////////////////////////////////////////////////////////////////////////

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

