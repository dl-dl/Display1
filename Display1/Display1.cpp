#include "stdafx.h"
#include "targetver.h"
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#include <windows.h>
#include "resource.h"

#include "Protocol.h"
#include "Graph.h"
#include "Recv4222.h"
#include "SpiStatusDlg.h"

struct ScreenData
{
	HDC hdcMem;
	HBITMAP hbmp;
	BITMAPINFO info;
	HGDIOBJ oldBmp;

	ScreenData()
	{
		hdcMem = NULL;
		hbmp = NULL;
		oldBmp = NULL;
		memset(&info, 0, sizeof(info));
	}

	~ScreenData()
	{
		SelectObject(hdcMem, oldBmp);
		DeleteDC(hdcMem);
		DeleteObject(hbmp);
	}

	void init(HWND hwnd)
	{
		HDC hdc = GetDC(hwnd);
		hdcMem = CreateCompatibleDC(hdc);
		ASSERT_DBG(hdcMem);
		hbmp = CreateCompatibleBitmap(hdc, SCREEN_DX, SCREEN_DY);
		ASSERT_DBG(hbmp);
		oldBmp = SelectObject(hdcMem, hbmp);
		ReleaseDC(hwnd, hdc);

		info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		info.bmiHeader.biBitCount = 24;
		info.bmiHeader.biPlanes = 1;
		info.bmiHeader.biCompression = BI_RGB;
		info.bmiHeader.biWidth = SCREEN_DX;
		info.bmiHeader.biHeight = SCREEN_DY;
	}

	void setData(unsigned int line, const void* pbits)
	{
		ASSERT_DBG(line < SCREEN_DX);
//		int res = SetDIBits(hdcMem, hbmp, SCREEN_DY - 1 - line, 1, pbits, &info, DIB_RGB_COLORS);
		const BYTE* p = (BYTE*)pbits;
		for (int y = 0; y < SCREEN_DY; ++y)
			SetPixel(hdcMem, line, y, RGB(p[y * 3], p[y * 3 + 1], p[y * 3 + 2]));
//		ASSERT_DBG(res);
	}

	void draw(HDC hdc) const
	{
		BOOL res = BitBlt(hdc, 0, 0, SCREEN_DX, SCREEN_DY, hdcMem, 0, 0, SRCCOPY);
		ASSERT_DBG(res);
	}
};

struct AppGlobals
{
	HINSTANCE hInst;
	//	HACCEL hAccelTable;
	const WCHAR* mainWndClassName;
	HWND mainWnd;
	ScreenData screen;
};

static AppGlobals appGlobals;

static LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);

static ATOM MyRegisterClass(const AppGlobals* g)
{
	WNDCLASSEXW wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style = 0; //CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = g->hInst;
	wcex.hIcon = LoadIcon(g->hInst, MAKEINTRESOURCE(IDI_DEVICEFACE1));
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_DEVICEFACE1);
	wcex.lpszClassName = g->mainWndClassName;
	wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	return RegisterClassExW(&wcex);
}

void __cdecl _translateSEH(unsigned int, EXCEPTION_POINTERS*)
{
	throw "SEH";
}

static void InitInstance(AppGlobals* g, HINSTANCE hInstance)
{
	g->hInst = hInstance;
	g->mainWndClassName = L"DisplayMainWndClass";
	//	g->hAccelTable = LoadAccelerators(g->hInst, MAKEINTRESOURCE(IDC_DEVICEFACE1));
	_set_se_translator(_translateSEH);
}

static HWND InitMainWnd(const AppGlobals* g, int nCmdShow)
{
	if (!MyRegisterClass(g))
		return FALSE;

	const WCHAR szTitle[] = L"Display";
	HWND hWnd = CreateWindowW(g->mainWndClassName, szTitle, WS_OVERLAPPEDWINDOW,
							  CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, g->hInst, nullptr);
	if (!hWnd)
		return NULL;

	return hWnd;
}

