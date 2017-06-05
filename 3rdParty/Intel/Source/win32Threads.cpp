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

#include <tchar.h>
#include <string>
#include <assert.h>     /* assert */
#include "win32Threads.h"
#include <strsafe.h>


// Win32 thread API
const int kMaxWinThreads = 64;

WinThreadData gWinThreadData[kMaxWinThreads];

HANDLE gWinThreadWorkEvent[kMaxWinThreads];
HANDLE gWinThreadStartEvent = NULL;
HANDLE gWinThreadDoneEvent = NULL;
int gNumWinThreads = 0;

DWORD dwThreadIdArray[kMaxWinThreads];
HANDLE hThreadArray[kMaxWinThreads];

#define CHECK_WIN_THREAD_FUNC(x) \
	do { \
		if(NULL == (x)) { \
			wchar_t wstr[256]; \
			swprintf_s(wstr, L"Error detected from call %s at line %d of main.cpp", _T(#x), __LINE__); \
			ReportWinThreadError(wstr); \
		} \
	} \
	while(0)

DWORD WINAPI CompressImageMT_Thread( LPVOID lpParam );

void ReportWinThreadError(const wchar_t *str) {

	// Retrieve the system error message for the last-error code.
	LPVOID lpMsgBuf;
	LPVOID lpDisplayBuf;
	DWORD dw = GetLastError(); 

	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | 
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		dw,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR) &lpMsgBuf,
		0, NULL );

	// Display the error message.

	lpDisplayBuf = (LPVOID)LocalAlloc(LMEM_ZEROINIT, 
		(lstrlen((LPCTSTR) lpMsgBuf) + lstrlen((LPCTSTR)str) + 40) * sizeof(TCHAR)); 
	StringCchPrintf((LPTSTR)lpDisplayBuf, 
		LocalSize(lpDisplayBuf) / sizeof(TCHAR),
		TEXT("%s failed with error %d: %s"), 
		str, dw, lpMsgBuf); 
	MessageBox(NULL, (LPCTSTR) lpDisplayBuf, TEXT("Error"), MB_OK); 

	// Free error-handling buffer allocations.

	LocalFree(lpMsgBuf);
	LocalFree(lpDisplayBuf);
}

	// Figure out how many cores there are on this machine
int GetProcessorCount()
{
	static int sProcessorCount;
	if (sProcessorCount == 0)
	{
		SYSTEM_INFO sysinfo = {};
		GetSystemInfo(&sysinfo);
		sProcessorCount = sysinfo.dwNumberOfProcessors;
		if (sProcessorCount < 1)
			sProcessorCount = 1;
	}

	return sProcessorCount;
}

void InitWin32Threads() {


	// Already initialized?
	if(gNumWinThreads > 0) {
		return;
	}
	
	SetLastError(0);

	gNumWinThreads = GetProcessorCount();

	if(gNumWinThreads >= MAXIMUM_WAIT_OBJECTS)
		gNumWinThreads = MAXIMUM_WAIT_OBJECTS;

	assert(gNumWinThreads <= kMaxWinThreads);

	// Create the synchronization events.
	for(int i = 0; i < gNumWinThreads; i++) {
		CHECK_WIN_THREAD_FUNC(gWinThreadWorkEvent[i] = CreateEvent(NULL, FALSE, FALSE, NULL));
	}

	CHECK_WIN_THREAD_FUNC(gWinThreadStartEvent = CreateEvent(NULL, TRUE, FALSE, NULL));
	CHECK_WIN_THREAD_FUNC(gWinThreadDoneEvent = CreateEvent(NULL, TRUE, FALSE, NULL));

	// Create threads
	for(int threadIdx = 0; threadIdx < gNumWinThreads; threadIdx++) {
		gWinThreadData[threadIdx].state = eThreadState_WaitForData;
		CHECK_WIN_THREAD_FUNC(hThreadArray[threadIdx] = CreateThread(NULL, 0, CompressImageMT_Thread, &gWinThreadData[threadIdx], 0, &dwThreadIdArray[threadIdx]));
	}
}

