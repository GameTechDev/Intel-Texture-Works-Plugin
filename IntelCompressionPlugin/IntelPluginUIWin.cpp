/*

  Copyright (c) 2015, Intel Corporation
  All rights reserved.

*/

#include "IntelPlugin.h"

#include "SaveOptionsDialog.h"
#include "IntelPluginUIWin.h"
#include "IntelPluginName.h"
#include "resource.h"




/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/
//Simple plugin About box
INT_PTR WINAPI AboutDlgProcIntel(HWND hDlg, UINT wMsg, WPARAM wParam, LPARAM /*lParam*/)
{
	switch  (wMsg) 
	{
		case WM_INITDIALOG:
			{
				//Set window name
				string windowTitle = DDSExporterPluginName;
				windowTitle += DDSExporterPluginVersion;
				SetWindowText(hDlg, windowTitle.c_str());
                CenterDialog(hDlg);	
			}
            break;

		case WM_CHAR:
			{
				TCHAR chCharCode = TCHAR(wParam);
				if (chCharCode == VK_ESCAPE || chCharCode == VK_RETURN)
					EndDialog(hDlg, 0);
			}
			break;
 
		case WM_LBUTTONDOWN:
				EndDialog(hDlg, 0);
            break;

		case WM_COMMAND:
			switch  (COMMANDID(wParam)) 
			{
				case OK:
					EndDialog(hDlg, 0);
                    break;

				case CANCEL:
					EndDialog(hDlg, 0);
					break;

				default:
                    return FALSE;
            }
            break;

      default:
		  return  FALSE;
	
	} // switch (wMsg)

    return  TRUE;
}

void ShowAboutIntel (AboutRecordPtr aboutPtr)
{
	PlatformData * platform = static_cast<PlatformData *>(aboutPtr->platformData);
	
	HWND h = reinterpret_cast<HWND>(platform->hwnd);

    DialogBoxParam(GetDLLInstance(), 
		           MAKEINTRESOURCE( IDD_ABOUT ),
	               h, 
				   AboutDlgProcIntel, 
				   0);
}


/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/
//Get name dialog
INT_PTR WINAPI DlgGetNameProc(HWND hDlg, UINT wMsg, WPARAM wParam, LPARAM lParam)
{
	static string * str;

	switch  (wMsg) 
	{
		case WM_INITDIALOG:
			{
				if (lParam)
				{
					str = reinterpret_cast<string*>(lParam);
					SetDlgItemTextA(hDlg, IDC_NAME_EDIT, str->c_str());
				}
                CenterDialog(hDlg);	
			}
            break;


		case WM_COMMAND:
			switch  (COMMANDID(wParam)) 
			{
				case IDOK:
					{
						char buf[MAX_PATH+1] = {};
						GetDlgItemTextA(hDlg, IDC_NAME_EDIT, buf, MAX_PATH);
						*str = buf;
						str = NULL;
						EndDialog(hDlg, IDOK);
					}
                    break;

				case IDCANCEL:
					str = NULL;
					EndDialog(hDlg, IDCANCEL);
					break;

				default:
                    return FALSE;
            }
            break;

      default:
		  return  FALSE;
	
	} // switch (wMsg)

    return  TRUE;
}

string GetPresetName(string str, HWND parent)
{
    if (DialogBoxParam(GetDLLInstance(), 
		           MAKEINTRESOURCE( IDD_GETNAME ),
	               parent, 
				   DlgGetNameProc, 
				   reinterpret_cast<LONG_PTR>(&str)) == IDOK)
	{
		return str;
	}

	return "";
}

