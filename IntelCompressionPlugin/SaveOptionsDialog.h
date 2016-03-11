/*

  Copyright (c) 2015, Intel Corporation
  All rights reserved.

*/

#pragma once

#include "PIUI.h"
#include "IntelPlugin.h"

#undef true
#undef false

#include <vector>
#include <map>


//This struct hold the UI data, all info do produced a compressed image is located here. used also to store/load into presets.
struct DialogData
{
	// NOTE - assumed to be POD.  
	std::string PresetName;           //The name of this preset
	uint32 CompressionTypeIndex;      //The current selection index of the combo box holding the compression. Translate into encoding with matrix CompressionVsTextureTypeMatrix in IntelPlugin.cpp
	TextureTypeEnum TextureTypeIndex;    //Holds the texture type of the image
	MipmapEnum  MipMapTypeIndex;     //Holds the mip map generation method

	uint32 MipLevel;			      // only valid if SetMipLevel == true, specified the mip level to be exported when in cube map mode.

	bool SetMipLevel;                 //Specify that a specific mip level has to be exported. Only valid for cube map mode.
	bool Normalize;                   //Specify if normalization of values is needed. Only valid for Normal Maps 
	bool FlipX;
	bool FlipY;
};

// Our main class - handles creation of and all input to the Export dialog UI.
class OptionsDialog : public PIDialog 
{
private:
	// Struct to hold a dropdown list item and the corresponding context info for it. 
	// The userData field can hold specific IDs depending on the dropdown. For example in the compression drop box it holds the CompressionTypeEnum ID. 
	// Assembled for each such dropdown in functions below.
	struct ComboItemAndContext
	{
		string itemText;
		string itemContextInfo;
		int    itemUserData;

		ComboItemAndContext(string text, string contextInfo, int userData=0) : itemText(text), itemContextInfo(contextInfo), itemUserData(userData) { }
	};


	// Data to init the dropdown lists (Windows calls them ComboBoxes)
	struct ComboData
	{
		const uint32 itemNum;								// WinForm ID of the dropdown list for this particular dropdown
		uint32 startIndex;									// Which item in the dropdown to select when creating the list - 0 unless a preset is being loaded.
		vector<ComboItemAndContext> itemAndContextStrings;	// list of strings and any corresponding context info for the dropdown list (context could be blank)

		explicit  ComboData(int num) : itemNum(num)  { }
	};
	
	enum {PRESETS_COMBO, COMPRESSION_COMBO, TEXTURETYPE_COMBO, MIPMAP_COMBO, NUMBEROF_COMBOS};

    IntelPlugin* plugin;              //Pointer to photoshop API
    IntelPlugin::Globals* globalParams;              //Pointer to photoshop API
	DialogData mDialogData;			// Current working data set - updated based on user interaction with UI
	map<string, DialogData> mPresets;

	vector<ComboData> gComboItems;					// The master list of dropdown init data.

	string mPathToPresetDirectory;
	uint32 MaxMipLevel;			// based on image properties

	void LoadPresets(void);
	void ReadPreset(string fname);

	void SaveNewPreset(string presetName, DialogData dd);
	void UpdatePreset(string presetName, DialogData dd);
	void DeletePreset(string presetName);

	void InitDataNoPreset(DialogData & dd);
	void InitDataFromPreset(string presetName);

	void SetUIFromData();

	void ExtractDataFromUI(DialogData & dd);
	
	void GetPresetNames(ComboData & comboItem);
	void GetCompressionNames(ComboData & comboItem);
	void GetTextureTypeNames(ComboData & comboItem);
	void GetMipMapNames(ComboData & comboItem);

	void PopulateMipLevelsCombo();
	void DisableUnavailableControls();
	void UpdateCompressionCombo();
	void UpdateMipMapCombo();

	void SetFontCompressionCombo();
	void InitComboItems();

	void InitComboFromItems(int32 comboItemsIndex);

	uint32 GetSelectedItem(uint32 comboBoxID);
	uint32 GetSelectedMipLevelIndex();
	void SetContextString(uint32 contextStringID, uint32 index);

	// overrides

	virtual void Init(void) override;
	virtual void Notify(int32 item) override;

	double Log2( double n );
		
public:
	explicit  OptionsDialog(IntelPlugin* globals);
	~OptionsDialog() {}
	
	bool LoadPresetNonUIMode(string name);
	void FillGlobalStruct();
	void GetGlobalStruct();
	const DialogData & GetData() const { return mDialogData; }

	static int32 DoModal(IntelPlugin* plugin);
};


#define PRESET_FILE_VERSION (int)100
#define LAST_SETTINGS_PRESET_NAME "-last-used-"

// #define AUTO_SAVE_PRESETS // sets presets to autosave

