#include <windows.h>

LANGUAGE LANG_NEUTRAL, SUBLANG_NEUTRAL
"IDD_NMDLG" DIALOG 0,0, 175, 40
STYLE  DS_CENTER | DS_MODALFRAME | DS_SHELLFONT | WS_CAPTION | WS_VISIBLE | WS_POPUP | WS_SYSMENU
CAPTION "Enter computers name"
FONT 10, "Arial"
{
	DEFPUSHBUTTON "OK", IDOK, 115, 10, 50, 14
	EDITTEXT 1337, 10, 10, 100, 14, ES_LEFT | WS_BORDER 
}