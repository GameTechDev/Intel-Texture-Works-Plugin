/////////////////////////////////////////////////////////////////////////////////////////////
// Copyright 2017 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");// you may not use this file except in compliance with the License.// You may obtain a copy of the License at//// http://www.apache.org/licenses/LICENSE-2.0//// Unless required by applicable law or agreed to in writing, software// distributed under the License is distributed on an "AS IS" BASIS,// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.// See the License for the specific language governing permissions and// limitations under the License.
/////////////////////////////////////////////////////////////////////////////////////////////

#include "PreviewDialog.h"
#include "IntelPluginUIWin.h"
#include "resource.h"

#include <CommCtrl.h>
#include <PIUI.h>
#include <vector>
#include <string>
#include <sstream>


using namespace DirectX;

//For mouse tracking, when panning zooming
POINT mouseDownPos, mouseOldpos;
bool mouseTracking = false;

//Offset of small preview area into larger 100% image buffer, used for panning
int  previewOffsetXTrackingStart = 0;
int  previewOffsetYTrackingStart = 0;

/*****************************************************************************/
template <typename T>
T clampGeneral(T x, T a, T b)
{
	return x < a ? a : (x > b ? b : x);
}

/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/
PreviewDialog::PreviewDialog(IntelPlugin* _plugin)
{
	hDlg = NULL;
	plugin = _plugin;
	globalParams = plugin->GetData();
	formatRecord = plugin->GetFormatRecord();

	previewOffsetX=0;
	previewOffsetY=0;
	previewSpecificChannel = -1; //-1=RGB,0=R,1=G,2=B,3=A
	previewPlanes = 3; //Default is RGB
	previewZoom = 1.0f;
	previewBuffer = NULL;
	previewCompressedBuffer = NULL;
	pixelStep = mouseStep = 0;
	previewBufferWidth = 0;
	previewBufferHeight = 0;
}

PreviewDialog::~PreviewDialog() 
{
	//Exiting the dialog, free space
	if (previewBuffer != NULL)
	{
		delete[] previewBuffer;
		previewBuffer = NULL;
	}
	
	if (previewCompressedBuffer != NULL)
	{
		delete[] previewCompressedBuffer;
		previewCompressedBuffer = NULL;
	}

}

void PreviewDialog::CalculateOptimizationParameters(int width, int height)
{
	int bigestValue = (width>height) ? width:height;
	float divVal = bigestValue/400.f; //400 found by trial and error, every 400 pixels we change opt step
	pixelStep = int(divVal) + 1; //how many pixels to skip when mouseTracking is on (panning)
	pixelStep = clampGeneral(pixelStep, 1, 4);
	mouseStep = int(pixelStep*bigestValue/100.f); //percentage of size threshold to update screen when mouse is moved
	
}

//Convert a channel value (R/G/B) from srcDataPtr to 8bit and copy into tgtDataPtr
//the blitting routines only support 8bit images
bool PreviewDialog::ConvertToUncompressedPreviewTo8Bit(unsigned8 *tgtDataPtr, ScratchImage *srcDataPtr, int index)
{	
	const Image *image = srcDataPtr->GetImages();

	if (image->format == DXGI_FORMAT_R8G8B8A8_UNORM)
	{
		//Get pointer to first (and only) image
		unsigned8 *rowData8bit = image->pixels;
		tgtDataPtr[0] = rowData8bit[index];
	}
	else if (image->format == DXGI_FORMAT_R16G16B16A16_FLOAT)
	{
		//Get pointer to first (and only) image, cast to 16bit for BC6
		unsigned16 *rowData16bit = reinterpret_cast<unsigned16 *>(image->pixels);
				
		//Inverse values of R/G channels only
		tgtDataPtr[0] = FloatToByte(F16toF32(rowData16bit[index]));	
	}

	return true;
}

//Return bytes in human readable format
std::string PreviewDialog::DisplayHumanReadable(int sizeInBytes)
{
	char buffer[MAX_PATH] = {};

	float MBinBytes = 1024.f*1024.f;
	float KBinBytes = 1024.f;

	if (sizeInBytes >= MBinBytes * 100)
		sprintf(buffer, "%d MB", sizeInBytes/(int)MBinBytes);
	else if (sizeInBytes >= MBinBytes)
		sprintf(buffer, "%.1f MB", sizeInBytes/MBinBytes);
	else if (sizeInBytes >= KBinBytes * 100)
		sprintf(buffer, "%d KB", sizeInBytes/(int)KBinBytes);
	else if (sizeInBytes >= KBinBytes)
		sprintf(buffer, "%.1f KB", sizeInBytes/KBinBytes);
	else
		sprintf(buffer, "%d B", sizeInBytes);
	
    return buffer;
}

