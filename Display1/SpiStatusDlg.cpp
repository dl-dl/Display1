#include "stdafx.h"
#include "targetver.h"
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#include <windows.h>
#include "resource.h"
#include "Recv4222.h"
#include "SpiStatusDlg.h"

INT_PTR CALLBACK DialogSpiStatus(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
	{
		char buff[1024 * 4];
		int numDev = listFtUsbDevices(buff);
		if (numDev > 0)
		{
			SetDlgItemTextA(hDlg, IDC_STATUS, buff);
			PostMessageA(GetDlgItem(hDlg, IDC_STATUS), EM_SETSEL, -1, -1);
			if (numDev > 0)
				SetDlgItemInt(hDlg, IDC_SLAVE, min(numDev - 1, slaveDevIdx), FALSE);
		}
		return (INT_PTR)TRUE;
	}

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			if (LOWORD(wParam) == IDOK)
			{
				BOOL t;
				int v1 = GetDlgItemInt(hDlg, IDC_SLAVE, &t, FALSE);
				slaveDevIdx = t ? v1 : -1;
			}
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}
