#pragma once
#include "stdafx.h"
#include <optional>

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
	static std::optional<int> ProcessMessages();

private:
	static LRESULT CALLBACK HandleMsgSetup(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
	static LRESULT CALLBACK HandleMsgThunk(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
	LRESULT HandleMsg(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
	int width;
	int height;
	HWND hWnd;
};