//Update text control 
void PreviewDialog::UpdateSizeText()
{
	int byteSize = plugin->GetCompressedByteSize();
	if (globalParams->MipMapTypeIndex != MipmapEnum::NONE)	// mip map size included
		byteSize = byteSize * 4 / 3;

	std::string text = DisplayHumanReadable(byteSize);

	SendMessage(GetDlgItem(hDlg, IDC_PREVIEWPROXY_TEXTSIZECOMPRESSED), WM_SETTEXT, 0, reinterpret_cast<LPARAM>(text.c_str()));

	text = DisplayHumanReadable(plugin->GetOriginalByteSize());
	SendMessage(GetDlgItem(hDlg, IDC_PREVIEWPROXY_TEXTSIZEUNCOMPRESSED), WM_SETTEXT, 0, reinterpret_cast<LPARAM>(text.c_str()));
}

//Win32 Preview window 
//Update the text contol which diplays the zoom level
void PreviewDialog::UpdateZoomText()
{
	char buf[10];
	sprintf(buf,"%3d%%", int(previewZoom*100.f));
	SendMessage(GetDlgItem(hDlg, IDC_PREVIEWPROXY_ZOOMTEXT), WM_SETTEXT, 0, reinterpret_cast<LPARAM>(buf));
}

//Issue a redraw on the client window in the previe areas.
//the redraw parameter indicates if the backgound needs to be erased.
void PreviewDialog::InvalidatePreviews(bool redraw)
{
	//Issue a redraw for these areas
	InvalidateRect (hDlg, &previewProxyRect, redraw);
	InvalidateRect (hDlg, &previewProxyCompressedRect, redraw);
}

//Clip offsets to bounds
void PreviewDialog::ClipOffsetsToBounds()
{
	previewOffsetX = clampGeneral(previewOffsetX, 0, int(previewDimensions.x*previewZoom) - previewBufferWidth);
	previewOffsetY = clampGeneral(previewOffsetY, 0, int(previewDimensions.y*previewZoom) - previewBufferHeight);
}

//Called after zoom, to recalculate the correct previewOffset.
//Parameter previosZoom is the previous zoom value. 
//Special handling required to have it zoom always towards the center of the image part visible
void PreviewDialog::ClipPreviewOffset(float previousZoom)
{
	//Half width of preview area in dialog
	float halfWidth = previewAreaDimensions.x*0.5f;
	float halfHeight = previewAreaDimensions.y*0.5f;

	//Calculate preview offset for CENTER for 100% zoom. We use the previous zoom value here.
	//Center is the image pixel(x,y) which is in the center of the currelty visible part
	float origX = (previewOffsetX+halfWidth)*(1.f/previousZoom);
	float origY = (previewOffsetY+halfHeight)*(1.f/previousZoom);

	//Get factor for this offset for unscaled image relative to image size
	origX = origX/previewDimensions.x;
	origY = origY/previewDimensions.y;

	//Calculate new offset now based on the new zoom factor
	previewOffsetX=static_cast<int>(previewDimensions.x*previewZoom*origX-halfWidth);
	previewOffsetY=static_cast<int>(previewDimensions.y*previewZoom*origY-halfHeight);

	//Clip offsets to bounds
	ClipOffsetsToBounds();
}

//Compute the client areas of the preview rectangles and the image width that is visible.
//Should be the size of the preview are unless the image is smaller.
//Returns a flag indicating the need for background clear.
//Is to be called after zoom or window resize
bool PreviewDialog::CalculateBounds()
{
	previewDimensions = plugin->GetPreviewDimensions();

	bool redraw = false;

	//Get window rectangles for bitmaps (for displayPixels) in client coordinates
	//Get in screen coordinates, for compressed texture
	GetWindowRect(GetDlgItem(hDlg, IDC_PREVIEWPROXY_COMPRESSED), &previewProxyCompressedRect);	
	//Translate to client coords
	MapWindowRect(NULL, hDlg, &previewProxyCompressedRect);
	
	//Get in screen coordinates, for original texture
	GetWindowRect(GetDlgItem(hDlg, IDC_PREVIEWPROXY_ORIGINAL), &previewProxyRect);	
	//Translate to client coords
	MapWindowRect(NULL, hDlg, &previewProxyRect);
			
	//Calculate width,height of preview area (calculated only once because both windows are the same)
	int width = previewProxyRect.right - previewProxyRect.left;
	int height = previewProxyRect.bottom - previewProxyRect.top;
	//Store locally in global
	previewAreaDimensions.x = width; 
	previewAreaDimensions.y = height;

	//If previewarea bigger than real image (scaled using zoom factor) then clip size and 
	//since the image displayed is smaller then the preview window flag a background clear
	if (width > previewDimensions.x*previewZoom)
	{
		redraw=true;
		width = int(previewDimensions.x*previewZoom);
	}
	if (height > previewDimensions.y*previewZoom)
	{
		redraw=true;
		height = int(previewDimensions.y*previewZoom);
	}

	//Store width of visible image (is same as preview area width, except when image is smaller than preview area, or zoomlevel <1)
	previewBufferWidth = width;
	previewBufferHeight = height;

	CalculateOptimizationParameters(previewAreaDimensions.x, previewAreaDimensions.y);

	//Return flag indicating the need of a background redraw or not
	return redraw;
}