void DestroyThreads()
{
	//if(gMultithreaded) 
	//{
		// Release all windows threads that may be active...
		for(int i=0; i < gNumWinThreads; i++) {
			gWinThreadData[i].state = eThreadState_Done;
		}

		// Send the event for the threads to start.
		CHECK_WIN_THREAD_FUNC(ResetEvent(gWinThreadDoneEvent));
		CHECK_WIN_THREAD_FUNC(SetEvent(gWinThreadStartEvent));

		// Wait for all the threads to finish....
		DWORD dwWaitRet = WaitForMultipleObjects(gNumWinThreads, hThreadArray, TRUE, INFINITE);
		if(WAIT_FAILED == dwWaitRet)
			ReportWinThreadError(L"DestroyThreads() -- WaitForMultipleObjects");

		// !HACK! This doesn't actually do anything. There is either a bug in the 
		// Intel compiler or the windows run-time that causes the threads to not
		// be cleaned up properly if the following two lines of code are not present.
		// Since we're passing INFINITE to WaitForMultipleObjects, that function will
		// never time out and per-microsoft spec, should never give this return value...
		// Even with these lines, the bug does not consistently disappear unless you
		// clean and rebuild. Heigenbug?
		//
		// If we compile with MSVC, then the following two lines are not necessary.
		else if(WAIT_TIMEOUT == dwWaitRet)
			OutputDebugString("DestroyThreads() -- WaitForMultipleObjects -- TIMEOUT");

		// Reset the start event
		CHECK_WIN_THREAD_FUNC(ResetEvent(gWinThreadStartEvent));
		CHECK_WIN_THREAD_FUNC(SetEvent(gWinThreadDoneEvent));

		// Close all thread handles.
		for(int i=0; i < gNumWinThreads; i++) {
			CHECK_WIN_THREAD_FUNC(CloseHandle(hThreadArray[i]));
		}

		for(int i =0; i < kMaxWinThreads; i++ ){
			hThreadArray[i] = NULL;
		}

		// Close all event handles...
		CHECK_WIN_THREAD_FUNC(CloseHandle(gWinThreadDoneEvent)); 
		gWinThreadDoneEvent = NULL;
			
		CHECK_WIN_THREAD_FUNC(CloseHandle(gWinThreadStartEvent)); 
		gWinThreadStartEvent = NULL;

		for(int i = 0; i < gNumWinThreads; i++) {
			CHECK_WIN_THREAD_FUNC(CloseHandle(gWinThreadWorkEvent[i]));
		}

		for(int i = 0; i < kMaxWinThreads; i++) {
			gWinThreadWorkEvent[i] = NULL;
		}

		gNumWinThreads = 0;
	//}
}

int GetBytesPerBlock(DXGI_FORMAT format)
{
	switch(format) 
	{
		default:
		case DXGI_FORMAT_BC1_UNORM_SRGB:
		case DXGI_FORMAT_BC1_UNORM:
			return 8;
				
		case DXGI_FORMAT_BC3_UNORM_SRGB:
		case DXGI_FORMAT_BC3_UNORM:
		case DXGI_FORMAT_BC7_UNORM_SRGB:
		case DXGI_FORMAT_BC7_UNORM:
        case DXGI_FORMAT_BC6H_UF16:
        case DXGI_FORMAT_BC6H_SF16:
			return 16;
	}
}

