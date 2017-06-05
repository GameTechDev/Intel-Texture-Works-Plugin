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

#include "IntelPluginName.h"

#include <PIExport.h>				// Export Photoshop header file.
#include <PIUtilities.h>			// SDK Utility library.
#include <PIColorSpaceSuite.h>
#include <FileUtilities.h>

#include <dxgiformat.h>
#include <win32Threads.h>
#include <directxtex.h>
#include <DirectXPackedVector.h>

inline float F16toF32(unsigned short f16)
{
	return DirectX::PackedVector::XMConvertHalfToFloat(f16);
}

inline unsigned short F32toF16(float val)
{
	return DirectX::PackedVector::XMConvertFloatToHalf(val);
}

inline unsigned char FloatToByte(double v)
{
	if (v > 1)	
		return 255;
	else if (v < 0)
		return 0;
	return static_cast<unsigned char>(v * 255);
}

inline unsigned char F16toByte(unsigned short f16)
{
	return FloatToByte(F16toF32(f16));
}

//Three different convert to 8bit functions (8->8, 16->8, 32->8 all photoshop specific)
inline unsigned8 ConvertTo8Bit(unsigned8 val)
{
	return val;
}
inline unsigned8 ConvertTo8Bit(unsigned16 val)
{
	//Photoshop uses an internal representation of 16 bit data of 0 - 32768
	double max_16bitPhotoshop = 32768;
	
	return FloatToByte(val/max_16bitPhotoshop);
}
inline unsigned8 ConvertTo8Bit(double val, bool gammaCorrect=true)
{
	//Photoshop uses for 32 bit a gamma of 1.0. So do gamma adjustment for gamma 2.2
	//Avoid gamma corrrection for preview
	if (gammaCorrect)
		val = pow(val, 1/2.2);
	return FloatToByte(val);
}

//Three different 16bit functions (8->16, 16->16, 32->16 all photoshop specific)
inline unsigned16 ConvertTo16Bit(unsigned8 val)
{
	return F32toF16(val/255.f);
}
inline unsigned16 ConvertTo16Bit(unsigned16 val)
{
	//Photoshop uses an internal representation of 16 bit data of 0 - 32768
	double max_16bitPhotoshop = 32768;
	
	return F32toF16(static_cast<float>(val/max_16bitPhotoshop));
}
inline unsigned16 ConvertTo16Bit(float val)
{
	//Photoshop uses for 32 bit a gamma of 1.0. So do gamma adjustment for gamma 2.2.
	//Avoid gamma corrrection for preview
	//if (gammaCorrect)
	//		val = pow(val, 1/2.2);

	return  F32toF16(val);
}

enum TextureTypeEnum
{
	COLOR, 
	COLOR_ALPHA, 
	CUBEMAP_LAYERS, 
	CUBEMAP_CROSSED, 
	NORMALMAP, 
	TEXTURE_TYPE_COUNT
};

enum MipmapEnum
{
	NONE, 
	AUTOGEN, 
	FROM_LAYERS
};

enum LoadInfoEnum
{
    USE_NONE = 0x0,
	USE_MIPMAPS = 0x1,
	USE_SEPARATEALPHA = 0x2
};

enum CompressionTypeEnum
{
	BC1, 
	BC1_SRGB, 
	BC3, 
	BC3_SRGB, 
	BC6H_FAST, 
	BC6H_FINE, 
	BC7_FAST, 
	BC7_FINE, 
	BC7_SRGB_FAST, 
	BC7_SRGB_FINE, 
	BC4, 
	BC5, 
	UNCOMPRESSED, 
	COMPRESSION_TYPE_COUNT
};

class IntelPlugin
{
public:

	// This is our structure that we use to pass globals between routines:
	struct Globals
	{ 
		Boolean					queryForParameters;
		Boolean					sameNames;

		PSBufferSuite2			*sPSBufferSuite64;
		PSColorSpaceSuite2      *sPSColorSpaceSuite64;
		bool                     gMultithreaded;
		DXGI_FORMAT              encoding_g;        //Which encoding to use
		bool                     fast_bc67;         //Use fast encoding, else use fine, only valid for BC6 and BC7
		TextureTypeEnum	      	 TextureTypeIndex;  //Col,Col+alpha,CubeFrmLayera,CubefromCross,NM
		MipmapEnum               MipMapTypeIndex;   //None,Autogen,FromLayers
		uint32                   MipLevel;			// only valid if SetMipLevel == true, specified the mip level to be exported when in cube map mode.
		bool                     SetMipLevel;       //Specify that a specific mip level has to be exported. Only valid for cube map mode.

		float					 exposure;	// multiplier for HDR viewing (and downsampling)

