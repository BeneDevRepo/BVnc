#pragma once

namespace Input {
	void init();
    void poll();
    bool isDown(unsigned char key);
    bool isUp(unsigned char key);
    bool pressed(unsigned char key);
    bool released(unsigned char key);
}