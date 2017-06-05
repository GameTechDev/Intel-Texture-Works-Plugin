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
#include "IntelPlugin.h"
#include "IntelPluginUIWin.h"
#include "SaveOptionsDialog.h"

using namespace DirectX;

	// this global pointer is necessary to use code from PIUSuites.cpp
SPBasicSuite * sSPBasic = NULL;


IntelPlugin& IntelPlugin::GetInstance()
{
	static IntelPlugin singleton;
	return singleton;
}

IntelPlugin::IntelPlugin(void)
{
	memset(&ps, 0, sizeof(ps));
}


IntelPlugin::~IntelPlugin(void)
{
}


// ===========================================================================
bool IntelPlugin::IsCombinationValid(TextureTypeEnum textype, CompressionTypeEnum comptype)
{
	if (textype < TextureTypeEnum::TEXTURE_TYPE_COUNT && comptype < CompressionTypeEnum::COMPRESSION_TYPE_COUNT)
	{
		// ---------------------------------------------------------------------------
		//Matrix of which compression options make sense depending on the selected texture type
		//Rows are the TextureTypeEnum, and Columns the Compression types
		bool CompressionVsTextureTypeMatrix[][CompressionTypeEnum::COMPRESSION_TYPE_COUNT] =
		{ 
		//   BC1,    BC1_SRGB,   BC3,    BC3_SRGB,   BC6H_FAST    BC6H_FINE   BC7_FAST   BC7_FINE   BC7_SRGB_FAST   BC7_SRGB_FINE   BC4     BC5     NONE
			{true,   true,       false,  false,      true,        true,       true,      true,      true,           true,           true,   false,  true}, //COLOR
			{false,  false,      true,   true,       false,       false,      true,      true,      true,           true,           false,  false,  true}, //COLOR+A
			{true,   true,       true,   true,       true,        true,       true,      true,      true,           true,           false,  false,  true}, //CUBEMAP+LAYER
			{true,   true,       true,   true,       true,        true,       true,      true,      true,           true,           false,  false,  true}, //CUBEMAP+CROSS
			{false,   false,     false,  false,      false,       false,      false,     false,     false,          false,          false,  true,   true}, //NORMAL MAP
		};

		return CompressionVsTextureTypeMatrix[textype][comptype];
	}

	return false;
}

/*****************************************************************************/
/*****************************************************************************/
//Cursor WAIT->ARROW convenince functions
void IntelPlugin::showLoadingCursor()
{
	::SetCursor( LoadCursor( 0, IDC_WAIT ) );
}

void IntelPlugin::showNormalCursor()
{
	//Forces a WM_SETCURSOR mes­sage.
    POINT pt; // Screen coordinates!
    ::GetCursorPos(&pt);
    ::SetCursorPos( pt.x, pt.y );
}

/*****************************************************************************/
/*****************************************************************************/
//Copy data from photoshop buffer into scrUncompressedImageScratch_
bool IntelPlugin::CopyDataForEncoding(ScratchImage *scrUncompressedImageScratch_, bool hasAlpha_, bool DoMipMaps_, bool gammaCorrect)
{
	//Do advanceState to get image data, fill the ps.formatRecord->data buffer
	FetchImageData();

	int planesToGet_ = ps.formatRecord->hiPlane - ps.formatRecord->loPlane + 1;

	if (ps.data->encoding_g == DXGI_FORMAT_BC6H_UF16)
	{
		//Allocate space for one rgba 16bit float image 
		scrUncompressedImageScratch_->Initialize2D(DXGI_FORMAT_R16G16B16A16_FLOAT, ps.formatRecord->imageSize.h, ps.formatRecord->imageSize.v, 1, 1, DDS_FLAGS_NONE);

		//Get pointer to first (and only) image, cast to 16bit for BC6
		unsigned16 *rowBigDataPtr = reinterpret_cast<unsigned16 *>(scrUncompressedImageScratch_->GetImages()->pixels);

		//Copy data from photoshop buffer into scrUncompressedImageScratch
		//Convert pixels to 16Bit for BC6 encoding
		if (ps.formatRecord->depth == 8)
		{
			ConvertToBC6From8Bit(rowBigDataPtr, planesToGet_, hasAlpha_);
		}
		else if (ps.formatRecord->depth == 16)
		{
			ConvertToBC6From16Bit(rowBigDataPtr, planesToGet_, hasAlpha_);
		}
		else if (ps.formatRecord->depth == 32)
		{
			ConvertToBC6From32Bit(rowBigDataPtr, planesToGet_, hasAlpha_);
		}
		else
		{		
			return false;
		}
	}
	else if (ps.data->encoding_g == DXGI_FORMAT_BC4_UNORM || ps.data->encoding_g == DXGI_FORMAT_BC5_UNORM)
	{
		//Allocate space for one rgba 8bit image 
		scrUncompressedImageScratch_->Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, ps.formatRecord->imageSize.h, ps.formatRecord->imageSize.v, 1, 1, DDS_FLAGS_NONE);
		
		//Get pointer to first (and only) image
		unsigned8 *rowBigDataPtr = scrUncompressedImageScratch_->GetImages()->pixels;

		//Copy data from photoshop buffer into scrUncompressedImageScratch
		//Convert pixels to 8bit for BC4/5 encoding
		if (ps.formatRecord->depth == 8)
		{
			ConvertToBC4or5From8Bit(rowBigDataPtr, planesToGet_, hasAlpha_);
		}
		else if (ps.formatRecord->depth == 16)
		{
			ConvertToBC4or5From16Bit(rowBigDataPtr, planesToGet_, hasAlpha_);
		}
		else if (ps.formatRecord->depth == 32)
		{
			ConvertToBC4or5From32Bit(rowBigDataPtr, planesToGet_, hasAlpha_, gammaCorrect);
		}
		else
		{		
			return false;
		}
	}
	else //BC1,3,7 or uncompressed
	{
		//Allocate space for one rgba 8bit image 
		scrUncompressedImageScratch_->Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, ps.formatRecord->imageSize.h, ps.formatRecord->imageSize.v, 1, 1, DDS_FLAGS_NONE);

		//If SRGB encoding, force Scratch image to SRGB format. So that the MipMap generation is done correctly for RGB images
		if (IsSRGB(ps.data->encoding_g) && DoMipMaps_)
		    scrUncompressedImageScratch_->OverrideFormat(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);

		//Get pointer to first (and only) image
		unsigned8 *rowBigDataPtr = scrUncompressedImageScratch_->GetImages()->pixels;
		
		//Copy data from photoshop buffer into scrUncompressedImageScratch
		//Convert pixels to 8bit for BC encoding
		if (ps.formatRecord->depth == 8)
		{
			ConvertToBCFrom8Bit(rowBigDataPtr, planesToGet_, hasAlpha_);
		}
		else if (ps.formatRecord->depth == 16)
		{
			ConvertToBCFrom16Bit(rowBigDataPtr, planesToGet_, hasAlpha_);
		}
		else if (ps.formatRecord->depth == 32)
		{
			ConvertToBCFrom32Bit(rowBigDataPtr, planesToGet_, hasAlpha_, gammaCorrect);
		}
		else
		{		
			return false;
		}
	}
	
	return true;
}


/*****************************************************************************/
/*****************************************************************************/
//Take an Uncompressed ScratchImage scrUncompressedImageScratch_ and Compresses it into scrImageScratch_
bool IntelPlugin::CompressToScratchImage(ScratchImage **scrImageScratch_, ScratchImage **scrUncompressedImageScratch_, bool hasAlpha_)
{
	//============================================================================================
	//============================================================================================
	//Compress image, section
	//ISPC is used for BC1,3,6,7. BC4,5 is done using DirectXTex Lib in one of the previous sections
	if (ps.data->encoding_g != DXGI_FORMAT_BC4_UNORM && ps.data->encoding_g != DXGI_FORMAT_BC5_UNORM && ps.data->encoding_g !=DXGI_FORMAT_R8G8B8A8_UNORM)
	{
		//How many mip levels are generated? If no mip map override this is 1 so that only one image is encoded
		size_t mipLevels = (*scrUncompressedImageScratch_)->GetMetadata().mipLevels;
	
		//Allocate space for Image + mip chain, according to what the uncompressed image size/miplevel
		//Check if cubemap or not (a cubemap has an arraysize of six)
		if ((*scrUncompressedImageScratch_)->GetMetadata().arraySize == 6)
		{
			//Image is a CubeMap
			(*scrImageScratch_)->InitializeCube(ps.data->encoding_g, (*scrUncompressedImageScratch_)->GetMetadata().width, 
											   (*scrUncompressedImageScratch_)->GetMetadata().height, 1, mipLevels);
		}
		else
		{
			//Image is a 2D texture
			(*scrImageScratch_)->Initialize2D(ps.data->encoding_g, (*scrUncompressedImageScratch_)->GetMetadata().width, 
											 (*scrUncompressedImageScratch_)->GetMetadata().height, 1, mipLevels, DDS_FLAGS_NONE);
		}
	
		//Get pointer to image chains for compressed and uncompressed scratchImage
		const Image* imgCompressed = (*scrImageScratch_)->GetImages();
		const Image* imgUnCompressedMipMap = (*scrUncompressedImageScratch_)->GetImages();
	
		//Number of total images (Image1+MipChain), (Image2+MipChain), ..... (Multiple Images image array only in case of Cube Maps)
		size_t nimgCompressed = (*scrImageScratch_)->GetImageCount();
		if (nimgCompressed == 0)
		{
			UserError("Can not allocate compressed image");
			return false;
		}
		
		//StopWatch stopWatch;
		//stopWatch.Reset();
		//stopWatch.Start();
		//ISPC is used on for BC1,3,6,7.
		//Iterate over the whole image chain (Image+MipChain) 
		for (size_t allImagesIndex = 0; allImagesIndex < nimgCompressed; allImagesIndex++)
		{
			//Fill in the struct needed by Intel ispc code with the uncompressed  image
			rgba_surface input;
			 
			//Get pointer to this image in the chain
			const Image* rgbaimg = &imgUnCompressedMipMap[allImagesIndex];
			
			input.height = int32_t(rgbaimg->height);
			input.width = int32_t(rgbaimg->width);
			input.ptr =  rgbaimg->pixels; //buffer in unsigned8* format
			input.stride = int32_t(rgbaimg->rowPitch); //number of bytes of a row

			//Determine if the image size is not multiples of 4.
			//In that case it needs padding when encoding
			bool DoPadding = ((input.height | input.width) & 0x3) != 0;

			//If it needs padding, process it
			if (DoPadding)
				input = DoPaddingToMultiplesOf4(input);

			//Call in ISPC compression functions, output compressed image into imgCompressed[allImagesIndex]
			ISPC_compression(input, imgCompressed[allImagesIndex], hasAlpha_);

			//If padding free buffer that got allocated
			if (DoPadding)
				delete [] input.ptr;
		}
		//stopWatch.Stop();
		//std::stringstream ss;
		//ss << stopWatch.TimeInMilliseconds();
		//errorMessage(ss.str(),"Time"); 
	}
	else if (ps.data->encoding_g == DXGI_FORMAT_R8G8B8A8_UNORM)
	{
		//Uncompressed do nothing, just swap pointers and free space
		delete *scrImageScratch_;
		*scrImageScratch_ = *scrUncompressedImageScratch_;
	    *scrUncompressedImageScratch_ = NULL;
	}
	else
	{
		//Compress into srcImageScratch using the DirectXTex BC4,5 routines
		HRESULT hr = Compress((*scrUncompressedImageScratch_)->GetImages(), (*scrUncompressedImageScratch_)->GetImageCount(), (*scrUncompressedImageScratch_)->GetMetadata(), 
			                  ps.data->encoding_g, TEX_COMPRESS_DEFAULT, 0.5f, **scrImageScratch_);
	    if (hr != S_OK)
	    {
			if (ps.data->encoding_g == DXGI_FORMAT_BC4_UNORM)
			    UserError("Could not compress to BC4");
			else
				UserError("Could not compress to BC5");
			
			return false;
	    }
	}
		
	return true;
}

/*****************************************************************************/
/*****************************************************************************/
//Conversion functions from Photoshop buffer to 8bit/16bit ready encoding buffer
bool IntelPlugin::ConvertToBC6From8Bit(unsigned16 *tgtDataPtr, int planesToGet, bool hasAlphaChannel)
{
	if (ps.formatRecord->depth != 8)
		return false;

	//Get image pointer
	unsigned8 *rowData = static_cast<unsigned8 *>(ps.formatRecord->data);
	
	//Copy image into RGBA buffer, alpha is not copied for now
	for (int height=0; height< ps.formatRecord->imageSize.v; height++)
	for (int width=0; width< ps.formatRecord->imageSize.h; width++)
	{
		int index = height*ps.formatRecord->imageSize.h*planesToGet + width*planesToGet;

		//We assign black to other channels if photoshop does not provide enough channels, as the compressed image must be RGBA
	    tgtDataPtr[0] = ConvertTo16Bit(rowData[index]);
	    tgtDataPtr[1] = (planesToGet > 1)? ConvertTo16Bit(rowData[index+1]) : 0;
	    tgtDataPtr[2] = (planesToGet > 2)? ConvertTo16Bit(rowData[index+2]) : 0;
		//BC6 does not have alpha, but we copy it for preview. It will get discarded on compression
	    tgtDataPtr[3] = hasAlphaChannel? ConvertTo16Bit(rowData[index+3]) : F32toF16(1.f);  
		tgtDataPtr +=4;
	}

	return true;
}

bool IntelPlugin::ConvertToBC6From16Bit(unsigned16 *tgtDataPtr, int planesToGet, bool hasAlphaChannel)
{
	if (ps.formatRecord->depth != 16)
		return false;
	
	unsigned16 *rowData16bit = static_cast<unsigned16 *>(ps.formatRecord->data);
	
	//Copy image into RGBA buffer, alpha is not copied for now
	for (int height=0; height< ps.formatRecord->imageSize.v; height++)
	for (int width=0; width< ps.formatRecord->imageSize.h; width++)
	{
		int index = height*ps.formatRecord->imageSize.h*planesToGet + width*planesToGet;
		
		//We assign black to other channels if photoshop does not provide enough channels, as the compressed text must be RGBA
		tgtDataPtr[0] = ConvertTo16Bit(rowData16bit[index]);
	    tgtDataPtr[1] = (planesToGet > 1)? ConvertTo16Bit(rowData16bit[index+1]) : 0;
		tgtDataPtr[2] = (planesToGet > 2)? ConvertTo16Bit(rowData16bit[index+2]) : 0;
		//BC6 does not have alpha, but we copy it for preview. It will get discarded on compression
		tgtDataPtr[3] = hasAlphaChannel? ConvertTo16Bit(rowData16bit[index+3]) : F32toF16(1.f);  

		tgtDataPtr +=4;
	}

	return true;
}