		// normal map processing
		bool                     Normalize;         //Specify if normalization of values is needed. Only valid for Normal Maps
		bool                     FlipX;
		bool                     FlipY;

		//Name of used preset for Descriptor parameter and batching
		Str255                  presetBatchName;    
		bool                    mipmapBatchAllowed; 
		bool                    alphaBatchSeperate;    

		// quick way to tell if we are currently showing preview UI
		bool					previewing;
	};


private:
	struct
	{
		FormatRecordPtr formatRecord;
		SPPlugin* pluginRef;
		int16* resultPtr;
		intptr_t* dataPtr;
		Globals* data;
	} ps;

	
	struct
	{
		// state at which previews were generated
		TextureTypeEnum			textureType;
		MipmapEnum              mipMap;

		bool                    flipRChannel;
		bool                    flipGChannel;

		DXGI_FORMAT				encoding;
		uint32                  mipLevel;		
		bool                    fastBc67;

		DirectX::ScratchImage	*compressedImage, *uncompressedImage;
		int						compressedSize;
		int						width, height;
	} preview;


	struct 
	{
	    DirectX::ScratchImage *readImagePtr;
	    int loadMipMapIndex;
	    bool isCubeMap;
	    bool hasMips;
		bool hasAlpha;
	} loadInfo;

public:
	FormatRecordPtr GetFormatRecord() const { return ps.formatRecord; }
	Globals* GetData() const { return ps.data; }
	int16 GetResult() const { return ps.resultPtr ? *ps.resultPtr : 0; }
	void SetResult(int e) { if (ps.resultPtr) *ps.resultPtr = static_cast<int16>(e); }

	//Decide which combination are valid, based in the CompressionVsTextureTypeMatrix table
	static bool IsCombinationValid(TextureTypeEnum textype, CompressionTypeEnum comptype);
	
	//Preview functions. AdvanceState () has to be called before entering this function so that the globals->exportParamBlock->data buffer is full;
	DirectX::ScratchImage* GetCompressedImageForPreview(int planesToGet_, int &compressedSize);
	DirectX::ScratchImage* GetUncompressedImageForPreview(int planesToGet_);
	
	//Copy data from photoshop buffer into scrUncompressedImageScratch_
	bool CopyDataForEncoding(DirectX::ScratchImage *scrUncompressedImageScratch_, bool hasAlpha_, bool DoMipMaps_, bool gammaCorrect);
	
	//Take an Uncompressed DirectX::ScratchImage scrUncompressedImageScratch_ and Compresses it into scrImageScratch_
	bool CompressToScratchImage(DirectX::ScratchImage **scrImageScratch_, DirectX::ScratchImage **scrUncompressedImageScratch_, bool hasAlpha_);

	//Conversion functions from Photoshop buffer to 8bit/16bit ready encoding buffer
	bool ConvertToBC6From8Bit(unsigned16 *tgtDataPtr, int planesToGet, bool hasAlphaChannel);
	bool ConvertToBC6From16Bit(unsigned16 *tgtDataPtr, int planesToGet, bool hasAlphaChannel);
	bool ConvertToBC6From32Bit(unsigned16 *tgtDataPtr, int planesToGet, bool hasAlphaChannel);

	bool ConvertToBC4or5From32Bit(unsigned8 *tgtDataPtr, int planesToGet, bool hasAlphaChannel, bool gammaCorrect);
	bool ConvertToBC4or5From16Bit(unsigned8 *tgtDataPtr, int planesToGet, bool hasAlphaChannel);
	bool ConvertToBC4or5From8Bit(unsigned8 *tgtDataPtr, int planesToGet, bool hasAlphaChannel);

	bool ConvertToBCFrom32Bit(unsigned8 *tgtDataPtr, int planesToGet, bool hasAlphaChannel, bool gammaCorrect);
	bool ConvertToBCFrom16Bit(unsigned8 *tgtDataPtr, int planesToGet, bool hasAlphaChannel);
	bool ConvertToBCFrom8Bit(unsigned8 *tgtDataPtr, int planesToGet, bool hasAlphaChannel);

	enum PREVIEW_OPTIONS
	{
		PREVIEW_SOURCE_ORIGINAL = 0x0,
		PREVIEW_SOURCE_COMPRESSED = 0x1,
		PREVIEW_SOURCE_MASK = 0x3,

		PREVIEW_CHANNEL_RED = 0x4,
		PREVIEW_CHANNEL_GREEN = 0x8,
		PREVIEW_CHANNEL_BLUE = 0x10,
		PREVIEW_CHANNEL_ALPHA = 0x20,
		PREVIEW_CHANNEL_RGB = PREVIEW_CHANNEL_RED | PREVIEW_CHANNEL_GREEN | PREVIEW_CHANNEL_BLUE,
		PREVIEW_CHANNEL_RGBA = PREVIEW_CHANNEL_RED | PREVIEW_CHANNEL_GREEN | PREVIEW_CHANNEL_BLUE | PREVIEW_CHANNEL_ALPHA,
		PREVIEW_CHANNEL_MASK = PREVIEW_CHANNEL_RGBA,
	};