/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/
//Load Dialog box
INT_PTR WINAPI DlgLoadProc(HWND hDlg, UINT wMsg, WPARAM wParam, LPARAM lParam)
{
	static unsigned8* result;

	switch  (wMsg) 
	{
		case WM_INITDIALOG:
			{
				if (lParam)
				{
					result = reinterpret_cast<unsigned8*>(lParam);

					//If no mip maps hide controls
					if (!(*result & LoadInfoEnum::USE_MIPMAPS))
					{
						ShowWindow(GetDlgItem(hDlg, IDC_LOADDIALOG_MIPMAPGROUP), SW_HIDE);
						ShowWindow(GetDlgItem(hDlg, IDC_LOADDIALOG_MIPMAPCHECK), SW_HIDE);
					}

					//If no alpha hide controls
					if (!(*result & LoadInfoEnum::USE_SEPARATEALPHA))
					{
						ShowWindow(GetDlgItem(hDlg, IDC_LOADDIALOG_ALPHAGROUP), SW_HIDE);
						ShowWindow(GetDlgItem(hDlg, IDC_LOADDIALOG_ALPHACHECK), SW_HIDE);

						//move mipmap controls up
						RECT rcGroup = {};
						RECT rcCheckbox = {};
						HWND group = GetDlgItem(hDlg, IDC_LOADDIALOG_MIPMAPGROUP);
						HWND checkbox = GetDlgItem(hDlg, IDC_LOADDIALOG_MIPMAPCHECK);
						
						//Get alpha rectangles in client coordinates and set them to mip map controls
                    	GetWindowRect(GetDlgItem(hDlg, IDC_LOADDIALOG_ALPHAGROUP), &rcGroup);
						MapWindowRect(NULL, hDlg, &rcGroup);
						MoveWindow(group, rcGroup.left, rcGroup.top, rcGroup.right - rcGroup.left, rcGroup.bottom - rcGroup.top, TRUE);
						
						GetWindowRect(GetDlgItem(hDlg, IDC_LOADDIALOG_ALPHACHECK), &rcCheckbox);
						MapWindowRect(NULL, hDlg, &rcCheckbox);
						MoveWindow(checkbox, rcCheckbox.left, rcCheckbox.top, rcCheckbox.right - rcCheckbox.left, rcCheckbox.bottom - rcCheckbox.top, TRUE);
					}

					//Reset result to pass back info
					*result = LoadInfoEnum::USE_NONE;
				}
                CenterDialog(hDlg);	
			}
            break;

		case WM_COMMAND:
			switch  (COMMANDID(wParam)) 
			{
				case IDOK:
					{
						EndDialog(hDlg, IDOK);
					}
                    break;

				case IDCANCEL:
					{
						*result = LoadInfoEnum::USE_NONE;
						EndDialog(hDlg, IDCANCEL);
					}
					break;
				case IDC_LOADDIALOG_MIPMAPCHECK:
					{
						HWND alphachkbox = GetDlgItem(hDlg, IDC_LOADDIALOG_ALPHACHECK);
						bool checked = (SendMessage(GetDlgItem(hDlg, IDC_LOADDIALOG_MIPMAPCHECK), BM_GETCHECK, 0, 0) == BST_CHECKED);
						
						//Set the flag depending on the checked state
						if (checked) 
						{
						    *result |= LoadInfoEnum::USE_MIPMAPS;

							//uncheck and disable separate alpha setting on mip maps;
							if (GetWindowLong(alphachkbox, GWL_STYLE) | WS_VISIBLE)
							{
								SendMessage(alphachkbox, BM_SETCHECK, 0, 0);
								::EnableWindow(alphachkbox, false);
							}
						}
						else
						{
						    *result &= ~LoadInfoEnum::USE_MIPMAPS;

							//reenable alpha checkbox
							if (GetWindowLong(alphachkbox, GWL_STYLE) | WS_VISIBLE)
							{
								::EnableWindow(alphachkbox, true);
							}
						}
					}
					break;
				case IDC_LOADDIALOG_ALPHACHECK:
					{
						bool checked = (SendMessage(GetDlgItem(hDlg, IDC_LOADDIALOG_ALPHACHECK), BM_GETCHECK, 0, 0) == BST_CHECKED);
						
						//set the flag depending on the checked state
						if (checked) 
						    *result |= LoadInfoEnum::USE_SEPARATEALPHA;
						else
						    *result &= ~LoadInfoEnum::USE_SEPARATEALPHA;
					}
					break;

				default:
                    return FALSE;
            }
            break;

	  case WM_CTLCOLORSTATIC: 
		  { 
			//set the background color of the chackbox controls
			 static HBRUSH hBrushColor; 
		     if (!hBrushColor) 
			 { 
				 hBrushColor = CreateSolidBrush(RGB(255, 255, 255)); 
				 SetBkColor((HDC)wParam, RGB(255, 255, 255)); 
			 } 
			 return (LRESULT)hBrushColor; 
		  }
		  break;

	  case WM_PAINT:
		   {
				PAINTSTRUCT ps;
				HDC hDC;
				hDC = BeginPaint(hDlg, &ps);	
				RECT rcParent = {5, 5, 295, 115};

				//Paint the white rectabgle where the checkboxes are positioned
				FillRect(hDC, &rcParent, static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
				EndPaint(hDlg, &ps);
				return FALSE;
			}
		  break;

      default:
		  return  FALSE;
	
	} // switch (wMsg)

    return  TRUE;
}

unsigned8 ShowLoadDialog (bool showAlphaGroup, bool showMipMapGroup, HWND parent)
{
	//encode flags into results
	unsigned8 result = LoadInfoEnum::USE_NONE;
	result |=  showAlphaGroup ? LoadInfoEnum::USE_SEPARATEALPHA :  LoadInfoEnum::USE_NONE;
	result |=  showMipMapGroup ? LoadInfoEnum::USE_MIPMAPS :  LoadInfoEnum::USE_NONE;

	if (DialogBoxParam(GetDLLInstance(), 
					MAKEINTRESOURCE( IDD_LOADDIALOG ),
					parent, 
					DlgLoadProc, 
					reinterpret_cast<LONG_PTR>(&result)) == IDOK)
	{
		return result;
	}

	return 0;
}


//Show just a message box
void errorMessage(std::string msg, std::string title)
{
	MessageBox(GetActiveWindow(), msg.c_str(), title.c_str(), MB_OK);
}

// end OutboundUIWin.cpp