bool IntelPlugin::ConvertToBC6From32Bit(unsigned16 *tgtDataPtr, int planesToGet, bool hasAlphaChannel)
{	
	if (ps.formatRecord->depth != 32)
		return false;

	float *rowData32bit = static_cast<float *>(ps.formatRecord->data);
	
	//Copy image into RGBA buffer, alpha is not copied for now
	for (int height=0; height< ps.formatRecord->imageSize.v; height++)
	for (int width=0; width< ps.formatRecord->imageSize.h; width++)
	{
		int index = height*ps.formatRecord->imageSize.h*planesToGet + width*planesToGet;
		
		//We assign black to other channels if photoshop does not provide enough channels, as the compressed text must be RGBA
		tgtDataPtr[0] = ConvertTo16Bit(rowData32bit[index]);
		tgtDataPtr[1] = (planesToGet > 1)? ConvertTo16Bit(rowData32bit[index+1]) : 0;
		tgtDataPtr[2] = (planesToGet > 2)? ConvertTo16Bit(rowData32bit[index+2]) : 0;
		//BC6 does not have alpha, but we copy it for preview. It will get discarded on compression
		tgtDataPtr[3] = hasAlphaChannel? ConvertTo16Bit(rowData32bit[index+2]) : F32toF16(1.f); 
		tgtDataPtr +=4;
	}

	return true;
}

bool IntelPlugin::ConvertToBC4or5From32Bit(unsigned8 *tgtDataPtr, int planesToGet, bool hasAlphaChannel, bool gammaCorrect)
{	
	if (ps.formatRecord->depth != 32)
		return false;

	float *rowData32bit = static_cast<float *>(ps.formatRecord->data);
	
	for (int height=0; height< ps.formatRecord->imageSize.v; height++)
	for (int width=0; width< ps.formatRecord->imageSize.h; width++)
	{
		int index = height*ps.formatRecord->imageSize.h*planesToGet + width*planesToGet;//height*ps.formatRecord->imageSize.h*ps.formatRecord->planes + width*ps.formatRecord->planes;

		tgtDataPtr[0] = ConvertTo8Bit(rowData32bit[index], gammaCorrect);
		tgtDataPtr[1] = (planesToGet>1)? ConvertTo8Bit(rowData32bit[index+1], gammaCorrect) : tgtDataPtr[0]; //if BC4 all channels get the R plane, if BC5 we take the RG planes
		tgtDataPtr[2] = (planesToGet>2)? ConvertTo8Bit(rowData32bit[index+2], gammaCorrect) : tgtDataPtr[0]; //Copy the B channel for preview, it will get discarded on compression
		tgtDataPtr[3] = hasAlphaChannel? ConvertTo8Bit(rowData32bit[index+3], gammaCorrect) : 255; //Copy the A channel for preview, it will get discarded on compression
		tgtDataPtr +=4;
	}

	return true;
}

bool IntelPlugin::ConvertToBC4or5From16Bit(unsigned8 *tgtDataPtr, int planesToGet, bool hasAlphaChannel)
{
	if (ps.formatRecord->depth != 16)
		return false;
	
	unsigned16 *rowData16bit = static_cast<unsigned16 *>(ps.formatRecord->data);

	for (int height=0; height< ps.formatRecord->imageSize.v; height++)
	for (int width=0; width< ps.formatRecord->imageSize.h; width++)
	{
		int index = height*ps.formatRecord->imageSize.h*planesToGet + width*planesToGet;//height*ps.formatRecord->imageSize.h*ps.formatRecord->planes + width*ps.formatRecord->planes;

		tgtDataPtr[0] = ConvertTo8Bit(rowData16bit[index]);
		tgtDataPtr[1] = (planesToGet>1)? ConvertTo8Bit(rowData16bit[index+1]) : tgtDataPtr[0]; //if BC4 all channels get the R plane, if BC5 we take the RG planes
		tgtDataPtr[2] = (planesToGet>2)? ConvertTo8Bit(rowData16bit[index+2]) : tgtDataPtr[0]; //Copy the B channel for preview, it will get discarded on compression
		tgtDataPtr[3] = hasAlphaChannel? ConvertTo8Bit(rowData16bit[index+3]) : 255; //Copy the A channel for preview, it will get discarded on compression
		tgtDataPtr +=4;
	}

	return true;
}

bool IntelPlugin::ConvertToBC4or5From8Bit(unsigned8 *tgtDataPtr, int planesToGet, bool hasAlphaChannel)
{
	if (ps.formatRecord->depth != 8)
		return false;

	//Get image pointer
	unsigned8 *rowData = static_cast<unsigned8 *>(ps.formatRecord->data);
	
	//Copy image into RGBA buffer, alpha is not copied 
	for (int height=0; height< ps.formatRecord->imageSize.v; height++)
	for (int width=0; width< ps.formatRecord->imageSize.h; width++)
	{
		int index = height*ps.formatRecord->imageSize.h*planesToGet + width*planesToGet;
		tgtDataPtr[0] = ConvertTo8Bit(rowData[index]);
		tgtDataPtr[1] = (planesToGet>1)? ConvertTo8Bit(rowData[index+1]) : tgtDataPtr[0]; //if BC4 all channels get the R plane, if BC5 we take the RG planes
		tgtDataPtr[2] = (planesToGet>2)? ConvertTo8Bit(rowData[index+2]) : tgtDataPtr[0]; //Copy the B channel for preview, it will get discarded on compression
		tgtDataPtr[3] = hasAlphaChannel? ConvertTo8Bit(rowData[index+3]) : 255; //Copy the A channel for preview, it will get discarded on compression
		tgtDataPtr +=4;
	}

	return true;
}

POINT IntelPlugin::GetPreviewDimensions()
{
	POINT ret = {ps.formatRecord->imageSize.h, ps.formatRecord->imageSize.v};
	if (ps.data->TextureTypeIndex==TextureTypeEnum::CUBEMAP_LAYERS)
	{
		ret.x *= 4;
		ret.y *= 3;
	}
	return ret;
}

int IntelPlugin::GetCompressedByteSize()
{
	return preview.compressedSize;
}

int IntelPlugin::GetOriginalBitsPerPixel()
{
	int planes = ps.formatRecord->planes;
	if (planes > 3 && ps.data->TextureTypeIndex!=TextureTypeEnum::COLOR_ALPHA)	// we have enough planes for alpha but are not exporting it right now
		planes--;

	return ps.formatRecord->depth * planes;
}

int IntelPlugin::GetOriginalByteSize()
{
	int byteSize =  ps.formatRecord->imageSize.h * ps.formatRecord->imageSize.v * ((GetOriginalBitsPerPixel() + 7) >> 3);

	switch (ps.data->TextureTypeIndex)
	{
	case TextureTypeEnum::CUBEMAP_LAYERS:
		byteSize *= 6;
		break;
	case TextureTypeEnum::CUBEMAP_CROSSED:	// a cross view only uses half the image resolution
		byteSize = byteSize >> 1;	
		break;
	}

	return byteSize;
}

int IntelPlugin::GetLayerCount()
{
	int layerCount = 0;
	for (auto layer = ps.formatRecord->documentInfo->layersDescriptor; layer; layer = layer->next)
		layerCount++;

	return layerCount;
}

// get RGB buffer
void IntelPlugin::FetchPreviewRGB(unsigned8 *dst, int width, int height, int xo, int yo, double zoom, int previewOptions, int matteColor)
{
	FetchImageData();

	if (!(previewOptions & PREVIEW_CHANNEL_MASK))
		previewOptions |= PREVIEW_CHANNEL_RGB;

	// default use original source
	int planes = ps.formatRecord->hiPlane - ps.formatRecord->loPlane + 1;
	int srcWidth = ps.formatRecord->theRect.right - ps.formatRecord->theRect.left;
	int srcHeight = ps.formatRecord->theRect.bottom - ps.formatRecord->theRect.top;
	int depthOrFormat = ps.formatRecord->depth;
	int rowPitch = ps.formatRecord->rowBytes; 
	const uint8 * pixelData = static_cast<const uint8 *>(ps.formatRecord->data);
	auto exposure = ps.data->exposure;

	int mipLevel = (ps.data->SetMipLevel ? ps.data->MipLevel : 0);
	zoom = zoom / (1 << mipLevel);

	//Note
	//The Get(Un)CompressedImageForPreview() functions and the preview scratchimage are only used if the image to be fetched is compressed
	//or its a cube map or has mip layers. Otherwise the above pixelData is used as is.

	// flush invalid previews based on parameter changes...
	if (preview.textureType != ps.data->TextureTypeIndex || preview.mipMap != ps.data->MipMapTypeIndex || preview.mipLevel != mipLevel || 
		preview.flipRChannel != ps.data->FlipX || preview.flipGChannel != ps.data->FlipY)
	{
		if (preview.uncompressedImage)
		{
			delete preview.uncompressedImage;
			preview.uncompressedImage = NULL;
		}

		if (preview.compressedImage)
		{
			delete preview.compressedImage;
			preview.compressedImage = NULL;
			preview.compressedSize = 0;
		}

		preview.textureType = ps.data->TextureTypeIndex;
		preview.mipMap = ps.data->MipMapTypeIndex;
		preview.mipLevel = mipLevel;
		preview.flipRChannel = ps.data->FlipX;
		preview.flipGChannel = ps.data->FlipY;
	}

	if (preview.encoding != ps.data->encoding_g || preview.fastBc67 != ps.data->fast_bc67)
	{
		if (preview.compressedImage)
		{
			delete preview.compressedImage;
			preview.compressedImage = NULL;
		}

		preview.encoding = ps.data->encoding_g;
		preview.fastBc67 = ps.data->fast_bc67;
	}

	if ((previewOptions & PREVIEW_SOURCE_MASK) == PREVIEW_SOURCE_COMPRESSED)
	{
		if (!preview.compressedImage)
			preview.compressedImage = GetCompressedImageForPreview(planes, preview.compressedSize);

		if (!preview.compressedImage)	// Compressed can return NULL, ie no compression, so set it to uncompressed version
			preview.compressedImage = GetUncompressedImageForPreview(planes); 

		if (preview.compressedImage)	// shouldn't be null
		{
			auto image = preview.compressedImage->GetImages();
			planes = 4;
			srcWidth = int(image->width);
			srcHeight = int(image->height);
			depthOrFormat = image->format;
			rowPitch = int(image->rowPitch);
			pixelData = image->pixels;
		}
	}
	else if (ps.data->MipMapTypeIndex == MipmapEnum::FROM_LAYERS || ps.data->TextureTypeIndex==TextureTypeEnum::CUBEMAP_CROSSED || ps.data->TextureTypeIndex==TextureTypeEnum::CUBEMAP_LAYERS)
	{
		if (!preview.uncompressedImage)
			preview.uncompressedImage = GetUncompressedImageForPreview(planes);

		if (preview.uncompressedImage)	// shouldn't be null
		{
			auto image = preview.uncompressedImage->GetImages();
			planes = 4;
			srcWidth = int(image->width);
			srcHeight = int(image->height);
			depthOrFormat = image->format;
			rowPitch = int(image->rowPitch);
			pixelData = image->pixels;
		}
	}

	preview.width = srcWidth;
	preview.height = srcHeight;

	int channelIndex[4] = {-1,-1,-1,-1};

	switch (previewOptions & PREVIEW_CHANNEL_MASK)
	{
			// pure channels will return as grayscale
		case PREVIEW_CHANNEL_RED:
			channelIndex[0] = channelIndex[1] = channelIndex[2] = 0;
			break;
		case PREVIEW_CHANNEL_GREEN:
			channelIndex[0] = channelIndex[1] = channelIndex[2] = planes > 1 ? 1 : -1;
			break;
		case PREVIEW_CHANNEL_BLUE:
			channelIndex[0] = channelIndex[1] = channelIndex[2] = planes > 2 ? 2 : -1;
			break;
		case PREVIEW_CHANNEL_ALPHA:
			channelIndex[0] = channelIndex[1] = channelIndex[2] = planes > 3 ? 3 : -1;
			break;

		default:	// a mixture of channels
			if ((previewOptions & PREVIEW_CHANNEL_RED) && planes > 0)
				channelIndex[0] = 0;
			if ((previewOptions & PREVIEW_CHANNEL_GREEN) && planes > 1)
				channelIndex[1] = 1;
			if ((previewOptions & PREVIEW_CHANNEL_BLUE) && planes > 2)
				channelIndex[2] = 2;
			if ((previewOptions & PREVIEW_CHANNEL_ALPHA) && planes > 3)
				channelIndex[2] = 3;
			break;
	}

	if ((previewOptions & PREVIEW_CHANNEL_ALPHA) && planes > 3)
	{
		if ((previewOptions & PREVIEW_CHANNEL_MASK) == PREVIEW_CHANNEL_ALPHA)	// only alpha?
		{
			channelIndex[0] = channelIndex[1] = channelIndex[2] = 3;
			exposure = 1;
		}
		else
			channelIndex[3] = 3;
	}

	switch (depthOrFormat)
	{
	case DXGI_FORMAT_R32G32B32A32_FLOAT:
	case 32:
		for (int yt=0; yt < height; yt++)
		{
			int y = int((yt + yo) * zoom);

			for (int xt=0; xt< width; xt++)
			{
				auto pixelDst = dst + (width * yt + xt) * 3;

				int x = int((xt + xo) * zoom);
				if (y < 0 || y >= srcHeight || x < 0 || x >= srcWidth)	// out of bounds?
				{
					pixelDst[0] = matteColor & 0xFF;
					pixelDst[1] = (matteColor >> 8) & 0xFF;
					pixelDst[2] = (matteColor >> 16) & 0xFF;
				}
				else
				{
					auto pixelSrc = reinterpret_cast<const float *>(pixelData + rowPitch * y) + x * planes;

					pixelDst[0] = channelIndex[0] >= 0 ? ConvertTo8Bit(pixelSrc[channelIndex[0]] * exposure, false) : 0;
					pixelDst[1] = channelIndex[1] >= 0 ? ConvertTo8Bit(pixelSrc[channelIndex[1]] * exposure, false) : 0;
					pixelDst[2] = channelIndex[2] >= 0 ? ConvertTo8Bit(pixelSrc[channelIndex[2]] * exposure, false) : 0;
				}
			}
		} break;

	case DXGI_FORMAT_R16G16B16A16_FLOAT:	// DX 16 bit is a float type
		for (int yt=0; yt < height; yt++)
		{
			int y = int((yt + yo) * zoom);

			for (int xt=0; xt< width; xt++)
			{
				auto pixelDst = dst + (width * yt + xt) * 3;

				int x = int((xt + xo) * zoom);
				if (y < 0 || y >= srcHeight || x < 0 || x >= srcWidth)	// out of bounds?
				{
					pixelDst[0] = matteColor & 0xFF;
					pixelDst[1] = (matteColor >> 8) & 0xFF;
					pixelDst[2] = (matteColor >> 16) & 0xFF;
				}
				else
				{
					auto pixelSrc = reinterpret_cast<const short *>(pixelData + rowPitch * y) + x * planes;

					pixelDst[0] = channelIndex[0] >= 0 ? FloatToByte(F16toF32(pixelSrc[channelIndex[0]]) * exposure) : 0;
					pixelDst[1] = channelIndex[1] >= 0 ? FloatToByte(F16toF32(pixelSrc[channelIndex[1]]) * exposure) : 0;
					pixelDst[2] = channelIndex[2] >= 0 ? FloatToByte(F16toF32(pixelSrc[channelIndex[2]]) * exposure) : 0;
				}
			}
		} break;

	case 16:	// photoshop 16 bit is integer type
		for (int yt=0; yt < height; yt++)
		{
			int y = int((yt + yo) * zoom);

			for (int xt=0; xt< width; xt++)
			{
				auto pixelDst = dst + (width * yt + xt) * 3;

				int x = int((xt + xo) * zoom);
				if (y < 0 || y >= srcHeight || x < 0 || x >= srcWidth)	// out of bounds?
				{
					pixelDst[0] = matteColor & 0xFF;
					pixelDst[1] = (matteColor >> 8) & 0xFF;
					pixelDst[2] = (matteColor >> 16) & 0xFF;
				}
				else
				{
					auto pixelSrc = reinterpret_cast<const uint16 *>(pixelData + rowPitch * y) + x * planes;

					pixelDst[0] = channelIndex[0] >= 0 ? ConvertTo8Bit(pixelSrc[channelIndex[0]]) : 0;
					pixelDst[1] = channelIndex[1] >= 0 ? ConvertTo8Bit(pixelSrc[channelIndex[1]]) : 0;
					pixelDst[2] = channelIndex[2] >= 0 ? ConvertTo8Bit(pixelSrc[channelIndex[2]]) : 0;
				}
			}
		} break;


	case DXGI_FORMAT_R8G8B8A8_UNORM:
	case 8:
		for (int yt=0; yt < height; yt++)
		{
			int y = int((yt + yo) * zoom);

			for (int xt=0; xt< width; xt++)
			{
				auto pixelDst = dst + (width * yt + xt) * 3;

				int x = int((xt + xo) * zoom);
				if (y < 0 || y >= srcHeight || x < 0 || x >= srcWidth)	// out of bounds?
				{
					pixelDst[0] = matteColor & 0xFF;
					pixelDst[1] = (matteColor >> 8) & 0xFF;
					pixelDst[2] = (matteColor >> 16) & 0xFF;
				}
				else
				{
					auto pixelSrc = reinterpret_cast<const uint8 *>(pixelData + rowPitch * y) + x * planes;

					pixelDst[0] = channelIndex[0] >= 0 ? pixelSrc[channelIndex[0]] : 0;
					pixelDst[1] = channelIndex[1] >= 0 ? pixelSrc[channelIndex[1]] : 0;
					pixelDst[2] = channelIndex[2] >= 0 ? pixelSrc[channelIndex[2]] : 0;
				}
			}
		} break;
	}
}

