/////////////////////////////////////////////////////////////////////////////////////////////
// Copyright 2017 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");// you may not use this file except in compliance with the License.// You may obtain a copy of the License at//// http://www.apache.org/licenses/LICENSE-2.0//// Unless required by applicable law or agreed to in writing, software// distributed under the License is distributed on an "AS IS" BASIS,// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.// See the License for the specific language governing permissions and// limitations under the License.
/////////////////////////////////////////////////////////////////////////////////////////////

#include <vector>
#include <list>
#include <sstream>
#include <fstream>
#include "PIUFile.h"
#include "SaveOptionsDialog.h"
#include "IntelPluginName.h"
#include "IntelPlugin.h"
#include "IntelPluginUIWin.h"
#include "resource.h"
#include "PreviewDialog.h"

typedef vector<string>(*VStringFunc)(void);

// Container for the data to associate a help button WinForm ID with the corresponding function to display the help text.
typedef struct {
	const int itemNum;		// WinForm ID of the help button
	VStringFunc func;		// Function to call to get the help text to display
} HelpButtonAndTextFunc;

// Struct to associate the Dropdown list WinForm ID with the Context Text WinForm ID
//  so we know where to display the context info when the user picks a different dropdown list item.
typedef struct {
	const int itemNum;			// DropDown list WinForm ID
	const int contextItemNum;	// Context Text WinForm ID.
} ComboAndContextStringID;

vector<string> GetCompressionHelpText(void);	// Helper functions to specify the text for the help button (?) next to various UI elements
vector<string> GetTextureTypeHelpText(void);
vector<string> GetPreCompressOpsHelpText(void);
vector<string> GetMipMapsHelpText(void);


HelpButtonAndTextFunc const helpButtonTextItem[] = {
	{ IDC_COMPRESSION_HELP, GetCompressionHelpText },
	{ IDC_TEXTURETYPE_HELP, GetTextureTypeHelpText },
	{ IDC_PRECOMPRESS_HELP, GetPreCompressOpsHelpText },
	{ IDC_MIPMAP_HELP, GetMipMapsHelpText }
};

// The list of associations.
ComboAndContextStringID const gComboContextItems[] = {
	{ IDC_COMPRESSION_COMBO, IDC_COMPRESSION_HINT },
	{ IDC_TEXTURETYPE_COMBO, IDC_TEXTURETYPE_HINT },
	{ IDC_MIPMAP_COMBO, IDC_MIPMAPS_HINT }
};

#if !__LP64__


OptionsDialog::OptionsDialog(IntelPlugin* plugin_) : PIDialog()  
{ 
	plugin = plugin_;
	globalParams = plugin->GetData();
	mPathToPresetDirectory.clear();
	MaxMipLevel =static_cast<int>(1 + floor(Log2(max((plugin->GetFormatRecord()->imageSize.h),(plugin->GetFormatRecord()->imageSize.v))))); 

	/*Get path to Local per user configuration files, %USERPROFILE%\\AppData\Local\\Intel\\PhotoshopDDSPlugin\\ */ 
	wchar_t* localAppData = 0;
    HRESULT hr = SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, NULL, &localAppData);

	//Path exists
	if (SUCCEEDED(hr))
	{
		//Convert wide char to char, append name of directory
  		char str[MAX_PATH+1] = {};
		wcstombs(str, localAppData, MAX_PATH);
		CoTaskMemFree(static_cast<void*>(localAppData));

		mPathToPresetDirectory = str;
			
		//Free mem

		//Create directory if does not exist
		mPathToPresetDirectory.append("\\Intel");
		if (CreateDirectory(mPathToPresetDirectory.c_str(), NULL) ||  ERROR_ALREADY_EXISTS == GetLastError())
		{
			mPathToPresetDirectory.append("\\PhotoshopDDSPlugin\\");
			if (!(CreateDirectory(mPathToPresetDirectory.c_str(), NULL) ||  ERROR_ALREADY_EXISTS == GetLastError()))
			{
					// Failed to create directory.
					plugin->UserError("Failed to Create Presets Directory");
			}
		}
		else
		{
				// Failed to create directory.
				plugin->UserError("Failed to Create Presets Directory");
		}

	}
	else
	{
		// Failed to create directory.
		plugin->UserError("Failed to get path to %USERPROFILE%\\AppData\\Local");
	}
}

// Calculates log2 of number.  
double OptionsDialog::Log2( double n )  
{  
    // log(n)/log(2) is log2.  
    return log( n ) / log( 2. );  
}

// ===========================================================================
bool OptionsDialog::LoadPresetNonUIMode(string nameOfPreset)
{
	//If not presets directory return with error
	if (mPathToPresetDirectory.empty())
		return false;
	
	//Initialize with defaults
	InitDataNoPreset(mDialogData);

	//This loads all the settings and the last-used setting
	LoadPresets();
	
	InitDataFromPreset(nameOfPreset);

	InitComboItems();

	return true;
}

// ===========================================================================
//Fill global plugin struct with UI data
void OptionsDialog::FillGlobalStruct()
{
	//Get the compressionType from the CompressionTypeComboBox, the combo box always has the list of the valid types
	//The itemUserData has the actual compression enumeration defined in IntelPlugin.h for a given combo box entry
	int copressionTypeID =  gComboItems[COMPRESSION_COMBO].itemAndContextStrings[mDialogData.CompressionTypeIndex].itemUserData;

	switch (copressionTypeID)
		{
			case CompressionTypeEnum::BC1:globalParams->encoding_g = DXGI_FORMAT_BC1_UNORM;
				globalParams->fast_bc67 = false;
				break;
			case CompressionTypeEnum::BC1_SRGB:globalParams->encoding_g = DXGI_FORMAT_BC1_UNORM_SRGB;
				globalParams->fast_bc67 = false;
				break;
			case CompressionTypeEnum::BC3:globalParams->encoding_g = DXGI_FORMAT_BC3_UNORM;
				globalParams->fast_bc67 = false;
				break;
			case CompressionTypeEnum::BC3_SRGB:globalParams->encoding_g = DXGI_FORMAT_BC3_UNORM_SRGB;
				globalParams->fast_bc67 = false;
				break;
			case CompressionTypeEnum::BC6H_FAST:globalParams->encoding_g = DXGI_FORMAT_BC6H_UF16; 
				globalParams->fast_bc67 = true;
				break;
			case CompressionTypeEnum::BC6H_FINE:globalParams->encoding_g = DXGI_FORMAT_BC6H_UF16;
				globalParams->fast_bc67 = false;
				break;
			case CompressionTypeEnum::BC7_FAST:globalParams->encoding_g = DXGI_FORMAT_BC7_UNORM;
				globalParams->fast_bc67 = true;
				break;
			case CompressionTypeEnum::BC7_FINE:globalParams->encoding_g = DXGI_FORMAT_BC7_UNORM;
				globalParams->fast_bc67 = false;
				break;
			case CompressionTypeEnum::BC7_SRGB_FAST:globalParams->encoding_g = DXGI_FORMAT_BC7_UNORM_SRGB;
				globalParams->fast_bc67 = true;
				break;
			case CompressionTypeEnum::BC7_SRGB_FINE:globalParams->encoding_g = DXGI_FORMAT_BC7_UNORM_SRGB;
				globalParams->fast_bc67 = false;
				break;
			case CompressionTypeEnum::BC4:globalParams->encoding_g = DXGI_FORMAT_BC4_UNORM;
				globalParams->fast_bc67 = false;
				break;
			case CompressionTypeEnum::BC5:globalParams->encoding_g = DXGI_FORMAT_BC5_UNORM;
				globalParams->fast_bc67 = false;
				break;
			case CompressionTypeEnum::UNCOMPRESSED:globalParams->encoding_g = DXGI_FORMAT_R8G8B8A8_UNORM;
				globalParams->fast_bc67 = false;
				break;
			default: globalParams->encoding_g = DXGI_FORMAT_BC1_UNORM;
				globalParams->fast_bc67 = false;
				break;
		}

		globalParams->TextureTypeIndex = mDialogData.TextureTypeIndex; //Col,Col+alpha,CubeFrmLayera,CubefromCross,NM
		globalParams->MipMapTypeIndex  = mDialogData.MipMapTypeIndex;  //None,Autogen,FromLayers
		globalParams->MipLevel      = mDialogData.MipLevel;			   // only valid if SetMipLevel == true
		globalParams->SetMipLevel   = mDialogData.SetMipLevel;
		globalParams->Normalize     = mDialogData.Normalize;
		globalParams->FlipX         = mDialogData.FlipX;
		globalParams->FlipY         = mDialogData.FlipY;

		//presetBatchName is a PString (first byte is the size), convert C to PString
		CToPStr(mDialogData.PresetName.c_str(), reinterpret_cast<char *>(globalParams->presetBatchName));
}

