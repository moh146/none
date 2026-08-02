#include "menu/menu.h"
#include <data/elements.h>
#include <data/variables.h>
#include <console/console.h>
#include <client/client.h>
#include <netc/netc.h>
#include <utilities/utilities.h>
#include <hashes.hpp>
#include <fonts.hpp>
#include "font.h"

LRESULT WINAPI h_wndproc(HWND handle, UINT message, WPARAM word_param, LPARAM long_param)
{
    ImGui_ImplWin32_WndProcHandler(handle, message, word_param, long_param);

    const auto& io = ImGui::GetIO();
    if (var->gui.is_open && (io.WantCaptureMouse || io.WantCaptureKeyboard))
    {
        return true;
    }

    return CallWindowProcA(var->winapi.wnd_proc, handle, message, word_param, long_param);
}

HRESULT __stdcall h_present(IDirect3DDevice9* self, const RECT* sourceRect, const RECT* destRect, HWND destWindowOverride, const RGNDATA* dirtyRegion)
{
    var->winapi.device_dx9 = self;

    if (var->winapi.device_dx9)
    {
        menu->initialize();
        menu->draw();
    }

    return menu->o_present(self, sourceRect, destRect, destWindowOverride, dirtyRegion);
}

HRESULT __stdcall h_reset(IDirect3DDevice9* self, D3DPRESENT_PARAMETERS* presentationParameters)
{
    menu->shutdown(true);
    HRESULT result = menu->o_reset(self, presentationParameters);
    menu->shutdown(false);
    return result;
}

BOOL WINAPI h_cursor(int pos_x, int pos_y)
{
    if (var->gui.is_open)
        return FALSE;

    return menu->o_cursor(pos_x, pos_y);
}

void c_menu::initialize()
{
    if (var->gui.initialized)
        return;

    var->winapi.device_dx9->GetCreationParameters(&var->winapi.device_par);
    var->winapi.wnd_proc = (WNDPROC)SetWindowLongW(var->winapi.device_par.hFocusWindow, GWL_WNDPROC, (LONG)h_wndproc);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = NULL;

    ImGui::StyleColorsDark();

    ImGui_ImplWin32_Init(var->winapi.device_par.hFocusWindow);
    ImGui_ImplDX9_Init(var->winapi.device_dx9);
    ImGui_ImplDX9_InvalidateDeviceObjects();

    var->editor.executor.SetLanguageDefinition(TextEditor::LanguageDefinition::Lua());
    var->editor.executor.SetText(xorstr_("-- Nebul Fexecutor"));

    var->gui.initialized = true;
}

void c_menu::draw()
{
    if (element->binds.menu_bind != 0 && GetAsyncKeyState(element->binds.menu_bind) & 1)
    {
        var->gui.is_open = !var->gui.is_open;
    }

    ImGui_ImplDX9_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    if (var->gui.is_open)
    {
        ImGui::SetNextWindowSize(ImVec2(var->window.size.x, var->window.size.y + 50), ImGuiCond_Once);
        ImGui::Begin("Nebul Fexecutor");
        {
            var->editor.executor.Render(xorstr_("Lua Editor"), ImVec2(ImGui::GetContentRegionAvail().x, 300));

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::Checkbox(xorstr_("Enable Dumping"), &element->dump.dump_enabled);
            ImGui::SameLine();
            ImGui::Checkbox(xorstr_("DebugHook Spoofer"), &element->info.break_debughook);
            ImGui::SameLine();
            ImGui::Checkbox(xorstr_("Bypass AntiCheats"), &element->info.bypass_lua_anticheats);

            ImGui::Spacing();

            if (ImGui::Button(xorstr_("Execute"), ImVec2(80, 25)))
            {
                std::string lua_code = var->editor.executor.GetText();
                if (element->content.loaded_client && !lua_code.empty())
                {
                    if (element->executor.item_current >= 0 && element->executor.item_current < (int)element->executor.resources_list.size())
                    {
                        const auto& selected_exec = element->executor.resources_list[element->executor.item_current];
                        var->Send_Script_Packet = true;
                        client->load_code(selected_exec.resource_name.c_str(), lua_code.c_str(), lua_code.length());
                    }
                }
            }

            ImGui::SameLine();
            if (ImGui::Button(xorstr_("Clear"), ImVec2(80, 25)))
            {
                var->editor.executor.SetText("");
            }

            ImGui::SameLine();
            if (ImGui::Button(xorstr_("Stop"), ImVec2(80, 25)))
            {
                if (element->executor.item_current >= 0 && element->executor.item_current < (int)element->executor.resources_list.size())
                {
                    const auto& selected_exec = element->executor.resources_list[element->executor.item_current];

                    auto it = std::find_if(
                        element->resource.resources_list.begin(),
                        element->resource.resources_list.end(),
                        [&](const s_resource& res) { return res.resource_name == selected_exec.resource_name; }
                    );

                    if (it != element->resource.resources_list.end())
                    {
                        client->stop_resource(it->resource_ptr);
                    }
                }
            }

            ImGui::SameLine();
            std::vector<const char*> items;
            for (const auto& item : element->executor.resources_list)
            {
                items.push_back(item.resource_name.c_str());
            }

            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            ImGui::Combo(xorstr_("##Select a Resource"), &element->executor.item_current, items.data(), (int)items.size(), -1);
        }
        ImGui::End();

        ImGui::GetIO().MouseDrawCursor = var->gui.is_open;
    }

    ImGui::EndFrame();
    ImGui::Render();
    ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
}

void c_menu::shutdown(bool before)
{
    if (before)
    {
        ImGui_ImplDX9_InvalidateDeviceObjects();
        return;
    }

    ImGui_ImplDX9_CreateDeviceObjects();
}

void c_menu::style()
{
    // Using default ImGui dark style
}

bool c_menu::release()
{
    var->winapi.mh_status = MH_CreateHook(&::SetCursorPos, &h_cursor, reinterpret_cast<LPVOID*>(&o_cursor));
    if (var->winapi.mh_status != MH_OK)
        return false;

    var->winapi.mh_status = MH_EnableHook(&::SetCursorPos);
    if (var->winapi.mh_status != MH_OK)
        return false;

    void* dw_present = reinterpret_cast<void*>(utilities::c_device::get_address(17));

    var->winapi.mh_status = MH_CreateHook(dw_present, &h_present, reinterpret_cast<LPVOID*>(&o_present));
    if (var->winapi.mh_status != MH_OK)
        return false;

    var->winapi.mh_status = MH_EnableHook(dw_present);
    if (var->winapi.mh_status != MH_OK)
        return false;

    void* dw_reset = reinterpret_cast<void*>(utilities::c_device::get_address(16));

    var->winapi.mh_status = MH_CreateHook(dw_reset, &h_reset, reinterpret_cast<LPVOID*>(&o_reset));
    if (var->winapi.mh_status != MH_OK)
        return false;

    var->winapi.mh_status = MH_EnableHook(dw_reset);
    if (var->winapi.mh_status != MH_OK)
        return false;

    return true;
}