bool IntelPlugin::ConvertToBCFrom32Bit(unsigned8 *tgtDataPtr, int planesToGet, bool hasAlphaChannel, bool gammaCorrect)
{	
	if (ps.formatRecord->depth != 32)
		return false;

	float *rowData32bit = static_cast<float *>(ps.formatRecord->data);
	
	for (int height=0; height< ps.formatRecord->imageSize.v; height++)
	for (int width=0; width< ps.formatRecord->imageSize.h; width++)
	{
		int index = height*ps.formatRecord->imageSize.h*planesToGet + width*planesToGet;//height*ps.formatRecord->imageSize.h*ps.formatRecord->planes + width*ps.formatRecord->planes;
		
		//We assign black to other channels if photoshop does not provide enough channels as the compressed text must be RGBA
		tgtDataPtr[0] = ConvertTo8Bit(rowData32bit[index], gammaCorrect);
		tgtDataPtr[1] = (planesToGet > 1)? ConvertTo8Bit(rowData32bit[index+1], gammaCorrect) : 0;
		tgtDataPtr[2] = (planesToGet > 2)? ConvertTo8Bit(rowData32bit[index+2], gammaCorrect) : 0;
		tgtDataPtr[3] = hasAlphaChannel? ConvertTo8Bit(rowData32bit[index+3], gammaCorrect) : 255;
		tgtDataPtr +=4;
	}

	return true;
}

bool IntelPlugin::ConvertToBCFrom16Bit(unsigned8 *tgtDataPtr, int planesToGet, bool hasAlphaChannel)
{
	if (ps.formatRecord->depth != 16)
		return false;
	
	unsigned16 *rowData16bit = static_cast<unsigned16 *>(ps.formatRecord->data);

	for (int height=0; height< ps.formatRecord->imageSize.v; height++)
	for (int width=0; width< ps.formatRecord->imageSize.h; width++)
	{
		int index = height*ps.formatRecord->imageSize.h*planesToGet + width*planesToGet;//height*ps.formatRecord->imageSize.h*ps.formatRecord->planes + width*ps.formatRecord->planes;
		
		//We assign black to other channels if photoshop does not provide enough channels as the compressed text must be RGBA
		tgtDataPtr[0] = ConvertTo8Bit(rowData16bit[index]);
		tgtDataPtr[1] = (planesToGet > 1)? ConvertTo8Bit(rowData16bit[index+1]) : 0;
		tgtDataPtr[2] = (planesToGet > 2)? ConvertTo8Bit(rowData16bit[index+2]) : 0;
		tgtDataPtr[3] = hasAlphaChannel? ConvertTo8Bit(rowData16bit[index+3]) : 255;
		tgtDataPtr +=4;
	}

	return true;
}

bool IntelPlugin::ConvertToBCFrom8Bit(unsigned8 *tgtDataPtr, int planesToGet, bool hasAlphaChannel)
{
	if (ps.formatRecord->depth != 8)
		return false;

	//Get image pointer
	unsigned8 *rowData = static_cast<unsigned8 *>(ps.formatRecord->data);
	
	//Copy data from photoshop buffer into local buffer
	for (int height=0; height< ps.formatRecord->imageSize.v; height++)
	for (int width=0; width< ps.formatRecord->imageSize.h; width++)
	{
		int index = height*ps.formatRecord->imageSize.h*planesToGet + width*planesToGet;//height*ps.formatRecord->imageSize.h*ps.formatRecord->planes + width*ps.formatRecord->planes;

		//We assign black to other channels is photoshop does not provide enought channels as the compressed text must be RGBA
		tgtDataPtr[0] = ConvertTo8Bit(rowData[index]);
		tgtDataPtr[1] = (planesToGet > 1)? ConvertTo8Bit(rowData[index+1]) : 0;
		tgtDataPtr[2] = (planesToGet > 2)? ConvertTo8Bit(rowData[index+2]) : 0;
		tgtDataPtr[3] = hasAlphaChannel? ConvertTo8Bit(rowData[index+3]) : 255;
		tgtDataPtr +=4;
	}

	return true;
}


/*****************************************************************************/
/*****************************************************************************/
//Compress rgba_surface into tgtPixels, uisng Intel ISPC encoders
bool IntelPlugin::ISPC_compression(rgba_surface &source, const Image& target, bool hasAlpha_rgba)  
{
	CompressionFunc* cmpFunc = NULL;

	switch (ps.data->encoding_g) 
	{
		case DXGI_FORMAT_BC1_UNORM_SRGB:
		case DXGI_FORMAT_BC1_UNORM:
			cmpFunc = CompressImageBC1;
			break;
				
		case DXGI_FORMAT_BC3_UNORM_SRGB:
		case DXGI_FORMAT_BC3_UNORM:
			cmpFunc = CompressImageBC3;
			break;

		case DXGI_FORMAT_BC7_UNORM_SRGB:
		case DXGI_FORMAT_BC7_UNORM:
			if (hasAlpha_rgba)
	    		cmpFunc = ps.data->fast_bc67 ? CompressImageBC7_alpha_veryfast : CompressImageBC7_alpha_basic;
			else
	    		cmpFunc = ps.data->fast_bc67 ? CompressImageBC7_veryfast : CompressImageBC7_basic;
			break;

	    case DXGI_FORMAT_BC6H_UF16:
		case DXGI_FORMAT_BC6H_SF16:
			cmpFunc = ps.data->fast_bc67 ? CompressImageBC6H_fast : CompressImageBC6H_slow;
			break;	

		default:
			break;
	}

	if (cmpFunc)
	{
		int slices = (source.width * source.height) / 0x40000;	// 256K pixels per chunk
		if (slices < 1)
			slices = 1;

		for (int i = 0; i < slices; i++)
		{
			if (i > 0 && !SetProgress(i, slices))	// allow an early out
				return false;

			int ylo = (i * source.height / slices) & ~0x3;	// 4 pixels per block row
			int yhi = ((i+1) * source.height / slices) & ~0x3;	// 4 pixels per block row

			if (yhi > source.height)
				yhi = source.height;

			if (yhi > ylo)
			{
				auto input = source;
				input.ptr += input.stride * ylo; 
				input.height = yhi - ylo;

				auto dst = target.pixels + target.rowPitch * (ylo >> 2);	// rowPitch is actually blockRowPitch here

				if (ps.data->gMultithreaded) 
					CompressImageMT(&input, dst, cmpFunc, ps.data->encoding_g);
				else
					CompressImageST(&input, dst, cmpFunc, ps.data->encoding_g);
			}
		}
		return true;
	}

	return false;
}

/*****************************************************************************/
/*****************************************************************************/
//Pad the surface size to boundaries of 4
//Calculates new width,height,stride and new buffer
//It does not deallocate the old buffer
//It does return the new surface 
//It is the responsibility of the user to deallocate the new/old buffer when not needed anymore
rgba_surface IntelPlugin::DoPaddingToMultiplesOf4(const rgba_surface &input)
{
	rgba_surface out;

	// now we're going to copy to a new buffer to do padding to boundaries of 4
	// boundaries of 4 are needed for the compression algorithms to work
	// since they rely on 4x4 blocks
	int pixelSize = 4 * (ps.data->encoding_g == DXGI_FORMAT_BC6H_UF16 ? 2 : 1);
	out.width = (input.width+3) & ~3;       //pad witdh to boundaries of 4
	out.height = (input.height+3) & ~3;     //pad height to boundaries of 4
	out.stride = out.width * pixelSize;                  //size of a row
	out.ptr = new uint8_t[out.height * out.stride];    //new buffer
			
	//Copy row by row into new buffer
	for (int y = 0; y < input.height; y++)
	{
		auto rowSrc = input.ptr + y * input.stride;
		auto rowDst = out.ptr + y * out.stride;

		//Copy existing rows
		memcpy(rowDst, rowSrc, input.width * pixelSize);

		//Pad to new width, copy the last pixel of the old row into the remaining pixels of new row
		for (int x=input.width; x < out.width; x++)	// trailing pixels
			memcpy(rowDst + x * pixelSize, rowSrc + (input.width-1) * pixelSize, pixelSize);
	}

	//Pad to new height, copy last row multiple times to fill new rows 
	for (int y = input.height; y < out.height; y++)	// extra rows
	{
		auto rowDst = out.ptr + y * out.stride;
		memcpy(rowDst, rowDst - out.stride, out.stride);
	}

	return out;
}

/*****************************************************************************/
/*****************************************************************************/
//AdvanceState () has to be called before entering this function so that the ps.formatRecord->data buffer is full;
ScratchImage* IntelPlugin::GetCompressedImageForPreview(int planesToGet_, int &compressedSize)
{  
	//Returns null if image to preview is uncompressed
	if (ps.data->encoding_g == DXGI_FORMAT_R8G8B8A8_UNORM)
	{
		//just compute size, the preview will use the uncompressed iamge for both views
		compressedSize = ps.formatRecord->imageSize.h*ps.formatRecord->imageSize.v*4;
		return NULL;
	}
	
	//Show loading cursor
	showLoadingCursor();

	bool hasAlpha_ = false;

	//Open a empty compressed scratch image
	ScratchImage *scrImageScratch = new ScratchImage(); 
	//Open a empty uncompressed scratch image
	ScratchImage *scrUncompressedImageScratch = new ScratchImage();

	//Flag if there is alpha channel and corresponding texture type Color+Alpha specified
	if (planesToGet_ > 3)
		hasAlpha_ = true;
	
	if (!CopyDataForEncoding(scrUncompressedImageScratch, hasAlpha_, false, true))
	{
		errorMessage("Unsupported bit depth", "Preview Error");
		return NULL;
	}
	
	//============================================================================================
	//============================================================================================
	//Special preview operation, create a horiz. crossed 2D image out of the cube map
	if (ps.data->TextureTypeIndex==TextureTypeEnum::CUBEMAP_CROSSED && IsCubeMapWithSetMipLevelOverride())
	{
		//If a SetMipLevel was specified you have to turn even Crossed image into proper CubeMap Scratchimages to the the mipmap
		//If this is a cube map, change scrUncompressedImageScratch into a CubeMap type image, section
		ConvertToCubeMapFromCross(&scrUncompressedImageScratch);

		//create a horiz. crossed 2D image out of the cube map
	    ConvertToHorizontalCrossFromCubeMap(&scrUncompressedImageScratch);
	}
	else if (ps.data->TextureTypeIndex == TextureTypeEnum::CUBEMAP_LAYERS)
	{
		//If this is a cube map, change scrUncompressedImageScratch into a CubeMap type image, section
		ConvertToCubeMapFromLayers(&scrUncompressedImageScratch, hasAlpha_);
		
		//create a horiz. crossed 2D image out of the cube map
	    ConvertToHorizontalCrossFromCubeMap(&scrUncompressedImageScratch);
	}
	
	//============================================================================================
	//============================================================================================
	//MipMap from Layers
	//Overwrite only the initial image MipLevel 0 from Layer 0, in order to generate automatic MipMaps
	if (IsMipMapsDefinedByLayer())
	{
		//We use this to avoid having to disable the visibility of all layers when creating mip level 0. 
		//By default all images are composited onto mip level 0
		CopyLayersIntoMipMaps(scrUncompressedImageScratch, hasAlpha_, 0, 1);
	}

	//============================================================================================
	//============================================================================================
	//If FlipX/Y true, invert R/G channels scrUncompressedImageScratch only if Normal Map, section
	if ((ps.data->FlipX || ps.data->FlipY) && (ps.data->TextureTypeIndex==TextureTypeEnum::NORMALMAP))
	{
		FlipXYChannelNormalMap(scrUncompressedImageScratch);
	}

	//============================================================================================
	//============================================================================================
	//If Normalization Postprocess option has been specified and this is a Normal Type texture type
	//Normalize all mip chain
	if (ps.data->Normalize && ps.data->TextureTypeIndex == TextureTypeEnum::NORMALMAP)
	{
		NormalizeNormalMapChain(scrUncompressedImageScratch);
	}

	//============================================================================================
	//============================================================================================
	//Compress scrUncompressedImageScratch into scrImageScratch, section
	ps.data->previewing = true;
	if (!CompressToScratchImage(&scrImageScratch, &scrUncompressedImageScratch, hasAlpha_))
	{
		return NULL;
	}

	//Get compressed size of first image
	compressedSize = int(scrImageScratch->GetImages()->slicePitch);
	
	//This is not needed when saving out the image, its just needed for SRGB or 32 bit formats
	//to compensate for monitor gamma and have it look ok in preview.
	if (ps.formatRecord->depth == 32)
	{
		//Force SRGB if 32bit, to diplay ok. Otherwise it previews to bright
		scrImageScratch->OverrideFormat(MakeSRGB(scrImageScratch->GetMetadata().format));
	}
	else
	{
		//If SRGB remove SRGB flag to display OK.
		DXGI_FORMAT fmt = scrImageScratch->GetMetadata().format;
		switch ( fmt )
		{
			case DXGI_FORMAT_BC1_UNORM_SRGB:
				scrImageScratch->OverrideFormat(DXGI_FORMAT_BC1_UNORM);
				break;
			case DXGI_FORMAT_BC3_UNORM_SRGB:
				scrImageScratch->OverrideFormat(DXGI_FORMAT_BC3_UNORM);
				break;
			case DXGI_FORMAT_BC7_UNORM_SRGB:
				scrImageScratch->OverrideFormat(DXGI_FORMAT_BC7_UNORM);
				break;
			default:
				break;
		}
	}
    
	//Decompress scrImageScratch image into and uncpressed one
	ScratchImage *destImage = new ScratchImage();
	HRESULT hr;

	auto targetFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	if (ps.formatRecord->depth == 32 && (ps.data->encoding_g == DXGI_FORMAT_BC6H_UF16 || ps.data->encoding_g == DXGI_FORMAT_BC6H_SF16))
		targetFormat = DXGI_FORMAT_R32G32B32A32_FLOAT;
	
	hr = Decompress( scrImageScratch->GetImages(), scrImageScratch->GetImageCount(), scrImageScratch->GetMetadata(), targetFormat, *destImage );
    
	if ( FAILED(hr) )
	{
		errorMessage("Can not decompress image", "Preview Error");
		delete destImage;
		destImage = NULL;
	}
	
	//Cleanup, section
	//Dispose compressed image
	if (scrUncompressedImageScratch != NULL)
	    delete scrUncompressedImageScratch;

	if (scrImageScratch != NULL)
	    delete scrImageScratch;

	//Go back to normal cursor
	showNormalCursor();

	return destImage;
}