bool CompressImageMT(const rgba_surface* input, BYTE* output, CompressionFunc* cmpFunc, DXGI_FORMAT compformat) 
{
	const int numThreads = gNumWinThreads;
	const int bytesPerBlock = GetBytesPerBlock(compformat);

	// We want to split the data evenly among all threads.
	const int linesPerThread = (input->height + numThreads - 1) / numThreads;

	// Load the threads.
	for(int threadIdx = 0; threadIdx < numThreads; threadIdx++) 
	{
		int y_start = (linesPerThread*threadIdx)/4*4;
		int y_end = (linesPerThread*(threadIdx+1))/4*4;
		if (y_end > input->height) y_end = input->height;
		
		WinThreadData *data = &gWinThreadData[threadIdx];
		data->input = *input;
		data->input.ptr = input->ptr + y_start * input->stride;
		data->input.height = y_end-y_start;
		data->output = output + (y_start/4) * (input->width/4) * bytesPerBlock;
		data->state = eThreadState_DataLoaded;
		data->threadIdx = threadIdx;
		data->cmpFunc = cmpFunc;
	}

	// Send the event for the threads to start.
	CHECK_WIN_THREAD_FUNC(ResetEvent(gWinThreadDoneEvent));
	CHECK_WIN_THREAD_FUNC(SetEvent(gWinThreadStartEvent));

	// Wait for all the threads to finish
	if(WAIT_FAILED == WaitForMultipleObjects(numThreads, gWinThreadWorkEvent, TRUE, INFINITE))
			ReportWinThreadError(L"CompressImageDXTWIN -- WaitForMultipleObjects");

	// Reset the start event
	CHECK_WIN_THREAD_FUNC(ResetEvent(gWinThreadStartEvent));
	CHECK_WIN_THREAD_FUNC(SetEvent(gWinThreadDoneEvent));

	return true;
}

DWORD WINAPI CompressImageMT_Thread( LPVOID lpParam ) 
{
	WinThreadData *data = reinterpret_cast<WinThreadData *>(lpParam);

	while(data->state != eThreadState_Done) {

		if(WAIT_FAILED == WaitForSingleObject(gWinThreadStartEvent, INFINITE))
			ReportWinThreadError(L"CompressImageDXTWinThread -- WaitForSingleObject");

		if(data->state == eThreadState_Done)
			break;

		data->state = eThreadState_Running;
		(*(data->cmpFunc))(&data->input, data->output);

		data->state = eThreadState_WaitForData;

		HANDLE workEvent = gWinThreadWorkEvent[data->threadIdx];
		if(WAIT_FAILED == SignalObjectAndWait(workEvent, gWinThreadDoneEvent, INFINITE, FALSE))
			ReportWinThreadError(L"CompressImageDXTWinThread -- SignalObjectAndWait");
	}

	return 0;
}


bool CompressImageST(const rgba_surface* input, uint8_t* output, CompressionFunc* cmpFunc, DXGI_FORMAT compformat)
{
	(*cmpFunc)(input, output);

	return true;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////
//CALL IN TO ISPC CODE
void CompressImageBC1(const rgba_surface* input, BYTE* output)
{
	CompressBlocksBC1(input, output);
}

void CompressImageBC3(const rgba_surface* input, BYTE* output)
{
	CompressBlocksBC3(input, output);
}

#define DECLARE_CompressImageBC7_profile(profile)								\
void CompressImageBC7_ ## profile(const rgba_surface* input, BYTE* output)		\
{																				\
	bc7_enc_settings settings;													\
	GetProfile_ ## profile(&settings);											\
	CompressBlocksBC7(input, output, &settings);								\
}

#define DECLARE_CompressImageBC6H_profile(profile)								\
void CompressImageBC6H_ ## profile(const rgba_surface* input, BYTE* output)		\
{																				\
	bc6h_enc_settings settings;													\
	GetProfile_bc6h_ ## profile(&settings);										\
	CompressBlocksBC6H(input, output, &settings);								\
}

DECLARE_CompressImageBC6H_profile(veryfast);
DECLARE_CompressImageBC6H_profile(fast);
DECLARE_CompressImageBC6H_profile(basic);
DECLARE_CompressImageBC6H_profile(slow);
DECLARE_CompressImageBC6H_profile(veryslow);

DECLARE_CompressImageBC7_profile(ultrafast);
DECLARE_CompressImageBC7_profile(veryfast);
DECLARE_CompressImageBC7_profile(fast);
DECLARE_CompressImageBC7_profile(basic);
DECLARE_CompressImageBC7_profile(slow);
DECLARE_CompressImageBC7_profile(alpha_ultrafast);
DECLARE_CompressImageBC7_profile(alpha_veryfast);
DECLARE_CompressImageBC7_profile(alpha_fast);
DECLARE_CompressImageBC7_profile(alpha_basic);
DECLARE_CompressImageBC7_profile(alpha_slow);