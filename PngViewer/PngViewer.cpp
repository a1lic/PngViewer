#define OEMRESOURCE

#include "PngViewer.h"
#include <Windows.h>
#include <tchar.h>
#include <dxgi1_3.h>
#include <d3d11.h>
#include <d2d1_2.h>
#include <d2d1_2helper.h>
#include <dcomp.h>

extern "C" bool SelectOpenFile(TCHAR *, unsigned int);
extern "C" unsigned char * LoadPng(const TCHAR *, size_t *, unsigned int *, unsigned int *, unsigned int *);

extern "C" int APIENTRY _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow)
{
	PngViewer * viewer;

	::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

	viewer = new PngViewer(hInstance);
	viewer->Show();
	delete viewer;

	::CoUninitialize();

	return 0;
}

ATOM PngViewer::class_atom;
short PngViewer::instances;

PngViewer::PngViewer(HINSTANCE instance)
{
	WNDCLASSEX wc;
	short prev_count;

	prev_count = ::InterlockedIncrement16(&instances);
	if(prev_count == 1)
	{
		::memset(&wc, 0, sizeof(WNDCLASSEX));
		wc.cbSize = sizeof(WNDCLASSEX);
		wc.lpfnWndProc = window_proc;
		wc.cbWndExtra = sizeof(PngViewer *);
		wc.hInstance = instance;
		wc.hIcon = ::LoadIcon(nullptr, MAKEINTRESOURCE(OIC_HAND));
		wc.hCursor = nullptr;
		wc.hbrBackground = (HBRUSH)(1 + COLOR_3DFACE);
		wc.lpszClassName = PNGVIEWER_CLASS;
		wc.hIconSm = reinterpret_cast<HICON>(::LoadImage(NULL, MAKEINTRESOURCE(OIC_HAND), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_SHARED));

		class_atom = ::RegisterClassEx(&wc);
	}

	this->method = PNG_LOAD_METHOD_WIC;
	this->png_buffer = nullptr;
	this->png_file_handle = INVALID_HANDLE_VALUE;
	this->instance = instance;
}

PngViewer::~PngViewer()
{
	short prev_count;

	if(this->png_file_handle != INVALID_HANDLE_VALUE)
	{
		::CloseHandle(this->png_file_handle);
	}

	if(this->png_buffer)
	{
		delete[] this->png_buffer;
	}

	prev_count = ::InterlockedDecrement16(&instances);
	if(prev_count == 0)
	{
		::UnregisterClass(PNGVIEWER_CLASS, this->instance);
	}
}

uintptr_t PngViewer::Show()
{
	MSG msg;

	this->window = ::CreateWindowEx(WS_EX_NOREDIRECTIONBITMAP, PNGVIEWER_CLASS, PNGVIEWER_TITLE, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, this->instance, this);
	if(this->window)
	{
		::ShowWindow(this->window, SW_SHOWDEFAULT);

		while(::GetMessage(&msg, nullptr, 0, 0) > 0)
		{
			::TranslateMessage(&msg);
			::DispatchMessage(&msg);
		}
	}
	else
	{
		msg.wParam = 0;
	}

	return msg.wParam;
}

bool PngViewer::on_create()
{
	MENUITEMINFO item;
	HMENU window_menu;

	this->device_gi = nullptr;
	this->gi_factory = nullptr;
	this->swap_chain = nullptr;
	this->comp_device = nullptr;
	this->comp_target = nullptr;
	this->comp_visual = nullptr;

	this->wic_factory = nullptr;
	if(::CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, __uuidof(IWICImagingFactory), (LPVOID *)&this->wic_factory) != S_OK)
	{
		return false;
	}

	if(!this->init_d2d())
	{
		return false;
	}
	this->draw_screen();

	item.cbSize = sizeof(MENUITEMINFO);

	window_menu = ::GetSystemMenu(this->window, FALSE);
	this->menu = ::CreateMenu();

	item.fMask = MIIM_FTYPE;
	item.fType = MFT_SEPARATOR;
	::InsertMenuItem(window_menu, -1, MF_BYPOSITION, &item);

	item.fMask = MIIM_SUBMENU | MIIM_STRING;
	item.hSubMenu = this->menu;
	item.dwTypeData = TEXT("アプリ(&A)");
	::InsertMenuItem(window_menu, -1, MF_BYPOSITION, &item);

	item.fMask = MIIM_ID | MIIM_STRING;
	item.wID = 1;
	item.dwTypeData = TEXT("PNGファイルを開く(&O)…\tCtrl+O");
	::InsertMenuItem(this->menu, -1, MF_BYPOSITION, &item);
	item.fMask = MIIM_FTYPE;
	item.fType = MFT_SEPARATOR;
	::InsertMenuItem(this->menu, -1, MF_BYPOSITION, &item);
	item.fMask = MIIM_ID | MIIM_STRING | MIIM_FTYPE;
	item.fType = MFT_RADIOCHECK;
	item.wID = 2;
	item.dwTypeData = TEXT("libpngを使用して読み込む(&L)");
	::InsertMenuItem(this->menu, -1, MF_BYPOSITION, &item);
	item.wID = 3;
	item.dwTypeData = TEXT("WICを使用して読み込む(&W)");
	::InsertMenuItem(this->menu, -1, MF_BYPOSITION, &item);
	item.fMask = MIIM_FTYPE;
	item.fType = MFT_SEPARATOR;
	::InsertMenuItem(this->menu, -1, MF_BYPOSITION, &item);
	item.fMask = MIIM_ID | MIIM_STRING;
	item.wID = 4;
	item.dwTypeData = TEXT("バージョン情報(&A)…");
	::InsertMenuItem(this->menu, -1, MF_BYPOSITION, &item);
	item.wID = 5;
	item.dwTypeData = TEXT("終了(&X)\tAlt+F4");
	::InsertMenuItem(this->menu, -1, MF_BYPOSITION, &item);

	return true;
}