/*****************************************************************************/
/*****************************************************************************/
//AdvanceState () has to be called before entering this function so that the ps.formatRecord->data buffer is full;
ScratchImage* IntelPlugin::GetUncompressedImageForPreview(int planesToGet_)
{  
	bool hasAlpha_ = false;

	//Open a empty uncompressed scratch image
	ScratchImage *scrUncompressedImageScratch = new ScratchImage();

	//Flag if there is alpha channel and corresponding texture type Color+Alpha specified
	if (planesToGet_ > 3)
		hasAlpha_ = true;
	
	if (!CopyDataForEncoding(scrUncompressedImageScratch, hasAlpha_, false, false))
	{
		errorMessage("Unsupported bit depth", "Preview Error");
		return NULL;
	}
	
	//============================================================================================
	//============================================================================================
	//Special preview operation, create a horiz. crossed 2D image out of the cube map
	if (ps.data->TextureTypeIndex==TextureTypeEnum::CUBEMAP_CROSSED && IsCubeMapWithSetMipLevelOverride())
	{
		//If a SetMipLevel was specified you have to turn even Crossed image into proper CubeMap Scratchimages to the the mipmap
		//If this is a cube map, change scrUncompressedImageScratch into a CubeMap type image, section
		ConvertToCubeMapFromCross(&scrUncompressedImageScratch);

		//create a horiz. crossed 2D image out of the cube map
	    ConvertToHorizontalCrossFromCubeMap(&scrUncompressedImageScratch);
	}
	else if (ps.data->TextureTypeIndex == TextureTypeEnum::CUBEMAP_LAYERS)
	{
		//If this is a cube map, change scrUncompressedImageScratch into a CubeMap type image, section
		ConvertToCubeMapFromLayers(&scrUncompressedImageScratch, hasAlpha_);
		
		//create a horiz. crossed 2D image out of the cube map
	    ConvertToHorizontalCrossFromCubeMap(&scrUncompressedImageScratch);
	}

	//============================================================================================
	//============================================================================================
	//MipMap from Layers
	//Overwrite only the initial image MipLevel 0 from Layer 0, in order to generate automatic MipMaps
	if (IsMipMapsDefinedByLayer())
	{
		//We use this to avoid having to disable the visibility of all layers when creating mip level 0. 
		//By default all images are composited onto mip level 0
		CopyLayersIntoMipMaps(scrUncompressedImageScratch, hasAlpha_, 0, 1);
	}
	
	//============================================================================================
	//============================================================================================
	//If FlipX/Y true, invert R/G channels scrUncompressedImageScratch only if Normal Map, section
	if ((ps.data->FlipX || ps.data->FlipY) && (ps.data->TextureTypeIndex==TextureTypeEnum::NORMALMAP))
	{
		FlipXYChannelNormalMap(scrUncompressedImageScratch);
	}

	//If 16 bit convert to 8 bit
	if (scrUncompressedImageScratch->GetMetadata().format == DXGI_FORMAT_R16G16B16A16_FLOAT)
	{
		//Decompress scrImageScratch image into and uncpressed one
		ScratchImage *destImage = new ScratchImage();
		HRESULT hr;
	
		hr = Convert( scrUncompressedImageScratch->GetImages(), scrUncompressedImageScratch->GetImageCount(), scrUncompressedImageScratch->GetMetadata(), DXGI_FORMAT_R8G8B8A8_UNORM, TEX_FILTER_DEFAULT, 0.5f, *destImage );
    
		if ( FAILED(hr) )
		{
			errorMessage("Can not convert image", "Preview Error");
			delete destImage;
			destImage = NULL;
		}
	
		//Cleanup, section
		//Dispose compressed image
		if (scrUncompressedImageScratch != NULL)
			delete scrUncompressedImageScratch;

		return destImage;
	}

	return scrUncompressedImageScratch;
}

/*****************************************************************************/
/*****************************************************************************/
//Return true if texture type cube maps and the setMipLevel is checked
bool IntelPlugin::IsCubeMapWithSetMipLevelOverride()
{
	if (ps.data->SetMipLevel &&
	   (ps.data->TextureTypeIndex == TextureTypeEnum::CUBEMAP_CROSSED || ps.data->TextureTypeIndex == TextureTypeEnum::CUBEMAP_LAYERS))
	   return true;

	return false;
}

/*****************************************************************************/
/*****************************************************************************/
//Returns true if mip maps are defined by layers
//Mip map form layers is not applicable to cube maps.
bool IntelPlugin::IsMipMapsDefinedByLayer()
{
	if (ps.data->MipMapTypeIndex == MipmapEnum::FROM_LAYERS &&
		ps.data->TextureTypeIndex != TextureTypeEnum::CUBEMAP_CROSSED &&
	    ps.data->TextureTypeIndex != TextureTypeEnum::CUBEMAP_LAYERS &&
		ps.formatRecord->documentInfo->layerCount > 1)
		return true;

	return false;
}


/*****************************************************************************/
/*****************************************************************************/
//Convert scrUncompressedImageScratch_ from a  crossed layout image to a cube map ScratchImage
void IntelPlugin::ConvertToCubeMapFromCross(ScratchImage **scrUncompressedImageScratch_)
{
		//Open a empty uncompressed scratch image
	    ScratchImage *cubemapUncompressedImageScratch = new ScratchImage();

		//Hold the rectangles which define the siz cube face in the large image
		DirectX::Rect coords[6];

		//These coords are for the following cubemap order +X,-X,+Y,-Y,+Z,-Z . This is the order CubemapGen likes
		//They define a absolute width/height multiplactor into the image to get the coords for horizontal.vertical crossed layout cubemap . 
		//CubeMaps have all their 6 faces equal.
		int crossedCoords[6][2] =  {{2,1}, {0,1}, {1,0}, {1,2}, {1,1}, {3,1}}; 

		//Setup width/height with Horziontal(4x3) cross layout as default
		int width = ps.formatRecord->imageSize.h/4;
		int height = ps.formatRecord->imageSize.v/3;

		//Determine if Vertical(3x4) cubemap layout
		if (ps.formatRecord->imageSize.h < ps.formatRecord->imageSize.v)
		{
			//Vertical Cross layout
			width = ps.formatRecord->imageSize.h/3;
			height = ps.formatRecord->imageSize.v/4;

			//flip the last coords if in vertical layout
			crossedCoords[5][0] = 1;
			crossedCoords[5][1] = 3;
		}

		//Compute final coordinates
		for (int i=0; i<6; i++)
		{
			coords[i] = DirectX::Rect(crossedCoords[i][0]*width, crossedCoords[i][1]*height, width, height);
		}		


		//Allocate a cubemap scratchImage
		if (ps.data->encoding_g == DXGI_FORMAT_BC6H_UF16)
		{
			//Allocate space for one cube map rgba 16bit float image 
			cubemapUncompressedImageScratch->InitializeCube(DXGI_FORMAT_R16G16B16A16_FLOAT, width, height, 1, 1);
		}
		else //BC1,3,7 or uncompressed
		{
			//Allocate space for one cube map rgba 8bit float image 
			cubemapUncompressedImageScratch->InitializeCube(DXGI_FORMAT_R8G8B8A8_UNORM, width, height, 1, 1);
		}


		//Copy from crossed cubemap from big scrUncompressedImageScratch_ to images in cubemapUncompressedImageScratch
		const Image *crossedImage = (*scrUncompressedImageScratch_)->GetImages();
		const Image *cubeImage = cubemapUncompressedImageScratch->GetImages();

		for (int i=0; i<6; i++)
		{
			DirectX::CopyRectangle(*crossedImage, coords[i], cubeImage[i], TEX_FILTER_DEFAULT, 0, 0);
		}


		//Free previous space
		delete *scrUncompressedImageScratch_;

		//Copy over pointer from ScratchImage which has CubeMap, 
		//now scrUncompressedImageScratch holds the crossed cubemap disected
		*scrUncompressedImageScratch_ = cubemapUncompressedImageScratch;
}

/*****************************************************************************/
/*****************************************************************************/
//Convert scrUncompressedImageScratch_ from document layers to a cube map DirectX::ScratchImage
bool IntelPlugin::ConvertToCubeMapFromLayers(ScratchImage **scrUncompressedImageScratch_, bool hasAlpha_)
{
	ReadChannelDesc *pChannel;
	ReadLayerDesc *layerDesc;
	char *pLayerData;
	
	if (ps.formatRecord->documentInfo->layerCount < 6)
	{
	    return false;
	}

	// Get a buffer to hold each channel as we process, formatRecord->planeBytes are computed the first time fetchImage() is called
	pLayerData = sPSBuffer->New(NULL, ps.formatRecord->imageSize.h * ps.formatRecord->imageSize.v * ps.formatRecord->planeBytes);

	if (pLayerData == NULL)
	{
		SetResult(memFullErr);
		return false;
	}

	//Open a empty uncompressed scratch image
    ScratchImage *cubemapUncompressedImageScratch = new ScratchImage();

	//All layers must have these names and feature the correspanding image +X,-X,+Y,-Y,+Z,-Z.
	//Any layers with different names will be ignored.
	//There must be at least 6 layers present including the background layer, otherwise some cube faces will be blank.
	//The final Cubemap dds will have the images in this order, +X,-X,+Y,-Y,+Z,-Z. This is the order CubemapGen likes and sort of a standard.

	//Which face goes into what index inside the cubemap 
	std::map<std::string, int> facesmap;
	facesmap["+X"]=0; 
	facesmap["-X"]=1; 
	facesmap["+Y"]=2;
	facesmap["-Y"]=3;
	facesmap["+Z"]=4;
	facesmap["-Z"]=5;

	//Setup width/height
	int width =  static_cast<int>((*scrUncompressedImageScratch_)->GetMetadata().width);
	int height = static_cast<int>((*scrUncompressedImageScratch_)->GetMetadata().height);

	//Allocate a cubemap scratchImage
	if (ps.data->encoding_g == DXGI_FORMAT_BC6H_UF16)
	{
		//Allocate space for one cube map rgba 16bit float image 
		cubemapUncompressedImageScratch->InitializeCube(DXGI_FORMAT_R16G16B16A16_FLOAT, width, height, 1, 1);
	}
	else //BC1,3,7 or uncompressed
	{
		//Allocate space for one cube map rgba 8bit float image 
		cubemapUncompressedImageScratch->InitializeCube(DXGI_FORMAT_R8G8B8A8_UNORM, width, height, 1, 1);
	}

	//*****************************************************
	//Copy from Layers into cubemapUncompressedImageScratch
	const Image *cubeImage = cubemapUncompressedImageScratch->GetImages();

	
	//Get the first layer in this layer list
	layerDesc = ps.formatRecord->documentInfo->layersDescriptor;
	
	//Cycle through remaining layers, Assign each layer to a different mip map in ScratchImage
	while (layerDesc != NULL)
	{	
		std::map<std::string,int>::iterator it;

		//Check layername for validity only these are supported "+X","-X","+Y","-Y","+Z","-Z"
		it = facesmap.find(layerDesc->name);
		if ( it != facesmap.end())
		{
			//Get index into SratchImage array for this face name
			int i = it->second;

			//Get the first channel in this channel list 
			pChannel = layerDesc->compositeChannelsList;
			int planeNumber = 0;

			//Get the RGB channels
			while (pChannel != NULL)
			{
				//Get pixel data from channel
				ReadLayerData(pChannel, pLayerData, ps.formatRecord->imageSize.h, ps.formatRecord->imageSize.v);

				for (size_t y = 0; y < cubeImage[i].height; y++)
				{
					for (size_t x = 0; x < cubeImage[i].width; x++)
					{
						int indexToImage = int(cubeImage[i].width*y*4 + x*4);  //the ScratchImage is always RGBA therefore pitch of 4
						int indexToLayerChannel = int(ps.formatRecord->imageSize.h*y + x);  //here the pitch is only 1

						CopyFromLayerChannelIntoImage(pLayerData, &cubeImage[i], indexToImage+planeNumber, indexToLayerChannel);
					}
				}

				pChannel = pChannel->next;
				planeNumber++;
			}


			//Get first layermask and set as alpha
			//If no layermask then use transparency channel 
			if (!(pChannel = layerDesc->layerMask))
			{
				//Get first Transparency channel and set as alpha
				pChannel = layerDesc->transparency;
			}

			//Get alpha
			if (hasAlpha_ && (pChannel != NULL))
			{
				//Get pixel data from channel
				ReadLayerData(pChannel, pLayerData, ps.formatRecord->imageSize.h, ps.formatRecord->imageSize.v);
			}
			else
			{
				FillLayerDataToWhite(pLayerData, ps.formatRecord->imageSize.h, ps.formatRecord->imageSize.v);
			}

			for (size_t y = 0; y < cubeImage[i].height; y++)
			{
				for (size_t x = 0; x < cubeImage[i].width; x++)
				{
					int indexToImage = int(cubeImage[i].width*y*4 + x*4);  //the ScratchImage is always RGBA therefore pitch of 4
					int indexToLayerChannel = int(ps.formatRecord->imageSize.h*y + x);  //here the pitch is only 1

					//Here the plane number is 3 for alpha
					CopyFromLayerChannelIntoImage(pLayerData, &cubeImage[i], indexToImage+3, indexToLayerChannel);
				}
			}
			
		}
				
		layerDesc = layerDesc->next; //Get next layer
	}
			
	sPSBuffer->Dispose(static_cast<char**>(&pLayerData));


	//Free previous space
	delete *scrUncompressedImageScratch_;

	//Copy over pointer from ScratchImage which has CubeMap, 
	//now scrUncompressedImageScratch holds the crossed cubemap disected
	*scrUncompressedImageScratch_ = cubemapUncompressedImageScratch;

	return true;
}

