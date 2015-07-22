#include <Windows.h>
#include <Shobjidl.h>
#include <tchar.h>

extern "C" const OPENFILENAME GetOpenFileTemplate;
extern "C" const COMDLG_FILTERSPEC FilterSpec;

extern "C" UINT_PTR CALLBACK Nothing(HWND hdlg, UINT uiMsg, WPARAM wParam, LPARAM lParam)
{
	return 0;
}

extern "C" bool SelectOpenFileClassic(TCHAR * File, unsigned int Size)
{
	OPENFILENAME _o;
	TCHAR * _f;

	_f = new TCHAR[MAX_PATH];
	::memset(_f, 0, sizeof(TCHAR) * MAX_PATH);
	_o = GetOpenFileTemplate;
	_o.lpstrFile = _f;
	if(::GetAsyncKeyState(VK_SHIFT) & 0x8000)
	{
		_o.Flags |= OFN_ENABLEHOOK | OFN_ENABLESIZING;
		_o.lpfnHook = ::Nothing;
	}

	if(::GetOpenFileName(&_o))
	{
		::_tcscpy_s(File, (size_t)Size, _f);
		delete[] _f;
		return true;
	}

	delete[] _f;
	return false;
}

extern "C" bool SelectOpenFile(TCHAR * File, unsigned int Size)
{
	HRESULT _r;
	IFileOpenDialog * _o;
	IShellItem * _s;
	WCHAR * FilePathW;
	DWORD _opt;

	if(::GetAsyncKeyState(VK_SHIFT) & 0x8000)
	{
		return ::SelectOpenFileClassic(File, Size);
	}
	else
	{
		_r = ::CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&_o));
		if(FAILED(_r))
		{
			return ::SelectOpenFileClassic(File, Size);
		}
	}

	::memset(File, 0, sizeof(TCHAR) * Size);
	_o->GetOptions(&_opt);
	_o->SetOptions(_opt | FOS_NOCHANGEDIR | FOS_NOTESTFILECREATE | FOS_HIDEMRUPLACES | FOS_DONTADDTORECENT);
	_o->SetFileTypes(1, &FilterSpec);

	_r = _o->Show(nullptr);
	if(SUCCEEDED(_r))
	{
		_r = _o->GetResult(&_s);
		if(SUCCEEDED(_r))
		{
			_s->GetDisplayName(SIGDN_FILESYSPATH, &FilePathW);
#if defined(UNICODE)
			::wcscpy_s(File, Size, FilePathW);
#else
			::WideCharToMultiByte(CP_ACP, 0, FilePathW, (int)::wcsnlen_s(FilePathW, MAX_PATH), File, Size, nullptr, nullptr);
#endif
			::CoTaskMemFree(FilePathW);
			_s->Release();
		}
	}
	_o->Release();
	return (File[0] != 0);
}
