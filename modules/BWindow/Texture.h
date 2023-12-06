#pragma once

#include "winapi_include.h"
#include <cstdint>

typedef uint8_t Corner;
constexpr Corner TC_TOP_LEFT     = 0b0001;
constexpr Corner TC_TOP_RIGHT    = 0b0010;
constexpr Corner TC_BOTTOM_LEFT  = 0b0100;
constexpr Corner TC_BOTTOM_RIGHT = 0b1000;
constexpr Corner TC_TOP          = 0b0011;
constexpr Corner TC_BOTTOM       = 0b1100;
constexpr Corner TC_LEFT         = 0b0101;
constexpr Corner TC_RIGHT        = 0b1010;

class Texture {
public:
    uint32_t *buffer;
    uint32_t width, height;
public:
    Texture(uint32_t width, uint32_t height);
    ~Texture();
    void clear(uint32_t color);
    void setPixel(uint32_t x, uint32_t y, uint32_t color);
    void line(int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint32_t color);
	void drawRect(int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint32_t color);
    void drawRectRel(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color);
    void fillRect(int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint32_t color);
    void fillRectRel(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color);
	void fillRectRounded(int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint16_t radius, uint32_t color);
	void fillRectRounded(int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint16_t radius, Corner corners, uint32_t color);
    void fillCircle(int32_t x, int32_t y, uint32_t r, uint32_t color);
	void blit(Texture* tex, int32_t x1, int32_t y1);
	void blitConstAlpha(Texture* tex, int32_t x1, int32_t y1, uint8_t alpha);
	void resize(uint32_t width, uint32_t height);
};