/*****************************************************************************/
/*****************************************************************************/
//Convert scrUncompressedImageScratch_ from a cube map to a horizontal crossed layout image 
void IntelPlugin::ConvertToHorizontalCrossFromCubeMap(ScratchImage **scrUncompressedImageScratch_)
{
		//Open a empty uncompressed scratch image
	    ScratchImage *crossedUncompressedImageScratch = new ScratchImage();

		//Hold the rectangles which define the siz cube face in the large image
		DirectX::Rect coord;

		//Which mipLevel to get for crossed image cubemap preview. Normaly 0 but can change when setMipLevels is specified
		int mipLevel = 0;

		//These coords are for the following cubemap order +X,-X,+Y,-Y,+Z,-Z . This is the order CubemapGen likes
		//They define a absolute width/height multiplactor into the image to get the coords for horizontal.vertical crossed layout cubemap . 
		//CubeMaps have all their 6 faces equal.
		int crossedCoords[6][2] =  {{2,1}, {0,1}, {1,0}, {1,2}, {1,1}, {3,1}}; 
	
		//Setup width/height with Horziontal(4x3) cross layout as default
		int width = static_cast<int>((*scrUncompressedImageScratch_)->GetMetadata().width);
		int height = static_cast<int>((*scrUncompressedImageScratch_)->GetMetadata().height);

		//============================================================================================
	    //============================================================================================
	    //Force mipmaps if setMipLevels on cube maps are specified
    	if (IsCubeMapWithSetMipLevelOverride())
		{
			//Open temporary imageScratch to save mipmaps
			ScratchImage *scrImageMipMapScratch = new ScratchImage(); 
		
			//Generate MipMaps from scrUncompressedImageScratch and save to scrImageMipMapScratch
			HRESULT hr = GenerateMipMaps((*scrUncompressedImageScratch_)->GetImages(), (*scrUncompressedImageScratch_)->GetImageCount(), 
										 (*scrUncompressedImageScratch_)->GetMetadata(), TEX_FILTER_DEFAULT|TEX_FILTER_SEPARATE_ALPHA, 0, *scrImageMipMapScratch );
			if( hr != S_OK )
			{
				UserError("Could not create MipMaps");
				return;
			}
	
		    //Override with mip level size
		    width = int(scrImageMipMapScratch->GetImage(ps.data->MipLevel, 0, 0)->width);
		    height = int(scrImageMipMapScratch->GetImage(ps.data->MipLevel, 0, 0)->height);

			//Set which mip level to geto for creating crossed image
			mipLevel = ps.data->MipLevel;

			//free previous space
			delete *scrUncompressedImageScratch_;
		
			//Copy over pointer from ScratchImage which has MipMap, now scrUncompressedImageScratch holds the initial image + mip chain
			(*scrUncompressedImageScratch_) = scrImageMipMapScratch;
		}
		
		coord = DirectX::Rect(0, 0, width, height);
		
		//Allocate a scratchImage
		if (ps.data->encoding_g == DXGI_FORMAT_BC6H_UF16)
		{
			//Allocate space for one cube map rgba 16bit float image 
			crossedUncompressedImageScratch->Initialize2D(DXGI_FORMAT_R16G16B16A16_FLOAT, width*4, height*3, 1, 1);
		}
		else //BC1,3,7 or uncompressed
		{
			//Allocate space for one cube map rgba 8bit float image 
			crossedUncompressedImageScratch->Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, width*4, height*3, 1, 1);
		}

		//Copy from cubemap scrUncompressedImageScratch_ to crossed horizontal layout in crossedUncompressedImageScratch
		const Image *crossedImage = crossedUncompressedImageScratch->GetImages();

		for (int i=0; i<6; i++)
		{
		    const Image *cubeImage = (*scrUncompressedImageScratch_)->GetImage(mipLevel, i, 0);
			DirectX::CopyRectangle(*cubeImage, coord, *crossedImage, TEX_FILTER_DEFAULT, crossedCoords[i][0]*width, crossedCoords[i][1]*height);
		}

		//Free previous space
		delete *scrUncompressedImageScratch_;

		//Copy over pointer from ScratchImage which has CubeMap, 
		//now scrUncompressedImageScratch holds the crossed cubemap disected
		*scrUncompressedImageScratch_ = crossedUncompressedImageScratch;
}

/*****************************************************************************/
/*****************************************************************************/
//Flip the X(Red) channel and or the Y(Green) channel of the normal map
void IntelPlugin::FlipXYChannelNormalMap(ScratchImage *scrUncompressedImageScratch_)
{
	//Get first on only image
	const Image *image = scrUncompressedImageScratch_->GetImages();

	//Traverse all pixels
	for (size_t i = 0; i < image->height*image->width; i++)
	{
		//scrUncompressedImageScratch is by default RGBA data so we need a pitch of 4
		int index = (int)i*4;
			
		if (image->format == DXGI_FORMAT_R8G8B8A8_UNORM)
		{
			//Get pointer to first (and only) image
			unsigned8 *rowBigDataPtr = image->pixels;

			//Inverse values of R/G channels only
			if (ps.data->FlipX)
				rowBigDataPtr[index] = 255 - rowBigDataPtr[index];

			if (ps.data->FlipY)
				rowBigDataPtr[index + 1] = 255 - rowBigDataPtr[index + 1];
		}
		else if (image->format == DXGI_FORMAT_R16G16B16A16_FLOAT)
		{
			//Get pointer to first (and only) image, cast to 16bit for BC6
		    unsigned16 *rowBigDataPtr = reinterpret_cast<unsigned16 *>(image->pixels);
				
			//Inverse values of R/G channels only
			float r = F16toF32(rowBigDataPtr[index]);
			float g = F16toF32(rowBigDataPtr[index+1]);

			if (ps.data->FlipX)
				rowBigDataPtr[index] = F32toF16(1.f - r);

			if (ps.data->FlipY)
				rowBigDataPtr[index+1] = F32toF16(1.f - g);
		}
	}
}

/*****************************************************************************/
/*****************************************************************************/
//Normalize all values of this Nomral map in main image and all mip maps
void IntelPlugin::NormalizeNormalMapChain(ScratchImage *scrUncompressedImageScratch_)
{
	//Open temporary imageScratch to save mipmaps
	for (size_t i = 0; i < scrUncompressedImageScratch_->GetImageCount(); i++)
	{
		if (auto image = scrUncompressedImageScratch_->GetImages() + i)
		{
			for (size_t y = 0; y < image->height; y++)
			{
				auto ptr = image->pixels + image->rowPitch * y;					
				if (image->format == DXGI_FORMAT_R8G8B8A8_UNORM)
				{
					for (size_t x = 0; x < image->width; x++)
					{
						auto p = ptr + x * 4;
						float r = static_cast<float>(p[0] - 128);
						float g = static_cast<float>(p[1] - 128);
						float b = static_cast<float>(p[2] - 128);
						float m = sqrt(r*r + g*g + b*b);
						if (m > 0)
						{
							m = 127/m;
							p[0] = static_cast<uint8_t>(r*m + 128);
							p[1] = static_cast<uint8_t>(g*m + 128);
							p[2] = static_cast<uint8_t>(b*m + 128);
						}
						else
						{
							//This is the default normal vector (0,0,1)
							p[0] = 128;
							p[1] = 128;
							p[2] = 255;
						}
					}
				}
				else if (image->format == DXGI_FORMAT_R16G16B16A16_FLOAT)
				{
					for (size_t x = 0; x < image->width; x++)
					{
						auto p = reinterpret_cast<uint16_t*>(ptr) + x * 4;
						float r = F16toF32(p[0]);
						float g = F16toF32(p[1]);
						float b = F16toF32(p[2]);
						float m = sqrt(r*r + g*g + b*b);
						if (m > 0)
						{
							m = 1.0f/m;
							p[0] = F32toF16(r * m);
							p[1] = F32toF16(g * m);
							p[2] = F32toF16(b * m);
						}
						else
						{
							//This is the default normal vector (0,0,1)
							p[0] = F32toF16(0);
							p[1] = F32toF16(0);
							p[2] = F32toF16(1);
						}
					}
				}
			}
		}
	}
}

/*****************************************************************************/
/*****************************************************************************/
/* Read in the channel data from the original document. */
void IntelPlugin::ReadLayerData(ReadChannelDesc *pChannel, char *pLayerData, int width, int height)
{
	// Make sure there is something for me to read from
	if (pChannel == NULL || pChannel->port == NULL || pLayerData == NULL)
		return;

	Boolean canRead;
	if (sPSChannelProcs->CanRead(pChannel->port, &canRead))
	{
		// this function should not error, tell the host accordingly
		SetResult(errPlugInHostInsufficient);
		return;
	}

	if (!canRead)
		return;

	// some local variables to play with
	VRect read_rect;
	PixelMemoryDesc destination;
			
	// What area of the document do we want to read from
	read_rect.top = 0;
	read_rect.left = 0;
	read_rect.bottom = height;
	read_rect.right = width;

	// set up the PixelMemoryDesc
	destination.data = pLayerData;
	destination.depth = pChannel->depth;
	destination.rowBits = width*pChannel->depth;
	destination.colBits = pChannel->depth;
	destination.bitOffset = 0;

	// Read this data into our buffer, you could check the read_rect to see if you got everything you desired
	if (sPSChannelProcs->ReadPixelsFromLevel(	
		pChannel->port,
		0,
		&read_rect,
		&destination))
	{
		SetResult(errPlugInHostInsufficient);
		return;
	}
}

void IntelPlugin::FillLayerDataToWhite(char *pLayerData, int width, int height)
{
	for (int x=0; x<width; x++)
	{
		for (int y=0; y<height; y++)
		{
			int indexToLayerChannel = y*width + x;

			if (ps.formatRecord->depth == 8)
			{
				unsigned8 *rowData = reinterpret_cast<unsigned8 *>(pLayerData);
				rowData[indexToLayerChannel] = 255;
			}
			else if (ps.formatRecord->depth == 16)
			{
				unsigned16 *rowData16bit = reinterpret_cast<unsigned16 *>(pLayerData);
				rowData16bit[indexToLayerChannel] = ConvertTo16Bit(static_cast<unsigned8>(255));
			}
			else if (ps.formatRecord->depth == 32)
			{
				float *rowData32bit = reinterpret_cast<float *>(pLayerData);
				rowData32bit[indexToLayerChannel] = 1.0;
			}
		}
	}
}

/*****************************************************************************/
/*****************************************************************************/
// Copy one component from layer channel to Image 
void IntelPlugin::CopyFromLayerChannelIntoImage(char *pLayerData, const Image *image, int indexToImage, int indexToLayerChannel)
{
	if (ps.data->encoding_g == DXGI_FORMAT_BC6H_UF16)
	{
		//Get pointer to first (and only) image, cast to 16bit for BC6
		unsigned16 *rowBigDataPtr = reinterpret_cast<unsigned16 *>(image->pixels);

		//Copy data from photoshop buffer into scrUncompressedImageScratch
		//Convert pixels to 16Bit for BC6 encoding
		if (ps.formatRecord->depth == 8)
		{
			unsigned8 *rowData = reinterpret_cast<unsigned8 *>(pLayerData);
			rowBigDataPtr[indexToImage] = ConvertTo16Bit(rowData[indexToLayerChannel]);
		}
		else if (ps.formatRecord->depth == 16)
		{
			unsigned16 *rowData16bit = reinterpret_cast<unsigned16 *>(pLayerData);
			rowBigDataPtr[indexToImage] = ConvertTo16Bit(rowData16bit[indexToLayerChannel]);
		}
		else if (ps.formatRecord->depth == 32)
		{
			float *rowData32bit = reinterpret_cast<float *>(pLayerData);
			rowBigDataPtr[indexToImage] = ConvertTo16Bit(rowData32bit[indexToLayerChannel]);
		}
	}
	else //BC1,3,7,4,5 or uncompressed (same treatment since we process by existing channel and they are all 8 bit)
	{
		//Get pointer to first (and only) image
		unsigned8 *rowBigDataPtr = image->pixels;
		
		//Convert pixels to 8bit for BC encoding
		if (ps.formatRecord->depth == 8)
		{
			unsigned8 *rowData = reinterpret_cast<unsigned8 *>(pLayerData); //Get image pointer
			rowBigDataPtr[indexToImage] = ConvertTo8Bit(rowData[indexToLayerChannel]);
		}
		else if (ps.formatRecord->depth == 16)
		{
			unsigned16 *rowData16bit = reinterpret_cast<unsigned16 *>(pLayerData); //Get image pointer
			rowBigDataPtr[indexToImage] = ConvertTo8Bit(rowData16bit[indexToLayerChannel]);
		}
		else if (ps.formatRecord->depth == 32)
		{
			float *rowData32bit = reinterpret_cast<float *>(pLayerData); //Get image pointer
			rowBigDataPtr[indexToImage] = ConvertTo8Bit(rowData32bit[indexToLayerChannel]);
		}
	}
}

/*****************************************************************************/
/*****************************************************************************/
//Copy document Layers into mipmap Images of ScratchImage
//startMipIndex: specify from which layer to start copy. 
//endMipIndex: until which layer to copy. You can leave this blank in which case it will copy al remaining layers.
//Each layer copies into the corresponding mip map level accorsing to its order from the base layer.
void IntelPlugin::CopyLayersIntoMipMaps(ScratchImage *scrUncompressedImageScratch_, bool hasAlpha_, int startMipIndex, int endMipIndex)
{
	int mipMapIndex = startMipIndex;  // mip level=0 is original image
	ReadChannelDesc *pChannel;
	ReadLayerDesc *layerDesc;
	char *pLayerData;

	// Get a buffer to hold each channel as we process, formatRecord->planeBytes are computed the first time fetchImage() is called
	pLayerData = sPSBuffer->New(NULL, ps.formatRecord->imageSize.h * ps.formatRecord->imageSize.v * ps.formatRecord->planeBytes);

	if (pLayerData == NULL)
	{
		SetResult(memFullErr);
		return;
	}

	//Get the first layer (usually Backround) in this layer list
	layerDesc = ps.formatRecord->documentInfo->layersDescriptor;

	//Skip initial layers to startLayerIndex
	int startLayerIndex = startMipIndex;
	while (startLayerIndex != 0)
	{
		layerDesc = layerDesc->next; 
		startLayerIndex--;
	}
			
	//Cycle through remaining layers, Assign each layer to a different mip map in ScratchImage
	while ((layerDesc != NULL) && (mipMapIndex <= endMipIndex))
	{	
		//There are not enough mip maps in this Image, there are too much layers
		if (int(scrUncompressedImageScratch_->GetImageCount()) <= mipMapIndex)
			break;
				
		//Get mip map image
		const Image *image = scrUncompressedImageScratch_->GetImages() + mipMapIndex;
		if (!image)
			break;

		//Get the first channel in this channel list 
		pChannel = layerDesc->compositeChannelsList;
		int planeNumber = 0;

		//Get the RGB channels
		while (pChannel != NULL)
		{
			//Get pixel data from channel
			ReadLayerData(pChannel, pLayerData, ps.formatRecord->imageSize.h, ps.formatRecord->imageSize.v);

			for (size_t y = 0; y < image->height; y++)
			{
				for (size_t x = 0; x < image->width; x++)
				{
					int indexToImage = int(image->width*y*4 + x*4);  //the ScratchImage is always RGBA therefore pitch of 4
					int indexToLayerChannel = int(ps.formatRecord->imageSize.h*y + x);  //here the pitch is only 1

					CopyFromLayerChannelIntoImage(pLayerData, image, indexToImage+planeNumber, indexToLayerChannel);
				}
			}

			pChannel = pChannel->next;
			planeNumber++;
		}


		//Get first layermask and set as alpha
		//If no layermask then use transparency channel 
		if (!(pChannel = layerDesc->layerMask))
		{
			//Get first Transparency channel and set as alpha
			pChannel = layerDesc->transparency;
		}

		//Get alpha
		if (hasAlpha_ && (pChannel != NULL))
		{
			//Get pixel data from channel
			ReadLayerData(pChannel, pLayerData, ps.formatRecord->imageSize.h, ps.formatRecord->imageSize.v);
		}
		else
		{
			FillLayerDataToWhite(pLayerData, ps.formatRecord->imageSize.h, ps.formatRecord->imageSize.v);
		}

		for (size_t y = 0; y < image->height; y++)
		{
			for (size_t x = 0; x < image->width; x++)
			{
				int indexToImage = int(image->width*y*4 + x*4);  //the ScratchImage is always RGBA therefore pitch of 4
				int indexToLayerChannel = int(ps.formatRecord->imageSize.h*y + x);  //here the pitch is only 1

				//Here the planeumber is 3 for alpha
				CopyFromLayerChannelIntoImage(pLayerData, image, indexToImage+3, indexToLayerChannel);
			}
		}
				
		layerDesc = layerDesc->next; //Get next layer
		mipMapIndex++; //Next mip map
	}
			
	sPSBuffer->Dispose(static_cast<char**>(&(pLayerData)));
}


