/////////////////////////////////////////////////////////////////////////////////////////////
// Copyright 2017 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");// you may not use this file except in compliance with the License.// You may obtain a copy of the License at//// http://www.apache.org/licenses/LICENSE-2.0//// Unless required by applicable law or agreed to in writing, software// distributed under the License is distributed on an "AS IS" BASIS,// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.// See the License for the specific language governing permissions and// limitations under the License.
/////////////////////////////////////////////////////////////////////////////////////////////

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

