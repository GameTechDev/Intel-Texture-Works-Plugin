# Intel&reg; Texture Works Plugin for Photoshop*

# Pre-Compiled Binary
## Getting Started (Installation)
1. Close Photoshop
2. Download the IntelTextureWorks_1.0.4.zip file and expand it on your local computer
3. Copy the desired plugin from either of the following unzipped folders
	* .../IntelTextureWorks_1.0.4\Plugins\x64\IntelTextureWorks.8bi
	* .../IntelTextureWorks_1.0.4\Plugins\Win32\IntelTextureWorks.8bi
4. Paste the plugin into the appropriate Photoshop Plugin folder
	* D:\Program Files\Adobe Photoshop CC 2014\Required\Plug-Ins\File Formats
	* D:\Program Files\Adobe\Adobe Photoshop CS6 (64 Bit)\Plug-ins\File Formats
5. Copy the cubemap scripts from:
	* .../IntelTextureWorks_1.0.4\PhotoshopScripts\IntelTextureWorks-ConvertCubeMap.jsx
	* .../IntelTextureWorks_1.0.4\PhotoshopScripts\IntelTextureWorks-CubeMapGaussianBlur.jsx
6. Paste the cubemap scripts into:
	* D:\Program Files\Adobe Photoshop CC 2014\Presets\Scripts

## Saving Files via Plugin
1. File > Save As
2. Select "Save as type" > **Intel&reg; Texture Works (\*.DDS;\*.DDS)**
2. Navigate to store location
3. Assign file name
4. Save
5. Select desired plugin options and preview (pan/zoom), as necessary
6. Ok

## Loading Files Saved via Plugin
Multiple resident DDS plugins can result in a texture display error on load. To avoid this, use the following process to reload textures saved with the Intel&reg; Texture Works plugin for Photoshop

1. File > Open As
2. Select **Intel&reg; Texture Works (\*.DDS;\*.DDS)** as type (to the right of "File name" field)
3. Select file
4. Select desired mipmap loading options if applicable
5. Select desired color profile loading options

## Logging Bugs, Enhancements, & Feedback
Use the [GitHub Issue Tracking System](https://github.com/GameTechDev/Intel-Texture-Works-Plugin/issues) to log your bugs, enhancement (requests), and feedback (general impressions appreciated). **Labels really help here - please use them**.

## NOTE:
* Not all authoring apps can read the latest BCn textures. We're keeping a running list of authoring app BCn load status on the Wiki [here](https://github.com/GameTechDev/Intel-Texture-Works-Plugin/wiki/BCn-App-Support)
* To implement BCn texture compression in your own apps and engines [download the sample source code here](https://software.intel.com/en-us/articles/fast-ispc-texture-compressor-update)
* The [FAQ](https://github.com/GameTechDev/Intel-Texture-Works-Plugin/wiki/FAQ) is also available on the Wiki

## Requirements

* Windows\* (32/64) versions 7, 8, 10
* Photoshop\* CS6 through CC2015



* * *


# Source Code
## Prerequisites ##

- Visual Studio 2012 (or possibly higher, although so far all builds created with 2012)
- Photoshop CS6 SDK, can be obtained from here: http://www.adobe.com/devnet/photoshop/sdk.html
- Adobe Photoshop 32 or 64 bit, CS6 or higher.
- Intel® Implicit SPMD Program Compiler (https://github.com/ispc/ispc/releases)
## First time set-up ##

- Install Photoshop CS6 SDK on developer machine
- Copy ispc.exe to 3rdParty\Intel\Tools\ directory
- Open IntelTextureTools solution 
- Go to Property Manager and edit settings for Microsoft.Cpp.Win32.user and Microsoft.Cpp.x64.user, adding the following User Macro
	PHOTOSHOP_SDK_CS6 = <full path to location of Photoshop CS6 SDK> 
- Choose platform appropriate to the version of Photoshop you wish to test with (Win32 or x64)
- Build! For convenience plugin binaries will be copied to "~/Plugins/[platform]" folder.

## To easily run/debug from Visual Studio ##

- create an alias/shortcut to the appropriate platform folder and place it in the matching Photoshop Plug-ins folder (this will allow photoshop to load plugin from build location) 
- in plugin project, select Properties->Configuration->Debugging->Target and browse to photoshop executable

## 3rd Party code ## 

This project utilizes the following code (located under 3rdParty folder)
- DirectXTex, from https://github.com/Microsoft/DirectXTex (Sourced from tag Jul2015)
- Intel, BC compression code and helpers 


```
* Other names and brands may be claimed by their owners.
```
