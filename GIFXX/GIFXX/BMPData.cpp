#include "GIF.h"

BMPData::BMPData(void* wrect)
{
	area = (WRECT*)wrect;
	data = new BYTE[area->width * area->height];
	ZeroMemory(data, area->width * area->height);
	GCE = false;
}

BMPData::~BMPData()
{
	delete[] pdata;
	delete[] data;
	delete[] raw;
}

void BMPData::InitData(BYTE* mem, int len)
{
	length = len;
	raw = new BYTE[length];
	ZeroMemory(data, length);
	int bsize;
	int npos = 0;
	int n = 0;
	while ((bsize = mem[npos]) != 0)
	{
		npos++;
		memcpy(raw + n, mem + npos, bsize);
		npos += bsize;
		n += bsize;
	}
	length = len;
}

void BMPData::InitPData(DWORD* p, int w, int h) //soooo lazy
{
	width = w;
	height = h;
	pdata = new DWORD[w * h];
	memcpy(pdata, p, w * h * 4);
}

void BMPData::SetInfo(bool i, bool lct)
{
	interlaced = i;
	LCT = lct;
}

void BMPData::SetGCE(bool t, int i, int ty, int d)
{
	transparent = t;
	index = i;
	type = ty;
	delay = d;
	GCE = true;
}

DWORD* BMPData::ProcessRawData(CTable* ct)
{
	if (LCT) //probably won't be used; i hope no one uses interlaced
		ct = LCTable;

	ct->SetLZW(LZW);

	DWORD* ntable = ct->table;
	BYTE** nmap = ct->map;
	DWORD* nmapsize = ct->mapsize;
	BYTE lzw = ct->LZW;
	int check = 1 << lzw;
	int clr = ct->clr;
	int end = ct->end;
	int pcode = -1;
	int npos = ct->pos;
	int n = 0;

	for (int bits = NextBits();; bits = NextBits())
	{
		if (npos < 4096 && npos >= check) //lzw check (>= to check 1 byte beforehand)
		{
			lzw++;
			check <<= 1;
		}
		if (bits < lzw) //lzw check ini aniugbafpignfd;g
			continue;

		DWORD code = NextCode(lzw);
		if (code == clr)
		{
			//init/reset table
			lzw = ct->LZW; //let's just reset this first
			check = 1 << lzw;
			for (int i = end + 1; i < npos; i++)
				delete[] nmap[i];
			npos = end + 1; //can't forget that!
			pcode = -1;
			continue;
		}
		else if (code == end)
			break;

		//add new code first
		if (npos < 4096 && pcode > -1) //limit to 4096
		{
			int tsize = nmapsize[pcode];
			nmap[npos] = new BYTE[tsize + 1];
			nmapsize[npos] = tsize + 1;
			memcpy(nmap[npos], nmap[pcode], tsize);
			if (code == npos)
				nmap[npos][tsize] = nmap[npos][0];
			else
				nmap[npos][tsize] = nmap[code][0];
			npos++;
		}
		memcpy(&data[n], nmap[code], nmapsize[code]);
		n += nmapsize[code];
		pcode = code;
	}
	ct->pos = npos; //cuz we messed around with npos a lot
	//time to write bitmap data!! ... backwards cuz bitmaps are weird. they go from down to up
	int rw = area->width;
	int rh = area->height;
	int px = area->top * width + area->left;
	int count = 0;
	if (transparent) //ugly but saves computing time
	{
		for (int x = 0; x < rh; x++) //weird. who cares. im tired
		{
			for (int y = 0; y < rw; y++)
			{
				if (data[count] == index)
				{
					count++;
					continue;
				}
				pdata[px + y] = ntable[data[count]];
				count++;
			}
			px += width;
		}
	}
	else
	{
		for (int x = 0; x < rh; x++) //weird. who cares. im tired
		{
			for (int y = 0; y < rw; y++)
			{
				pdata[px + y] = ntable[data[count]];
				count++;
			}
			px += width;
		}
	}

	DWORD* npdata = new DWORD[width * height];
	if (type < 3)
	{
		memcpy(npdata, pdata, width * height * 4);
		if (type == 2)
		{
			px = area->top * width + area->left;
			DWORD back = ntable[ct->backcolor];
			for (int x = 0; x < rh; x++) //weird. who cares. im tired
			{
				for (int y = 0; y < rw; y++)
				{
					pdata[px + y] = back;
					count++;
				}
				px += width;
			}
		}
	}
	else
		return NULL;
	return npdata;
}

int BMPData::NextBits() //no error-checking; blame the gif
{
	buffer |= raw[pos] << size; //try pos++ here next time
	pos++;
	size += 8;
	return size;
}

DWORD BMPData::NextCode(int LZW)
{
	DWORD a = ((1 << LZW) - 1);
	DWORD code = buffer & ((1 << LZW) - 1);
	buffer >>= LZW;
	size -= LZW;
	return code;
}