#include "windows.h"
#include <cstdio>
#pragma comment(lib, "msimg32")

typedef HDC(WINAPI* LPBeginPaint)(HWND, LPPAINTSTRUCT);
typedef BOOL(WINAPI* LPEndPaint)(HWND, const LPPAINTSTRUCT);

LPBeginPaint pBeginPaint;
LPEndPaint pEndPaint;

struct PInfo
{
	DWORD pid;
	HWND listView;
	HWND defView;
	RECT rect;
	LPVOID func1; //Message Callback
	LPVOID func2; //Bitmap Data Query
};

struct GIFData
{
	int width;
	int height;
	int framecount;
};

struct Frame
{
	int delay;
	DWORD* pdata;
};

PInfo* info = NULL;
GIFData* data = NULL;
DWORD** bmpdata = NULL;
int* delay = NULL;
int* map = NULL;
HANDLE proc = NULL;
HMODULE hmod = NULL;
HANDLE m_proc = NULL;
BITMAPINFOHEADER bi;
HDC m_hdc = NULL;
RECT r;
int rwidth = 0;
int rheight = 0;
HDC memdc = NULL;
HBITMAP hbmp = NULL;
BLENDFUNCTION ftn = { AC_SRC_OVER, 0, 0xFF, AC_SRC_ALPHA };
HDC xmemdc = NULL;
HBITMAP xhbmp = NULL;
int width = 0;
int height = 0;
int framecount = 0;
int frame = 0;
int size = 0;
HANDLE hLoop = NULL;
bool running = false;
DWORD lasttick = 0;

