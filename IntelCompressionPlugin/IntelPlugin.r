// ADOBE SYSTEMS INCORPORATED
// Copyright  1993 - 2002 Adobe Systems Incorporated
// All Rights Reserved
//
// NOTICE:  Adobe permits you to use, modify, and distribute this
// file in accordance with the terms of the Adobe license agreement
// accompanying it.  If you have received this file from a source
// other than Adobe, then your use, modification, or distribution
// of it requires the prior written permission of Adobe.
//-------------------------------------------------------------------
//-------------------------------------------------------------------------------
//
//	File:
//		Outbound.r
//
//	Copyright 1990-1992, Thomas Knoll.
//	All Rights Reserved.
//
//	Description:
//		This file contains the resource information
//		for the Export module Outbound, a module that
//		creates a file and stores raw pixel data in it.
//
//	Use:
//		This module shows how to export raw data to a file.
//		It uses a simple "FileUtilities" library that comes
//		with the SDK.  You use it via File>>Export>>Outbound.
//
//-------------------------------------------------------------------------------

#include "IntelPluginName.h"

//-------------------------------------------------------------------------------
//	Definitions -- Required by include files.
//-------------------------------------------------------------------------------

// The About box and resources are created in PIUtilities.r.
// You can easily override them, if you like.

#define plugInName			DDSExporterPluginName
#define plugInCopyrightYear	"2015"
#define plugInDescription \
	"A DDS Export plug-in for Adobe Photoshop¨."

//-------------------------------------------------------------------------------
//	Definitions -- Required by other resources in this rez file.
//-------------------------------------------------------------------------------

// Dictionary (aete) resources:

#define vendorName				DDSExporterPluginName
#define plugInAETEComment		"history export plug-in"

#define plugInSuiteID			'sdK5'
#define plugInClassID			'ddsX'
#define plugInEventID			typeNull // must be this

//-------------------------------------------------------------------------------
//	Set up included files for Macintosh and Windows.
//-------------------------------------------------------------------------------

#include "PIDefines.h"

#ifdef __PIMac__
	#include "PIGeneral.r"
	#include "PIUtilities.r"
#elif defined(__PIWin__)
	#define Rez
	#include "PIGeneral.h"
	#include "PIUtilities.r"
#endif

#include "PITerminology.h"
#include "PIActions.h"
	

//-------------------------------------------------------------------------------
//	PiPL resource
//-------------------------------------------------------------------------------

resource 'PiPL' (ResourceID, plugInName " PiPL", purgeable)
{
    {
		Kind { ImageFormat },
		Name { plugInName },
		Category { ".."vendorName },
		Version { (latestFormatVersion << 16) | latestFormatSubVersion },

		#ifdef __PIMac__
			#if (defined(__x86_64__))
				CodeMacIntel64 { "PluginMain" },
			#endif
			#if (defined(__i386__))
				CodeMacIntel32 { "PluginMain" },
			#endif
		#else
			#if defined(_WIN64)
				CodeWin64X86 { "PluginMain" },
			#else
				CodeWin32X86 { "PluginMain" },
			#endif
		#endif

		// ClassID, eventID, aete ID, uniqueString:
		HasTerminology { plugInClassID, 
		                 plugInEventID, 
						 ResourceID, 
						 vendorName " " plugInName },
		
		SupportedModes
		{
			noBitmap, doesSupportGrayScale,
			noIndexedColor, doesSupportRGBColor,
			noCMYKColor, noHSLColor,
			noHSBColor, doesSupportMultichannel,
			noDuotone, noLABColor
		},
		
		EnableInfo
		{
			"in (PSHOP_ImageMode, GrayScaleMode, RGBMode, MultichannelMode) || PSHOP_ImageDepth == 16 || PSHOP_ImageDepth == 32"
		},


		// New for Photoshop 8, document sizes that are really big 
		// 32 bit row and columns, 2,000,000 current limit but we can handle more
		PlugInMaxSize { 32767, 32767 },

		// For older Photoshops that only support 30000 pixel documents, 
		// 16 bit row and columns
		FormatMaxSize { { 32767, 32767 } },

		FormatMaxChannels { {   1, 24, 24, 24, 24, 24, 
							   24, 24, 24, 24, 24, 24 } },
	
		FmtFileType { 'DDS ', 'DDSX' },
		//ReadTypes { { 'DDSX', '    ' } },
		FilteredTypes { { 'DDSX', '    ' } },
		ReadExtensions { { 'DDS ' } },
		WriteExtensions { { 'DDS ' } },
		FilteredExtensions { { 'DDS ' } },
		FormatFlags { fmtDoesNotSaveImageResources, 
		              fmtCanRead, 
					  fmtCanWrite, 
					  fmtCanWriteIfRead, 
					  fmtCanWriteTransparency, 
					  fmtCanCreateThumbnail },
		FormatICCFlags { iccCannotEmbedGray,
						 iccCannotEmbedIndexed,
						 iccCannotEmbedRGB,
						 iccCannotEmbedCMYK },
		FormatLayerSupport { doesSupportFormatLayers }
		}
	};


