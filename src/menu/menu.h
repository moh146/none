#pragma once
#include <includes.h>

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

class c_menu {
private:
	using present_t = HRESULT(__stdcall*)(IDirect3DDevice9*, const RECT*, const RECT*, HWND, const RGNDATA*);
	using reset_t = HRESULT(__stdcall*)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);
	using cursor_t = BOOL(WINAPI*)(int, int);
public:
	present_t o_present;
	reset_t o_reset;
	cursor_t o_cursor;

	bool release();
	void initialize();
	void draw();
	void shutdown(bool before);
	void style();
};

inline c_menu* menu = new c_menu();