//Copy into the previewBuffer/previewCompressedBuffer which gets displayed in PaintProxy the part of the image that is visible
//The image that is visible is determined using the previewOffset (resulting from panning the image) and
//the zoomLevel specified
void PreviewDialog::updatePreviewBuffer()
{
	static int colorMask[] = { 
		IntelPlugin::PREVIEW_CHANNEL_RGB, 
		IntelPlugin::PREVIEW_CHANNEL_RED, 
		IntelPlugin::PREVIEW_CHANNEL_GREEN, 
		IntelPlugin::PREVIEW_CHANNEL_BLUE, 
		IntelPlugin::PREVIEW_CHANNEL_ALPHA 
	};

	int mask = colorMask[previewSpecificChannel+1];	// see definition, -1 is used for all channels

	plugin->FetchPreviewRGB(previewBuffer, previewAreaDimensions.x, previewAreaDimensions.y, previewOffsetX, previewOffsetY, 1.f/previewZoom, mask);
	plugin->FetchPreviewRGB(previewCompressedBuffer, previewAreaDimensions.x, previewAreaDimensions.y, previewOffsetX, previewOffsetY, 1.f/previewZoom, mask | IntelPlugin::PREVIEW_SOURCE_COMPRESSED);
	UpdateSizeText();
}

//Draw two image proxies
void PreviewDialog::PaintProxy()
{
	PSPixelMap pixels;
	PSPixelMap pixelsCompressed;
	//PSPixelMask mask;
	PAINTSTRUCT  ps;
	//POINT	mapOrigin; 
	HDC		hDC;

	//Src rect which will get rendered form inside previeBuffer
	VRect inRect = {0, 0, previewBufferHeight, previewBufferWidth};
	
	//If there is an error return
	if (plugin->GetResult() != noErr)
		return;

	//Compute offset to centre the image if its smaller than the preview Area
	int centreXOffset = 0;
	int centreYOffset = 0;
	if (previewAreaDimensions.x > previewBufferWidth)
		centreXOffset = static_cast<int>((previewAreaDimensions.x - previewBufferWidth)*0.5);
	if (previewAreaDimensions.y > previewBufferHeight)
		centreYOffset = static_cast<int>((previewAreaDimensions.y - previewBufferHeight)*0.5);
	
	hDC = BeginPaint(hDlg, &ps);	

	//Paint the black frame with a one pixel space
	//between the image and the frame
	InflateRect(&previewProxyRect, 2, 2);
	FrameRect(hDC, &previewProxyRect, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));	
	InflateRect(&previewProxyRect, -2, -2);
	InflateRect(&previewProxyCompressedRect, 2, 2);
	FrameRect(hDC, &previewProxyCompressedRect, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));	
	InflateRect(&previewProxyCompressedRect, -2, -2);
	
	//Init the PSPixel map
	pixels.version = 1;
	pixels.bounds.top = 0;
	pixels.bounds.left = 0;
	pixels.bounds.bottom = inRect.bottom;
	pixels.bounds.right = inRect.right;
	pixels.imageMode = plugInModeRGBColor;
	pixels.rowBytes = previewAreaDimensions.x*3;
	pixels.colBytes = 3;
	pixels.planeBytes = 1;
	pixels.baseAddr = previewBuffer;

	pixels.mat = NULL;
	pixels.masks = NULL;
	pixels.maskPhaseRow = 0;
	pixels.maskPhaseCol = 0;

	pixelsCompressed = pixels;
	pixelsCompressed.baseAddr = previewCompressedBuffer;

	(plugin->GetFormatRecord()->displayPixels)(&pixels, 
		                           &inRect,//&pixels.bounds, 
								   previewProxyRect.top + centreYOffset, 
								   previewProxyRect.left + centreXOffset, 
								   hDC);

	(plugin->GetFormatRecord()->displayPixels)(&pixelsCompressed, 
		                           &inRect,//&pixels.bounds, 
								   previewProxyCompressedRect.top + centreYOffset, 
								   previewProxyCompressedRect.left + centreXOffset, 
								   hDC);

	EndPaint(hDlg, &ps);
}

