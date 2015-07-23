#include <Windows.h>
#include "png.h"

extern "C" void OnPngRead(png_structp Png, png_bytep Buffer, png_size_t Length)
{
	DWORD _d;
	HANDLE PngFile;

	PngFile = (HANDLE)::png_get_io_ptr(Png);
	::ReadFile(PngFile, Buffer, (DWORD)Length, &_d, nullptr);
}

extern "C" bool SetupPngHandling(const TCHAR * File, HANDLE * PngFile, png_structp * Png, png_infop * PngInfo)
{
	HANDLE _PngFile;
	png_structp _Png;
	png_infop _PngInfo;

	_PngFile = ::CreateFile(File, FILE_READ_DATA, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
	if(_PngFile == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	_Png = ::png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
	if(!_Png)
	{
		::CloseHandle(_PngFile);
		return false;
	}

	_PngInfo = ::png_create_info_struct(_Png);
	if(!_PngInfo)
	{
		::png_destroy_read_struct(&_Png, (png_infopp)nullptr, (png_infopp)nullptr);
		::CloseHandle(_PngFile);
		return false;
	}

	/*
	if(::setjmp(png_jmpbuf(_Png)))
	{
	::png_destroy_read_struct(&_Png, &_PngInfo, (png_infopp)nullptr);
	::CloseHandle(PngFile);
	return false;
	}
	*/

	::png_set_read_fn(_Png, _PngFile, OnPngRead);

	*PngFile = _PngFile;
	*Png = _Png;
	*PngInfo = _PngInfo;
	return true;
}

extern "C" unsigned char * LoadPng(const TCHAR * File, size_t * RawLength, unsigned int * Width, unsigned int * Height, unsigned int * WidthRaw)
{
	HANDLE PngFile;
	png_structp Png;
	png_infop PngInfo;
	png_byte ** Image;
	unsigned char * PngImageBuffer;
	unsigned char * _b;
	size_t _s;
	unsigned int _w;
	unsigned int _wr;
	unsigned int _h;
	unsigned int _y;
	unsigned int OppositeY;

	if(!::SetupPngHandling(File, &PngFile, &Png, &PngInfo))
	{
		return nullptr;
	}

	::png_read_png(Png, PngInfo, PNG_TRANSFORM_BGR, nullptr);

	Image = ::png_get_rows(Png, PngInfo);
	if(!Image)
	{
		::png_destroy_read_struct(&Png, &PngInfo, (png_infopp)nullptr);
		::CloseHandle(PngFile);
		return nullptr;
	}

	_w = ::png_get_image_width(Png, PngInfo);
	_h = ::png_get_image_height(Png, PngInfo);
	_wr = (unsigned int)::png_get_rowbytes(Png, PngInfo);
	_s = (size_t)_wr * (size_t)_h;

	*RawLength = _s;
	*Width = _w;
	*Height = _h;
	*WidthRaw = _wr;
	//::_tprintf_s(_T("LoadPng: 幅:%u(%u) 嵩:%u 全長:%u\n"), _w, _wr, _h, _s);

	PngImageBuffer = new unsigned char[_s];
	if(PngImageBuffer)
	{
		for(_y = 0; _y < _h; _y++)
		{
			//OppositeY = _h - (_y + 1);
			OppositeY = _y;
			_b = &PngImageBuffer[OppositeY * _wr];
			::memcpy(_b, Image[_y], _wr);
		}
	}
	//::_puttc(_T('\n'), stderr);
	::png_destroy_read_struct(&Png, &PngInfo, (png_infopp)nullptr);
	::CloseHandle(PngFile);

	return PngImageBuffer;
}
