/*

  Copyright (c) 2015, Intel Corporation
  All rights reserved.

*/

#pragma once

void    ShowAboutIntel (AboutRecordPtr aboutPtr);        // Pop about box.
std::string GetPresetName(std::string str, HWND parent); //Get preset name dialog
unsigned8 ShowLoadDialog (bool showAlphaGroup, bool showMipMapGroup, HWND parent); //Load dialog
void        errorMessage(std::string msg, std::string title); //pops a message dialog