//Zoom proxy preview
//Calculates new bounds (width/height of viewable area), new offsets, 
//updates the buffer, text controls, and issues a win32 redraw
void PreviewDialog::zoomImage(float previousZoom, float currentZoom)
{
	previewZoom = currentZoom;

	//Calculate preview area sizes because of new zoom
	bool redraw = CalculateBounds();

	//Calculate new offset because zoom changed
	ClipPreviewOffset(previousZoom);

	//Update the pixel buffer with this zoomLevel
	updatePreviewBuffer();
						
	//Update ui which shows the zoom level
	UpdateZoomText();
						
	//Issue a redraw for these areas
	InvalidatePreviews(redraw);
}

//Fill the preview compression table with valid entries for the texture type defined in gGlobalParams->TextureTypeIndex
void PreviewDialog::FillCompressionCombo()
{
	compressionModesTable_.clear();

	//BC1, BC1_SRGB, BC3, BC3_SRGB, BC6H_FAST, BC6H_FINE, BC7_FAST, BC7_FINE, BC7_SRGB_FAST, BC7_SRGB_FINE, BC4, BC5, NONE
	if (plugin->IsCombinationValid(globalParams->TextureTypeIndex,CompressionTypeEnum::BC1))
		compressionModesTable_.push_back(CompressModesStruct(DXGI_FORMAT_BC1_UNORM, "BC1   4bpp  (Linear)", false));

	if (plugin->IsCombinationValid(globalParams->TextureTypeIndex,CompressionTypeEnum::BC1_SRGB))
		compressionModesTable_.push_back(CompressModesStruct(DXGI_FORMAT_BC1_UNORM_SRGB, "BC1   4bpp  (sRGB, DX10+)", false));
	
	if (plugin->IsCombinationValid(globalParams->TextureTypeIndex,CompressionTypeEnum::BC3))
		compressionModesTable_.push_back(CompressModesStruct(DXGI_FORMAT_BC3_UNORM, "BC3   8bpp  (Linear)", false));

	if (plugin->IsCombinationValid(globalParams->TextureTypeIndex,CompressionTypeEnum::BC3_SRGB))
		compressionModesTable_.push_back(CompressModesStruct(DXGI_FORMAT_BC3_UNORM_SRGB, "BC3   8bpp  (sRGB, DX10+)", false));
	
	if (plugin->IsCombinationValid(globalParams->TextureTypeIndex,CompressionTypeEnum::BC6H_FAST))
		compressionModesTable_.push_back(CompressModesStruct(DXGI_FORMAT_BC6H_UF16, "BC6H  8bpp  Fast (Linear, DX11+)", true));
	
	if (plugin->IsCombinationValid(globalParams->TextureTypeIndex,CompressionTypeEnum::BC6H_FINE))
		compressionModesTable_.push_back(CompressModesStruct(DXGI_FORMAT_BC6H_UF16, "BC6H  8bpp  Fine (Linear, DX11+)", false));
	
	if (plugin->IsCombinationValid(globalParams->TextureTypeIndex,CompressionTypeEnum::BC7_FAST))
		compressionModesTable_.push_back(CompressModesStruct(DXGI_FORMAT_BC7_UNORM, "BC7   8bpp  Fast (Linear, DX11+)", true));
	
	if (plugin->IsCombinationValid(globalParams->TextureTypeIndex,CompressionTypeEnum::BC7_FINE))
		compressionModesTable_.push_back(CompressModesStruct(DXGI_FORMAT_BC7_UNORM, "BC7   8bpp  Fine (Linear, DX11+)", false));
	
	if (plugin->IsCombinationValid(globalParams->TextureTypeIndex,CompressionTypeEnum::BC7_SRGB_FAST))
		compressionModesTable_.push_back(CompressModesStruct(DXGI_FORMAT_BC7_UNORM_SRGB, "BC7   8bpp  Fast (sRGB, DX11+)", true));
	
	if (plugin->IsCombinationValid(globalParams->TextureTypeIndex,CompressionTypeEnum::BC7_SRGB_FINE))
		compressionModesTable_.push_back(CompressModesStruct(DXGI_FORMAT_BC7_UNORM_SRGB, "BC7   8bpp  Fine (sRGB, DX11+)", false));
	
	if (plugin->IsCombinationValid(globalParams->TextureTypeIndex,CompressionTypeEnum::BC4))
		compressionModesTable_.push_back(CompressModesStruct(DXGI_FORMAT_BC4_UNORM, "BC4   4bpp  (Linear, Grayscale)", false));
	
	if (plugin->IsCombinationValid(globalParams->TextureTypeIndex,CompressionTypeEnum::BC5))
		compressionModesTable_.push_back(CompressModesStruct(DXGI_FORMAT_BC5_UNORM, "BC5   8bpp  (Linear, 2 Channel tangent map)", false));
	
	if (plugin->IsCombinationValid(globalParams->TextureTypeIndex,CompressionTypeEnum::UNCOMPRESSED))
		compressionModesTable_.push_back(CompressModesStruct(DXGI_FORMAT_R8G8B8A8_UNORM, "none  32bpp", false));
}

