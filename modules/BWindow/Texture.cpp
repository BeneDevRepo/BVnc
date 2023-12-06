#include "Texture.h"

#include <cmath>
#include <algorithm>

Texture::Texture(uint32_t width, uint32_t height): width(width), height(height) {
    this->buffer = new uint32_t[width*height];
}

Texture::~Texture() {
    delete[] buffer;
}

void Texture::clear(uint32_t color) {
    for (uint32_t i = 0; i < this->width * this->height; i++)
        this->buffer[i] = color;
}

void Texture::setPixel(uint32_t x, uint32_t y, uint32_t color) {
    if(x>=this->width || y>=this->height) return;
    this->buffer[y * this->width + x] = color;
}

void Texture::line(int32_t x1_in, int32_t y1_in, int32_t x2_in, int32_t y2_in, uint32_t color) {
    auto map = [](float v, float vMin, float vMax, float outMin, float outMax)->float{ float t=(v-vMin)/(vMax-vMin); return outMin+(outMax-outMin)*t; };
	y1_in = y1_in;// Invert y axis
	y2_in = y2_in;
    int32_t dx = x2_in - x1_in;
    int32_t dy = y2_in - y1_in;
    bool hor = abs(dx) > abs(dy);
    if(hor) {
        int32_t x1 = std::min<int32_t>(x1_in, x2_in);
        int32_t x2 = std::max<int32_t>(x1_in, x2_in);
        int32_t y1 = (x1_in < x2_in) ? y1_in : y2_in;
        int32_t y2 = (x1_in < x2_in) ? y2_in : y1_in;
        for(int32_t x=std::max<int32_t>(x1, 0); x<x2; x++) {
            setPixel(x, (int32_t)map(x, x1, x2, y1, y2), color);
			if(x >= (int32_t)width-1)
				break;
        }
    } else {
        int32_t y1 = std::min<int32_t>(y1_in, y2_in);
        int32_t y2 = std::max<int32_t>(y1_in, y2_in);
        int32_t x1 = (y1_in < y2_in) ? x1_in : x2_in;
        int32_t x2 = (y1_in < y2_in) ? x2_in : x1_in;
        for(int32_t y=std::max<int32_t>(y1, 0); y<y2; y++) {
            setPixel((int32_t)map(y, y1, y2, x1, x2), y, color);
			if(y >= (int32_t)height-1)
				break;
        }
    }
}

void Texture::drawRect(int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint32_t color) {
	this->line(x1, y1, x2, y1, color);
	this->line(x1, y2, x2, y2, color);
	this->line(x1, y1, x1, y2, color);
	this->line(x2, y1, x2, y2, color);
}

void Texture::drawRectRel(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color) {
	this->drawRect(x, y, x+w, y+h, color);
}

void Texture::fillRect(int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint32_t color) {
	for(int32_t y=std::max<int32_t>(y1, 0); y<std::min<int32_t>(y2, (int32_t)this->height); y++)
		for(int32_t x=std::max<int32_t>(x1, 0); x<std::min<int32_t>(x2, (int32_t)this->width); x++)
			this->buffer[y * this->width + x] = color;
}

void Texture::fillRectRel(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color) {
	this->fillRect(x, y, x+w, y+h, color);
}

void Texture::fillRectRounded(int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint16_t r, uint32_t color) {
	auto sq = [](int32_t a)->int32_t { return a*a; };
	for(int32_t y=std::max<int32_t>(y1, 0); y<std::min<int32_t>(y2, (int32_t)this->height); y++) {
		for(int32_t x=std::max<int32_t>(x1, 0); x<std::min<int32_t>(x2, (int32_t)this->width); x++) {
			if((x-x1) < r && (y-y1) < r) // upper left
				if(sq(x1+r-x) + sq(y1+r-y) > sq(r))
					continue;
			if((x2-1-x) < r && (y-y1) < r) // upper right
				if(sq(x2-1-r-x) + sq(y1+r-y) > sq(r))
					continue;
			if((x-x1) < r && (y2-1-y) < r) // lower left
				if(sq(x1+r-x) + sq(y2-1-r-y) > sq(r))
					continue;
			if((x2-1-x) < r && (y2-1-y) < r) // lower right
				if(sq(x2-1-r-x) + sq(y2-1-r-y) > sq(r))
					continue;
			this->buffer[y * this->width + x] = color;
		}
	}
}

