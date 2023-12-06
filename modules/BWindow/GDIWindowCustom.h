#pragma once

#include "Window.h"

struct WindowButton {
	int32_t x, y, r;
	bool isRight; // right side of Window
	uint32_t color;
	LRESULT eventType;
};

class GDIWindowCustom {
public:
	Window win;

private:
	Texture winTexture;
	uint8_t numButtons;
	WindowButton *buttons;

public:
	uint32_t posX, posY;
    uint32_t width, height;
    Texture graphics;

private:
    HDC device_context;

private:
	static LRESULT StaticWndProc(void *const win, const UINT msg, const WPARAM wParam, const LPARAM lParam);
	LRESULT WndProc(const UINT msg, const WPARAM wParam, const LPARAM lParam);

public:
	GDIWindowCustom(const int width, const int height);
	~GDIWindowCustom();
	void updateScreen();
    void blitTexture(const Texture& tex);

public:
	inline bool shouldClose() { return win.shouldClose; }
	inline void pollMsg() { win.pollMsg(); }
};