#include "Window.h"

#include <iostream>


thread_local uint32_t Window::numWindows = 0;
thread_local void *Window::mainFiber = nullptr;

void Window::fiberProc() {
	while(true) {
		MSG msg;
		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}
		SwitchToFiber(mainFiber);
	}
}

Window::Window(const int width, const int height, const WindowStyle windowStyle):
		fallbackListener(
			[](void *const win, const UINT msg, const WPARAM wParam, const LPARAM lParam)->LRESULT {
				return DefWindowProc(((Window*)win)->wnd, msg, wParam, lParam);
			}, this),
		windowStyle(windowStyle),
		resizable(true),
		shouldClose(false),

		width(width), height(height),
		mouseX(0), mouseY(0),

		wnd(NULL) {

	if(numWindows++ == 0) {
		mainFiber = ConvertThreadToFiber(0);
		std::cout << "Creating First Window." << "\n";
	}

	// PFIBER_START_ROUTINE
	messageFiber = CreateFiber(0, staticFiberProc, this);

	WNDCLASSW win_class{};
	win_class.hInstance = GetModuleHandle(NULL);
	win_class.hCursor = LoadCursor(NULL, IDC_ARROW);
	win_class.lpfnWndProc = StaticWndProc;
	win_class.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	win_class.lpszClassName = L"win32app";


	if (!RegisterClassW(&win_class)) {
		printf("Error registering Window Class: %i\n", (int)GetLastError());
		exit(-1);
	}

	CreateWindowExW(
		windowStyle.extendedStyle,
		win_class.lpszClassName, // lpClassName
		L"Title", // lpWindowName
		windowStyle.baseStyle,
		100, // x
		100, // y
		width, // width
		height, // height
		NULL, // hWndParent
		NULL, // hMenu
		NULL, // hInstance
		this  // lpParam
		);

	ShowWindow(wnd, SW_SHOW);
	UpdateWindow(wnd);

	RegisterHotKey(wnd, 11, MOD_NOREPEAT, VK_F11);

	SetTimer(wnd, 0xDEB, 1000, NULL); // debug
}

Window::~Window() {
	if(!this->wnd)
		return;

	DestroyWindow(this->wnd); // Destroy window

	if(--numWindows == 0) {
		ConvertFiberToThread();
		std::cout << "Destroyed last Window." << "\n";
	}
}

void Window::pollMsg() {
	SwitchToFiber(messageFiber);
}

LRESULT WINAPI Window::StaticWndProc(HWND wnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	Window *targetWindow;
	if(msg == WM_NCCREATE) {
		LPCREATESTRUCT lpcs = (LPCREATESTRUCT)lParam;
		targetWindow = (Window*)(lpcs->lpCreateParams);
		targetWindow->wnd = wnd;
		SetWindowLongPtr(wnd, GWLP_USERDATA, (LONG_PTR)targetWindow);
	} else
 		targetWindow = (Window*)GetWindowLongPtr(wnd, GWLP_USERDATA);

	if (targetWindow)
		return targetWindow->WndProc(msg, wParam, lParam);
	return DefWindowProc(wnd, msg, wParam, lParam);
}

LRESULT Window::WndProc(const UINT msg, const WPARAM wParam, const LPARAM lParam) {
    switch (msg) {
		// case WM_KEYDOWN:
		// std::cout << (char)wParam << "\n";
		// 	break;

		// case WM_CHAR:
		// 	std::cout << (char)wParam << "\n";
		// 	break;

		case WM_NCLBUTTONDOWN: //WM_NCLBUTTONUP
			switch(wParam) {
				case HTCLOSE:
					shouldClose = true;
        			return 0;

				case HTMINBUTTON:
					ShowWindow(this->wnd, SW_MINIMIZE);
					return 0;

				// case HTMAXBUTTON:
				// 	ShowWindow(this->wnd, SW_MAXIMIZE);
				// 	return 0;
			}
			break;

		case WM_QUERYOPEN:
			return TRUE;

		case WM_HOTKEY:
			std::cout << "Hotkey Pressed: " << wParam << "\n";
			break;

		case WM_LBUTTONDOWN:
		case WM_RBUTTONDOWN:
		case WM_MBUTTONDOWN:
			SetCapture(wnd);
			break;

		case WM_LBUTTONUP:
		case WM_RBUTTONUP:
		case WM_MBUTTONUP:
			ReleaseCapture();
			break;

		case WM_MOUSEMOVE:
			this->mouseX = GET_X_LPARAM(lParam);
			this->mouseY = GET_Y_LPARAM(lParam);
			break;

		case WM_ENTERSIZEMOVE:
			std::cout << "WM EnterSizeMove\n";
			SetTimer(wnd, 0x69420, 0, NULL);
			break;

		case WM_EXITSIZEMOVE:
			std::cout << "WM ExitSizeMove\n";
			KillTimer(wnd, 0x69420);
			break;

		case WM_TIMER:
			if(wParam == 0x69420)
				SwitchToFiber(mainFiber);
			// if(wParam == 0xDEB)
			// 	printf("Active: %d, Focused: %d\n", GetActiveWindow()==wnd, GetFocus()==wnd);
			return 0;

		case WM_GETMINMAXINFO:
			{
				RECT minWindow;
				minWindow.left = 0;
				minWindow.right = 1;
				minWindow.top = 0;
				minWindow.bottom = 1;
				AdjustWindowRectEx(&minWindow, windowStyle.baseStyle, false, windowStyle.extendedStyle);
				MINMAXINFO *const mmi = (MINMAXINFO*)lParam;
				mmi->ptMinTrackSize.x = minWindow.right - minWindow.left;
				mmi->ptMinTrackSize.y = minWindow.bottom - minWindow.top;
			}
			return 0;
			break;

		case WM_SIZING:
		case WM_SIZE:
			{
				RECT cliRect;
				GetClientRect(wnd, &cliRect);
				width = cliRect.right - cliRect.left;
				height = cliRect.bottom - cliRect.top;
			}
			break;


        // case WM_CLOSE:
		// 	// std::cout << "WM_CLOSE";
        // case WM_DESTROY:
		// 	// shouldClose = true;
        //     // PostQuitMessage(0);
        //     return 0;
    }
    return fallbackListener(msg, wParam, lParam);
}