//-------------------------------------------------------------------------------

//-------------------------------------------------------------------------------
//	Dictionary (scripting) resource
//-------------------------------------------------------------------------------

resource 'aete' (ResourceID, plugInName " dictionary", purgeable)
{
	1, 0, english, roman,									/* aete version and language specifiers */
	{
		vendorName,											/* vendor suite name */
		DDSExporterPluginName,	    	  		            /* optional description */
		plugInSuiteID,										/* suite ID */
		1,													/* suite code, must be 1 */
		1,													/* suite level, must be 1 */
		{},													/* structure for filters */
		{													/* non-filter plug-in class here */
			vendorName " " plugInName,						/* unique class name */
			plugInClassID,										/* class ID, must be unique or Suite ID */
			plugInAETEComment,								/* optional description */
			{												/* define inheritance */
				"<Inheritance>",							/* must be exactly this */
				keyInherits,								/* must be keyInherits */
				classExport,								/* parent: Format, Import, Export */
				"parent class export",						/* optional description */
				flagsSingleProperty,						/* if properties, list below */
				
				"preset",									/* our path */
				keyPreset,									/* common key */
				typeChar,						            /* preset namecorrect path for platform */
				"Preset name",								/* optional description */
				flagsSingleProperty,

				"mipmap",									/* mipmaps on/off */
				keyMipMap,									/* common key */
				typeBoolean,					            /* basic value type */
				"MipMaps enabled",							/* optional description */
				flagsSingleProperty,

				"alphaseprate",								/* use dedicated alpha channel for transp */
				keyAlphaS,									/* common key */
				typeBoolean,					            /* basic value type */
				"Alpha seperate",							/* optional description */
				flagsSingleProperty,
				
				/* no more properties */
			},
			{}, /* elements (not supported) */
			/* class descriptions */
		},
		{}, /* comparison ops (not supported) */
		{}	/* any enumerations */
	}
};

//-------------------------------------------------------------------------------
//	Resource text entries
//
//	Entering these as separate resources because then they
//	transfer directly over to windows via CNVTPIPL.
//
//	If I use a Macintosh 'STR#' resource, I could put all these
//	strings into a single resource, but there is no
//	parallel on Windows.  'STR ' resources, which hold
//	one string per ID, exist on Windows and Macintosh.
//-------------------------------------------------------------------------------

// Prompt string:
resource StringResource (kPrompt, plugInName " Prompt", purgeable)
{
	"DDS export to file:"
};

// Creator and type:
resource StringResource (kCreatorAndType, plugInName " CreatorAndType", purgeable)
{
	"Copyright © 2015, Intel Corporation. All rights reserved."		// creator
	"????"		// type

};

//-------------------------------------------------------------------------------

// end Outbound.r
