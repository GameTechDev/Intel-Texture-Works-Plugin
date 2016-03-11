/*

  Copyright (c) 2015, Intel Corporation
  All rights reserved.

*/

#pragma once

#include "IntelPlugin.h"

#include <vector>
#include <list>

class ScalableOrdinate
{
	double anchor;	// 0 = left/top, 1 =  right/bottom
	double offset;	// simple offset to add

public:
	ScalableOrdinate() { anchor = offset = 0; }

	double Get(double lo, double hi) const
	{
		return lo * (1-anchor) + hi * anchor + offset;
	}

	void Set(double _anchor, double _offset)
	{
		anchor = _anchor;
		offset = _offset;
	}

	void Set(double lo, double hi, double v)
	{
		anchor = (v - lo) / (hi - lo);
		offset = 0;

			// auto select segment to pin to here
		if (anchor > 0.6666)
			anchor = 1;
		else if (anchor > 0.33333)
			anchor = 0.5;
		else
			anchor = 0;

		Set(lo, hi, anchor, v);
	}

	void Set(double lo, double hi, double _anchor, double v)
	{
		anchor = _anchor;
		offset = v - (lo * (1-anchor) + hi * anchor);
	}
};

class ScalableRect
{
	ScalableOrdinate left, top, right, bottom;
public:
	void Set(const RECT& parent, const RECT& rc)
	{
		left.Set(parent.left, parent.right, rc.left);
		right.Set(parent.left, parent.right, rc.right);
		top.Set(parent.bottom, parent.top, rc.top);
		bottom.Set(parent.bottom, parent.top, rc.bottom);
	}
	ScalableRect(const RECT& parent, const RECT& rc)	// construct
	{
		Set(parent, rc);
	}

	RECT Get(const RECT& parent) const
	{
		RECT rc;
		rc.left = int(left.Get(parent.left, parent.right));
		rc.right = int(right.Get(parent.left, parent.right));
		rc.top = int(top.Get(parent.bottom, parent.top));
		rc.bottom = int(bottom.Get(parent.bottom, parent.top));
		return rc;
	}
};


class PreviewDialog 
{
private:
	HWND hDlg;
	std::list<std::pair<HWND, ScalableRect>> scaledPositions;

	//Structure for representing Compression pairs for the Preview Combo box
	struct CompressModesStruct
	{
		DXGI_FORMAT format;
		std::string formatName;
		bool fast;
	
		CompressModesStruct(DXGI_FORMAT format_, std::string formatname_, bool fast_) : format(format_), formatName(formatname_), fast(fast_) { }
	};

	//All available preview compression options for the texture type defined in gGlobalParams->TextureTypeIndex
	std::vector<CompressModesStruct>  compressionModesTable_;

	//Pointer to photoshop API

	IntelPlugin* plugin;
    IntelPlugin::Globals* globalParams;              
	const FormatRecord* formatRecord;	// data about the source image from photoshop (caution!)

	//Areas in client space where the image proxies are located
	RECT     previewProxyRect;
	RECT     previewProxyCompressedRect;

	//Size of image displayed in preview buffer. 
	//Normaly the preview are, except when the image (or the zoomed image) is smaller than the preview area. 
	int      previewBufferWidth;
	int      previewBufferHeight;

	//Preview buffer, just the size of the preview area
	uint8_t  *previewBuffer;
	uint8_t  *previewCompressedBuffer;

	//Stores width,height of preview area, convenience variable
	POINT previewAreaDimensions;
	POINT previewDimensions;

	//Zoom scale of original image. 100%=1
	float previewZoom;

	//Offset of small preview area into larger 100% image buffer, used for panning
	int  previewOffsetX;
	int  previewOffsetY;
	
	int  previewSpecificChannel; //-1=RGB,0=R,1=G,2=B,3=A
	int  previewPlanes;          //Default is RGB=3

	static void RelativeScale(HWND hwnd, HWND parent_hwnd, float maxDimension,int minWidth);
	static BOOL CALLBACK WndEnumProc(HWND, LPARAM);
	static INT_PTR WINAPI PreviewProc(HWND hwnd, UINT wMsg, WPARAM wParam, LPARAM lParam);
	BOOL WindowProc(UINT wMsg, WPARAM wParam, LPARAM lParam);

	//Preview Optimization parameters
	int pixelStep;
	int mouseStep;


public:

	//Getter/Setters
	void setPreviewSpecificChannel(int channel) { previewSpecificChannel=channel;}
	void getPreviewOffset(int &X, int &Y)       { X = previewOffsetX; Y=previewOffsetY;}
	void setPreviewOffset(int X, int Y)         { previewOffsetX = X; previewOffsetY = Y;}
    float getCurrentZoom()                      { return previewZoom; }
	int   getMouseStep()                        { return mouseStep;}
	bool IsSelectedEncodingDifferent(int index);
	void SetGlobalEncoding(int index);
									 

	//Allocate preview buffer for this proxy area
	void allocateBuffers();
	
	//Show main UI
	void Modal();

	//First function to call
	void Init();
	
	bool ConvertToUncompressedPreviewTo8Bit(unsigned8 *tgtDataPtr, DirectX::ScratchImage *srcDataPtr, int index);
	void UpdateZoomText();
	void InvalidatePreviews(bool redraw);
	void ClipOffsetsToBounds();
	void ClipPreviewOffset(float previousZoom);
	bool CalculateBounds();
	void updatePreviewBuffer();
	void PaintProxy();
	void zoomImage(float previousZoom, float currentZoom);
	void FillCompressionCombo();
	void InitPreviewComboBox();
	void zoomToFit();
	void UpdateSizeText();
	std::string DisplayHumanReadable(int sizeInBytes);
	void CalculateOptimizationParameters(int width, int height);


	explicit  PreviewDialog(IntelPlugin* plugin);
	~PreviewDialog();
};