static WPARAM MainLoop(const AppGlobals* g)
{
	MSG msg;
	while (GetMessage(&msg, nullptr, 0, 0))
	{
		//		if (!TranslateAccelerator(msg.hwnd, g->hAccelTable, &msg))
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return msg.wParam;
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
					  _In_opt_ HINSTANCE hPrevInstance,
					  _In_ LPWSTR    lpCmdLine,
					  _In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	InitInstance(&appGlobals, hInstance);
	if (!(appGlobals.mainWnd = InitMainWnd(&appGlobals, nCmdShow)))
		return FALSE;

	appGlobals.screen.init(appGlobals.mainWnd);

	ShowWindow(appGlobals.mainWnd, nCmdShow);
	UpdateWindow(appGlobals.mainWnd);

	if (IDOK != DialogBox(appGlobals.hInst, MAKEINTRESOURCE(IDD_DIALOG1), appGlobals.mainWnd, DialogSpiStatus))
		return 1;
	if (slaveDevIdx < 0)
		return 1;

	HANDLE hComm = CreateThread(NULL, 0, ftRecv, 0, 0, NULL);
	if (NULL == hComm)
		return 2;

	MainLoop(&appGlobals);

	terminateComm = true;
	WaitForSingleObject(hComm, 5000);
	CloseHandle(hComm);

	return 0;
}

static BOOL ProcessMenu(HWND hWnd, WORD wmId)
{
	switch (wmId)
	{
	case IDM_EXIT:
		DestroyWindow(hWnd);
		break;
	default:
		return FALSE;
	}
	return TRUE;
}

void gPostMsg(int userMsg, int param1, int param2)
{
	PostMessageW(appGlobals.mainWnd, WM_USER + userMsg, param1, param2);
}

void gPostMsg(int userMsg, int param1, void* param2)
{
	PostMessageW(appGlobals.mainWnd, WM_USER + userMsg, param1, (LPARAM)param2);
}

static inline void redraw(HDC hdc)
{
	appGlobals.screen.draw(hdc);
}

static void drawFrame(HDC hdc)
{
	MoveToEx(hdc, SCREEN_DX, 0, nullptr);
	LineTo(hdc, SCREEN_DX, SCREEN_DY);
	LineTo(hdc, 0, SCREEN_DY);
}

class PaintContext
{
public:
	PAINTSTRUCT ps;
	HDC hdc;
	HWND hWnd;

	PaintContext(HWND hWnd_)
	{
		hWnd = hWnd_;
		hdc = BeginPaint(hWnd, &ps);
	}
	~PaintContext()
	{
		EndPaint(hWnd, &ps);
	}
	//	PaintContext(const PaintContext&) = delete;
	//	const PaintContext& operator=(const PaintContext&) = delete;
};


LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	try
	{
		switch (message)
		{
		case WM_COMMAND:
		{
			if (!ProcessMenu(hWnd, LOWORD(wParam)))
				return DefWindowProc(hWnd, message, wParam, lParam);
		}
		break;

		case (WM_USER + WM_USER_MSG_LINE_DATA):
		{
			PointDataMsg* p = reinterpret_cast<PointDataMsg*>(lParam);
			appGlobals.screen.setData(p->addr, p->data);
			delete p;
			InvalidateRect(hWnd, nullptr, FALSE);
		}
		break;
		case WM_LBUTTONDOWN:
		{
			const int x = LOWORD(lParam);
			const int y = HIWORD(lParam);
		}
		break;

		case WM_PAINT:
		{
			PaintContext ctx(hWnd);
			redraw(ctx.hdc);
			drawFrame(ctx.hdc);
		}
		break;

		case WM_ERASEBKGND:
			return 1;
			break;

		case WM_DESTROY:
			PostQuitMessage(0);
			break;

		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
	}
	catch (...)
	{
		Beep(880, 200);
	}
	return 0;
}
