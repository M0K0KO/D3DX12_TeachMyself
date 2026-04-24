#pragma once
#include <optional>
#include <functional>
#include <Windows.h>
#include <vector>
#include <string>
#include "Mouse.h"
#include "Keyboard.h"

class Window
{
public:
	class WindowClass
	{
	public:
		static const wchar_t* GetName();
		static HINSTANCE GetInstance();

	private:
		WindowClass();
		~WindowClass();
		WindowClass(const WindowClass&) = delete;
		WindowClass& operator= (const WindowClass&) = delete;
		static constexpr const wchar_t* wndClassName = L"Moko Engine Window";
		static WindowClass wndClass;
		HINSTANCE hInst;
	};

public:
	Window(int width, int height, const wchar_t* name);
	~Window();
	Window(const Window&) = delete;
	Window& operator= (const Window&) = delete;
	const HWND GetHWND();
	const int GetWidth();
	const int GetHeight();
	const float GetAspectRatio();
	void SetTitle(const std::wstring& title);
	void EnableCursor();
	void DisableCursor();
	bool CursorEnabled() const;
	static std::optional<int> ProcessMessages();

	using ResizeCallback = std::function<void(uint32_t, uint32_t)>;
	void SetResizeCallback(ResizeCallback callback) { m_resizeCallback = callback; }



private:
	void ConfineCursor();
	void FreeCursor();
	void HideCursor();
	void ShowCursor();

	void EnableImGuiMouse();
	void DisableImGuiMouse();
	void EnableImGuiKeyboard();
	void DisableImGuiKeyboard();

	static LRESULT CALLBACK HandleMsgSetup(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
	static LRESULT CALLBACK HandleMsgThunk(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
	LRESULT HandleMsg(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

public:
	Keyboard kbd;
	Mouse mouse;

private:
	int width;
	int height;
	HWND hWnd;

	std::vector<BYTE> rawBuffer;
	bool cursorEnabled = true;

	ResizeCallback m_resizeCallback;
	bool m_isResizing = false;

	bool m_imGuiInputEnabled = true;
};