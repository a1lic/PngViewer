#pragma once
#include <Windows.h>
#include <dxgi1_3.h>
#include <dcomp.h>

#define PNGVIEWER_CLASS TEXT("PngViewer")
#define PNGVIEWER_TITLE TEXT("Direct2D PNG Viewer")

class PngViewer
{
	HINSTANCE instance;
	HWND window;
	HMENU menu;
	IDXGIDevice3 * device_gi;
	IDXGIFactory3 * gi_factory;
	IDXGISwapChain2 * swap_chain;
	IDCompositionDevice * comp_device;
	IDCompositionTarget * comp_target;
	IDCompositionVisual * comp_visual;
	SIZE swap_chain_region;
	unsigned char * png_buffer;
	unsigned int png_width;
	unsigned int png_height;
	unsigned int png_line_length;
	size_t png_length;
	bool resizing;
	static ATOM class_atom;
	static short instances;
public:
	PngViewer(HINSTANCE);
	~PngViewer();
	uintptr_t Show();
private:
	bool on_create();
	void on_destroy();
	void on_size(USHORT, USHORT);
	void on_close();
	//void on_command(WORD, WORD, HWND);
	void on_system_command(WORD);
	void on_enter_size_move();
	void on_exit_size_move();
	bool init_d2d();
	void draw_screen();
	void load_png_to_buffer();
	static LRESULT CALLBACK window_proc(HWND, UINT, WPARAM, LPARAM);
};