void PngViewer::on_destroy()
{
	if(this->comp_visual)
	{
		this->comp_visual->Release();
	}
	if(this->comp_target)
	{
		this->comp_target->Release();
	}
	if(this->comp_device)
	{
		this->comp_device->Release();
	}
	if(this->swap_chain)
	{
		this->swap_chain->Release();
	}
	if(this->gi_factory)
	{
		this->gi_factory->Release();
	}
	if(this->device_gi)
	{
		this->device_gi->Release();
	}
	if(this->wic_factory)
	{
		this->wic_factory->Release();
	}

	::SetMenu(this->window, nullptr);
	::DestroyMenu(this->menu);
}

void PngViewer::on_size(USHORT width, USHORT height)
{
	if(this->resizing)
	{
		return;
	}

	this->on_exit_size_move();
}

void PngViewer::on_close()
{
	::ShowWindow(this->window, SW_HIDE);
	::DestroyWindow(this->window);
}

void PngViewer::on_system_command(WORD id)
{
	TCHAR fn[MAX_PATH];

	switch(id)
	{
	case 1:
		if(::SelectOpenFile(fn, MAX_PATH))
		{
			if(this->png_buffer)
			{
				delete[] this->png_buffer;
				this->png_buffer = nullptr;
			}
			if(this->png_file_handle != INVALID_HANDLE_VALUE)
			{
				::CloseHandle(this->png_file_handle);
				this->png_file_handle = INVALID_HANDLE_VALUE;
			}
			switch(this->method)
			{
			case PNG_LOAD_METHOD_LIBPNG:
				this->png_buffer = ::LoadPng(fn, &this->png_length, &this->png_width, &this->png_height, &this->png_line_length);
				if(this->png_buffer)
				{
					this->load_png_to_buffer();
				}
				break;
			case PNG_LOAD_METHOD_WIC:
				this->png_file_handle = ::CreateFile(fn, FILE_READ_DATA, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
				if(this->png_file_handle != INVALID_HANDLE_VALUE)
				{
					this->load_png_with_wic();
					::SetFilePointer(this->png_file_handle, 0, 0, FILE_BEGIN);
				}
				break;
			}
		}
		break;
	case 2:
		this->method = PNG_LOAD_METHOD_LIBPNG;
		break;
	case 3:
		this->method = PNG_LOAD_METHOD_WIC;
		break;
	case 4:
		break;
	case 5:
		::PostMessage(this->window, WM_CLOSE, 0, 0);
		break;
	}
}

void PngViewer::on_right_button_up(WORD modifier, WORD client_x, WORD client_y)
{
	POINT p;
	int id;

	::SendMessage(this->window, WM_ENTERMENULOOP, 0, 0);
	::GetCursorPos(&p);
	id = (int)::TrackPopupMenu(this->menu, TPM_RIGHTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD, p.x, p.y, 0, this->window, nullptr);
	if(id > 0)
	{
		::PostMessage(this->window, WM_SYSCOMMAND, MAKEWPARAM(id, 0), 0);
	}
}

void PngViewer::on_enter_menu_loop()
{
	MENUITEMINFO item;

	item.cbSize = sizeof(MENUITEMINFO);

	item.fMask = MIIM_STATE;
	item.fState = (this->method == PNG_LOAD_METHOD_LIBPNG) ? MFS_CHECKED : MFS_UNCHECKED;
	::SetMenuItemInfo(this->menu, 2, MF_BYCOMMAND, &item);
	item.fState = (this->method == PNG_LOAD_METHOD_WIC) ? MFS_CHECKED : MFS_UNCHECKED;
	::SetMenuItemInfo(this->menu, 3, MF_BYCOMMAND, &item);
}

void PngViewer::on_enter_size_move()
{
	this->resizing = true;
}

void PngViewer::on_exit_size_move()
{
	RECT client_rect;
	SIZE new_size;

	if(::IsIconic(this->window))
	{
		this->resizing = false;
		return;
	}

	::GetClientRect(this->window, &client_rect);

	new_size.cx = client_rect.right - client_rect.left;
	new_size.cy = client_rect.bottom - client_rect.top;

	if((this->swap_chain_region.cx != new_size.cx) || (this->swap_chain_region.cy != new_size.cy))
	{
		this->swap_chain_region = new_size;
		this->swap_chain->ResizeBuffers(0, new_size.cx, new_size.cy, DXGI_FORMAT_B8G8R8A8_UNORM, 0);
		if(this->png_buffer)
		{
			this->load_png_to_buffer();
		}
		else if(this->png_file_handle != INVALID_HANDLE_VALUE)
		{
			this->load_png_with_wic();
		}
		else
		{
			this->draw_screen();
		}
	}

	this->resizing = false;
}

bool PngViewer::init_d2d()
{
	DXGI_SWAP_CHAIN_DESC1 swap_descriptor;
	RECT client_rect;
	ID3D11Device * device;
	IDXGISwapChain1 * swap_chain_1;
	HRESULT r;

	if(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION, &device, nullptr, nullptr) != S_OK)
	{
		return false;
	}
	r = device->QueryInterface(__uuidof(IDXGIDevice3), (void **)&this->device_gi);
	device->Release();
	if(r != S_OK)
	{
		return false;
	}

	if(CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, __uuidof(IDXGIFactory3), (void **)&this->gi_factory) != S_OK)
	{
		return false;
	}

	::GetClientRect(this->window, &client_rect);
	this->swap_chain_region.cx = client_rect.right - client_rect.left;
	this->swap_chain_region.cy = client_rect.bottom - client_rect.top;

	::memset(&swap_descriptor, 0, sizeof(DXGI_SWAP_CHAIN_DESC1));
	swap_descriptor.Width = this->swap_chain_region.cx;
	swap_descriptor.Height = this->swap_chain_region.cy;
	swap_descriptor.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	swap_descriptor.SampleDesc.Count = 1;
	swap_descriptor.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swap_descriptor.BufferCount = 2;
	swap_descriptor.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
	swap_descriptor.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;

	if(this->gi_factory->CreateSwapChainForComposition(this->device_gi, &swap_descriptor, nullptr, &swap_chain_1) != S_OK)
	{
		return false;
	}

	r = swap_chain_1->QueryInterface(__uuidof(IDXGISwapChain2), (void **)&this->swap_chain);
	swap_chain_1->Release();
	if(r != S_OK)
	{
		return false;
	}

	if(DCompositionCreateDevice(this->device_gi, __uuidof(IDCompositionDevice), (void **)&this->comp_device) != S_OK)
	{
		return false;
	}

	if(this->comp_device->CreateTargetForHwnd(this->window, true, &this->comp_target) != S_OK)
	{
		return false;
	}

	if(this->comp_device->CreateVisual(&this->comp_visual) != S_OK)
	{
		return false;
	}

	this->comp_visual->SetContent(this->swap_chain);
	this->comp_target->SetRoot(this->comp_visual);
	this->comp_device->Commit();

	return true;
}

