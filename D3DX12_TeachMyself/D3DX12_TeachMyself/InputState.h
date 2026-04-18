#pragma once
#include <bitset>
#include "Window.h"

struct InputState
{
	std::bitset<256> keyDown;
	std::bitset<256> keyPressed;
	std::bitset<256> keyReleased;

	int mouseX = 0, mouseY = 0;
	bool leftDown = false;
	bool rightDown = false;
	bool inWindow = false;

	int rawDeltaX = 0;
	int rawDeltaY = 0;
	int wheelDelta = 0;

	void Capture(Window& wnd);

	bool IsKeyDown(unsigned char k)     const { return keyDown[k]; }
	bool WasKeyPressed(unsigned char k) const { return keyPressed[k]; }
	bool WasKeyReleased(unsigned char k)const { return keyReleased[k]; }

	bool IsLeftDown()  const { return leftDown; }
	bool IsRightDown() const { return rightDown; }
};