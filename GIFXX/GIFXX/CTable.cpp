#include "GIF.h"

CTable::CTable(DWORD length, BYTE colorres, BYTE back, BYTE aspect)
{
	size = length;
	pos = length;
	res = colorres;
	backcolor = back;
	ratio = aspect;
}

CTable::~CTable()
{
	delete[] table;
	for (int i = 0; i < 4096; i++)
		delete[] map[i];
	delete[] map;
	delete[] mapsize;
}

void CTable::InitTable(BYTE* cbase)
{
	table = new DWORD[size];
	map = new BYTE*[4096]; //max 4096 entries
	mapsize = new DWORD[4096];

	//Seems like color resolution doesn't affect anything?
	for (DWORD i = 0; i < size; i++)
	{
		table[i] = 0xFF000000 + cbase[2] + (cbase[1] << 8) + (cbase[0] << 16);
		map[i] = new BYTE{ (BYTE)i };
		mapsize[i] = 1;
		cbase += 3;
	}
	/*if (res == 7)
	{
		for (DWORD i = 0; i < size; i++)
		{
			table[i] = 0xFF000000 + cbase[2] + (cbase[1] << 8) + (cbase[0] << 16);
			map[i] = new BYTE{ (BYTE)i };
			mapsize[i] = 1;
			cbase += 3;
		}
	}
	else
	{
		double mod = 255.0 / ((2 << res) - 1);
		for (DWORD i = 0; i < size; i++)
		{
			//table[i] = 0xFF000000 + (DWORD)(cbase[0] * mod) + ((DWORD)(cbase[1] * mod) << 8) + ((DWORD)(cbase[2] * mod) << 16);
			table[i] = 0xFF000000 + (DWORD)(cbase[2] * mod) + ((DWORD)(cbase[1] * mod) << 8) + ((DWORD)(cbase[0] * mod) << 16);
			map[i] = new BYTE{ (BYTE)i };
			mapsize[i] = 1;
			cbase += 3;
		}
	}*/
}

void CTable::SetLZW(BYTE lzw)
{
	LZW = lzw;
	clr = 1 << lzw;
	end = clr + 1;
	pos = end + 1;
}