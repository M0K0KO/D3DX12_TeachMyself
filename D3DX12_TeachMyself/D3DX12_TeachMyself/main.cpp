#include "Application.h"

int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
	try
	{
		return Application{}.Run();
	}
	catch (const std::exception& e)
	{
		MessageBoxA(nullptr, e.what(), "EXCEPTION THROWN", MB_OK | MB_ICONEXCLAMATION);
	}

	return -1;
}