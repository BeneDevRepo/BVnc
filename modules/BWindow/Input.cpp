#include "Input.h"

#include "winapi_include.h"

#include <cstdint>

static unsigned char _buf1[256], _buf2[256];
static unsigned char *_keys, *_lastKeys;

void Input::init() {
	_keys = _buf1;
	_lastKeys = _buf2;
}

void Input::poll() {
	(uint32_t&)_lastKeys ^= (uint32_t&)_keys;     // a = A XOR B
	(uint32_t&)_keys     ^= (uint32_t&)_lastKeys; // b = B XOR ( A XOR B ) = A
	(uint32_t&)_lastKeys ^= (uint32_t&)_keys;     // a = ( A XOR B ) XOR A = B
    GetKeyboardState(_keys);
}

bool isDown(unsigned char key) {
	return _keys[key] & 0x80;
}

bool isUp(unsigned char key) {
	return !(_keys[key] & 0x80);
}

bool Input::pressed(unsigned char key) {
    return _keys[key] & 0x80 && !(_lastKeys[key] & 0x80);
}

bool Input::released(unsigned char key) {
    return !(_keys[key] & 0x80) && _lastKeys[key] & 0x80;
}