void PngViewer::draw_screen()
{
	static const D2D1_FACTORY_OPTIONS options = { D2D1_DEBUG_LEVEL_INFORMATION };
	D2D1_BITMAP_PROPERTIES1 bmp_properties;
	ID2D1Factory2 * d2_factory;
	ID2D1Device1 * d2_device;
	ID2D1DeviceContext1 * context;
	IDXGISurface2 * surface;
	ID2D1Bitmap1 * bitmap;
	ID2D1SolidColorBrush * brush;
	D2D1_POINT_2F ellipseCenter;
	D2D1_ELLIPSE ellipse;
	D2D1_COLOR_F brush_color;

	if(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, options, &d2_factory) != S_OK)
	{
		return;
	}

	if(d2_factory->CreateDevice(this->device_gi, &d2_device) != S_OK)
	{
		d2_factory->Release();
		return;
	}

	if(d2_device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &context) != S_OK)
	{
		d2_device->Release();
		d2_factory->Release();
		return;
	}

	if(this->swap_chain->GetBuffer(0, __uuidof(IDXGISurface2), (void **)&surface) != S_OK)
	{
		context->Release();
		d2_device->Release();
		d2_factory->Release();
		return;
	}

	::memset(&bmp_properties, 0, sizeof(D2D1_BITMAP_PROPERTIES1));
	bmp_properties.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
	bmp_properties.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
	bmp_properties.bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW;

	if(context->CreateBitmapFromDxgiSurface(surface, &bmp_properties, &bitmap) != S_OK)
	{
		surface->Release();
		context->Release();
		d2_device->Release();
		d2_factory->Release();
		return;
	}

	context->SetTarget(bitmap);

	// Draw something
	context->BeginDraw();
	context->Clear();

	//bitmap->CopyFromMemory(nullptr, nullptr, 0);

	brush_color = D2D1::ColorF(0.18f, 0.55f, 0.34f, 0.75f);
	context->CreateSolidColorBrush(brush_color, &brush);
	ellipseCenter = D2D1::Point2F(this->swap_chain_region.cx / 2.0F, this->swap_chain_region.cy / 2.0F);
	ellipse = D2D1::Ellipse(ellipseCenter, this->swap_chain_region.cx / 2.0F, this->swap_chain_region.cy / 2.0F);
	context->FillEllipse(ellipse, brush);

	context->EndDraw();

	this->swap_chain->Present(1, 0);

	brush->Release();
	bitmap->Release();
	surface->Release();
	context->Release();
	d2_device->Release();
	d2_factory->Release();
}

