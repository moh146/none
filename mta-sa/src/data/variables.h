#pragma once
#include <includes.h>

class c_variables {
public:
	struct
	{
		HMODULE hModule;
		MH_STATUS mh_status;
		WNDPROC wnd_proc;
		D3DDEVICE_CREATION_PARAMETERS device_par;
		IDirect3DDevice9* device_dx9{ nullptr };
	} winapi;

	struct
	{
		ImGuiWindowFlags flags{ ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoResize };
		ImVec2 size{ 800, 500 };
	} window;

	struct
	{
		bool initialized{ false };
		bool is_open{ false };
		bool is_dump_open{ false };
	} gui;

	struct
	{
		TextEditor executor;
	} editor;


	bool Send_Script_Packet = false;
};

inline c_variables* var = new c_variables();