//Fill Preview combo with all current compression settings and select the current one
//The current settings are taken from the global plugin struct
void PreviewDialog::InitPreviewComboBox()
{
	//Init Combo Box Instead of the static text 
	HWND combo = GetDlgItem(hDlg, IDC_PREVIEWPROXY_COMBOBOX);
				
	//Clear combo
	SendMessage(combo, CB_RESETCONTENT, 0, 0);
				
	//Fill compressionModesTable
	FillCompressionCombo();

	//Fill Combo with predefined compressionModesTable entries
	for (int i=0;i<compressionModesTable_.size();i++)
	{
		SendMessage(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(compressionModesTable_[i].formatName.c_str()));

		//Set selected index, if this is the matching index.
		if (globalParams->encoding_g == compressionModesTable_[i].format && globalParams->fast_bc67 == compressionModesTable_[i].fast)
			SendMessage(combo, CB_SETCURSEL, WPARAM(i), 0);
	}

	//Define Font
	LOGFONT lf = {}; // to define the font
	lf.lfHeight = 14;
	//lf.lfWidth = ;
	lf.lfWeight = FW_NORMAL;
	lf.lfCharSet = ANSI_CHARSET;
	lf.lfOutPrecision = OUT_DEFAULT_PRECIS;
	lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
	lf.lfQuality = PROOF_QUALITY;
	lf.lfPitchAndFamily = FIXED_PITCH | FF_MODERN;
	strcpy(lf.lfFaceName,"Consolas");

	if (auto hFont  = ::CreateFontIndirect(&lf))
	{
		SendMessage(combo, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), LPARAM(TRUE));
	}
}

//Calculate zoom value so that image is shown in its entirety
void PreviewDialog::zoomToFit()
{
	if (previewDimensions.x > previewDimensions.y)
	{
		//Image is wider than tall
		previewZoom = clampGeneral (float(previewAreaDimensions.x) / previewDimensions.x, 0.01f, 1.f);
	}
	else
	{
		//Image is taller than wide
		previewZoom = clampGeneral (float(previewAreaDimensions.y) / previewDimensions.y, 0.01f, 1.f);
	}
}

//Allocate preview buffer for this proxy area, proxyBuffer can only be RGB. Therefore 3
void PreviewDialog::allocateBuffers()
{
	if (previewBuffer != NULL)
		delete[] previewBuffer;

	if (previewCompressedBuffer != NULL)
		delete[] previewCompressedBuffer;


	previewBuffer = new unsigned8[previewAreaDimensions.x*previewAreaDimensions.y*3];
	previewCompressedBuffer = new unsigned8[previewAreaDimensions.x*previewAreaDimensions.y*3];
}


//Init planes and fill photoshop buffer with image
void PreviewDialog::Init()
{
	//Store number of available planes
	previewPlanes = plugin->GetFormatRecord()->planes;
	previewPlanes = clampGeneral(previewPlanes, 1, 4);
}

//Test if global state different then the one selected in combo box
bool PreviewDialog::IsSelectedEncodingDifferent(int index)
{
	if (index < compressionModesTable_.size())
	{
		return (!(globalParams->encoding_g == compressionModesTable_[index].format &&
					globalParams->fast_bc67  == compressionModesTable_[index].fast));
	}

	return false;
}

//Set global state encoding from table. The index into the table is taken form the combo box
void PreviewDialog::SetGlobalEncoding(int index)
{	
	if (index < compressionModesTable_.size())
	{
		globalParams->encoding_g = compressionModesTable_[index].format;
		globalParams->fast_bc67  = compressionModesTable_[index].fast;
	}
}