void PngViewer::load_png_to_buffer()
{
	static const D2D1_FACTORY_OPTIONS options = { D2D1_DEBUG_LEVEL_INFORMATION };
	D2D1_RECT_U rect;
	D2D1_BITMAP_PROPERTIES1 bmp_properties;
	ID2D1Factory2 * d2_factory;
	ID2D1Device1 * d2_device;
	ID2D1DeviceContext1 * context;
	IDXGISurface2 * surface;
	ID2D1Bitmap1 * bitmap;

	if(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, options, &d2_factory) != S_OK)
	{
		return;
	}

	if(d2_factory->CreateDevice(this->device_gi, &d2_device) != S_OK)
	{
		d2_factory->Release();
		return;
	}

	if(d2_device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &context) != S_OK)
	{
		d2_device->Release();
		d2_factory->Release();
		return;
	}

	if(this->swap_chain->GetBuffer(0, __uuidof(IDXGISurface2), (void **)&surface) != S_OK)
	{
		context->Release();
		d2_device->Release();
		d2_factory->Release();
		return;
	}

	::memset(&bmp_properties, 0, sizeof(D2D1_BITMAP_PROPERTIES1));
	bmp_properties.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
	bmp_properties.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
	bmp_properties.bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW;

	if(context->CreateBitmapFromDxgiSurface(surface, &bmp_properties, &bitmap) != S_OK)
	{
		surface->Release();
		context->Release();
		d2_device->Release();
		d2_factory->Release();
		return;
	}

	/*bmp_properties.bitmapOptions = D2D1_BITMAP_OPTIONS_NONE;
	if(context->CreateBitmapFromDxgiSurface(surface, &bmp_properties, &texture) != S_OK)
	{
		bitmap->Release();
		surface->Release();
		context->Release();
		d2_device->Release();
		d2_factory->Release();
		return;
	}*/

	context->SetTarget(bitmap);
	rect.left = 0;
	rect.top = 0;
	rect.right = this->png_width;
	rect.bottom = this->png_height;
	bitmap->CopyFromMemory(&rect, this->png_buffer, this->png_line_length);

	// Draw something
	//context->BeginDraw();
	//context->Clear();
	//context->DrawBitmap(texture);
	//context->EndDraw();

	this->swap_chain->Present(1, 0);

	//texture->Release();
	bitmap->Release();
	surface->Release();
	context->Release();
	d2_device->Release();
	d2_factory->Release();
}