//Fill UI data struct with global plugin struct
void OptionsDialog::GetGlobalStruct()
{
	int CompressionTypeIndex=0;
	CompressionTypeEnum compressionID;
	
	//Find compression ID.
	switch (globalParams->encoding_g)
	{
		case DXGI_FORMAT_BC1_UNORM:
			compressionID = CompressionTypeEnum::BC1;
			break;
		case DXGI_FORMAT_BC1_UNORM_SRGB: 
			compressionID = CompressionTypeEnum::BC1_SRGB;
			break;
		case DXGI_FORMAT_BC3_UNORM:      
			compressionID = CompressionTypeEnum::BC3;
			break;
		case DXGI_FORMAT_BC3_UNORM_SRGB: 
			compressionID = CompressionTypeEnum::BC3_SRGB;
			break;
		case DXGI_FORMAT_BC6H_UF16:
			if (globalParams->fast_bc67)
				compressionID = CompressionTypeEnum::BC6H_FAST;
			else 
				compressionID = CompressionTypeEnum::BC6H_FINE;
			break;
		case DXGI_FORMAT_BC7_UNORM:
			if (globalParams->fast_bc67)
				compressionID = CompressionTypeEnum::BC7_FAST;
			else
				compressionID = CompressionTypeEnum::BC7_FINE;
			break;
		case DXGI_FORMAT_BC7_UNORM_SRGB:
			if (globalParams->fast_bc67)
				compressionID = CompressionTypeEnum::BC7_SRGB_FAST;
			else
				compressionID = CompressionTypeEnum::BC7_SRGB_FINE;
			break;
		case DXGI_FORMAT_BC4_UNORM:
			compressionID = CompressionTypeEnum::BC4;
			break;
		case DXGI_FORMAT_BC5_UNORM:
			compressionID = CompressionTypeEnum::BC5;
			break;
		case DXGI_FORMAT_R8G8B8A8_UNORM:
			compressionID = CompressionTypeEnum::UNCOMPRESSED;
			break;
		default: compressionID = CompressionTypeEnum::BC1;
			break;
	}

	//Iteration over all available compression for this texture type and find the compression combobox index.
	//Compression types which are not available are not show in the combo box, therefore the index is not incremented if matrix is false
	for (int i=0; i<CompressionTypeEnum::COMPRESSION_TYPE_COUNT; i++)
	{
		//If this compression mode is available for this texture type
		if (IntelPlugin::IsCombinationValid(globalParams->TextureTypeIndex, static_cast<CompressionTypeEnum>(i)))
		{
			//If its the selected encoding and this encoding is available then break out
			if (i == compressionID )
				break;
		
			CompressionTypeIndex++;
		}
	}

	mDialogData.CompressionTypeIndex = CompressionTypeIndex;
	mDialogData.TextureTypeIndex = globalParams->TextureTypeIndex;  //Col,Col+alpha,CubeFrmLayera,CubefromCross,NM
	mDialogData.MipMapTypeIndex  = globalParams->MipMapTypeIndex;   //None,Autogen,FromLayers
	mDialogData.MipLevel         = globalParams->MipLevel;			// only valid if SetMipLevel == true
	mDialogData.SetMipLevel   = globalParams->SetMipLevel;
	mDialogData.Normalize     = globalParams->Normalize;
	mDialogData.FlipX         = globalParams->FlipX;
	mDialogData.FlipY         = globalParams->FlipY;
}

// ===========================================================================

//Fills in Combo items structures for Presets, Compression, Textype, and MipMap generation
//fills control id, startindex and strings
void OptionsDialog::InitComboItems()
{
	gComboItems.reserve(NUMBEROF_COMBOS);

	gComboItems.push_back(ComboData(IDC_PRESET_COMBO));
	GetPresetNames(gComboItems[PRESETS_COMBO]);

	gComboItems.push_back(ComboData(IDC_COMPRESSION_COMBO));
	GetCompressionNames(gComboItems[COMPRESSION_COMBO]);

	gComboItems.push_back(ComboData(IDC_TEXTURETYPE_COMBO));
	GetTextureTypeNames(gComboItems[TEXTURETYPE_COMBO]);

	gComboItems.push_back(ComboData(IDC_MIPMAP_COMBO));
	GetMipMapNames(gComboItems[MIPMAP_COMBO]);
}

// ===========================================================================