//Message Callback
void Message(char* msg)
{
	//Allocate memory and write the message
	LPVOID mem_page = VirtualAllocEx(proc, NULL, strlen(msg) + 1, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	WriteProcessMemory(proc, mem_page, msg, strlen(msg) + 1, NULL);

	//Create a thread that enters at Callback with a pointer to the message at mem_page
	HANDLE thread = CreateRemoteThread(proc, NULL, 0, (LPTHREAD_START_ROUTINE)info->func1, mem_page, NULL, NULL);

	//Release the memory region taken by mem_page
	VirtualFreeEx(proc, mem_page, 0, MEM_RELEASE | MEM_DECOMMIT);
}

//Bitmap Data Query
void QueryBMPData()
{
	//Allocate memory and write the frame
	LPVOID mem_page = VirtualAllocEx(proc, NULL, 4, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	WriteProcessMemory(proc, mem_page, &frame, 4, NULL);

	//Create a thread that enters at SendBMPData with a pointer to the frame index
	HANDLE thread = CreateRemoteThread(proc, NULL, 0, (LPTHREAD_START_ROUTINE)info->func2, mem_page, NULL, NULL);

	//Release the memory region taken by mem_page
	VirtualFreeEx(proc, mem_page, 0, MEM_RELEASE | MEM_DECOMMIT);
}

//Detour for BeginPaint
HDC BPDetour(HWND hwnd, LPPAINTSTRUCT lpPaint)
{
	//If window handle matches, replace the HDC
	if (running && hwnd == info->listView)
	{
		ReleaseDC(hwnd, pBeginPaint(hwnd, lpPaint));
		return memdc;
	}

	//Return the original BeginPaint if window does not match
	return pBeginPaint(hwnd, lpPaint);
}

//Detour for EndPaint
BOOL EPDetour(HWND hwnd, const LPPAINTSTRUCT lpPaint)
{
	//If window handle matches, redraw the desktop
	if (running && hwnd == info->listView)
	{
		//Fill background with bitmap information
		//SetDIBits(xmemdc, xhbmp, 0, height, bmpdata[frame], (BITMAPINFO*)&bi, DIB_RGB_COLORS);
		StretchDIBits(xmemdc, 0, 0, rwidth, rheight, 0, 0, width, height, bmpdata[frame], (BITMAPINFO*)&bi, DIB_RGB_COLORS, SRCCOPY); //diff rasterop

		//Alpha blend desktop onto background
		AlphaBlend(xmemdc, 0, 0, rwidth, rheight, memdc, 0, 0, rwidth, rheight, ftn);

		//BitBlt memory DC buffer onto desktop
		m_hdc = GetDC(hwnd);
		BitBlt(m_hdc, 0, 0, rwidth, rheight, xmemdc, 0, 0, SRCCOPY);
		ReleaseDC(hwnd, m_hdc);
	}

	//Return the original EndPaint
	return pEndPaint(hwnd, lpPaint);
}

//Write detour to BPDetour for BeginPaint
void HookBP()
{
	//Get BeginPaint base address and copy the function
	FARPROC BPFunc = GetProcAddress(GetModuleHandleA("user32.dll"), "BeginPaint");
	DWORD dwProtect;
	VirtualProtectEx(m_proc, (LPVOID)BPFunc, 32, PAGE_EXECUTE_READWRITE, &dwProtect);
	pBeginPaint = (LPBeginPaint)VirtualAllocEx(m_proc, NULL, 32, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
	memcpy(pBeginPaint, BPFunc, 32);
	/**(__int64*)pBeginPaint = *(__int64*)BPFunc;
	*(DWORD*)((__int64)pBeginPaint + 8) = *(DWORD*)((__int64)BPFunc + 8);*/

	//mov rax, ________
	//jmp rax
	*(WORD*)BPFunc = 0xB848;
	//*(__int64*)((__int64)BPFunc + 2) = (__int64)*(DWORD*)((__int64)&BPDetour + 1) + (__int64)&BPDetour + 5; //Debug version ASM
	*(__int64*)((__int64)BPFunc + 2) = (__int64)&BPDetour;
	*(WORD*)((__int64)BPFunc + 10) = 0xE0FF;
	VirtualProtectEx(m_proc, (LPVOID)BPFunc, 32, dwProtect, NULL);
}

//Write detour to EPDetour for EndPaint
void HookEP()
{
	//Get EndPaint address and copy the function
	FARPROC EPFunc = GetProcAddress(GetModuleHandleA("user32.dll"), "EndPaint");
	DWORD dwProtect;
	VirtualProtectEx(m_proc, (LPVOID)EPFunc, 32, PAGE_EXECUTE_READWRITE, &dwProtect);
	pEndPaint = (LPEndPaint)VirtualAllocEx(m_proc, NULL, 32, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
	memcpy(pEndPaint, EPFunc, 32);
	/**(__int64*)pEndPaint = *(__int64*)EPFunc;
	*(DWORD*)((__int64)pEndPaint + 8) = *(DWORD*)((__int64)EPFunc + 8);*/

	//mov rax, ________
	//jmp rax
	*(WORD*)EPFunc = 0xB848;
	//*(__int64*)((__int64)EPFunc + 2) = (__int64)*(DWORD*)((__int64)&EPDetour + 1) + (__int64)&EPDetour + 5; //Debug version ASM
	*(__int64*)((__int64)EPFunc + 2) = (__int64)&EPDetour;
	*(WORD*)((__int64)EPFunc + 10) = 0xE0FF;
	VirtualProtectEx(m_proc, (LPVOID)EPFunc, 32, dwProtect, NULL);
}

RECT irl = { 0, 0, 1, 1 };

//Loop the screen refresh and change frames
//Going to compare DWORD and int but the values should be small enough
void InvalidLoop()
{
	for (;; Sleep(1))
	{
		if (GetTickCount() - lasttick >= delay[frame])
		{
			frame = map[frame];
			lasttick = GetTickCount();
			InvalidateRect(info->listView, &irl, true);
		}
	}
}

//Initialization called by launcher
void Initialization(LPVOID param)
{
	info = (PInfo*)param;

	//Get parent handle from process ID
	proc = OpenProcess(PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ | PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION, false, info->pid);
	
	//Get window rect for desktop
	GetClientRect(info->listView, &r);
	rwidth = r.right - r.left;
	rheight = r.bottom - r.top;

	//Get desktop DC and create a compatible dc/bitmap
	m_hdc = GetDC(info->listView);
	memdc = CreateCompatibleDC(m_hdc);
	hbmp = CreateCompatibleBitmap(m_hdc, rwidth, rheight);
	SelectObject(memdc, hbmp);

	//Copy desktop dc to memory and release
	BitBlt(memdc, 0, 0, rwidth, rheight, m_hdc, 0, 0, SRCCOPY);
	ReleaseDC(info->listView, m_hdc);

	//Use null DC for a compatible dc/bitmap
	HDC tmp = GetDC(NULL);
	xmemdc = CreateCompatibleDC(tmp);
	xhbmp = CreateCompatibleBitmap(tmp, rwidth, rheight);
	SelectObject(xmemdc, xhbmp);
	ReleaseDC(NULL, tmp);

	HookBP();
	HookEP();
}

//GIF Initialization
void InitializeGIF(LPVOID param)
{
	//Before doing anything, check if anything old exists...
	if (running)
	{
		//memory leaks here.. oh well
		running = false;
		TerminateThread(hLoop, 0);
		delete[] bmpdata;
		delete[] delay;
		delete[] map;
	}
	data = (GIFData*)param;
	width = data->width;
	height = data->height;
	framecount = data->framecount;
	frame = 0;
	size = width * height; //in DWORDs

	//Initialize the arrays that will store bitmap data
	bmpdata = new DWORD*[framecount];
	delay = new int[framecount];
	map = new int[framecount];

	//Let's just initialize the bitmap info here
	bi.biSize = sizeof(BITMAPINFOHEADER);
	bi.biWidth = width;
	bi.biHeight = height * -1; //Because BMP goes bottom up, we use negative height
	bi.biPlanes = 1;
	bi.biBitCount = 32;
	bi.biCompression = BI_RGB;
	bi.biSizeImage = 0;
	bi.biXPelsPerMeter = 0;
	bi.biYPelsPerMeter = 0;
	bi.biClrUsed = 0;
	bi.biClrImportant = 0;

	QueryBMPData();
}

//Store frame data
void ReceiveBMPData(LPVOID param)
{
	Frame* fdata = (Frame*)param;
	bmpdata[frame] = fdata->pdata;
	delay[frame] = fdata->delay;
	map[frame] = frame + 1;

	frame++;

	if (frame < framecount)
		QueryBMPData();
	else
	{
		//Start the animated background
		map[frame - 1] = 0;
		frame = 0;
		lasttick = GetTickCount();
		running = true;

		//InvalidateRect refresh loop
		hLoop = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)&InvalidLoop, NULL, NULL, NULL);
	}
}

BOOL APIENTRY DllMain(HANDLE hModule, DWORD dwReason, LPVOID lpReserved)
{
	if (dwReason == DLL_PROCESS_ATTACH)
	{
		m_proc = GetCurrentProcess();
		hmod = (HMODULE)hModule;
		DisableThreadLibraryCalls(hmod);
	}

	return TRUE;
}