//	maxDimension is a percentage of parent size (null = desktop)
//  eg passing max dim 0.3 means the window must fit within a rectangle that is
//  0.3 times the size of the parent.
//  minWidth sets a lower limit to stop dialogs collapsing too much
void PreviewDialog::RelativeScale(HWND hwnd, HWND parent_hwnd, float maxDimension, int minWidth)
{
	if (parent_hwnd == NULL)
		parent_hwnd = GetDesktopWindow();

    RECT rcSelf = {};
    RECT rcParent = {};

	GetWindowRect(hwnd, &rcSelf);
	GetWindowRect(parent_hwnd, &rcParent);

	// pick a size relative to desktop/parent size
	float scale = (rcParent.right - rcParent.left) * maxDimension / (rcSelf.right - rcSelf.left);
	float scaleY = (rcParent.bottom - rcParent.top) * maxDimension / (rcSelf.bottom - rcSelf.top);

	if (scaleY < scale)
		scale = scaleY;

		// make sure we don't make it too small
	float scaleMin = float(minWidth) / (rcSelf.right - rcSelf.left);
	if (scale < scaleMin)
		scale = scaleMin;

	rcSelf.right = int((rcSelf.right - rcSelf.left) * scale + rcSelf.left);
	rcSelf.bottom = int((rcSelf.bottom - rcSelf.top) * scale + rcSelf.top);

	// our coordinates are screen based, convert to client here
	MapWindowRect(NULL, parent_hwnd, &rcSelf);
	MoveWindow(hwnd, rcSelf.left, rcSelf.top, rcSelf.right - rcSelf.left, rcSelf.bottom - rcSelf.top, FALSE);
}

BOOL CALLBACK PreviewDialog::WndEnumProc(HWND hwnd, LPARAM self_lparam)
{
	PreviewDialog* self = reinterpret_cast<PreviewDialog*>(self_lparam);

	RECT rcParent;
	RECT rc;

	GetClientRect(self->hDlg, &rcParent);
	GetWindowRect(hwnd, &rc);
	MapWindowRect(NULL, self->hDlg, &rc);

	self->scaledPositions.push_back(std::make_pair(hwnd, ScalableRect(rcParent, rc)));
	return TRUE;
}
 
	// message handler
