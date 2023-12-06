#include "GDIWindowCustom.h"

static constexpr uint32_t FRAME_INSET = 10;
static constexpr uint32_t CAPTION_HEIGHT = 30;

GDIWindowCustom::GDIWindowCustom(const int width, const int height):
			win(
				width + FRAME_INSET*2,
				height + CAPTION_HEIGHT + FRAME_INSET, {
					.baseStyle = WS_POPUP,
					.extendedStyle = WS_EX_LAYERED | WS_EX_APPWINDOW
				}
			),

			width(width),
			height(height),
			winTexture(this->win.width, this->win.height),
			graphics(this->width, this->height),

			numButtons(0),
			buttons(nullptr),

			device_context(nullptr) {

	win.setFallbackListener(SubWindowListener(StaticWndProc, this));

	this->numButtons = 3;
	this->buttons = new WindowButton[numButtons];
	for(uint8_t i = 0; i < numButtons; i++) {
		this->buttons[i].isRight = true;
		this->buttons[i].x = -(CAPTION_HEIGHT/2 + CAPTION_HEIGHT*i);
		this->buttons[i].y = (int32_t)CAPTION_HEIGHT/2;
		this->buttons[i].r = 8;
	}
	this->buttons[0].color = 0xff0000;
	this->buttons[0].eventType = HTCLOSE;
	this->buttons[1].color = 0xffff00;
	this->buttons[1].eventType = HTMINBUTTON;
	this->buttons[2].color = 0x00ff00;
	this->buttons[2].eventType = HTMAXBUTTON;

	this->device_context = GetDC(win.wnd);

	// Get start pos of Window
	RECT winRect;
	GetWindowRect(win.wnd, &winRect);
	this->posX = winRect.left;
	this->posY = winRect.top;
}

GDIWindowCustom::~GDIWindowCustom() {
	ReleaseDC(win.wnd, this->device_context); // release DC
	delete[] buttons;
}

void GDIWindowCustom::blitTexture(const Texture& tex) {
	HBITMAP hbmp = CreateBitmap(tex.width, tex.height, 1, 32, tex.buffer);
	HDC hdcMem = CreateCompatibleDC(this->device_context);
	HBITMAP prevBMP = SelectBitmap(hdcMem, hbmp);
	SIZE size{(long)tex.width, (long)tex.height};
	POINT relPos{0, 0};
	BLENDFUNCTION pBlend{};
	pBlend.BlendOp = AC_SRC_OVER;
	pBlend.BlendFlags = 0;
	pBlend.SourceConstantAlpha = 255;
  	pBlend.AlphaFormat = AC_SRC_ALPHA;
	UpdateLayeredWindow(
				this->win.wnd, // dest window
				this->device_context, // dest hdc
				NULL, //NULL, // POINT* new screen pos
				&size, // SIZE* new screen size
				hdcMem, // source hdc
				&relPos, // POINT* location of layer in device context
				0, // COLORREF color key
				&pBlend, // BLENDFUNCTION* blendfunc
				ULW_ALPHA // dwFlags
	);
	// BitBlt(
	// 		this->device_context, // destination device context
	// 		0, 0, // xDest, yDest (upper left corner of dest rect)
	// 		width, height, // width, height of dest rect
	// 		hdcMem, // source device context
	// 		0, 0, // xSrc, ySrc (upper left corner of source rect)
	// 		SRCCOPY // raster operation code
	// 		);
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

void GDIWindowCustom::updateScreen() {
	constexpr auto bw = [] (uint8_t bri) constexpr->uint32_t { return bri<<16 | bri<<8 | bri<<0; };
	const uint8_t alpha = 255;
	// const uint8_t alpha = hasFocus ? 255 : 150;
	this->winTexture.fillRectRounded(0, 0, this->win.width, this->win.height, 10, alpha << 24 | bw(200)); // Background
	this->winTexture.fillRectRounded(0, 0, this->win.width, CAPTION_HEIGHT, 8, TC_TOP, alpha << 24 | bw(45)); // Caption
	// this->winTexture.fillCircle(this->fullWidth-1 - CAPTION_HEIGHT/2, CAPTION_HEIGHT/2, 8, alpha<<24 | 0xff0000);
	for(uint8_t i = 0; i < this->numButtons; i++)
		this->winTexture.fillCircle(this->buttons[i].isRight * (this->win.width-1) + this->buttons[i].x, this->buttons[i].y, this->buttons[i].r, alpha<<24 | this->buttons[i].color);
	this->winTexture.blitConstAlpha(&this->graphics, FRAME_INSET, FRAME_INSET*0 + CAPTION_HEIGHT, alpha);

    this->blitTexture(this->winTexture);
}

LRESULT GDIWindowCustom::StaticWndProc(void *const win, const UINT msg, const WPARAM wParam, const LPARAM lParam) {
	return ((GDIWindowCustom*)win)->WndProc(msg, wParam, lParam);
}

LRESULT GDIWindowCustom::WndProc(const UINT msg, const WPARAM wParam, const LPARAM lParam) {
    switch (msg) {
		case WM_MOUSEMOVE:
			this->win.mouseX -= FRAME_INSET;
			this->win.mouseY -= CAPTION_HEIGHT;
			break;

		case WM_NCHITTEST:
			{
				const uint32_t xPos = GET_X_LPARAM(lParam) - this->posX;
				const uint32_t yPos = GET_Y_LPARAM(lParam) - this->posY;
				// printf("(%d, %d)\n", xPos, yPos);

				if(xPos >= this->win.width)
					return HTNOWHERE;
				if(yPos >= this->win.height)
					return HTNOWHERE;
				
				if(this->win.resizable) {
					const bool leftBorder = xPos < FRAME_INSET;
					const bool rightBorder = xPos >= this->win.width - FRAME_INSET;
					const bool topBorder = yPos < FRAME_INSET;
					const bool bottomBorder = yPos >= this->win.height - FRAME_INSET;

					if(topBorder && leftBorder)
						return HTTOPLEFT;
					if(topBorder && rightBorder)
						return HTTOPRIGHT;

					if(bottomBorder && leftBorder)
						return HTBOTTOMLEFT;
					if(bottomBorder && rightBorder)
						return HTBOTTOMRIGHT;

					if(leftBorder)
						return HTLEFT;
					if(rightBorder)
						return HTRIGHT;

					if(topBorder)
						return HTTOP;
					if(bottomBorder)
						return HTBOTTOM;
				}

				constexpr auto inCircle =
					[](uint32_t posX, uint32_t posY, int32_t cX, int32_t cY, int32_t r)->bool{ return (posX-cX)*(posX-cX) + (posY-cY)*(posY-cY) < r*r; };

				for(uint8_t i = 0; i < this->numButtons; i++)
					if(inCircle(xPos, yPos, this->buttons[i].isRight * (this->win.width-1) + this->buttons[i].x, this->buttons[i].y, this->buttons[i].r))
						return this->buttons[i].eventType;

				if(yPos < CAPTION_HEIGHT)
					return HTCAPTION;
			}
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
			this->width = ((RECT*)lParam)->right - ((RECT*)lParam)->left - FRAME_INSET*2;
			this->height = ((RECT*)lParam)->bottom - ((RECT*)lParam)->top - FRAME_INSET - CAPTION_HEIGHT;
			winTexture.resize(win.width, win.height);
			graphics.resize(width, height);
			break;
	}
	return DefWindowProc(win.wnd, msg, wParam, lParam);
}