/*****************************************************************************/
/*****************************************************************************/
//Saves a special mip version of acube map out as a normal cube map. Special function to save out low res cubemaps
HRESULT IntelPlugin::SaveCubeMipLevelToDDSFile(ScratchImage *scrImageScratch_, Blob& blob)
{
		//Open a empty uncompressed scratch image
	    ScratchImage customMipLevelScratch;
		Image customMipLevel[6];

		//Get specific mip images
		for (int i=0; i<6; i++)
			customMipLevel[i] = *scrImageScratch_->GetImage(ps.data->MipLevel, i, 0);

		//Init new cube map from these images
		customMipLevelScratch.InitializeCubeFromImages(customMipLevel, 6);
		
		//Save to file
		return SaveToDDSMemory(customMipLevelScratch.GetImages(), customMipLevelScratch.GetImageCount(), customMipLevelScratch.GetMetadata(), DDS_FLAGS_NONE, blob);
}

void IntelPlugin::InitData()
{
	memset(ps.data, 0, sizeof(*ps.data));

	ps.data->gMultithreaded = GetProcessorCount() > 1;
	ps.data->encoding_g     = DXGI_FORMAT_BC3_UNORM;
	ps.data->queryForParameters = true;

	ps.data->TextureTypeIndex = TextureTypeEnum::COLOR;   //Col,Col+alpha,CubeFromLayer,CubefromCross,NM
	ps.data->MipMapTypeIndex = MipmapEnum::NONE;      //None,Autogen,FromLayers
	ps.data->MipLevel=0;			                   // only valid if SetMipLevel == true
	ps.data->SetMipLevel   = false;
	ps.data->Normalize     = false;
	ps.data->FlipX         = false;
	ps.data->FlipY         = false;
	ps.data->fast_bc67     = false;
	ps.data->exposure	   = 1;
	ps.data->mipmapBatchAllowed = false;
	ps.data->alphaBatchSeperate = false;
} 

void IntelPlugin::DoWritePrepare()
{
	ps.formatRecord->maxData = 0;   //this signifies that we will provide our own buffer
	ps.formatRecord->layerData = 0; //we dont want the layers handed in separately, we read them ourselfes
}

void IntelPlugin::DoWriteStart()
{
	InitData();
	
	/* check with the scripting system whether to pop our dialog */
	
	ps.data->queryForParameters = ReadScriptParamsForWrite();
	
	//If Multithreading init win32 threads and events
	if (ps.data->gMultithreaded)
	{
		InitWin32Threads();
	}

	//Show main dialog and get parameters
	if (OptionsDialog::DoModal(this) != IDOK) 
		SetResult(userCanceledErr);

	if (GetResult() == noErr)
	{
		DoWriteDDS();
	}

	DisposeImageData();

	DestroyThreads();
}

void IntelPlugin::DoWriteContinue()
{
	// should not be called as data has already been disposed
}

void IntelPlugin::DoWriteFinish()
{
	// only called if not cancelled
	WriteScriptParamsForWrite(); // should be different for read/write
}


//Report error to Photoshop, plugin aborts by itself depicting this error message automatically
void IntelPlugin::UserError(const char *usrerror)
{
	unsigned char *pErrorString = reinterpret_cast<unsigned char*>(ps.formatRecord->errorString);
	
	if (pErrorString != NULL && (strlen(usrerror) < 256))
	{
		*ps.resultPtr = errReportString;

		*pErrorString = unsigned char(strlen(usrerror));

		memcpy(pErrorString+1, usrerror, unsigned char(*pErrorString));
	}
}

void IntelPlugin::FetchImageData()
{
	if (!ps.formatRecord->data)
	{
		int planesToGet = ps.formatRecord->planes;
		if (planesToGet > 4)
			planesToGet = 4;

		ps.formatRecord->theRect.left = 0;
		ps.formatRecord->theRect.right = ps.formatRecord->imageSize.h;
		ps.formatRecord->theRect.top = 0;
		ps.formatRecord->theRect.bottom = ps.formatRecord->imageSize.v;
		ps.formatRecord->loPlane = 0;
		ps.formatRecord->hiPlane = planesToGet - 1;
		//The offset in bytes between planes of data in the buffers, for 8 bit interleved data this is 1. Doing this operation computes correctly for depth 16,32 etc 
		ps.formatRecord->planeBytes = (ps.formatRecord->depth + 7) >> 3; 
		//The offset in bytes between columns of data in the buffer. usually 1 for non-interleaved data, or hiPlane-loPlane+1 for interleaved data. 
		ps.formatRecord->colBytes = (ps.formatRecord->hiPlane - ps.formatRecord->loPlane + 1) * ps.formatRecord->planeBytes;
		//The offset in bytes between rows of data in the buffer.
		ps.formatRecord->rowBytes = ps.formatRecord->colBytes * (ps.formatRecord->theRect.right - ps.formatRecord->theRect.left);

	    // seems we have to allocate for ourselves here because we set maxData to 0
		uint32 bufferSize = 
			(ps.formatRecord->theRect.bottom - ps.formatRecord->theRect.top) * 
			ps.formatRecord->rowBytes;

		if (ps.formatRecord->data = sPSBuffer->New( &bufferSize, bufferSize))
		{
			if (ps.formatRecord->documentInfo->layerCount > 1)
			{
				//Because Layermode is enabled for dds mip loading we can not just use the alpha as ususal when there are layers.
		        //RGBA should come from the merged layer data, all this is done in FillFromCompositedLayers().
		        FillFromCompositedLayers();
			}
			else
			{
			    SetResult(ps.formatRecord->advanceState());
			}
		}
		else
			SetResult(memFullErr);

	}
}

void IntelPlugin::DisposeImageData()
{
	if (ps.formatRecord->data)
	{
		sPSBuffer->Dispose(reinterpret_cast<char**>(&ps.formatRecord->data));
		ps.formatRecord->data = NULL;
	}

	if (preview.compressedImage)
	{
		delete preview.compressedImage;
		preview.compressedImage = NULL;
	}
	if (preview.uncompressedImage)
	{
		delete preview.uncompressedImage;
		preview.uncompressedImage = NULL;
	}
}



bool IntelPlugin::SetProgress(int part, int total)
{
	if (ps.formatRecord)
	{
		if (ps.data->previewing)	// don't do progress if we have UI! Allow cancel though
		{
			return !ps.formatRecord->abortProc();
		}
		else
		{
			if (ps.formatRecord->progressProc)
				ps.formatRecord->progressProc(part, total);
			if (ps.formatRecord->abortProc)
			{
				if (ps.formatRecord->abortProc())	// TRUE if user aborted
				{
					SetResult(userCanceledErr);
					return false;
				}
			}
		}
	}

	return true;	// continue!
}


/*****************************************************************************/
//Main compression function which calls all other functions and saves dds
void IntelPlugin::DoWriteDDS()
{ 
	if (GetResult() != noErr)
		return;

	bool DoMipMaps = ps.data->MipMapTypeIndex == MipmapEnum::AUTOGEN || ps.data->MipMapTypeIndex == MipmapEnum::FROM_LAYERS;

	//============================================================================================
	//============================================================================================
	//Setup Photoshop callback structs and what data to get, section

	bool hasAlpha = HasAlpha();

	ScratchImage* scrUncompressedImageScratch = new ScratchImage();

	if (!CopyDataForEncoding(scrUncompressedImageScratch, hasAlpha, DoMipMaps, true))
	{
		UserError("Unsupported bit depth");
		return;
	}

	//============================================================================================
	//============================================================================================
	//If this is a cube map, change scrUncompressedImageScratch into a CubeMap type image, section
	if (ps.data->TextureTypeIndex==TextureTypeEnum::CUBEMAP_CROSSED)
	{
		ConvertToCubeMapFromCross(&scrUncompressedImageScratch);
	}
	else if (ps.data->TextureTypeIndex==TextureTypeEnum::CUBEMAP_LAYERS)
	{
		if (!ConvertToCubeMapFromLayers(&scrUncompressedImageScratch, hasAlpha))
		{
    		UserError("Cubemap has not enough layers available. Consult the documentation (question mark next to the TextureType drop down) on how to create cubemaps");
			return;
		}
	}

	//============================================================================================
	//============================================================================================
	//MipMap from Layers
	//Here we overwrite only the initial image MipLevel 0 from Layer 0, in order to generate automatic MipMaps
	//the second step of MipMap from layers is furter down
	if (IsMipMapsDefinedByLayer())
	{
		//We use this to avoid having to disable the visibility of all layers when creating mip level 0. 
		//By default all images are composited onto mip level 0
		CopyLayersIntoMipMaps(scrUncompressedImageScratch, hasAlpha, 0, 1);
	}

	//============================================================================================
	//============================================================================================
	//If FlipX/Y true, invert R/G channels scrUncompressedImageScratch only if Normal Map, section
	if ((ps.data->FlipX || ps.data->FlipY) && (ps.data->TextureTypeIndex==TextureTypeEnum::NORMALMAP))
	{
		FlipXYChannelNormalMap(scrUncompressedImageScratch);
	}
	
	//============================================================================================
	//============================================================================================
	//If MipMaps generate using DirectXTexLib, section
	//Also force mipmaps if setMipLevels on cube maps are specified
	if (DoMipMaps || IsCubeMapWithSetMipLevelOverride())
	{
		//Open temporary imageScratch to save mipmaps
		ScratchImage *scrImageMipMapScratch = new ScratchImage(); 
		
		DWORD mimapFilter = TEX_FILTER_DEFAULT|TEX_FILTER_SEPARATE_ALPHA; 

		//Use the non Windows Imaging Components path if BC6 due to problems in mipmapping
		if (ps.data->encoding_g == DXGI_FORMAT_BC6H_UF16)
			mimapFilter |= TEX_FILTER_FORCE_NON_WIC;

		//Generate MipMaps from scrUncompressedImageScratch and save to scrImageMipMapScratch
		HRESULT hr = GenerateMipMaps(scrUncompressedImageScratch->GetImages(), scrUncompressedImageScratch->GetImageCount(), 
				                     scrUncompressedImageScratch->GetMetadata(), mimapFilter, 0, *scrImageMipMapScratch );
		if( hr != S_OK )
		{
			UserError("Could not create MipMaps");
			return;
		}

		//free previous space
		delete scrUncompressedImageScratch;
		
		//Copy over pointer from ScratchImage which has MipMap, now scrUncompressedImageScratch holds the initial image + mip chain
		scrUncompressedImageScratch = scrImageMipMapScratch;

		
		//MipMap from Layers
		//If we should get the mip maps from layers then override the existing mip maps with the layer images
		if (IsMipMapsDefinedByLayer())
		{
			//Copy all Layers starting 1. We omit 0 becasue we already copied it above.
			CopyLayersIntoMipMaps(scrUncompressedImageScratch, hasAlpha, 1);
		}	
	}


	// If Normalization Postprocess option has been specified and this is a Normal Type texture type
	// Normalize all mip chain
	if (ps.data->Normalize && ps.data->TextureTypeIndex == TextureTypeEnum::NORMALMAP)
		NormalizeNormalMapChain(scrUncompressedImageScratch);


	// Compress scrUncompressedImageScratch into scrImageScratch, section
	ScratchImage* scrImageScratch = new ScratchImage();

	ps.data->previewing = false;
	if (!CompressToScratchImage(&scrImageScratch, &scrUncompressedImageScratch, hasAlpha))
		return;

	// Save compressed DirectXTex image structure to file, section
	Blob blob;
	HRESULT hr;

	// Save compressed image

	if (IsCubeMapWithSetMipLevelOverride()) 	//If setMipLevel on cubemap specified
		hr = SaveCubeMipLevelToDDSFile(scrImageScratch, blob);
	else
		hr = SaveToDDSMemory(scrImageScratch->GetImages(), scrImageScratch->GetImageCount(), scrImageScratch->GetMetadata(), DDS_FLAGS_NONE, blob);

	if (hr == S_OK)
	{
		if (int size = int(blob.GetBufferSize()))
		{
			if (!WriteFile(reinterpret_cast<HANDLE>(ps.formatRecord->dataFork), blob.GetBufferPointer(), size, reinterpret_cast<LPDWORD>(&size), NULL)) 
				SetResult(writErr);
			else if (size < int(blob.GetBufferSize()))
				SetResult(dskFulErr);
		}
	}
	else
		UserError("Failed to save");


	// Cleanup

	if (scrImageScratch != NULL)
	    delete scrImageScratch;
	
	if (scrUncompressedImageScratch != NULL)
	    delete scrUncompressedImageScratch;
}

bool IntelPlugin::ReadScriptParamsForWrite()
{
	// Populate this array if we're expecting any keys,
	// must be NULLID terminated:
	DescriptorKeyIDArray array = { NULLID };
	
	
	// Assume we want to pop our dialog unless explicitly told not to:
	Boolean				returnValue = true;
	
	auto descParams = ps.formatRecord->descriptorParameters;

	if (HostDescriptorAvailable(descParams, NULL))
	{ // descriptor suite is available, go ahead and open descriptor:
	
		auto reader = descParams->readDescriptorProcs;

		// PIUtilities routine to open descriptor handed to us by host:
		if (PIReadDescriptor token = reader->openReadDescriptorProc(descParams->descriptor, array))
		{ // token was valid, so read keys from it:

			DescriptorKeyID		key = NULLID;	// the next key
			DescriptorTypeID	type = NULLID;	// the type of the key we read
			int32				flags = 0;		// any flags for the key
			while (reader->getKeyProc(token, &key, &type, &flags))
			{ // got a valid key.  Figure out where to put it:
			
				switch (key)
				{ // match a key to its expected type:case keyAmount:

					case keyPreset:
						reader->getStringProc(token, &(ps.data->presetBatchName));
						break;

					// ignore all other cases and classes
					// See PIActions.h and PIUtilities.h for
					// routines and macros for scripting functions.
				
				} // key
				
			} // PIGetKey

			// PIUtilities routine that automatically deallocates,
			// closes, and sets token to NULL:
				
			if (OSErr err = HostCloseReader(descParams, ps.formatRecord->handleProcs, &token))
			{ // an error did occur while we were reading keys:
			
				if (err == errMissingParameter) // missedParamErr == -1715
				{ // missing parameter somewhere.  Walk IDarray to find which one.
				}
				else
				{ // serious error.  Return it as a global result:
					SetResult(err);
				}
					
			} // stickyError
						
		} // didn't have a valid token
		
		// Whether we had a valid token or not, we were given information
		// as to whether to pop our dialog or not.  PIUtilities has a routine
		// to check that and return TRUE if we should pop it, FALSE if not:	
		returnValue = HostPlayDialog(descParams);
	
	} // descriptor suite unavailable
	
	return !!returnValue;
}