// Scan the %USERPROFILE%\\AppData\Local\\Intel\\PhotoshopDDSPlugin\\ folder to find .preset files and reads them in as preset data to populate the Presets dialog.
void OptionsDialog::LoadPresets(void)
{
	mPresets.clear();
	
	//List all prest files 
	string fullPath = mPathToPresetDirectory+"*.preset";

	// Find the first file in the directory.
	WIN32_FIND_DATA ffd;
	HANDLE hFind = FindFirstFile(fullPath.c_str(), &ffd);

	if (INVALID_HANDLE_VALUE == hFind)
	{
		return;
	}

	// Walk the list of files in the dir that match our pattern and add the names to our list, skipping  the 'none' preset file (handled separately).
	list<string> fileNames;

	do
	{
		if (!(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
		{
			string fname(ffd.cFileName);
			fileNames.push_back(fname);
		}

	} while (FindNextFile(hFind, &ffd) != 0);


	DWORD dwError = GetLastError();
	if (dwError != ERROR_NO_MORE_FILES)
	{
		return;
	}

	FindClose(hFind);

	// Now, read the rest of the files and load up the presets menu with them.
	for (auto it = fileNames.begin(); it != fileNames.end(); ++it)
	{
		string presetPath = mPathToPresetDirectory + *it;
		ReadPreset(presetPath);
	}
}

// ===========================================================================

//Fills in the DialogData structs which are stored in the mPresets array
void OptionsDialog::ReadPreset(string fname)
{
	ifstream fileIn;
	fileIn.open(fname);

	if (fileIn.is_open())
	{
		string line;
		uint32 lineNum = 0;

		DialogData dd;

		size_t foundBSlash = fname.rfind("\\");
		string justName = fname;

		if (foundBSlash != string::npos)
		{
			justName = fname.substr(foundBSlash+1);
		}
		else
		{
			size_t foundFSlash = fname.rfind("/");	// handle those other OSes.
			if (foundFSlash != string::npos)
			{
				justName = fname.substr(foundFSlash+1);
			}
		}

		size_t foundDot = justName.rfind(".");

		if (foundDot != string::npos)
		{
			dd.PresetName = justName.substr(0, foundDot);
		}
		else
		{
			dd.PresetName = justName;
		}

		try 
		{
			while (getline(fileIn, line))
			{
				bool done = false;
				switch (lineNum-1)
				{
					case -1: if (atoi(line.c_str()) != PRESET_FILE_VERSION)		// version check
							 throw std::exception();

					case 0: dd.CompressionTypeIndex = atoi(line.c_str());    break;
					case 1: dd.TextureTypeIndex = static_cast<TextureTypeEnum>(atoi(line.c_str()));        break;
					case 2: dd.MipMapTypeIndex =  static_cast<MipmapEnum>(atoi(line.c_str()));         break;
					case 3: dd.MipLevel = atoi(line.c_str());                break;
					case 4: dd.SetMipLevel = atoi(line.c_str()) == 1;        break;
					case 5: dd.Normalize = atoi(line.c_str()) == 1;          break;
					case 6: dd.FlipX = atoi(line.c_str()) == 1;              break;

					case 7: dd.FlipY = atoi(line.c_str()) == 1;
						done = true;
						break;

					default:
						done = true;
				}

				++lineNum;

				if (done)
				{
					break;
				}
			}

			if (dd.PresetName.compare(LAST_SETTINGS_PRESET_NAME) == 0)
			{
				mDialogData = dd;
			}
			else
			{
				mPresets[dd.PresetName] = dd;
			}
		}
		catch (std::exception &)
		{
			// bad settings read
		}

		fileIn.close();

	}
}

// ===========================================================================
// Writes the passed-in settings out to the specified file, so they'll be available as a preset next time the plugin loads.
// Also updates the presets dropdown list.
void OptionsDialog::SaveNewPreset(string presetName, DialogData dd)
{
	bool fileWriteSucceeded = true;
	dd.PresetName = presetName;

	//path to new preset file
	string fullPath = mPathToPresetDirectory + presetName + ".preset";

	ofstream fileOut;
	fileOut.open(fullPath);
	
	if (fileOut.is_open())
	{
		fileOut << PRESET_FILE_VERSION << "\n";
		fileOut << dd.CompressionTypeIndex << "\n";
		fileOut << dd.TextureTypeIndex << "\n";
		fileOut << dd.MipMapTypeIndex << "\n";
		fileOut << dd.MipLevel << "\n";
		fileOut << (dd.SetMipLevel ? "1" : "0") << "\n";
		fileOut << (dd.Normalize ? "1" : "0") << "\n";
		fileOut << (dd.FlipX ? "1" : "0") << "\n";
		fileOut << (dd.FlipY ? "1" : "0") << "\n";

		fileOut.close();
	}
	else
	{
		fileWriteSucceeded = false;
		errorMessage("Can not save "+fullPath, "Preset save erorr");
	}


	if (fileWriteSucceeded)
	{
		if ((presetName.compare(LAST_SETTINGS_PRESET_NAME) == 0) && (!mPresets.empty()))
		{
			// TODO
		}
		else
		{
			mPresets[presetName] = dd;
		}

		// Update the main working data
		InitDataFromPreset(presetName);

		mDialogData.PresetName = presetName;

		// rebuild combo items data list for presets
		ComboData & cd = gComboItems[PRESETS_COMBO];		
		cd.itemAndContextStrings.clear();
		GetPresetNames(cd);

		// rebuild combobox for presets -- making sure it has the right selected item (newly created preset)
		InitComboFromItems(PRESETS_COMBO);							

		// Update UI to reflect new Preset.
		SetUIFromData();
	}
}

void OptionsDialog::UpdatePreset(string presetName, DialogData dd)
{
	dd.PresetName = presetName;
	
	//path to existing preset file
	string fullPath = mPathToPresetDirectory + presetName + ".preset";

	ofstream fileOut;
	fileOut.open(fullPath);
	
	//Save change into preset file
	if (fileOut.is_open())
	{
		fileOut << PRESET_FILE_VERSION << "\n";
		fileOut << dd.CompressionTypeIndex << "\n";
		fileOut << dd.TextureTypeIndex << "\n";
		fileOut << dd.MipMapTypeIndex << "\n";
		fileOut << dd.MipLevel << "\n";
		fileOut << (dd.SetMipLevel ? "1" : "0") << "\n";
		fileOut << (dd.Normalize ? "1" : "0") << "\n";
		fileOut << (dd.FlipX ? "1" : "0") << "\n";
		fileOut << (dd.FlipY ? "1" : "0") << "\n";

		fileOut.close();
	}
	else
	{
		errorMessage("Can not save "+fullPath, "Preset save erorr");
	}

	//Save change also into mPresets array
	mPresets[presetName] = dd;
}

// ===========================================================================
// Remove a preset from the list and delete the .preset file for it.
void OptionsDialog::DeletePreset(string presetName)
{
	if (mPresets.erase(presetName))
	{
		// rebuild combo items data list for presets
		ComboData & cd = gComboItems[PRESETS_COMBO];
		cd.itemAndContextStrings.clear();
		GetPresetNames(cd);

		// rebuild combobox for presets -- making sure it has the right selected item (newly created preset)
		InitComboFromItems(PRESETS_COMBO);

		// Update UI to reflect new Preset.
		SetUIFromData();

		//path to presets file
		string fullPath = mPathToPresetDirectory + presetName + ".preset";

		BOOL fileDeleted = DeleteFile(fullPath.c_str());

		if (!fileDeleted)
		{
			errorMessage("Can not delete "+fullPath, "Preset delete erorr");
		}
	}
}

// ===========================================================================
// Initialize a DialogData structure (dd) with default settings
void OptionsDialog::InitDataNoPreset(DialogData & dd)
{
	dd.PresetName = LAST_SETTINGS_PRESET_NAME;
	dd.CompressionTypeIndex = 0;
	dd.TextureTypeIndex = TextureTypeEnum::COLOR;
	dd.MipMapTypeIndex = MipmapEnum::NONE;

	dd.SetMipLevel = false;

	dd.MipLevel = 0;

	dd.Normalize = false;
	dd.FlipX = false;
	dd.FlipY = false;
}

// ===========================================================================
// Populate the mDialogData with the UI settings from the specified Preset in mPresets.
void OptionsDialog::InitDataFromPreset(string presetName)
{
	auto item = mPresets.find(presetName);
	if (item != mPresets.end())
		mDialogData = item->second;
}

// ===========================================================================

//Fill now the combo controls with the strings stored in the ComboData structs
void OptionsDialog::InitComboFromItems(int32 comboItemsIndex)
{
	ComboData & cd = gComboItems[comboItemsIndex];

	PIComboBox combo = GetItem(cd.itemNum);
	combo.Clear();

	if (!cd.itemAndContextStrings.empty())
	{
		//Add the description strings to the Combo box 
		for (size_t i = 0; i < cd.itemAndContextStrings.size(); ++i)
		{
			combo.AppendItem(cd.itemAndContextStrings[i].itemText.c_str());
		}

		//Set the selected item
		uint32 selectedIndex = cd.startIndex;
		combo.SetCurrentSelection(selectedIndex);

		//Iterate over only the 3 combo boxes and set the text of the according Context Control
		//Compression, Textype, and MipMap
		for (uint32 j = 0; j < sizeof(gComboContextItems) / sizeof(ComboAndContextStringID); ++j)
		{
			if (cd.itemNum == gComboContextItems[j].itemNum)
			{
				PIText sText = GetItem(gComboContextItems[j].contextItemNum);
				sText.SetText(cd.itemAndContextStrings[selectedIndex].itemContextInfo);
				break;
			}
		}
	}
}

// ===========================================================================
// Override the combo box font so as to get alignment right
void OptionsDialog::SetFontCompressionCombo()
{
	PIComboBox comboCompression = GetItem(IDC_COMPRESSION_COMBO);
	PIComboBox comboTexType = GetItem(IDC_TEXTURETYPE_COMBO);
	PIComboBox comboMipMap = GetItem(IDC_MIPMAP_COMBO);
	
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
		SendMessage(comboCompression.GetItem(), WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
		SendMessage(comboTexType.GetItem(), WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
		SendMessage(comboMipMap.GetItem(), WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
	}
}

// ===========================================================================
// Main initialization of the UI - Load the preset files, build out the programmatically populated UI items, and set the initial UI state.
void OptionsDialog::Init(void)
{
	//Set window name
	string windowTitle = DDSExporterPluginName;
	windowTitle += DDSExporterPluginVersion;
	SetWindowText(GetDialog(), windowTitle.c_str());

	//If not presets directory return with error
	if (mPathToPresetDirectory.empty())
		return;
	
	InitDataNoPreset(mDialogData);	// sets defaults, may be overridden in next call

	LoadPresets();

	
	//Fills in gComboItems array for Presets, Compression, Textype, and MipMap generation
	InitComboItems();

	//Now fill in Combo boxes  with text and the correxponding context controls with text
	for (int32 i = 0; i < gComboItems.size(); ++i)
	{
		InitComboFromItems(i);
	}

	// Override the combo box font so as to get alignment right
	SetFontCompressionCombo();

	PICheckBox setMipLevel = GetItem(IDC_CUBEMIPLEVEL_CHECK);
	setMipLevel.SetChecked(mDialogData.SetMipLevel);

	//Fill in the miplevel selector for cube maps
	if (mDialogData.SetMipLevel)
	{
		PopulateMipLevelsCombo();
	}
	else
	{
		PIComboBox mipLevelCombo = GetItem(IDC_MIPLEVEL_COMBO);
		mipLevelCombo.Clear();
	}
	
	PICheckBox normalizeCheck = GetItem(IDC_NORMALIZE_CHECK);
	normalizeCheck.SetChecked(mDialogData.Normalize);

	PICheckBox flipXCheck = GetItem(IDC_FLIPX_CHECK);
	flipXCheck.SetChecked(mDialogData.FlipX);

	PICheckBox flipYCheck = GetItem(IDC_FLIPY_CHECK);
	flipYCheck.SetChecked(mDialogData.FlipY);

	//Disable any controls they do not apply based on the choosen texture type
	DisableUnavailableControls();
}

// ===========================================================================
// Update the UI state based on the current state of the current mDialogData struct.
void OptionsDialog::SetUIFromData()
{
	//Change entries in compression/mipmap combo based on texture type
    UpdateCompressionCombo();
	UpdateMipMapCombo();

	//Initialize compression combo from mDialogData.CompressionTypeIndex
	PIComboBox CompressionCombo = GetItem(IDC_COMPRESSION_COMBO);
	CompressionCombo.SetCurrentSelection(mDialogData.CompressionTypeIndex);
	//Update context string based on selected entry
	SetContextString(IDC_COMPRESSION_HINT, IDC_COMPRESSION_COMBO);

	//Initialize texture type combo from mDialogData.TextureTypeIndex
	PIComboBox TextureTypeCombo = GetItem(IDC_TEXTURETYPE_COMBO);
	TextureTypeCombo.SetCurrentSelection(mDialogData.TextureTypeIndex);
	//Update context string based on selected entry
	SetContextString(IDC_TEXTURETYPE_HINT, IDC_TEXTURETYPE_COMBO);

	//Initialize mip map creation combo from mDialogData.MipMapTypeIndex
	PIComboBox MipMapCombo = GetItem(IDC_MIPMAP_COMBO);
	MipMapCombo.SetCurrentSelection(mDialogData.MipMapTypeIndex);
	//Update context string based on selected entry
	SetContextString(IDC_MIPMAPS_HINT, IDC_MIPMAP_COMBO);


	PICheckBox mipLevelCheck = GetItem(IDC_CUBEMIPLEVEL_CHECK);
	bool wasMipLevelChecked = mipLevelCheck.GetChecked();
	mipLevelCheck.SetChecked(mDialogData.SetMipLevel);

	if (wasMipLevelChecked && !mDialogData.SetMipLevel)
	{
		PIComboBox MipMapLevelCombo = GetItem(IDC_MIPLEVEL_COMBO);
		MipMapLevelCombo.Clear();
	}
	else if (!wasMipLevelChecked && mDialogData.SetMipLevel)
	{
		PopulateMipLevelsCombo();
	}
	else if (wasMipLevelChecked && mDialogData.SetMipLevel)
	{
		PIComboBox MipMapLevelCombo = GetItem(IDC_MIPLEVEL_COMBO);
		MipMapLevelCombo.SetCurrentSelection(mDialogData.MipLevel);
	}

	PICheckBox NormalizeCheckBox = GetItem(IDC_NORMALIZE_CHECK);
	NormalizeCheckBox.SetChecked(mDialogData.Normalize);

	PICheckBox FlipXCheckBox = GetItem(IDC_FLIPX_CHECK);
	FlipXCheckBox.SetChecked(mDialogData.FlipX);

	PICheckBox FlipYCheckBox = GetItem(IDC_FLIPY_CHECK);
	FlipYCheckBox.SetChecked(mDialogData.FlipY);


	//Disable any controls they do not apply based on the choosen texture type
	DisableUnavailableControls();
}

// ===========================================================================

// Called whenever user interacts with the UI - clicks a button or ticks a check box, selects a dropdown, etc.
//parameters index: has the resource index of the control acted upon.
void OptionsDialog::Notify(int index)
{
	//Shows MessageBox for help [?] buttons. Index into array and get function which return strings to display
	for (uint32 i = 0; i < sizeof(helpButtonTextItem) / sizeof(HelpButtonAndTextFunc); ++i)
	{
		if (index == helpButtonTextItem[i].itemNum)
		{
			vector<string> captionAndText = helpButtonTextItem[i].func();

			MessageBox(GetActiveWindow(), captionAndText[0].c_str(), captionAndText[1].c_str(), 0);
			return;
		}
	}

	//One of the Combo boxes was changed, update context string
	for (uint32 i = 0; i < sizeof(gComboContextItems) / sizeof(ComboAndContextStringID); ++i)
	{
		if (index == gComboContextItems[i].itemNum)
		{
	        //Update context string based on selected entry
			SetContextString(gComboContextItems[i].contextItemNum, index);
			break;		// no return - want this to fall through to the end block that backs up the change.
		}
	}

	if (index == IDC_CUBEMIPLEVEL_CHECK)
	{
		PICheckBox mipLevelCheck = GetItem(IDC_CUBEMIPLEVEL_CHECK);

		if (mipLevelCheck.GetChecked())
		{
			PopulateMipLevelsCombo();
		}
		else
		{
			PIComboBox mipLevelCombo = GetItem(IDC_MIPLEVEL_COMBO);
			mipLevelCombo.Clear();
		}
		// no return - want this to fall through to the end block that backs up the change.
	}
	
	//Change preset, preset combo whas changed
	if (index == IDC_PRESET_COMBO)
	{
		PIComboBox presetCombo = GetItem(IDC_PRESET_COMBO);
		string selectedItem;

		//if (command == CBN_SELCHANGE)
		{
			//Get the string entry of the selected item in the combo box
			presetCombo.GetCurrentSelection(selectedItem);

			//Did we select a different preset
			if (selectedItem.compare(mDialogData.PresetName) != 0)
			{
				//Store changes in current preset before swapping
				
#ifdef AUTO_SAVE_PRESETS
				UpdatePreset(mDialogData.PresetName, mDialogData);
#endif

				//Load new data in struct
				InitDataFromPreset(selectedItem);

				//Update UI from struct
				SetUIFromData();
			
				// If we want a callback or notify that a preset has changed, here is where to place that call.
			}
		}
		return;
	}

	if (index == IDC_PRESETDELETE_BUTTON)
	{
		PIComboBox presetCombo = GetItem(IDC_PRESET_COMBO);
		string selectedPreset;
		presetCombo.GetCurrentSelection(selectedPreset);

		if (selectedPreset.compare(LAST_SETTINGS_PRESET_NAME) == 0)
		{
			MessageBox(GetActiveWindow(), "You must select a preset from the dropdown to be deleted", "Delete Preset", MB_OK);
			return;
		}

		stringstream ss;
		// TODO -- localization?
		ss << "Are you sure you want to delete the preset: " << selectedPreset << "?";

		int delResult = MessageBox(GetActiveWindow(), ss.str().c_str(), "Delete Preset", MB_OKCANCEL);
		
		if (delResult == IDOK)
		{
			DeletePreset(selectedPreset);
		}

		return;
	}


	if (index == IDC_PRESETSAVE_BUTTON)
	{
		PIComboBox presetCombo = GetItem(IDC_PRESET_COMBO);
		string newPresetName;
		presetCombo.GetCurrentSelection(newPresetName);

		newPresetName = GetPresetName(newPresetName, GetActiveWindow());

		if (!newPresetName.empty())
		{
			bool doSave = mPresets.find(newPresetName) == mPresets.end();
			if (!doSave)	// already exists?
			{
				doSave = IDOK == MessageBox(GetActiveWindow(), "That Preset name exists - save over it?", "Confirm Save", MB_OKCANCEL);
			}


			if (doSave)
			{
				DialogData dd;
				ExtractDataFromUI(dd);

				SaveNewPreset(newPresetName, dd);
			}
		}

		return;
	}
	
	if (index == IDC_PREVIEW_BUTTON)
	{
		//Copy over UI to global struct for preview routined
		FillGlobalStruct();

		//Show previewUI (modal does not return until OK is pressed)
		PreviewDialog dlg(plugin);
		dlg.Modal();
		
		//Copy any changes back into UI struct (preview can change compression)
		GetGlobalStruct();

		//Update UI from struct
		SetUIFromData();

		return;
	}

	if (index == IDOK)	// OK button pressed.
	{
		PIComboBox presetCombo = GetItem(IDC_PRESET_COMBO);
		presetCombo.GetCurrentSelection(mDialogData.PresetName);

		ExtractDataFromUI(mDialogData);

		//We have the none preset set so save only to the none preset so that it remember the next time it loads
		if (mDialogData.PresetName.compare(LAST_SETTINGS_PRESET_NAME) == 0)
		{
			UpdatePreset(LAST_SETTINGS_PRESET_NAME, mDialogData);
		}
		else
		{
			//Update currently set preset and none preset so that it remember the next time it loads

#ifdef AUTO_SAVE_PRESETS
				UpdatePreset(mDialogData.PresetName, mDialogData);
#endif

			UpdatePreset(LAST_SETTINGS_PRESET_NAME, mDialogData);
		}

		return;
	}

	auto old = mDialogData;

	//Some other changes happened to the UI which did not have custom actions just update the struct
	ExtractDataFromUI(mDialogData);

	//Rebuild CompressionTypes Combo box and disable not applicable controls 
	//If Texture or MipMap type changed (apply last to have mDialog updated)
	if (old.TextureTypeIndex != mDialogData.TextureTypeIndex || old.MipMapTypeIndex != mDialogData.MipMapTypeIndex)
	{
		//Update compression/mipmap combo box
		UpdateCompressionCombo();
		UpdateMipMapCombo();

		//Disable any controls they do not apply based on the choosen texture type
		DisableUnavailableControls();
	}
}

// ===========================================================================
// Interrogate the state of the UI elements to find their current selections and copy that into the specified DialogData struct (dd)
void OptionsDialog::ExtractDataFromUI(DialogData & dd)
{
	dd.CompressionTypeIndex = GetSelectedItem(IDC_COMPRESSION_COMBO);
	dd.TextureTypeIndex = static_cast<TextureTypeEnum>(GetSelectedItem(IDC_TEXTURETYPE_COMBO));
	dd.MipMapTypeIndex = static_cast<MipmapEnum>(GetSelectedItem(IDC_MIPMAP_COMBO));

	PICheckBox MipLevelCheck = GetItem(IDC_CUBEMIPLEVEL_CHECK);
	dd.SetMipLevel = MipLevelCheck.GetChecked();

	if (dd.SetMipLevel)
	{
		uint32 MipLevelIndex = GetSelectedMipLevelIndex();
		dd.MipLevel = MipLevelIndex;
	}

	PICheckBox NormalizeCheck = GetItem(IDC_NORMALIZE_CHECK);
	dd.Normalize = NormalizeCheck.GetChecked();

	PICheckBox FlipXCheck = GetItem(IDC_FLIPX_CHECK);
	dd.FlipX = FlipXCheck.GetChecked();

	PICheckBox FlipYCheck = GetItem(IDC_FLIPY_CHECK);
	dd.FlipY = FlipYCheck.GetChecked();
}

//Enables or disables caontrols depending on the texture type
void OptionsDialog::DisableUnavailableControls()
{
	PICheckBox NormalizeCheck = GetItem(IDC_NORMALIZE_CHECK);
	PICheckBox FlipXCheck = GetItem(IDC_FLIPX_CHECK);
	PICheckBox FlipYCheck = GetItem(IDC_FLIPY_CHECK);
	PICheckBox MipLevelCheck = GetItem(IDC_CUBEMIPLEVEL_CHECK);
	PIComboBox combo = GetItem(IDC_MIPLEVEL_COMBO);

	//if texture type normal map then enable/disable relevant checkboxes
	if (mDialogData.TextureTypeIndex != TextureTypeEnum::NORMALMAP)
	{
		//disable and clear normalize checkbox
		::EnableWindow(NormalizeCheck.GetItem(), false);
		NormalizeCheck.SetChecked(false);
		//disable and clear flipx checkbox
		::EnableWindow(FlipXCheck.GetItem(), false);
		FlipXCheck.SetChecked(false);
		//disable and clear flipy checkbox
		::EnableWindow(FlipYCheck.GetItem(), false);
		FlipYCheck.SetChecked(false);
	}
	else
	{
		//enable normalize, flipx,y checkboxes
		::EnableWindow(NormalizeCheck.GetItem(), true);
		::EnableWindow(FlipXCheck.GetItem(), true);
		::EnableWindow(FlipYCheck.GetItem(), true);
	}

	//if texture type is/isnot cubemap then enable/disable combo+checkbox
	if (mDialogData.TextureTypeIndex != TextureTypeEnum::CUBEMAP_CROSSED && 
		mDialogData.TextureTypeIndex != TextureTypeEnum::CUBEMAP_LAYERS)
	{
		//disable miplevel checkbox and combo
		::EnableWindow(MipLevelCheck.GetItem(), false);
		::EnableWindow(combo.GetItem(), false);
		//uncheck and clear
		MipLevelCheck.SetChecked(false);
		combo.Clear();
	}
	else
	{
		//enable miplevel checkbox and combo
		::EnableWindow(MipLevelCheck.GetItem(), true);
		::EnableWindow(combo.GetItem(), true);
	}
	
}

//Update compression combo box based on textyreType. Convenience funtion
void OptionsDialog::UpdateCompressionCombo()
{
	//Fill new entries into struct
	GetCompressionNames(gComboItems[COMPRESSION_COMBO]);
	
	//Clear and fill combo box from struct
	InitComboFromItems(COMPRESSION_COMBO);
}

//Update mipmap combo box based on textyreType. Convenience funtion
void OptionsDialog::UpdateMipMapCombo()
{
	//Fill new entries into struct
	GetMipMapNames(gComboItems[MIPMAP_COMBO]);
	
	//Clear and fill combo box from struct
	InitComboFromItems(MIPMAP_COMBO);
}

// ===========================================================================
// Populate the data struct for the Presets dropdown list - uses the mPresets filled in from LoadPresets().
void OptionsDialog::GetPresetNames(ComboData & comboItem)
{
	comboItem.startIndex = -1;
	comboItem.itemAndContextStrings.clear();

	//push info for stored presets

	for (auto it = mPresets.begin(); it != mPresets.end(); ++it)
	{
		comboItem.itemAndContextStrings.push_back(ComboItemAndContext(it->first, ""));
		if (it->first.compare(mDialogData.PresetName) == 0)
		{
			comboItem.startIndex = static_cast<uint32>(comboItem.itemAndContextStrings.size()-1);
		}
	}
}

// ===========================================================================

void OptionsDialog::GetCompressionNames(ComboData & comboItem)
{
	comboItem.itemAndContextStrings.clear();

	//BC1, BC1_SRGB, BC3, BC3_SRGB, BC6H_FAST, BC6H_FINE, BC7_FAST, BC7_FINE, BC7_SRGB_FAST, BC7_SRGB_FINE, BC4, BC5, NONE
	if (IntelPlugin::IsCombinationValid(mDialogData.TextureTypeIndex,CompressionTypeEnum::BC1))
	    comboItem.itemAndContextStrings.push_back(ComboItemAndContext("BC1   4bpp  (Linear)", "Also DXT1. Maximum compatibility. No Alpha Channel", CompressionTypeEnum::BC1));

	if (IntelPlugin::IsCombinationValid(mDialogData.TextureTypeIndex,CompressionTypeEnum::BC1_SRGB))
		comboItem.itemAndContextStrings.push_back(ComboItemAndContext("BC1   4bpp  (sRGB, DX10+)", "No Alpha Channel. Use SRGB gamma.", CompressionTypeEnum::BC1_SRGB));

	if (IntelPlugin::IsCombinationValid(mDialogData.TextureTypeIndex,CompressionTypeEnum::BC3))
		comboItem.itemAndContextStrings.push_back(ComboItemAndContext("BC3   8bpp  (Linear)", "Also DXT5. Maximum compatibility. Supports Alpha.", CompressionTypeEnum::BC3));

	if (IntelPlugin::IsCombinationValid(mDialogData.TextureTypeIndex,CompressionTypeEnum::BC3_SRGB))
		comboItem.itemAndContextStrings.push_back(ComboItemAndContext("BC3   8bpp  (sRGB, DX10+)", "Supports Alpha. Use sRGB gamma.", CompressionTypeEnum::BC3_SRGB));
	
	if (IntelPlugin::IsCombinationValid(mDialogData.TextureTypeIndex,CompressionTypeEnum::BC6H_FAST))
		comboItem.itemAndContextStrings.push_back(ComboItemAndContext("BC6H  8bpp  Fast (Linear, DX11+)", "16 bit HDR images. No alpha. Only DX11+ level H/W. Fast Intel compression settings.", CompressionTypeEnum::BC6H_FAST));
	
	if (IntelPlugin::IsCombinationValid(mDialogData.TextureTypeIndex,CompressionTypeEnum::BC6H_FINE))
		comboItem.itemAndContextStrings.push_back(ComboItemAndContext("BC6H  8bpp  Fine (Linear, DX11+)", "16 bit HDR images. No alpha. Only DX11+ level H/W. Fine Intel compression settings.", CompressionTypeEnum::BC6H_FINE));
	
	if (IntelPlugin::IsCombinationValid(mDialogData.TextureTypeIndex,CompressionTypeEnum::BC7_FAST))
		comboItem.itemAndContextStrings.push_back(ComboItemAndContext("BC7   8bpp  Fast (Linear, DX11+)", "Best quality. Supports Alpha. Only DX11+ level H/W. Fast Intel compression settings.", CompressionTypeEnum::BC7_FAST));
	
	if (IntelPlugin::IsCombinationValid(mDialogData.TextureTypeIndex,CompressionTypeEnum::BC7_FINE))
		comboItem.itemAndContextStrings.push_back(ComboItemAndContext("BC7   8bpp  Fine (Linear, DX11+)", "Best quality. Supports Alpha. Only DX11+ level H/W. Fine Intel compression settings.", CompressionTypeEnum::BC7_FINE));
	
	if (IntelPlugin::IsCombinationValid(mDialogData.TextureTypeIndex,CompressionTypeEnum::BC7_SRGB_FAST))
		comboItem.itemAndContextStrings.push_back(ComboItemAndContext("BC7   8bpp  Fast (sRGB, DX11+)", "Best quality. Supports Alpha. Only DX11+ level H/W. Fast Intel compression settings. Use sRGB gamma.", CompressionTypeEnum::BC7_SRGB_FAST));
	
	if (IntelPlugin::IsCombinationValid(mDialogData.TextureTypeIndex,CompressionTypeEnum::BC7_SRGB_FINE))
		comboItem.itemAndContextStrings.push_back(ComboItemAndContext("BC7   8bpp  Fine (sRGB, DX11+)", "Best quality. Supports Alpha. Only DX11+ level H/W. Fine Intel compression settings. Use sRGB gamma.", CompressionTypeEnum::BC7_SRGB_FINE));
	
	if (IntelPlugin::IsCombinationValid(mDialogData.TextureTypeIndex,CompressionTypeEnum::BC4))
		comboItem.itemAndContextStrings.push_back(ComboItemAndContext("BC4   4bpp  (Linear, Grayscale)", "For Grayscale images, smallest size. Only the first image channel is used.", CompressionTypeEnum::BC4));
	
	if (IntelPlugin::IsCombinationValid(mDialogData.TextureTypeIndex,CompressionTypeEnum::BC5))
		comboItem.itemAndContextStrings.push_back(ComboItemAndContext("BC5   8bpp  (Linear, 2 Channel tangent map)", "Use for Normalmap encoding. Best quality. Only the first two image channels are used.", CompressionTypeEnum::BC5));
	
	if (IntelPlugin::IsCombinationValid(mDialogData.TextureTypeIndex,CompressionTypeEnum::UNCOMPRESSED))
		comboItem.itemAndContextStrings.push_back(ComboItemAndContext("none  32bpp", "Lossless, no compression applied.", CompressionTypeEnum::UNCOMPRESSED));
	
	if (mDialogData.CompressionTypeIndex < comboItem.itemAndContextStrings.size())
	{
		comboItem.startIndex = mDialogData.CompressionTypeIndex;
	}
	else	// Somehow have an index off the end of the list?  Reset to 0... best to be safe.
	{
		comboItem.startIndex = 0;
	}
}

// ===========================================================================

void OptionsDialog::GetTextureTypeNames(ComboData & comboItem)
{
	comboItem.itemAndContextStrings.push_back(ComboItemAndContext("Color", "Export only the RGB channels."));
	comboItem.itemAndContextStrings.push_back(ComboItemAndContext("Color + Alpha", "Export RGB and alpha channel."));
	comboItem.itemAndContextStrings.push_back(ComboItemAndContext("Cube Map from Layers", "Export a cube map using layers for faces (6 layers required)."));
	comboItem.itemAndContextStrings.push_back(ComboItemAndContext("Cube Map from crossed image", "Export a cube map from faces arranged in a horizontal or vertical crossed format."));
	comboItem.itemAndContextStrings.push_back(ComboItemAndContext("Normal Map", "Export as a normal map."));

	if (mDialogData.TextureTypeIndex < comboItem.itemAndContextStrings.size())
	{
		comboItem.startIndex = mDialogData.TextureTypeIndex;
	}
	else	// Somehow have an index off the end of the list?  Reset to 0... best to be safe.
	{
		comboItem.startIndex = 0;
	}
}

// ===========================================================================

void OptionsDialog::GetMipMapNames(ComboData & comboItem)
{
	comboItem.itemAndContextStrings.clear();

	comboItem.itemAndContextStrings.push_back(ComboItemAndContext("None", "No Mip Maps."));
	comboItem.itemAndContextStrings.push_back(ComboItemAndContext("Auto Generate", "Autogenerate Mip Maps."));

	//If cube maps user can not specify its own mip levels
	if (mDialogData.TextureTypeIndex != TextureTypeEnum::CUBEMAP_CROSSED && mDialogData.TextureTypeIndex != TextureTypeEnum::CUBEMAP_LAYERS)
		comboItem.itemAndContextStrings.push_back(ComboItemAndContext("From Layers", "Mip Maps are specified by the user, stored in layers."));

	if (mDialogData.MipMapTypeIndex < comboItem.itemAndContextStrings.size())
	{
		comboItem.startIndex = mDialogData.MipMapTypeIndex;
	}
	else	// Somehow have an index off the end of the list?  Reset to 0... best to be safe.
	{
		comboItem.startIndex = 0;
	}
}

// ===========================================================================

//Fills the combo which allows to slect a mip level for cube map export
void OptionsDialog::PopulateMipLevelsCombo()
{
	PIComboBox mipLevelCombo = GetItem(IDC_MIPLEVEL_COMBO);
	mipLevelCombo.Clear();

	for (uint32 i = 0; i <= MaxMipLevel; ++i)
	{
		stringstream s;
		s << i;
		mipLevelCombo.AppendItem(s.str().c_str());
	}
	
	uint32 selectedIndex = mDialogData.MipLevel;	// Convert level to index
	mipLevelCombo.SetCurrentSelection(selectedIndex);
}

// ===========================================================================

vector<string> GetCompressionHelpText(void)
{
	vector<string> vs;

	// Help Text
	vs.push_back("Specify the compression settings for the dds texture.\n\n\
BC1:  Also known as DXT1.  Uses 4 bits per pixel and contains RGB types of data.  Useful for color maps or normal maps if memory is tight.\n\n\
BC3:  Also known as DXT5.  Uses 8 bits per pixel and contains RGBA types of data.  Useful for color maps with full alpha, packing color and mono maps together\n\n\
BC4:  Uses 4 bits per pixel and contains grey-scale types of data.  Useful for height maps, gloss maps, font atlases or any other grey-scale image.\n\n\
BC5:  Uses 8 bits per pixel and contains 2 x grey-scale types of data.  Useful for tangent space normal maps.\n\n\
BC6:  Uses 8 bits per pixel and contains RGB floating point types of data. Useful for HDR 16 images but works only on DX11+ level hardware.\n\n\
BC7:  Uses 8 bits per pixel and contains RGBA types of data.  Useful for high quality color maps, color maps with full alpha.  It provides the best quality compression ratio but needs DX11+ level hardware.");		

	vs.push_back("Compression Options");		// Window Caption

	return vs;
}

// ===========================================================================

vector<string> GetTextureTypeHelpText(void)
{
	vector<string> vs;

	// Help Text
	vs.push_back("Specify in what way the data in your texture should be exported.\n\n\
Color:  Export only the RGB part of the image. Use for Images without alpha or when alpha is not wanted.  Can also be combined with BC4 to get the R channel as a grey-scale image, or BC5 to get the RG channels as a tangent space normal map.\n\n\
Color+Alpha:  Same as Color, but also save the alpha channel.\n\n\
CubeMap from Layers:  Export as a cube map consisting of six separate images specified in Layers. The layer names designate the face that each layer corresponds to. \n\
\"-X\" is Left, \"+Z\" is Front, \"+X\" is Right, \"-Z\" is Back, \"+Y\" is Top, \"-Y\" is Bottom.\n\n\
CubeMap Crossed:  Export as a cube map.  The image has all the cube map faces unwrapped needs to be in horizontal or vertical cross layout with aspect ratio 4x3 or 3x4.\n\n\
For cube maps, a specific mip level can be selected for exporting a low resolution cube map which is useful for low end platforms or when a blurred cube map is needed.\n\n\
Normal Map:  Export as Normal Map.  The Normalization option is available as a post process step which normalizes the values.  The Flip Red(X) and Flip Green (Y) \
option is also available as a post-process which inverts the X, Y component of the Normal Map.\n\n\
Depending on the texture type chosen, the compression formats that are not compatible are removed and cannot be selected.  The special \
post-process operations that do not apply are disabled.");

	vs.push_back("Texture Type Options");		// Window Caption

	return vs;
}

// ===========================================================================

vector<string> GetPreCompressOpsHelpText(void)
{
	vector<string> vs;

	vs.push_back("These are convenience functions that are applied before a Normal map export.\n\n\
Normalization:  This operation normalizes the Normal Map vector values for the image and all its mip maps.\n\n\
Flip Red (X):  This operation inverts the Red channel of the Normal Map.\n\n\
Flip Green (Y):  This operation inverts the Green channel of the Normal Map so that bumps become indents and indents become bumps.\n\
Note that in order to flip normal maps correctly both the Red and Green channel have to be inverted.");		// Help Text

	vs.push_back("Pre Compression Normal Map Operations");		// Window Caption

	return vs;
}

// ===========================================================================

vector<string> GetMipMapsHelpText(void)
{
	vector<string> vs;

	vs.push_back("Specify if the image will have mip maps pre-calculated in the dds file.\n\n\
None:  No Mip Maps will be generated for this image.\n\n\
Auto Generate:  The Mip Maps will be generated automatically by resizing the original image using a box filter.  The whole Mip Map range will be created \
starting from the original size until a 1x1 texture is reached.\n\n\
From Layers:  The Mip Maps will be created from the Layers within the image.  It is the responsibility of the creator to specify the correct size and the correct amount. \
The base layer will hold the first mip level (i.e. mip level 0, or the original image) and each layer after that specifies the next mip map in the chain.\n\n\
Its not needed to specify all the mip maps, the user can  choose to create a small consecutive range. The rest will be autogenerated.\n\n\
Please remember that each mip level is half the size in dimension of its previous level image and the smallest size is 1x1.");		// Help Text
	vs.push_back("MipMap Generation Options");		// Window Caption

	return vs;
}

// ===========================================================================

//Changes the context Text field of the combo box
//parameters contextStringID: id of context text control, index: ID of combo box control
void OptionsDialog::SetContextString(uint32 contextStringID, uint32 index)
{
	//Get controls
	PIText sText = GetItem(contextStringID);
	PIComboBox combo = GetItem(index);

	//Get the index straight from control
	int combo_index = int(SendMessage(combo.GetItem(), CB_GETCURSEL, 0, 0));
	
	// find the struct with the combo box data for the selected combo box
	// search the array where cobo items are stored
	for (uint32 j = 0; j < gComboItems.size(); ++j)
	{
		//Correct combo box
		if (gComboItems[j].itemNum == index)
		{
			sText.SetText(gComboItems[j].itemAndContextStrings[combo_index].itemContextInfo);
			return;
		}
	}
}

// ===========================================================================
uint32 OptionsDialog::GetSelectedItem(uint32 comboBoxID)
{
	PIComboBox combo = GetItem(comboBoxID);

	//Get the index straight from control
	int index = int(SendMessage(combo.GetItem(), CB_GETCURSEL, 0, 0));

	return index;
}

// ===========================================================================
uint32 OptionsDialog::GetSelectedMipLevelIndex()
{
	PIComboBox combo = GetItem(IDC_MIPLEVEL_COMBO);

	string selectedText;
	combo.GetCurrentSelection(selectedText);

	// Could replace atoi() call with a struct to track the values pushed into the dropdown, or a GetSelectedIndex() function on the PIComboBox itself.
	int mipLevelValue = atoi(selectedText.c_str());
	
	return mipLevelValue;
}

int32 OptionsDialog::DoModal(IntelPlugin* plugin)
{
	OptionsDialog dialog(plugin);
	int32 id = IDOK;
	
	if (plugin->GetData()->queryForParameters)
	{
		//Interactive mode, show UI
		id = dialog.Modal(NULL, NULL, IDD_MAINDIALOG);
	}
	else
	{
		//presetBatchName is a PString (first byte is the size)
		string presetStringname = reinterpret_cast<char *>(plugin->GetData()->presetBatchName)+1;

		//Load preset without UI
		id = dialog.LoadPresetNonUIMode(presetStringname)?1:2;
	}

	if (id == IDOK)		
	{
		//Fill gloabl struct with UI info
		dialog.FillGlobalStruct();
	}

	return id;
}


#endif // #if !__LP64__



// end PropetizerUI.cpp
