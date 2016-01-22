#include "GIF.h"

GIF::GIF(char* path)
{
	width = 0;
	height = 0;
	pos = 0;
	framecount = 0;
	GCExt = false;

	if (!OpenGIF(path))
		return;

	if (!ProcessHeader())
		return;

	if (GCT)
	{
		if (!ProcessCT(GCTable))
			return;
		DWORD fill = GCTable->table[header->BackgroundColor];
		DWORD pxcount = width * height;
		for (int i = 0; i < pxcount; i++)
			pdata[i] = fill;
	}

	while (!ProcessBlock());

	ProcessData();

	success = true;
	cout << "done" << endl;
}

GIF::~GIF()
{
	delete[] mem;
	delete[] pdata;
	for (int i = 0; i < framecount; i++)
		delete bmpData[i];
	delete[] bmpData;
}

bool GIF::OpenGIF(char* path)
{
	ifstream file(path, ios::binary | ios::ate);
	if (file)
	{
		size = file.tellg();
		size++; //Add 1 byte for a convenience-based null-terminator
		file.seekg(0, file.beg);
		mem = new byte[size];
		file.read((char*)mem, size);
		file.close();
		mem[size - 1] = 0x00; //null-terminator
		cout << "file size: " << size - 1 << endl;
		return true;
	}
	cout << "file does not exist" << endl;
	cin.get();
	return false;
}

bool GIF::ProcessHeader()
{
	if (sizeof(GIFHEADER) - 1 < size)
	{
		header = (GIFHEADER*)mem;
		if (memcmp("GIF", header->Signature, 3) != 0)
			goto error;

		if (memcmp("87a", header->Version, 3) == 0)
			v89 = false;
		else if (memcmp("89a", header->Version, 3) == 0)
			v89 = true;
		else
			goto error;

		width = header->Width;
		height = header->Height;
		pdata = new DWORD[width * height];
		ZeroMemory(pdata, width * height * 4);

		BYTE b = header->Info;
		GCTable = new CTable(2 << GetBits(b, 0, 3), GetBits(b, 4, 3), header->BackgroundColor, header->AspectRatio);
		GCT = GetBits(b, 7, 1) == 1;
		pos += 13; //proper size of GIFHEADER
		return true;
	}

error:
	cout << "header error" << endl;
	cin.get();
	return false;
}

bool GIF::ProcessCT(CTable* ct)
{
	if (pos + ct->size * 3 <= size) //Each entry has 3 bytes
	{
		ct->InitTable(mem + pos);
		pos += ct->size * 3;
		return true;
	}
	cout << "couldn't open color table" << endl;
	cin.get();
	return false;
}

bool GIF::ProcessBlock()
{
	switch (mem[pos])
	{
	case 0x21:
	{
		pos++;
		switch (mem[pos])
		{
		case 0xF9:
		{
			pos += 2; //increment and skip byte count (0x04)
			GCE = new GCEXT();
			GCE->Info = mem[pos];
			GCE->Delay = *(WORD*)(mem + pos + 1);
			GCE->TCIndex = mem[pos + 3];
			pos += 4; //proper size of GCEXT
			GCExt = true;
		}
			break;
		default: //skip other extensions
			pos++;
			break;
		}
	}
		break;
	case 0x2C:
	{
		pos++;
		BMPData** tmpData = bmpData;
		bmpData = new BMPData*[framecount + 1];
		for (int i = 0; i < framecount; i++)
			bmpData[i] = tmpData[i];
		BMPData* tmpBMP = new BMPData(mem + pos);
		bmpData[framecount] = tmpBMP;
		pos += 8; //proper size of WRECT
		BYTE b = mem[pos];
		tmpBMP->SetInfo(GetBits(b, 6, 1) == 1, GetBits(b, 7, 1) == 1);
		if (tmpBMP->LCT)
			tmpBMP->LCTable = new CTable(2 << GetBits(b, 0, 3), GCTable->res, header->BackgroundColor, header->AspectRatio);
		pos++;
		if (GCExt)
		{
			b = GCE->Info;
			tmpBMP->SetGCE(GetBits(b, 0, 1) == 1, GCE->TCIndex, GetBits(b, 2, 3), GCE->Delay);
			GCExt = false;
		}
		if (tmpBMP->LCT)
			ProcessCT(tmpBMP->LCTable);

		tmpBMP->LZW = mem[pos];
		pos++;
		int length = 0;
		int npos = pos;
		while (!ProcessDataLength(&length));
		bmpData[framecount]->InitData(mem + npos, length);
		framecount++;
	}
		break;
	case 0x3B:
		cout << "done processing frames" << endl;
		return true;
	default: //handle random blocks and terminators
		pos += mem[pos++];
		break;
	}
	return false;
}

bool GIF::ProcessDataLength(int* length)
{
	int i = mem[pos];
	*length += i;
	pos += i + 1;
	return i == 0;
}

void GIF::ProcessData()
{
	for (int i = 0; i < framecount; i++)
	{
		BMPData* tmp = bmpData[i];
		tmp->InitPData(pdata, width, height);
		DWORD* pdw = tmp->ProcessRawData(GCTable);
		if (pdw)
		{
			delete[] pdata;
			pdata = pdw;
		}
	}
}

BYTE GIF::GetBits(BYTE val, BYTE start, BYTE count)//assume start and count are valid
{
	return (val >> start) & ((1 << count) - 1);
}