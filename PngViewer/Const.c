#include <Windows.h>
#include <Shobjidl.h>

const OPENFILENAME GetOpenFileTemplate = {
	.lStructSize = sizeof(OPENFILENAME),
	.lpstrFilter = TEXT("PNGファイル\0*.png\0"),
	.nFilterIndex = 1,
	.nMaxFile = MAX_PATH,
	.Flags = OFN_DONTADDTORECENT | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_LONGNAMES | OFN_NOTESTFILECREATE
};

const COMDLG_FILTERSPEC FilterSpec = {
	.pszName = L"PNGファイル",
	.pszSpec = L"*.png"
};

const BITMAPFILEHEADER BmpFileHeaderTemplate = {
	.bfType = 'MB',
	.bfSize = sizeof(BITMAPV5HEADER) + sizeof(BITMAPFILEHEADER),
	.bfOffBits = sizeof(BITMAPV5HEADER) + sizeof(BITMAPFILEHEADER)
};

const BITMAPV5HEADER BmpHeaderTemplate = {
	.bV5Size = sizeof(BITMAPV5HEADER),
	.bV5Planes = 1,
	.bV5BitCount = 32,
	.bV5Compression = BI_BITFIELDS /*BI_RGB*/,
	.bV5XPelsPerMeter = 3780,
	.bV5YPelsPerMeter = 3780,
	.bV5RedMask = 0xFF0000UL,
	.bV5GreenMask = 0xFF00UL,
	.bV5BlueMask = 0xFFUL,
	.bV5AlphaMask = 0xFF000000UL,
	.bV5CSType = LCS_sRGB
};