void PngViewer::load_png_with_wic()
{
	static const D2D1_FACTORY_OPTIONS options = { D2D1_DEBUG_LEVEL_INFORMATION };
	D2D1_BITMAP_PROPERTIES1 bmp_properties;
	ID2D1Factory2 * d2_factory;
	ID2D1Device1 * d2_device;
	ID2D1DeviceContext1 * context;
	IDXGISurface2 * surface;
	IWICBitmapDecoder * decoder;
	IWICBitmapFrameDecode * frame_decoder;
	IWICFormatConverter * format_converter;
	ID2D1Bitmap1 * png_texture;
	ID2D1Bitmap1 * bitmap;

	if(this->wic_factory->CreateDecoderFromFileHandle(reinterpret_cast<ULONG_PTR>(this->png_file_handle), nullptr, WICDecodeMetadataCacheOnLoad, &decoder) != S_OK)
	{
		return;
	}

	if(decoder->GetFrame(0, &frame_decoder) != S_OK)
	{
		decoder->Release();
		return;
	}

	if(this->wic_factory->CreateFormatConverter(&format_converter) != S_OK)
	{
		frame_decoder->Release();
		decoder->Release();
		return;
	}

	format_converter->Initialize(frame_decoder, GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeMedianCut);

	frame_decoder->Release();
	decoder->Release();

	if(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, options, &d2_factory) != S_OK)
	{
		format_converter->Release();
		return;
	}

	if(d2_factory->CreateDevice(this->device_gi, &d2_device) != S_OK)
	{
		format_converter->Release();
		d2_factory->Release();
		return;
	}

	if(d2_device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &context) != S_OK)
	{
		format_converter->Release();
		d2_device->Release();
		d2_factory->Release();
		return;
	}

	if(this->swap_chain->GetBuffer(0, __uuidof(IDXGISurface2), (void **)&surface) != S_OK)
	{
		format_converter->Release();
		context->Release();
		d2_device->Release();
		d2_factory->Release();
		return;
	}

	::memset(&bmp_properties, 0, sizeof(D2D1_BITMAP_PROPERTIES1));
	bmp_properties.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
	bmp_properties.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
	bmp_properties.bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW;
	if(context->CreateBitmapFromDxgiSurface(surface, &bmp_properties, &bitmap) != S_OK)
	{
		format_converter->Release();
		surface->Release();
		context->Release();
		d2_device->Release();
		d2_factory->Release();
		return;
	}

	bmp_properties.bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET;
	if(context->CreateBitmapFromWicBitmap(format_converter, &bmp_properties, &png_texture) != S_OK)
	{
		format_converter->Release();
		bitmap->Release();
		surface->Release();
		context->Release();
		d2_device->Release();
		d2_factory->Release();
		return;
	}

	context->SetTarget(bitmap);
	bitmap->CopyFromBitmap(nullptr, png_texture, nullptr);
	this->swap_chain->Present(1, 0);

	format_converter->Release();
	png_texture->Release();
	bitmap->Release();
	surface->Release();
	context->Release();
	d2_device->Release();
	d2_factory->Release();
}

LRESULT CALLBACK PngViewer::window_proc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	PngViewer * _this;
	LRESULT r;

	if(Msg == WM_CREATE)
	{
		_this = reinterpret_cast<PngViewer *>(reinterpret_cast<const CREATESTRUCT *>(lParam)->lpCreateParams);
	}
	else
	{
		_this = reinterpret_cast<PngViewer *>(::GetProp(hWnd, TEXT("PngViewerInstance")));
	}

	switch(Msg)
	{
	case WM_CREATE:
		::SetProp(hWnd, TEXT("PngViewerInstance"), _this);
		_this->window = hWnd;
		r = _this->on_create() ? 0 : -1L;
		break;

	case WM_DESTROY:
		r = 0;
		_this->on_destroy();
		::RemoveProp(hWnd, TEXT("PngViewerInstance"));
		::PostQuitMessage(0);
		break;

	case WM_SIZE:
		r = 0;
		_this->on_size(LOWORD(lParam), HIWORD(lParam));
		break;

	case WM_CLOSE:
		r = 0;
		_this->on_close();
		break;

	case WM_SYSCOMMAND:
		if(LOWORD(wParam) < 0xF000U)
		{
			r = 0;
			_this->on_system_command(LOWORD(wParam));
		}
		else
		{
			r = ::DefWindowProc(hWnd, Msg, wParam, lParam);
		}
		break;

	case WM_RBUTTONUP:
		r = 0;
		_this->on_right_button_up(LOWORD(wParam), LOWORD(lParam), HIWORD(lParam));
		break;

	case WM_ENTERMENULOOP:
		r = 0;
		_this->on_enter_menu_loop();
		break;

	case WM_ENTERSIZEMOVE:
		r = 0;
		_this->on_enter_size_move();
		break;

	case WM_EXITSIZEMOVE:
		r = 0;
		_this->on_exit_size_move();
		break;

	default:
		r = ::DefWindowProc(hWnd, Msg, wParam, lParam);
		break;
	}

	return r;
}