		// returns byte size of entire image
	void FetchPreviewRGB(unsigned8* dst, int width, int height, int x, int y, double zoom, int previewOptions = PREVIEW_SOURCE_ORIGINAL | PREVIEW_CHANNEL_RGB, int matteColor = 0);
	POINT GetPreviewDimensions();
	int	 GetCompressedByteSize();

	int  GetOriginalBitsPerPixel();
	int  GetOriginalByteSize();
	int GetLayerCount();

	//Copy from layer channel to Image
	void CopyFromLayerChannelIntoImage(char *pLayerData, const DirectX::Image *image, int indexToImage, int indexToLayerChannel);
	
	//Copy document Layers into mipmap Images of ScratchImage
	void CopyLayersIntoMipMaps(DirectX::ScratchImage *scrUncompressedImageScratch_, bool hasAlpha_, int startMipIndex, int endMipIndex=INT_MAX);

	//Compress rgba_surface into tgtPixels, uisng Intel ISPC encoders
	bool ISPC_compression(rgba_surface &input_rgba, const DirectX::Image& target, bool hasAlpha_rgba);

	//Pad the surface size to boundaries of 4
	rgba_surface DoPaddingToMultiplesOf4(const rgba_surface &input);

	//Return true if texture type cube maps and the setMipLevel is checked
	bool IsCubeMapWithSetMipLevelOverride();
	
	//Returns true if mip maps are defined by layers
	bool IsMipMapsDefinedByLayer();

	//Convert scrUncompressedImageScratch_ from a  crossed layout image to a cube map DirectX::ScratchImage
	void ConvertToCubeMapFromCross(DirectX::ScratchImage **scrUncompressedImageScratch_);

	//Convert scrUncompressedImageScratch_ from document layers to a cube map DirectX::ScratchImage
	bool ConvertToCubeMapFromLayers(DirectX::ScratchImage **scrUncompressedImageScratch_, bool hasAlpha_);

	//Convert scrUncompressedImageScratch_ from a cube map to a horizontal crossed layout image 
	void ConvertToHorizontalCrossFromCubeMap(DirectX::ScratchImage **scrUncompressedImageScratch_);

	//Flip the X(Red) channel and or the Y(Green) channel of thei normal map
	void FlipXYChannelNormalMap(DirectX::ScratchImage *scrUncompressedImageScratch_);

	//Normalize all values of this Nomral map in main image and all mip maps
	void NormalizeNormalMapChain(DirectX::ScratchImage *scrUncompressedImageScratch_);

	//Saves a special mip version of acube map out as a normal cube map. Special function to save out low res cubemaps
	HRESULT SaveCubeMipLevelToDDSFile(DirectX::ScratchImage* scrImageScratch_, DirectX::Blob& blob);

	//Copy from layer Channel into a buffer
	void ReadLayerData(ReadChannelDesc *pChannel, char *pLayerData, int width, int height);
	void FillLayerDataToWhite(char *pLayerData, int width, int height);

	void FillFromCompositedLayers();

	//Does the image loaded have alpha, ist it specified to have alpha
	bool HasAlpha() { return (ps.formatRecord->planes > 3) && (ps.data->TextureTypeIndex==TextureTypeEnum::COLOR_ALPHA);}
	
	//Show hide wait cursor
	void showLoadingCursor();
    void showNormalCursor();

private:
	void InitData();
	void DoWritePrepare();
	void DoWriteStart();
	void DoWriteDDS();
	void DoReadPrepare();
	void DoReadStart();
	void DoReadContinue();
	void DoReadFinish();
	void DoReadLayerStart();

	// scripting support
	bool ReadScriptParamsForWrite();
	OSErr WriteScriptParamsForWrite();
	bool ReadScriptParamsForRead();
	OSErr WriteScriptParamsForRead();

	void DoFilterFile();

	void CreateDataHandle();
	void LockHandles();
	void UnlockHandles();

	IntelPlugin(void);
	~IntelPlugin(void);

public:

	void DoWriteFinish();
	void DoWriteContinue();
	void PluginMain(const int16 selector, FormatRecordPtr formatParamBlock, intptr_t* data, int16* result);
	void UserError(const char* usrerror);
	void FetchImageData();
	void DisposeImageData();

	bool SetProgress(int part, int total);	// true if can continue

	static IntelPlugin& GetInstance();
};