OSErr IntelPlugin::WriteScriptParamsForWrite (void)
{
	OSErr						gotErr = writErr;
			
	if (auto descParams = ps.formatRecord->descriptorParameters)
	{
		if (auto writeProcs = descParams->writeDescriptorProcs)
		{
			if (auto token = writeProcs->openWriteDescriptorProc())
			{
				// now write our data
				writeProcs->putStringProc(token, keyPreset, ps.data->presetBatchName);

				sPSHandle->Dispose(descParams->descriptor);
				PIDescriptorHandle	h;
				writeProcs->closeWriteDescriptorProc(token, &h);
				descParams->descriptor = h;

				gotErr = noErr;	// we're ok!
			}
		}
	}
	
	return gotErr;
}

bool IntelPlugin::ReadScriptParamsForRead()
{
	// Populate this array if we're expecting any keys,
	// must be NULLID terminated:
	DescriptorKeyIDArray array = { NULLID };
	
	// Assume we want to pop our dialog unless explicitly told not to:
	Boolean				returnValue = true;
	
	auto descParams = ps.formatRecord->descriptorParameters;

	if (HostDescriptorAvailable(descParams, NULL))
	{ // descriptor suite is available, go ahead and open descriptor:
	
		auto reader = descParams->readDescriptorProcs;

		// PIUtilities routine to open descriptor handed to us by host:
		if (PIReadDescriptor token = reader->openReadDescriptorProc(descParams->descriptor, array))
		{ // token was valid, so read keys from it:

			DescriptorKeyID		key = NULLID;	// the next key
			DescriptorTypeID	type = NULLID;	// the type of the key we read
			int32				flags = 0;		// any flags for the key
			Boolean             mipFlag = false;
			Boolean             alphaFlag = false;
			while (reader->getKeyProc(token, &key, &type, &flags))
			{ // got a valid key.  Figure out where to put it:
			
				switch (key)
				{ // match a key to its expected type:case keyAmount:

					case keyMipMap:
						reader->getBooleanProc(token, &mipFlag);
						ps.data->mipmapBatchAllowed = !!mipFlag; //conve form Boolean to bool
						break;
					case keyAlphaS:
						reader->getBooleanProc(token, &alphaFlag);
						ps.data->alphaBatchSeperate = !!alphaFlag; //conve form Boolean to bool
						break;

					// ignore all other cases and classes
					// See PIActions.h and PIUtilities.h for
					// routines and macros for scripting functions.
				
				} // key
				
			} // PIGetKey

			// PIUtilities routine that automatically deallocates,
			// closes, and sets token to NULL:
				
			if (OSErr err = HostCloseReader(descParams, ps.formatRecord->handleProcs, &token))
			{ // an error did occur while we were reading keys:
			
				if (err == errMissingParameter) // missedParamErr == -1715
				{ // missing parameter somewhere.  Walk IDarray to find which one.
				}
				else
				{ // serious error.  Return it as a global result:
					SetResult(err);
				}
					
			} // stickyError
						
		} // didn't have a valid token
		
		// Whether we had a valid token or not, we were given information
		// as to whether to pop our dialog or not.  PIUtilities has a routine
		// to check that and return TRUE if we should pop it, FALSE if not:	
		returnValue = HostPlayDialog(descParams);
	
	} // descriptor suite unavailable
	
	return !!returnValue;

	//auto descParams = ps.formatRecord->descriptorParameters;

	//return descParams->playInfo == plugInDialogDisplay;
}

OSErr IntelPlugin::WriteScriptParamsForRead ()
{
	OSErr						gotErr = writErr;
			
	if (auto descParams = ps.formatRecord->descriptorParameters)
	{
		if (auto writeProcs = descParams->writeDescriptorProcs)
		{
			if (auto token = writeProcs->openWriteDescriptorProc())
			{
				// now write our data
				Boolean temp = ps.data->mipmapBatchAllowed;
				writeProcs->putBooleanProc(token, keyMipMap, temp);
				temp = ps.data->alphaBatchSeperate;
				writeProcs->putBooleanProc(token, keyAlphaS, temp);

				sPSHandle->Dispose(descParams->descriptor);
				PIDescriptorHandle	h;
				writeProcs->closeWriteDescriptorProc(token, &h);
				descParams->descriptor = h;

				gotErr = noErr;	// we're ok!
			}
		}
	}
	
	return gotErr;
}

void IntelPlugin::DoFilterFile (void)
{
	// File can be tested for validity here
	//SetResult(formatCannotRead);
	SetResult(noErr);
}

void IntelPlugin::DoReadPrepare()
{
	ps.formatRecord->maxData = 0;
	loadInfo.isCubeMap = false;
	loadInfo.readImagePtr = NULL;
	loadInfo.loadMipMapIndex = 0;
	loadInfo.hasMips = false;
	loadInfo.hasAlpha = false;
}

void IntelPlugin::DoReadStart()
{
	LARGE_INTEGER fileSize;
	
	showLoadingCursor();

	//Read any descriptor values for scripting support and return if we are in batch mode.
	ps.data->queryForParameters = ReadScriptParamsForRead();

	//Rewind to start of file
	OSErr err= PSSDKSetFPos (ps.formatRecord->dataFork, fsFromStart, 0);
	if (err != noErr) 
	{
		SetResult(err);
		showNormalCursor();
		return;
	}

	//Get filesize
	GetFileSizeEx(reinterpret_cast<HANDLE>(ps.formatRecord->dataFork), &fileSize);

	//Allocate buffer equal filesize
	uint8 *wholeFileBuffer = new uint8[size_t(fileSize.QuadPart)];
	int32 readCount = int32(fileSize.QuadPart);
	
	//ReadIn whole file
	err = PSSDKRead (ps.formatRecord->dataFork, &readCount, wholeFileBuffer);
	if (err != noErr) 
	{
		SetResult(err);
		showNormalCursor();
		return;
	}

	//If not enough bytes read, error
	if (err == noErr && readCount != fileSize.QuadPart)
	{
		SetResult(eofErr);
		showNormalCursor();
		return;
	}

	//Load DDS from memory
	ScratchImage *ddsCompressedImage = new ScratchImage;
	TexMetadata  readImageInfo;

	HRESULT hr = LoadFromDDSMemory( wholeFileBuffer, readCount, DDS_FLAGS_NONE, &readImageInfo, *ddsCompressedImage);

	//Check if the image suports alpha. Note For DX, BC1 supports alpha for us not
	bool alphaIsWhite = ddsCompressedImage->IsAlphaAllOpaque();
	bool compressedImageHasAlpha = (DirectX::HasAlpha(readImageInfo.format) &&  !alphaIsWhite &&
		                            readImageInfo.format != DXGI_FORMAT_BC1_UNORM && 
		                            readImageInfo.format != DXGI_FORMAT_BC1_UNORM_SRGB); 
	loadInfo.hasAlpha = compressedImageHasAlpha;
	
	//Check if there are mip maps
	bool compressedImageHasMipMaps = (readImageInfo.mipLevels > 1) ? true : false;

	bool separateAlphaChannel =  ps.data->alphaBatchSeperate; //get predefined values for batching from descriptors in ReadScriptParamsForRead()
	bool loadDDSMipMaps       =  ps.data->mipmapBatchAllowed; //get predefined values for batching from descriptors in ReadScriptParamsForRead
	bool compressedImageIsCubemap = (readImageInfo.arraySize == 6) ? true : false;

	//For cube maps disable alpha and mip maps for now
	if (compressedImageIsCubemap)
	{
		compressedImageHasAlpha = compressedImageHasMipMaps = false;
		separateAlphaChannel = false;
	}

	//Show load dialog
	//Do we need the user to make a selection regarding alpha or mip maps?
	if (compressedImageHasAlpha || compressedImageHasMipMaps)
	{
		//ask user if he wants them, and not in batch
		if (ps.data->queryForParameters)
		{
			unsigned8 loadDialogResult = ShowLoadDialog (compressedImageHasAlpha, compressedImageHasMipMaps, GetActiveWindow());
			
			//decode result 
			separateAlphaChannel = (loadDialogResult & LoadInfoEnum::USE_SEPARATEALPHA) ? true : false;
			loadDDSMipMaps = (loadDialogResult & LoadInfoEnum::USE_MIPMAPS) ? true : false; 

			//store descriptor values for scripting. Actual write happens in DoReadFinish()
			ps.data->alphaBatchSeperate = separateAlphaChannel;
			ps.data->mipmapBatchAllowed = loadDDSMipMaps;
		}
	}

	//init to no layers
	ps.formatRecord->layerData = 0; 

	//Do we have mip maps, is this a cube map, or a simple image
	if (compressedImageIsCubemap)
	{
		//Cube map
		loadInfo.isCubeMap = true;
		ps.formatRecord->layerData = 6; //specify the creatoin of six layers
	}
	else if (compressedImageHasMipMaps)
	{
		//Image has mip maps
		if (loadDDSMipMaps)
		{
			loadInfo.hasMips = true;
			ps.formatRecord->layerData = uint32(readImageInfo.mipLevels); //specify creation of layers
		}
	}

	//By default we use layer transparency when the image has alpha, but when the separatealpha flag 
	//is set we use the background image with a dedicated alpha channel
	if (!separateAlphaChannel && compressedImageHasAlpha && ps.formatRecord->layerData == 0)
	{
		ps.formatRecord->layerData = 1;
	}

	// pick a target format
	auto targetFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

	if (readImageInfo.format == DXGI_FORMAT_BC6H_SF16 || readImageInfo.format == DXGI_FORMAT_BC6H_UF16)	// 16 bit short floats
		targetFormat = DXGI_FORMAT_R32G32B32A32_FLOAT;
	else if (BitsPerColor(readImageInfo.format) > 16)
		targetFormat = DXGI_FORMAT_R32G32B32A32_FLOAT;
	else if (BitsPerColor(readImageInfo.format) > 8)
		targetFormat = DXGI_FORMAT_R16G16B16A16_UNORM;
	else if (IsSRGB(readImageInfo.format))
		targetFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;


	// Is it already in the correct format?
	if (readImageInfo.format == targetFormat)
	{
		// No conversion required, just swap pointers
		loadInfo.readImagePtr = ddsCompressedImage;
		ddsCompressedImage = NULL;
	}
	else 
	{
		loadInfo.readImagePtr = new ScratchImage();
		if (IsCompressed(readImageInfo.format))	// is it compressed?
			hr = Decompress( ddsCompressedImage->GetImages(), ddsCompressedImage->GetImageCount(), ddsCompressedImage->GetMetadata(), targetFormat, *loadInfo.readImagePtr);
		else
			hr = Convert( ddsCompressedImage->GetImages(), ddsCompressedImage->GetImageCount(), ddsCompressedImage->GetMetadata(), targetFormat, 0, 0, *loadInfo.readImagePtr);
	}
	
	if ( FAILED(hr) )
	{
		UserError("Can not load image.");
		delete loadInfo.readImagePtr;
		delete ddsCompressedImage;
		delete[] wholeFileBuffer;
		showNormalCursor();
		return;
	}

	// update the metadata
	readImageInfo = loadInfo.readImagePtr->GetMetadata();

	//Setup formatRecord for loading
	ps.formatRecord->imageRsrcData = NULL;
	ps.formatRecord->imageRsrcSize = 0;
	ps.formatRecord->imageMode = plugInModeRGBColor;
	if (ps.formatRecord->HostSupports32BitCoordinates && 
		ps.formatRecord->PluginUsing32BitCoordinates)
	{
		ps.formatRecord->imageSize32.v = static_cast<int16>(readImageInfo.height);
	    ps.formatRecord->imageSize32.h = static_cast<int16>(readImageInfo.width);
	}
	else
	{
		ps.formatRecord->imageSize.v = static_cast<int16>(readImageInfo.height);
	    ps.formatRecord->imageSize.h = static_cast<int16>(readImageInfo.width);
	}
	ps.formatRecord->depth = int(BitsPerColor(targetFormat));

	if (separateAlphaChannel)
	{
		ps.formatRecord->planes = compressedImageHasAlpha ? 4 : 3;	// 4 channels for alpha
		ps.formatRecord->transparencyPlane = 3;
	}
	else
	{
		ps.formatRecord->planes = (ps.formatRecord->layerData > 0) ? 4 : 3;	// layers have alpha, background not so much
		ps.formatRecord->transparencyPlane = 3;
	}

	ps.formatRecord->transparencyMatting = 0;

	
	//Cleanup
	delete[] wholeFileBuffer;

	if (ddsCompressedImage)
		delete ddsCompressedImage;
}

void IntelPlugin::DoReadContinue()
{

	//Prepare formatRecord to get whole image
	ps.formatRecord->loPlane = 0;
	ps.formatRecord->hiPlane = ps.formatRecord->planes - 1;
	
	ps.formatRecord->theRect.left = 0;
	ps.formatRecord->theRect.right = ps.formatRecord->imageSize.h;
	ps.formatRecord->theRect.top = 0;
	ps.formatRecord->theRect.bottom = ps.formatRecord->imageSize.v;
	
	//The offset in BYTES between planes of data in the buffers, for 8 bit interleved data this is 1. Doing this operation computes correctly for depth 16 bit, 32 bit etc 
	ps.formatRecord->planeBytes = (ps.formatRecord->depth + 7) >> 3;
	//The offset in bytes between columns of data in the buffer. usually 1 for non-interleaved data, or hiPlane-loPlane+1 for interleaved data. 
	ps.formatRecord->colBytes = (ps.formatRecord->hiPlane - ps.formatRecord->loPlane + 1) * ps.formatRecord->planeBytes;
	//The offset in bytes between rows of data in the buffer.
	ps.formatRecord->rowBytes = ps.formatRecord->colBytes * (ps.formatRecord->theRect.right - ps.formatRecord->theRect.left);
	

	//Allocate buffer for ->data field
	//seems we have to allocate for ourselves here because we set maxData to 0
	uint32 bufferSize =	(ps.formatRecord->theRect.bottom - ps.formatRecord->theRect.top) * ps.formatRecord->rowBytes;
	Ptr pixelData = sPSBuffer->New( &bufferSize, bufferSize );

	if (pixelData == NULL)
	{
		SetResult(memFullErr);
		return;
	}
	else
	{
		memset(pixelData, 0, bufferSize); //init buffer to 0
		//Assign buffer to photoshop buffer
		ps.formatRecord->data = pixelData;
	}
	

	//Get pointer to  image
	const Image *img; 

	//Get the right image for this iteration (this function is called multiple times in case of mips or cube map)
	if (loadInfo.isCubeMap)
	{
		//Cube map load six images as layers
		img = loadInfo.readImagePtr->GetImage(0, loadInfo.loadMipMapIndex, 0);
		loadInfo.loadMipMapIndex++;
	}
	else if (loadInfo.hasMips)
	{
		//Load image maps as layers
		img = loadInfo.readImagePtr->GetImage(loadInfo.loadMipMapIndex, 0, 0);
		loadInfo.loadMipMapIndex++;
	}
	else
	{
		//Normal single mip image
		img= loadInfo.readImagePtr->GetImages();
	}
	

	//Now copy image into photoshop buffer
	unsigned8 *rowSrcDataPtr = img->pixels;
	unsigned8 *rowTgtDataPtr = reinterpret_cast<unsigned8 *>(pixelData);
	
	for (size_t y=0; y<img->height; y++)
	{
		for (size_t x=0; x<img->width; x++)
		{

			switch (img->format)
			{
			case DXGI_FORMAT_R8G8B8A8_UNORM:
			case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
				{
					unsigned8 *src = rowSrcDataPtr + y * img->rowPitch + x * 4; 
					unsigned8 *dst = rowTgtDataPtr + y * ps.formatRecord->rowBytes + x * ps.formatRecord->colBytes;
					for (int p=0; p<ps.formatRecord->planes; p++)
					{
						dst[p] = src[p];

						//If we are loading a cube map face then force alpha to white
						if (loadInfo.isCubeMap && loadInfo.hasAlpha)
							dst[ps.formatRecord->planes-1] = 255;
					}
				} break;

			case DXGI_FORMAT_R16G16B16A16_UNORM:	
				{
					const uint16 *src = reinterpret_cast<const uint16*>(rowSrcDataPtr + y * img->rowPitch + x * 8); 
					uint16 *dst = reinterpret_cast<uint16*>(rowTgtDataPtr + y * ps.formatRecord->rowBytes + x * ps.formatRecord->colBytes);
					for (int p=0; p<ps.formatRecord->planes; p++)
					{
						dst[p] = src[p];

						//If we are loading a cube map face then force alpha to white
						if (loadInfo.isCubeMap && loadInfo.hasAlpha)
							dst[ps.formatRecord->planes-1] = ConvertTo16Bit(static_cast<unsigned8>(255));
					}
				} break;

			case DXGI_FORMAT_R32G32B32A32_FLOAT:	// any hdr should be decompressed to this format if possible
				{
					const float *src = reinterpret_cast<const float*>(rowSrcDataPtr + y * img->rowPitch + x * 16); 
					float *dst = reinterpret_cast<float*>(rowTgtDataPtr + y * ps.formatRecord->rowBytes + x * ps.formatRecord->colBytes);
					for (int p=0; p<ps.formatRecord->planes; p++)
					{
						dst[p] = src[p];

						//If we are loading a cube map face then force alpha to white
						if (loadInfo.isCubeMap && loadInfo.hasAlpha)
							dst[ps.formatRecord->planes-1] = 1.0f;
					}
				} break;

			default:
				// not recognized
				break;
			}
		}
	}

	SetResult(ps.formatRecord->advanceState());
	

	//Cleanup
	ps.formatRecord->data = NULL;
	sPSBuffer->Dispose(&pixelData);
}