BOOL PreviewDialog::WindowProc(UINT wMsg, WPARAM wParam, LPARAM lParam)   // Win32 Change
{
    switch  (wMsg)
	{
		case WM_INITDIALOG:
			{
				//Save as static for cross functiona call use
				// Initialize compression name text control, or combo box with names
				InitPreviewComboBox();
				
				EnumChildWindows(hDlg, WndEnumProc, reinterpret_cast<LPARAM>(this));

					// pick a reasonable default for current view
				RelativeScale(hDlg, NULL, 0.4f, 600);

				//Center dialog and update zoom level ui indication
				CenterDialog(hDlg);

				Init();
				
				//Calculate initial bounds with zoom of 1 preview area sizes
	            CalculateBounds();

				//Calculate zoom so that image is shown in its entirety
				zoomToFit();
					
				//Calculate new bounds because zoom changed
	            CalculateBounds();

				//Calculate new offset because zoom changed
				ClipPreviewOffset(1.f);
				
				//Update zoom level ui text
				UpdateZoomText();

				//Allocate preview buffer for this proxy area, proxyBuffer can only be RGB. 
				allocateBuffers();

				//Fill preview buffer
				updatePreviewBuffer();
				
				//Load zoom icons into the buttons
				HANDLE hMyIcon = LoadImage(GetDLLInstance(), "ZOOMINICON",IMAGE_ICON,13,13,NULL);
				HANDLE hMyIcon2 = LoadImage(GetDLLInstance(), "ZOOMOUTICON",IMAGE_ICON,13,13,NULL);
				if (hMyIcon != NULL)
				{
					SendDlgItemMessage(hDlg, IDC_PREVIEWPROXY_ZOOMIN, BM_SETIMAGE, IMAGE_ICON, reinterpret_cast<LPARAM>(hMyIcon));
				}
				if (hMyIcon2 != NULL)
				{
					SendDlgItemMessage(hDlg, IDC_PREVIEWPROXY_ZOOMOUT, BM_SETIMAGE, IMAGE_ICON, reinterpret_cast<LPARAM>(hMyIcon2));
				}
				
				if (auto slider = GetDlgItem(hDlg, IDC_EXPOSURE_SLIDER))
				{
					if (plugin->GetFormatRecord()->depth > 16)
					{
						ShowWindow(slider, SW_SHOW);
						SendMessage(slider, TBM_SETRANGEMIN, FALSE, -1000);
						SendMessage(slider, TBM_SETRANGEMAX, TRUE, 1000);
						SendMessage(slider, TBM_SETPOS, TRUE, 0);
					}
					else
					{
						ShowWindow(slider, SW_HIDE);
					}
				}

				return  TRUE;
			}
		case WM_PAINT:
			PaintProxy();
			return FALSE;

		case WM_SIZE:
			{
				//new width, height of window
				int width=LOWORD (lParam); 
				int height=HIWORD (lParam); 

				if (true)
				{
					RECT rect = {};
					rect.right = width;
					rect.bottom = height;
					for (auto it = scaledPositions.begin(); it != scaledPositions.end(); ++it)
					{
						auto hwnd = it->first;
						auto scalableRect = it->second;
						if (IsWindow(hwnd))
						{
							RECT rc = scalableRect.Get(rect);
							MoveWindow(hwnd, rc.left, rc.top, rc.right - rc.left, rc.bottom-rc.top,FALSE);
						}
					}
					CalculateBounds();
					allocateBuffers();
				}

				//Update view
				float prevZoom = getCurrentZoom();
				zoomImage(prevZoom, prevZoom);
				InvalidateRect(hDlg, NULL, true);
				break;
			}
		case WM_COMMAND:
			{
				float prevZoom = 0;
				float newZoom = 1;
				int idd = COMMANDID (wParam);              // WIN32 Change

				switch  (idd)
				{
					case OK:
					case CANCEL:
						EndDialog(hDlg, idd);
						break;

					case IDC_PREVIEWPROXY_ZOOMIN:
						//Zoom in button was pressed
						//Compute new zoom level
						prevZoom = getCurrentZoom();
						newZoom = prevZoom + 0.1f;
						newZoom =clampGeneral(newZoom, 0.05f, 4.f);
						zoomImage(prevZoom, newZoom);
						break;
					case IDC_PREVIEWPROXY_ZOOMOUT:
						//Zoom in button was pressed
						//Compute new zoom level
						prevZoom = getCurrentZoom();
						newZoom = prevZoom - 0.1f;
						newZoom =clampGeneral(newZoom, 0.05f, 4.f);
						zoomImage(prevZoom, newZoom);
						break;
					case IDC_PREVIEWPROXY_ZOOM1X:
						//Zoom 1x button was pressed
						//Compute new zoom level
						prevZoom = getCurrentZoom();
						zoomImage(prevZoom, 1);
						break;
					case IDC_PREVIEWPROXY_ZOOM2X:
						//Zoom 2x button was pressed
						//Compute new zoom level
						prevZoom = getCurrentZoom();
						zoomImage(prevZoom, 2);
						break;
					case IDC_PREVIEWPROXY_ZOOM4X:
						//Zoom 4x button was pressed
						//Compute new zoom level
						prevZoom = getCurrentZoom();
						zoomImage(prevZoom, 4);
						break;
					case IDC_PREVIEWPROXY_ZOOMHALF:
						//Zoom 1/2x button was pressed
						//Compute new zoom level
						prevZoom = getCurrentZoom();
						zoomImage(prevZoom, 0.5f);
						break;
					case IDC_PREVIEWPROXY_ZOOMQUARTER:
						//Zoom 1/2x button was pressed
						//Compute new zoom level
						prevZoom = getCurrentZoom();
						zoomImage(prevZoom, 0.25f);
						break;
					case IDC_PREVIEWPROXY_ZOOMFIT:
						//Calculate zoom so that image is shown in its entirety
						//Compute new zoom level
						prevZoom = getCurrentZoom();
						zoomToFit();
						zoomImage(prevZoom, getCurrentZoom());
						break;
					case IDC_PREVIEWPROXY_RGB:
						setPreviewSpecificChannel(-1); //RGB
						//Update the pixel buffer with this channel info
						updatePreviewBuffer();
						//Issue a redraw for these areas
						InvalidatePreviews(false);
						break;
					case IDC_PREVIEWPROXY_R:
						setPreviewSpecificChannel(0); //R
						//Update the pixel buffer with this channel info
						updatePreviewBuffer();
						//Issue a redraw for these areas
						InvalidatePreviews(false);
						break;
					case IDC_PREVIEWPROXY_G:
						setPreviewSpecificChannel(1); //G
						//Update the pixel buffer with this channel info
						updatePreviewBuffer();
						//Issue a redraw for these areas
						InvalidatePreviews(false);
						break;
					case IDC_PREVIEWPROXY_B:
						setPreviewSpecificChannel(2); //B
						//Update the pixel buffer with this channel info
						updatePreviewBuffer();
						//Issue a redraw for these areas
						InvalidatePreviews(false);
						break;
					case IDC_PREVIEWPROXY_A:
						setPreviewSpecificChannel(3); //A
						//Update the pixel buffer with this channel info
						updatePreviewBuffer();
						//Issue a redraw for these areas
						InvalidatePreviews(false);
						break;
					case IDC_PREVIEWPROXY_COMBOBOX:
						{
							//Did the combo box change
							if ( HIWORD(wParam) == CBN_SELCHANGE) 
							{   
								//Get its current index
				                HWND combo = GetDlgItem(hDlg, IDC_PREVIEWPROXY_COMBOBOX);
								int index = int(SendMessage(combo, CB_GETCURSEL, 0, 0));
					        	
								//If encoding different
								if (IsSelectedEncodingDifferent(index))
								{
									//Store settigns selected in combo box
									SetGlobalEncoding(index);
								}
							
								//Update the pixel buffer with this channel info
								updatePreviewBuffer();

								//Issue a redraw for these areas
								InvalidatePreviews(false);
							}
						}
						break;
					default:
						return FALSE;
				}
				break;
			}
		case WM_DESTROY:
			{
				return FALSE;
			}
		case WM_LBUTTONDOWN:
			        //Set focus to window, since the combo will have the focus by default
			        SetFocus(GetActiveWindow());
			        
					//Start mouse tracking
					GetCursorPos(&mouseDownPos); //Get mouse pos in screen coords
					mouseOldpos = mouseDownPos;  //Store old mouse position for update every 5 pixels

					//Store offset value
					getPreviewOffset(previewOffsetXTrackingStart, previewOffsetYTrackingStart);
					SetCapture(hDlg);
                    break; 

		case WM_MOUSEMOVE:
			        if (GetCapture() == hDlg)
					{
						//Do we track the mouse for panning
						POINT newpos;
						GetCursorPos(&newpos); //Get mouse pos in screen coords

						//Calculate new offset values
						setPreviewOffset(previewOffsetXTrackingStart - (newpos.x - mouseDownPos.x),
							                            previewOffsetYTrackingStart - (newpos.y - mouseDownPos.y));
	
						//Clip offsets to bounds
						ClipOffsetsToBounds();
						
						//Update only if mouse moved 5 pixels (otherwise lock gaps)
						if ((abs(mouseOldpos.x - newpos.x) > getMouseStep()) || (abs(mouseOldpos.y - newpos.y) > getMouseStep()))
						{
							mouseOldpos = newpos;
						    updatePreviewBuffer();
						}

						//Issue a redraw for these areas
						InvalidatePreviews(false);
					}
					break;
		case WM_LBUTTONUP:
					//Stop mouse tracking
					if (GetCapture() == hDlg)
						ReleaseCapture();
					break;

		case WM_CAPTURECHANGED:
					updatePreviewBuffer();
					//Issue a redraw for these areas
					InvalidatePreviews(false);
                    break; 

		case WM_MOUSEWHEEL:
					{
						//Zoom in/out using mouse wheel, update value and text
				        float prevZoom = getCurrentZoom();
						float zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
						float newZoom = float(prevZoom * pow(1.05, zDelta / WHEEL_DELTA));
						newZoom =clampGeneral(newZoom, 0.05f, 4.f);
						zoomImage(prevZoom, newZoom);
						break;
					}
		case WM_HSCROLL:
		case WM_VSCROLL:
			switch(LOWORD(wParam))
			{
			case SB_ENDSCROLL:
			case SB_THUMBPOSITION:
			case SB_THUMBTRACK:
				if (auto slider = GetDlgItem(hDlg, IDC_EXPOSURE_SLIDER))
				{
					auto pos = SendMessage(slider, TBM_GETPOS, 0, 0);
					float v = float(pow(2, pos * 0.01));
					if (globalParams->exposure != v)
					{
						globalParams->exposure = v;
						updatePreviewBuffer();
						InvalidatePreviews(false);
					}
				}
				break;
			}
			break;
 		default:
			return  FALSE;
	}

    return  TRUE;
}

