/*

  Copyright (c) 2015, Intel Corporation
  All rights reserved.

*/

#pragma once

//Change this define to rename the plugin
//It changes the pipl photoshop resource and also the window UI on runtime.

//Be sure also to change the TargetName for the DDSExporter Project in "ConfigurationProperties->General->TargetName" (left click on DDSExporter),
//both for Win32 and x64 settigns.
#define DDSExporterPluginName "Intel® Texture Works"
#define DDSExporterPluginVersion " v1.0.4"

// Terminology specific to this plug-in.
#define kPrompt				16100
#define kCreatorAndType		kPrompt+1

// scripting keys
#define keyPreset 'pres'
#define keyMipMap 'mipm'
#define keyAlphaS 'alps'

