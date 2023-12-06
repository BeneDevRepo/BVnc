#include "GDIWindow.h"

static constexpr uint32_t FRAME_INSET = 10;
static constexpr uint32_t CAPTION_HEIGHT = 30;

GDIWindow::GDIWindow(const int width, const int height):
			win(
				width,
				height, {
					.baseStyle = WS_CAPTION | WS_SYSMENU | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_SIZEBOX,
					.extendedStyle = WS_EX_APPWINDOW
				}
			),

			width(width),
			height(height),
			winTexture(this->win.width, this->win.height),
			graphics(this->width, this->height),

			// numButtons(0),
			// buttons(nullptr),

			device_context(nullptr) {

	win.setFallbackListener(SubWindowListener(StaticWndProc, this));

	// this->numButtons = 3*0;
	// this->buttons = new WindowButton[numButtons];
	// for(uint8_t i = 0; i < numButtons; i++) {
	// 	this->buttons[i].isRight = true;
	// 	this->buttons[i].x = -(CAPTION_HEIGHT/2 + CAPTION_HEIGHT*i);
	// 	this->buttons[i].y = (int32_t)CAPTION_HEIGHT/2;
	// 	this->buttons[i].r = 8;
	// }
	// this->buttons[0].color = 0xff0000;
	// this->buttons[0].eventType = HTCLOSE;
	// this->buttons[1].color = 0xffff00;
	// this->buttons[1].eventType = HTMINBUTTON;
	// this->buttons[2].color = 0x00ff00;
	// this->buttons[2].eventType = HTMAXBUTTON;

	this->device_context = GetDC(win.wnd);

	// Get start pos of Window
	RECT winRect;
	GetWindowRect(win.wnd, &winRect);
	this->posX = winRect.left;
	this->posY = winRect.top;
}

GDIWindow::~GDIWindow() {
	ReleaseDC(win.wnd, this->device_context); // release DC
	// delete[] buttons;
}

void GDIWindow::blitTexture(const Texture& tex) {
	HBITMAP hbmp = CreateBitmap(tex.width, tex.height, 1, 32, tex.buffer);
	HDC hdcMem = CreateCompatibleDC(this->device_context);
	HBITMAP prevBMP = SelectBitmap(hdcMem, hbmp);

	// SIZE size{(long)tex.width, (long)tex.height};
	// POINT relPos{0, 0};
	// BLENDFUNCTION pBlend{};
	// pBlend.BlendOp = AC_SRC_OVER;
	// pBlend.BlendFlags = 0;
	// pBlend.SourceConstantAlpha = 255;
  	// pBlend.AlphaFormat = AC_SRC_ALPHA;
	// UpdateLayeredWindow(
	// 			this->win.wnd, // dest window
	// 			this->device_context, // dest hdc
	// 			NULL, //NULL, // POINT* new screen pos
	// 			&size, // SIZE* new screen size
	// 			hdcMem, // source hdc
	// 			&relPos, // POINT* location of layer in device context
	// 			0, // COLORREF color key
	// 			&pBlend, // BLENDFUNCTION* blendfunc
	// 			ULW_ALPHA // dwFlags
	// );
	BitBlt(
			this->device_context, // destination device context
			0, 0, // xDest, yDest (upper left corner of dest rect)
			width, height, // width, height of dest rect
			hdcMem, // source device context
			0, 0, // xSrc, ySrc (upper left corner of source rect)
			SRCCOPY // raster operation code
			);

	SelectObject(hdcMem, prevBMP);
	DeleteObject(hbmp);
	DeleteDC(hdcMem);

    // SetDIBitsToDevice(
    //     this->device_context, // destination device context
    //     0, 0, // xDest, yDest (upper left corner of dest rect)
    //     this->width, this->height, // width, height
    //     0, 0, // xSrc, ySrc (lower left corner of source rect)
    //     0, // startScan
    //     this->height, // cLines
    //     tex.buffer, // buffer
    //     &tex.bit_map_info,
    //     DIB_RGB_COLORS
    //     );
}

void GDIWindow::updateScreen() {
	constexpr auto bw = [] (uint8_t bri) constexpr->uint32_t { return bri<<16 | bri<<8 | bri<<0; };

	this->winTexture.fillRect(0, 0, this->win.width, this->win.height, 0xFF << 24 | bw(200)); // Background

	this->winTexture.blitConstAlpha(&this->graphics, 0, 0, 0xFF);

    this->blitTexture(this->winTexture);

    // this->blitTexture(this->graphics);
}

LRESULT GDIWindow::StaticWndProc(void *const win, const UINT msg, const WPARAM wParam, const LPARAM lParam) {
	return ((GDIWindow*)win)->WndProc(msg, wParam, lParam);
}

LRESULT GDIWindow::WndProc(const UINT msg, const WPARAM wParam, const LPARAM lParam) {
    switch (msg) {
		case WM_MOUSEMOVE:
			this->win.mouseX -= FRAME_INSET;
			this->win.mouseY -= CAPTION_HEIGHT;
			break;

		case WM_ACTIVATE:
		case WM_WINDOWPOSCHANGING:
			{
				RECT winRect;
				GetWindowRect(win.wnd, &winRect);
				this->posX = winRect.left;
				this->posY = winRect.top;
			}
			break;

		case WM_SIZING:
			this->width = win.width;
			this->height = win.height;
			winTexture.resize(win.width, win.height);
			graphics.resize(width, height);
			break;
	}
	return DefWindowProc(win.wnd, msg, wParam, lParam);
}