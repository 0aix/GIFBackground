#include "windows.h"
#include <cstdio>
#include <iostream>
#include <fstream>
using namespace std;

class GIF;
struct GIFHEADER;
struct GCEXT;
class CTable;
class BMPData;
struct WRECT;

class GIF
{
public:
	BYTE* mem;
	GIFHEADER* header;
	GCEXT* GCE;
	CTable* GCTable;
	DWORD* pdata;
	BMPData** bmpData;
	DWORD size;
	DWORD width;
	DWORD height;
	DWORD pos;
	int framecount;
	bool v89;
	bool GCT;
	bool GCExt;
	bool success;

	bool OpenGIF(char*);
	bool ProcessHeader();
	bool ProcessCT(CTable*);
	bool ProcessBlock();
	bool ProcessDataLength(int*);
	void ProcessData();
	BYTE GetBits(BYTE, BYTE, BYTE);
	GIF(char*);
	~GIF();
};

struct GIFHEADER
{
	BYTE Signature[3];
	BYTE Version[3];
	WORD Width;
	WORD Height;
	BYTE Info;
	BYTE BackgroundColor;
	BYTE AspectRatio;
};

struct GCEXT
{
	BYTE Info;
	WORD Delay;
	BYTE TCIndex;
};

class CTable
{
public:
	DWORD* table;
	BYTE** map;
	DWORD* mapsize;
	DWORD size;
	DWORD pos;
	BYTE res;
	BYTE backcolor;
	BYTE ratio;
	BYTE LZW;
	DWORD clr;
	DWORD end;

	CTable(DWORD, BYTE, BYTE, BYTE);
	~CTable();
	void InitTable(BYTE*);
	void SetLZW(BYTE);
};

class BMPData
{
public:
	DWORD* pdata;
	BYTE* data;
	BYTE* raw;
	DWORD buffer;
	int size;
	int pos;
	WRECT* area;
	bool GCE;
	bool interlaced;
	bool transparent;
	int index; //transparent color
	int type; //disposal
	int delay; //animation
	int length;
	int width;
	int height;

	CTable* LCTable;
	bool LCT; //lazy
	BYTE LZW;
	BMPData(void*);
	~BMPData();
	void InitData(BYTE*, int);
	void InitPData(DWORD*, int, int);
	void SetInfo(bool, bool);
	void SetGCE(bool, int, int, int);
	DWORD* ProcessRawData(CTable*);
	int NextBits();
	DWORD NextCode(int);
};

struct WRECT
{
	WORD left;
	WORD top;
	WORD width;
	WORD height;
};