void Texture::fillRectRounded(int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint16_t r, uint8_t corners, uint32_t color) {
	auto sq = [](int32_t a)->int32_t { return a*a; };
	for(int32_t y=std::max<int32_t>(y1, 0); y<std::min<int32_t>(y2, (int32_t)this->height); y++) {
		for(int32_t x=std::max<int32_t>(x1, 0); x<std::min<int32_t>(x2, (int32_t)this->width); x++) {
			if(corners & TC_TOP_LEFT)
				if((x-x1) < r && (y-y1) < r) // upper left
					if(sq(x1+r-x) + sq(y1+r-y) > sq(r))
						continue;
			if(corners & TC_TOP_RIGHT)
				if((x2-1-x) < r && (y-y1) < r) // upper right
					if(sq(x2-1-r-x) + sq(y1+r-y) > sq(r))
						continue;
			if(corners & TC_BOTTOM_LEFT)
				if((x-x1) < r && (y2-1-y) < r) // lower left
					if(sq(x1+r-x) + sq(y2-1-r-y) > sq(r))
						continue;
			if(corners & TC_BOTTOM_RIGHT)
				if((x2-1-x) < r && (y2-1-y) < r) // lower right
					if(sq(x2-1-r-x) + sq(y2-1-r-y) > sq(r))
						continue;
			this->buffer[y * this->width + x] = color;
		}
	}
}

void Texture::fillCircle(int32_t x, int32_t y, uint32_t r, uint32_t color) {
	auto sq = [](int32_t a)->int32_t { return a*a; };
	for(int32_t yl=std::max<int32_t>(y-r, 0); yl<std::min<int32_t>(y+r, (int32_t)this->height); yl++) {
		for(int32_t xl=std::max<int32_t>(x-r, 0); xl<std::min<int32_t>(x+r, (int32_t)this->width); xl++) {
			if(sq(x-xl) + sq(y-yl) > sq(r))
				continue;
			this->buffer[yl * this->width + xl] = color;
		}
	}
}

void Texture::blit(Texture* tex, int32_t x1, int32_t y1) {
	for(uint32_t y=0; y<tex->height; y++) {
		for(uint32_t x=0; x<tex->width; x++) {
			if(x1 + (int32_t)x < 0 || y1 + (int32_t)y < 0)
				continue;
			uint32_t xTarget = x1 + x;
			uint32_t yTarget = y1 + y;
			if(xTarget < this->width && yTarget < this->height)
				buffer[yTarget * this->width + xTarget] = tex->buffer[y*tex->width+x];
		}
	}
}

void Texture::blitConstAlpha(Texture* tex, int32_t x1, int32_t y1, uint8_t alpha) {
	static constexpr uint32_t COLORMASK = 0x00ffffff;
	const uint32_t alphaBits = alpha << 24;
	for(uint32_t y=0; y<tex->height; y++) {
		for(uint32_t x=0; x<tex->width; x++) {
			if(x1 + (int32_t)x < 0 || y1 + (int32_t)y < 0)
				continue;
			uint32_t xTarget = x1 + x;
			uint32_t yTarget = y1 + y;
			if(xTarget < this->width && yTarget < this->height)
				buffer[yTarget * this->width + xTarget] = (tex->buffer[y*tex->width+x] & COLORMASK) | alphaBits;
		}
	}
}

void Texture::resize(uint32_t _width, uint32_t _height) {
	this->width = _width;
	this->height = _height;
	delete[] this->buffer;
	this->buffer = new uint32_t[_width * _height];
}