// static callback for DialogBoxParam
INT_PTR WINAPI PreviewDialog::PreviewProc(HWND hwnd, UINT wMsg, WPARAM wParam, LPARAM lParam)   // Win32 Change
{
	PreviewDialog *previewDialog = NULL; 
	if (wMsg == WM_INITDIALOG)
	{
		previewDialog = reinterpret_cast<PreviewDialog *>(lParam);
		previewDialog->hDlg = hwnd;
		SetWindowLongPtr(hwnd, DWLP_USER, reinterpret_cast<LONG_PTR>(previewDialog));
	}
	else if (hwnd)
	{
		previewDialog = reinterpret_cast<PreviewDialog *>(GetWindowLongPtr(hwnd, DWLP_USER));
	}

	if (previewDialog)
		return previewDialog->WindowProc(wMsg, wParam, lParam);

	return 0;
}

//Show the UI dialog
void PreviewDialog::Modal()
{
	//If not enought layers for cubemaps preview exit
	if ((globalParams->TextureTypeIndex == TextureTypeEnum::CUBEMAP_LAYERS) && plugin->GetLayerCount() < 6)
	{
		errorMessage("Cubemap has not enough layers available. Consult the documentation (question mark next to the TextureType drop down) on how to create cubemaps", "Preview Error");
		return;
	}
		
	DialogBoxParam(GetDLLInstance(),
		           MAKEINTRESOURCE( IDD_PREVIEW ),
					GetActiveWindow(),
					PreviewProc,
					reinterpret_cast<LPARAM>(this));
}
