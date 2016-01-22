#include "GIF.h"

char m_path[MAX_PATH];
char l_path[] = "GIFX.dll";
__int64 length = 0;

HMODULE hmod;
FARPROC func;
__int64 offset1 = 0;
__int64 offset2 = 0;
__int64 offset3 = 0;

void LoadModule()
{
	//Load DLL locally
	hmod = LoadLibrary(l_path);
	
	//Import functions and calculate offset from base
	func = GetProcAddress(hmod, "Initialization");
	offset1 = (__int64)func - (__int64)hmod;
	func = GetProcAddress(hmod, "InitializeGIF");
	offset2 = (__int64)func - (__int64)hmod;
	func = GetProcAddress(hmod, "ReceiveBMPData");
	offset3 = (__int64)func - (__int64)hmod;

	//Release DLL
	FreeLibrary(hmod);

	//Global path name to load DLL remotely
	GetFullPathName(l_path, MAX_PATH, m_path, NULL);
	length = strlen(m_path);
}

struct PInfo
{
	DWORD pid;
	HWND listView;
	HWND defView;
	RECT rect;
	LPVOID func1; //Message Callback
	LPVOID func2; //Bitmap Data Query Callback
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

HWND LWND = NULL;
HWND DWND = NULL;
HANDLE proc = NULL;
RECT rect;
DWORD pid = NULL;
DWORD tid = NULL;
__int64 baseaddr = NULL;
GIF* gif = NULL;
LPVOID* framework = NULL;

void Callback(LPVOID param)
{
	cout << (char*)param << endl;
}

void SendBMPData(LPVOID param)
{
	int frame = *(int*)param;
	Frame* fdata = new Frame;
	fdata->delay = gif->bmpData[frame]->delay * 10; //Delay in milliseconds

	//Allocate memory for the bitmap data
	LPVOID mem_page = VirtualAllocEx(proc, NULL, gif->width * gif->height * 4 , MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	WriteProcessMemory(proc, mem_page, gif->bmpData[frame]->pdata, gif->width * gif->height * 4, NULL);

	//Save pointer for deallocation later on
	//framework[frame] = mem_page;

	//Save the pointer to pass on
	fdata->pdata = (DWORD*)mem_page;

	//Allocate memory for the frame data
	//We can use mem_page again because we weren't going to deallocate the other bitmap data anyways
	mem_page = VirtualAllocEx(proc, NULL, sizeof(Frame), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	WriteProcessMemory(proc, mem_page, fdata, sizeof(Frame), NULL);

	//Create a thread that enters at "ReceiveBMPData" in the DLL
	CreateRemoteThread(proc, NULL, 0, (LPTHREAD_START_ROUTINE)(baseaddr + offset3), mem_page, NULL, NULL);

	//Release the memory region taken by mem_page
	VirtualFreeEx(proc, mem_page, 0, MEM_RELEASE | MEM_DECOMMIT);
}

//Deallocate the frame data
void DeallocData(int frames)
{
	for (int i = 0; i < frames; i++)
		VirtualFreeEx(proc, framework[i], 0, MEM_RELEASE | MEM_DECOMMIT);
}

//Get thread ID and process handle
void ProcInfo()
{
	tid = GetWindowThreadProcessId(LWND, &pid);
	proc = OpenProcess(PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ | PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION, false, pid);
}

//Inject the DLL and locate its base address
void RemoteLoadModule()
{
	//Get LoadLibraryA pointer (fixed address)
	FARPROC LLFunc = GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA");

	//Allocate and write memory with strlen + 1 (null character terminator) to store the DLL path (m_path)
	//Allocate QWORD + QWORD + Function .... total = strlen + 47
	LPVOID mem_page = VirtualAllocEx(proc, NULL, length + 47, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);

	WriteProcessMemory(proc, mem_page, new byte[] { 0x48, 0x83, 0xEC, 0x28 }, 4, NULL);
	WriteProcessMemory(proc, (LPVOID)((__int64)mem_page + 4), new byte[] { 0x48, 0x8D, 0x0D }, 3, NULL);
	WriteProcessMemory(proc, (LPVOID)((__int64)mem_page + 7), new DWORD{ 18 }, 4, NULL);
	WriteProcessMemory(proc, (LPVOID)((__int64)mem_page + 11), new byte[] { 0xFF, 0x15 }, 2, NULL);
	WriteProcessMemory(proc, (LPVOID)((__int64)mem_page + 13), new DWORD{ (DWORD)length + 13 }, 4, NULL);
	WriteProcessMemory(proc, (LPVOID)((__int64)mem_page + 17), new byte[] { 0x48, 0x89, 0x05 }, 3, NULL);
	WriteProcessMemory(proc, (LPVOID)((__int64)mem_page + 20), new DWORD{ (DWORD)length + 14 }, 4, NULL);
	WriteProcessMemory(proc, (LPVOID)((__int64)mem_page + 24), new byte[] { 0x48, 0x83, 0xC4, 0x28 }, 4, NULL);
	WriteProcessMemory(proc, (LPVOID)((__int64)mem_page + 28), new byte{ 0xC3 }, 1, NULL);
	WriteProcessMemory(proc, (LPVOID)((__int64)mem_page + 29), m_path, length + 1, NULL);
	WriteProcessMemory(proc, (LPVOID)((__int64)mem_page + length + 30), &LLFunc, 8, NULL);

	HANDLE thread = CreateRemoteThread(proc, NULL, 0, (LPTHREAD_START_ROUTINE)mem_page, NULL, NULL, NULL);

	//Wait until thread exits
	WaitForSingleObject(thread, INFINITE);

	//Get Module Address
	ReadProcessMemory(proc, (LPVOID)((_int64)mem_page + length + 38), &baseaddr, 8, NULL);

	//Release the memory region taken by mem_page
	VirtualFreeEx(proc, mem_page, 0, MEM_RELEASE | MEM_DECOMMIT);

	CloseHandle(thread);
}

//Initialize the module
void Initialize()
{
	//PInfo object stores the handle and function pointers
	PInfo* info = new PInfo;
	info->pid = GetCurrentProcessId();
	info->listView = LWND;
	info->defView = DWND;
	info->rect = rect;
	info->func1 = &Callback;
	info->func2 = &SendBMPData;
	
	//Allocate and write memory with our PInfo object (size of PInfo)
	LPVOID mem_page = VirtualAllocEx(proc, NULL, sizeof(PInfo), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	WriteProcessMemory(proc, mem_page, info, sizeof(PInfo), NULL);
	
	//Create a thread that enters at "Initialize" in the DLL
	CreateRemoteThread(proc, NULL, 0, (LPTHREAD_START_ROUTINE)(baseaddr + offset1), mem_page, NULL, NULL);

	//Release the memory region taken by mem_page
	VirtualFreeEx(proc, mem_page, 0, MEM_RELEASE | MEM_DECOMMIT);
}

//Initialize the GIF Data in the remote library
void InitializeGIF()
{
	//GIFData object stores width, height, and frame count
	GIFData* data = new GIFData;
	data->width = gif->width;
	data->height = gif->height;
	data->framecount = gif->framecount;

	//Allocate and write memory with our GIFData object (size of GIFData)
	LPVOID mem_page = VirtualAllocEx(proc, NULL, sizeof(GIFData), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	WriteProcessMemory(proc, mem_page, data, sizeof(GIFData), NULL);

	//Create a thread that enters at "InitializeGIF" in the DLL
	CreateRemoteThread(proc, NULL, 0, (LPTHREAD_START_ROUTINE)(baseaddr + offset2), mem_page, NULL, NULL);

	//Release the memory region taken by mem_page
	VirtualFreeEx(proc, mem_page, 0, MEM_RELEASE | MEM_DECOMMIT);
}

int main()
{
	//Pre-Windows 8 explorer structure
	HWND progman = FindWindowA("Progman", "Program Manager");
	DWND = FindWindowEx(progman, NULL, "SHELLDLL_DefView", NULL);
	LWND = FindWindowEx(DWND, NULL, NULL, NULL);

	//Windows 8 explorer structure
	if (!DWND)
	{
		for (HWND parent = FindWindowEx(NULL, NULL, "WorkerW", NULL); parent; parent = FindWindowEx(NULL, parent, "WorkerW", NULL))
		{
			DWND = FindWindowEx(parent, NULL, "SHELLDLL_DefView", NULL);
			if (DWND)
			{
				LWND = FindWindowEx(DWND, NULL, NULL, NULL);
				break;
			}
		}
	}

	GetClientRect(LWND, &rect);

	if (!LWND)
	{
		cout << "Explorer not found" << endl;
		cin.get();
		return 0;
	}

	LoadModule();
	ProcInfo();
	RemoteLoadModule();
	Initialize();

	while (true)
	{
		delete gif;
		//delete[] framework;
		gif = NULL;
		//framework = NULL;
		cout << "GIF: ";
		char buffer[MAX_PATH];
		cin.getline(buffer, MAX_PATH);
		gif = new GIF(buffer);
		if (gif->success)
		{
			//framework = new LPVOID[gif->framecount];
			InitializeGIF();
		}
		cout << "New GIF? Hit enter";
		cin.get();
		//if (gif->success)
		//	DeallocData(gif->framecount);
	}
	return 0;
}