void IntelPlugin::DoReadFinish()
{
	delete loadInfo.readImagePtr;
	loadInfo.readImagePtr = NULL;
	WriteScriptParamsForRead();
	showNormalCursor();
}

void IntelPlugin::DoReadLayerStart()
{
	static uint16  gLayerNameUtf16[10];
	char           gLayerNameAscii[10];
	int i=0;

	//Assign layer names
	if (loadInfo.hasMips)
	{
		//Create mip layer name
		sprintf(gLayerNameAscii,"Mip%d", loadInfo.loadMipMapIndex);
		
		do
		{
			gLayerNameUtf16[i] = gLayerNameAscii[i];
			i++;
		} while (gLayerNameAscii[i] != '\0');
		gLayerNameUtf16[i] = '\0';
							
		ps.formatRecord->layerName = gLayerNameUtf16;
	}
	else if (loadInfo.isCubeMap)
	{
		char* facesmap[] = {"+X","-X","+Y","-Y","+Z","-Z"};

		sprintf(gLayerNameAscii,"%s", facesmap[loadInfo.loadMipMapIndex]);
		
		do
		{
			gLayerNameUtf16[i] = gLayerNameAscii[i];
			i++;
		} while (gLayerNameAscii[i] != '\0');
		gLayerNameUtf16[i] = '\0';
							
		ps.formatRecord->layerName = gLayerNameUtf16;
	}
	else
	{
		//ps.formatRecord->layerName = L"Layer 0";
	}
}

//-------------------------------------------------------------------------------
//
// CreateDataHandle
//
// Create a handle to our Data structure. Photoshop will take ownership of this
// handle and delete it when necessary.
//-------------------------------------------------------------------------------
void IntelPlugin::CreateDataHandle(void)
{
	Handle h = sPSHandle->New(sizeof(Globals));
	if (h != NULL)
		*ps.dataPtr = reinterpret_cast<intptr_t>(h);
	else
		*ps.resultPtr = memFullErr;
}

//-------------------------------------------------------------------------------
//
// LockHandles
//
// Lock the handles and get the pointers for data
// Set the global error, *ps.resultPtr, if there is trouble
//
//-------------------------------------------------------------------------------
void IntelPlugin::LockHandles(void)
{
	if ( ! (*ps.dataPtr) )
	{
		*ps.resultPtr = formatBadParameters;
		return;
	}
	
	Boolean oldLock = FALSE;
	sPSHandle->SetLock(reinterpret_cast<Handle>(*ps.dataPtr), true, 
		               reinterpret_cast<Ptr *>(&ps.data), &oldLock);
	
	if (ps.data == NULL)
	{
		*ps.resultPtr = memFullErr;
		return;
	}
}

//-------------------------------------------------------------------------------
//
// UnlockHandles
//
// Unlock the handles used by the data and params pointers
//
//-------------------------------------------------------------------------------
void IntelPlugin::UnlockHandles(void)
{
	Boolean oldLock = FALSE;
	if (*ps.dataPtr)
		sPSHandle->SetLock(reinterpret_cast<Handle>(*ps.dataPtr), false, 
		                   reinterpret_cast<Ptr *>(&ps.data), &oldLock);
}



void IntelPlugin::PluginMain(const int16 selector,
						             FormatRecordPtr formatRecord,
						             intptr_t * data,
						             int16 * result)
{
	//---------------------------------------------------------------------------
	//	Store persistent data
	//---------------------------------------------------------------------------

	ps.formatRecord = formatRecord;
	ps.pluginRef = reinterpret_cast<SPPluginRef>(formatRecord->plugInRef);
	ps.resultPtr = result;
	ps.dataPtr = data;

	//---------------------------------------------------------------------------
	//	(2) Check for about box request.
	//
	// 	The about box is a special request; the parameter block is not filled
	// 	out, none of the callbacks or standard data is available.  Instead,
	// 	the parameter block points to an AboutRecord, which is used
	// 	on Windows.
	//---------------------------------------------------------------------------
	if (selector == formatSelectorAbout)
	{
		AboutRecordPtr aboutRecord = reinterpret_cast<AboutRecordPtr>(formatRecord);
		sSPBasic = aboutRecord->sSPBasic;
		ShowAboutIntel(aboutRecord);
	}
	else
	{ // do the rest of the process as normal:

		sSPBasic = ps.formatRecord->sSPBasic;

		if (formatRecord->advanceState == NULL)
		{
			*ps.resultPtr = errPlugInHostInsufficient;
			return;
		}

		// new for Photoshop 8, big documents, rows and columns are now > 30000 pixels
		//if (formatRecord->HostSupports32BitCoordinates)
		//	formatRecord->PluginUsing32BitCoordinates = true;


		//-----------------------------------------------------------------------
		//	(3) Allocate and initalize ps.data.
		//
		//-----------------------------------------------------------------------

 		if ( ! (*ps.dataPtr) )
		{
			CreateDataHandle();
			if (*ps.resultPtr != noErr) return;
			LockHandles();
			if (*ps.resultPtr != noErr) return;
			InitData();
		}

		if (*ps.resultPtr == noErr)
		{
			LockHandles();
			if (*ps.resultPtr != noErr) return;
		}


		//-----------------------------------------------------------------------
		//	(4) Dispatch selector.
		//-----------------------------------------------------------------------
		switch (selector)
		{
			case formatSelectorReadPrepare:
				DoReadPrepare();
				break;
			case formatSelectorReadStart:
				DoReadStart();
				break;
			case formatSelectorReadContinue:
				DoReadContinue(); //This is run when no layers are set
				break;
			case formatSelectorReadFinish:
				DoReadFinish();
				break;

			case formatSelectorOptionsPrepare:
				ps.formatRecord->maxData = 0;
				break;
			case formatSelectorOptionsStart:
				ps.formatRecord->data = NULL;
				break;
			case formatSelectorOptionsContinue:
				break;
			case formatSelectorOptionsFinish:
				break;

				// estimations
			case formatSelectorEstimatePrepare:
				ps.formatRecord->maxData = 0;
				break;
			case formatSelectorEstimateStart:
				{
					int dataBytes = ps.formatRecord->imageSize32.h * ps.formatRecord->imageSize32.v;
					formatRecord->minDataBytes = dataBytes >> 1;
					formatRecord->maxDataBytes = dataBytes * 4;
					formatRecord->data = NULL;
				}
				break;
			case formatSelectorEstimateContinue:
				break;
			case formatSelectorEstimateFinish:
				break;

			case formatSelectorWritePrepare:
				DoWritePrepare();
				break;
			case formatSelectorWriteStart:
				DoWriteStart();
				break;
			case formatSelectorWriteContinue:
				DoWriteContinue();
				break;
			case formatSelectorWriteFinish:
				DoWriteFinish();
				break;

			case formatSelectorFilterFile:
				DoFilterFile();
				break;

			case formatSelectorReadLayerStart:
				DoReadLayerStart();
			break;
			case formatSelectorReadLayerContinue:
				DoReadContinue(); //this is run when layers are set
			break;
            case formatSelectorReadLayerFinish:
			break;

			case formatSelectorWriteLayerStart:
			break;
            case formatSelectorWriteLayerContinue:
			break;
            case formatSelectorWriteLayerFinish:
			break;
		}
			
		//-----------------------------------------------------------------------
		//	(5) Unlock data, and exit resource.
		//
		//	Result is automatically returned in *result, which is
		//	pointed to by ps.resultPtr.
		//-----------------------------------------------------------------------	
		
		UnlockHandles();
	
	} // about selector special		

	// release any suites that we may have acquired
	if (selector == formatSelectorAbout ||
		selector == formatSelectorWriteFinish ||
		selector == formatSelectorReadFinish ||
		selector == formatSelectorOptionsFinish ||
		selector == formatSelectorEstimateFinish ||
		selector == formatSelectorFilterFile ||
		*ps.resultPtr != noErr)
	{
		PIUSuitesRelease();
	}

}


DLLExport MACPASCAL void PluginMain (const int16 selector,
						             FormatRecordPtr formatRecord,
						             intptr_t * data,
						             int16 * result)
{
	try 
	{ 
		IntelPlugin::GetInstance().PluginMain(selector, formatRecord, data, result);
	}
	catch(...)
	{
		if (NULL != result)
			*result = -1;
	}

} // end PluginMain


//Fill ps.formatRecord->data with the mergedLayers.
void IntelPlugin::FillFromCompositedLayers()
{
	ReadChannelDesc *pChannel;
	char *pLayerData;
	
	int planesToGet_ = ps.formatRecord->hiPlane - ps.formatRecord->loPlane + 1;

	// Get a buffer to hold each channel as we process, formatRecord->planeBytes are computed the first time fetchImage() is called
	pLayerData = sPSBuffer->New(NULL, ps.formatRecord->imageSize.h * ps.formatRecord->imageSize.v * ps.formatRecord->planeBytes);

	if (pLayerData == NULL)
	{
		SetResult(memFullErr);
		return;
	}
		
	//Get the composite channel in this channel list 
    pChannel = ps.formatRecord->documentInfo->mergedCompositeChannels;

	int planeNumber = 0;

	//Copy the RGB channels
	while (pChannel != NULL)
	{
		//Get pixel data from channel
		ReadLayerData(pChannel, pLayerData, ps.formatRecord->imageSize.h, ps.formatRecord->imageSize.v);

		///Copy int data rgb
		for (size_t y = 0; y < ps.formatRecord->imageSize.v; y++)
		{
			for (size_t x = 0; x < ps.formatRecord->imageSize.h; x++)
			{
				int indexToImage = static_cast<int>(ps.formatRecord->imageSize.h*y*planesToGet_ + x*planesToGet_);  //the ScratchImage is always RGBA therefore pitch of 4
				int indexToLayerChannel = static_cast<int>(ps.formatRecord->imageSize.h*y + x);  //here the pitch is only 1
				
				if (ps.formatRecord->depth == 8)
				{
					unsigned8 *rowBigDataPtr = (unsigned8 *)ps.formatRecord->data;
					unsigned8 *rowData = reinterpret_cast<unsigned8 *>(pLayerData); //Get image pointer
					rowBigDataPtr[indexToImage+planeNumber] = rowData[indexToLayerChannel];
				}
				else if (ps.formatRecord->depth == 16)
				{
					unsigned16 *rowBigDataPtr = (unsigned16 *)ps.formatRecord->data;
					unsigned16 *rowData = reinterpret_cast<unsigned16 *>(pLayerData); //Get image pointer
					rowBigDataPtr[indexToImage+planeNumber] = rowData[indexToLayerChannel];
				}
				else if (ps.formatRecord->depth == 32)
				{
					
					float *rowBigDataPtr = (float *)ps.formatRecord->data;
					float *rowData = reinterpret_cast<float *>(pLayerData); //Get image pointer
					rowBigDataPtr[indexToImage+planeNumber] = rowData[indexToLayerChannel];
				}
			}
		}

		pChannel = pChannel->next;
		planeNumber++;
	}

	
	//Get first alpha channel and set as alpha, of not get transparency planes
	if (!(pChannel = ps.formatRecord->documentInfo->alphaChannels))
	{
		pChannel = ps.formatRecord->documentInfo->mergedTransparency;
	}
	
	//Copy alpha
	if ((ps.formatRecord->planes > 3) && (pChannel != NULL))
	{
		//Get pixel data from channel
		ReadLayerData(pChannel, pLayerData, ps.formatRecord->imageSize.h, ps.formatRecord->imageSize.v);

		//Copy into data alpha
		for (size_t y = 0; y < ps.formatRecord->imageSize.v; y++)
		{
			for (size_t x = 0; x < ps.formatRecord->imageSize.h; x++)
			{
				int indexToImage = static_cast<int>(ps.formatRecord->imageSize.h*y*planesToGet_ + x*planesToGet_);  //the ScratchImage is always RGBA therefore pitch of 4
				int indexToLayerChannel = static_cast<int>(ps.formatRecord->imageSize.h*y + x);  //here the pitch is only 1
				
				//Here the planeumber is 3 for alpha
				if (ps.formatRecord->depth == 8)
				{
					unsigned8 *rowBigDataPtr = (unsigned8 *)ps.formatRecord->data;
					unsigned8 *rowData = reinterpret_cast<unsigned8 *>(pLayerData); //Get image pointer
					rowBigDataPtr[indexToImage+3] = rowData[indexToLayerChannel];
				}
				else if (ps.formatRecord->depth == 16)
				{
					unsigned16 *rowBigDataPtr = (unsigned16 *)ps.formatRecord->data;
					unsigned16 *rowData = reinterpret_cast<unsigned16 *>(pLayerData); //Get image pointer
					rowBigDataPtr[indexToImage+3] = rowData[indexToLayerChannel];
				}
				else if (ps.formatRecord->depth == 32)
				{
					
					float *rowBigDataPtr = (float *)ps.formatRecord->data;
					float *rowData = reinterpret_cast<float *>(pLayerData); //Get image pointer
					rowBigDataPtr[indexToImage+3] = rowData[indexToLayerChannel];
				}
			}
		}
	}
		
			
	sPSBuffer->Dispose(static_cast<char**>(&pLayerData));	
}