/*
#include "OpenGLWindow.h" // header

#define SOGL_IMPLEMENTATION_WIN32 // Include actual implementation in this File
#include "OpenGL/opengl_include.h"
#include "OpenGL/loader/wglext.h"

#include <iostream>


typedef HGLRC WINAPI wglCreateContextAttribsARB_type(HDC hdc, HGLRC hShareContext, const int *attribList);
wglCreateContextAttribsARB_type *wglCreateContextAttribsARB;

typedef BOOL WINAPI wglChoosePixelFormatARB_type(HDC hdc, const int *piAttribIList, const FLOAT *pfAttribFList, UINT nMaxFormats, int *piFormats, UINT *nNumFormats);
wglChoosePixelFormatARB_type *wglChoosePixelFormatARB;



static void fatal_error(const char *const errorMsg) {
	MessageBoxA(NULL, errorMsg, "FAILURE", MB_OK);
	exit(1);
}

static void init_opengl_extensions(); // Dummy context Nonsense


OpenGLWindow::OpenGLWindow(const int width, const int height):
			win(width, height),
			device_context(nullptr) {

	win.setFallbackListener(SubWindowListener(StaticWndProc, this));

	this->device_context = GetDC(win.wnd);

	init_opengl_extensions();

	const int pixel_format_attribs[] = {
		WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
		WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
		WGL_DOUBLE_BUFFER_ARB, GL_TRUE,
		WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_ARB,
		WGL_COLOR_BITS_ARB, 32,
		WGL_DEPTH_BITS_ARB, 24,
		WGL_STENCIL_BITS_ARB, 8,
		WGL_ACCELERATION_ARB, WGL_FULL_ACCELERATION_ARB,
		WGL_SAMPLE_BUFFERS_ARB, GL_TRUE,
		WGL_SAMPLES_ARB, 4,
		0
	};


	int pixel_format;
    UINT num_formats;
    wglChoosePixelFormatARB(this->device_context, pixel_format_attribs, 0, 1, &pixel_format, &num_formats);
    if (!num_formats) {
        fatal_error("Failed to set the OpenGL pixel format.");
    }

    PIXELFORMATDESCRIPTOR pfd;
    DescribePixelFormat(this->device_context, pixel_format, sizeof(pfd), &pfd);
    if (!SetPixelFormat(this->device_context, pixel_format, &pfd)) {
        fatal_error("Failed to set the OpenGL pixel format.");
    }

    const int gl_attribs[] = {
        WGL_CONTEXT_MAJOR_VERSION_ARB, 4,
        WGL_CONTEXT_MINOR_VERSION_ARB, 6,
        WGL_CONTEXT_PROFILE_MASK_ARB,  WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
        0,
    };

    opengl_context = wglCreateContextAttribsARB(this->device_context, 0, gl_attribs);
    if (!opengl_context) {
        fatal_error("Failed to create OpenGL context.");
    }

    if (!wglMakeCurrent(this->device_context, this->opengl_context)) {
        fatal_error("Failed to activate OpenGL rendering context.");
    }

	// Load actual OpenGL Functions from Driver
	if (!sogl_loadOpenGL()) {
		const char **failures = sogl_getFailures();
		while (*failures)
			printf("SOGL WIN32 EXAMPLE: Failed to load function %s\n", *failures++);
	}
}

OpenGLWindow::~OpenGLWindow() {
	wglDeleteContext(this->opengl_context);
	ReleaseDC(this->win.wnd, this->device_context); // release DC
}



void OpenGLWindow::updateScreen() {
}

LRESULT OpenGLWindow::StaticWndProc(void *const win, const UINT msg, const WPARAM wParam, const LPARAM lParam) {
	return ((OpenGLWindow*)win)->WndProc(msg, wParam, lParam);
}

LRESULT OpenGLWindow::WndProc(const UINT msg, const WPARAM wParam, const LPARAM lParam) {
    switch (msg) {
		case WM_SIZE:
		case WM_SIZING:
			glViewport(0, 0, win.width, win.height);
			break;
	}
	return DefWindowProc(win.wnd, msg, wParam, lParam);
}


























// https://gist.github.com/nickrolfe/1127313ed1dbf80254b614a721b3ee9c

// Before we can load extensions, we need a dummy OpenGL context, created using a dummy window.
// We use a dummy window because you can only set the pixel format for a window once. For the
// real window, we want to use wglChoosePixelFormatARB (so we can potentially specify options
// that aren't available in PIXELFORMATDESCRIPTOR), but we can't load and use that before we
// have a context.

static void init_opengl_extensions() {
    WNDCLASSW window_class = {
        .style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC,
        .lpfnWndProc = DefWindowProcA,
        .hInstance = GetModuleHandle(0),
        .lpszClassName = L"OpenglWindowClassName",
    };

    if (!RegisterClassW(&window_class)) {
        fatal_error("Failed to register dummy OpenGL window.");
    }

    HWND dummy_window = CreateWindowExW(
        0,
        window_class.lpszClassName,
        L"Dummy OpenGL Window",
        0,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        0,
        0,
        window_class.hInstance,
        0);

    if (!dummy_window) {
        fatal_error("Failed to create dummy OpenGL window.");
    }

    HDC dummy_dc = GetDC(dummy_window);

    PIXELFORMATDESCRIPTOR pfd = {
        .nSize = sizeof(pfd),
        .nVersion = 1,
        .dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
        .iPixelType = PFD_TYPE_RGBA,
        .cColorBits = 32,
        .cAlphaBits = 8,
        .cDepthBits = 24,
        .cStencilBits = 8,
        .iLayerType = PFD_MAIN_PLANE,
    };

    int pixel_format = ChoosePixelFormat(dummy_dc, &pfd);
    if (!pixel_format) {
        fatal_error("Failed to find a suitable pixel format.");
    }
    if (!SetPixelFormat(dummy_dc, pixel_format, &pfd)) {
        fatal_error("Failed to set the pixel format.");
    }

    HGLRC dummy_context = wglCreateContext(dummy_dc);
    if (!dummy_context) {
        fatal_error("Failed to create a dummy OpenGL rendering context.");
    }

    if (!wglMakeCurrent(dummy_dc, dummy_context)) {
        fatal_error("Failed to activate dummy OpenGL rendering context.");
    }

    wglCreateContextAttribsARB = (wglCreateContextAttribsARB_type*)wglGetProcAddress("wglCreateContextAttribsARB");
    wglChoosePixelFormatARB = (wglChoosePixelFormatARB_type*)wglGetProcAddress("wglChoosePixelFormatARB");

    wglMakeCurrent(dummy_dc, 0);
    wglDeleteContext(dummy_context);
    ReleaseDC(dummy_window, dummy_dc);
    DestroyWindow(dummy_window);
}

*/