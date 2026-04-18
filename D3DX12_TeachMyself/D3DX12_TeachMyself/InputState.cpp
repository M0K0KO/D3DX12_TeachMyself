#include "InputState.h"

void InputState::Capture(Window& wnd)
{
	keyPressed.reset();
	keyReleased.reset();
	rawDeltaX = rawDeltaY = 0;
	wheelDelta = 0;

	while (auto e = wnd.kbd.ReadKey())
	{
		if (e->IsPress()) keyPressed.set(e->GetCode());
		if (e->IsRelease()) keyReleased.set(e->GetCode());
	}

	for (int i = 0; i < 256; i++)
	{
		keyDown[i] = wnd.kbd.KeyIsPressed(i);
	}

	while (!wnd.mouse.IsEmpty())
	{
		auto e = wnd.mouse.Read();
		if (!e.IsValid()) break;
		using Type = Mouse::Event::Type;
		switch (e.GetType())
		{
		case Type::WheelUp:   wheelDelta += 1; break;
		case Type::WheelDown: wheelDelta -= 1; break;
		default: break;
		}
	}

	while (auto d = wnd.mouse.ReadRawDelta())
	{
		rawDeltaX += d->x;
		rawDeltaY += d->y;
	}

	mouseX = wnd.mouse.GetPosX();
	mouseY = wnd.mouse.GetPosY();
	leftDown = wnd.mouse.LeftIsPressed();
	rightDown = wnd.mouse.RightIsPressed();
	inWindow = wnd.mouse.IsInWindow();
}