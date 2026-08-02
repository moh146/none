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
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.IniFilename = NULL;
    ImGui::StyleColorsDark();
    io.Fonts->AddFontFromMemoryTTF(museo500_binary, sizeof museo500_binary, 14);
    static const ImWchar icon_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
    ImFontConfig icons_config;
    icons_config.MergeMode = true;
    icons_config.PixelSnapH = true;
    io.Fonts->AddFontFromMemoryTTF(&font_awesome_binary, sizeof font_awesome_binary, 13, &icons_config, icon_ranges);

    io.Fonts->AddFontFromMemoryTTF(museo900_binary, sizeof museo900_binary, 28);
    style();

    ImGui_ImplWin32_Init(var->winapi.device_par.hFocusWindow);
    ImGui_ImplDX9_Init(var->winapi.device_dx9);
    ImGui_ImplDX9_InvalidateDeviceObjects();

    var->editor.executor.SetLanguageDefinition(TextEditor::LanguageDefinition::Lua());
    var->editor.executor.SetText(xorstr_("-- Menu By @n1tro.101"));

    var->gui.initialized = true;
}
bool CCheckbox(const char* label, bool* v)
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);
    const ImVec2 label_size = ImGui::CalcTextSize(label, NULL, true);

    const float square_sz = ImGui::GetFrameHeight();
    const ImVec2 pos = window->DC.CursorPos;
    const ImRect total_bb(pos, pos + ImVec2(square_sz + (label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f), label_size.y + style.FramePadding.y * 2.0f));

    ImGui::ItemSize(total_bb, style.FramePadding.y);
    if (!ImGui::ItemAdd(total_bb, id))
        return false;

    bool hovered, held;
    bool pressed = ImGui::ButtonBehavior(total_bb, id, &hovered, &held);
    if (pressed)
    {
        *v = !(*v);
        ImGui::MarkItemEdited(id);
    }

    // Render
    const ImRect check_bb(pos, pos + ImVec2(square_sz, square_sz));
    ImGui::RenderFrame(check_bb.Min, check_bb.Max, ImGui::GetColorU32(ImVec4(0.2f, 0.2f, 0.2f, 1.0f)), true, style.FrameRounding);

    ImU32 check_col = ImGui::GetColorU32(ImVec4(0.0f, 1.0f, 0.0f, 1.0f)); 
    if (*v)
    {

        const float pad = ImMax(1.0f, (float)(int)(square_sz / 6.0f));
        ImGui::RenderCheckMark(window->DrawList, check_bb.Min + ImVec2(pad, pad), check_col, square_sz - pad * 2.0f);
    }

    if (label_size.x > 0.0f)
    {
        ImGui::RenderText(ImVec2(check_bb.Max.x + style.ItemInnerSpacing.x, check_bb.Min.y + style.FramePadding.y), label);
    }

    return pressed;
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
        ImGui::SetNextWindowSize(ImVec2(var->window.size.x, var->window.size.y + 50));
        ImGui::Begin("##empty", nullptr,
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoResize);
        {
            {
                ImVec2 pos = ImGui::GetWindowPos();
                ImVec2 size = ImGui::GetWindowSize();
                ImDrawList* draw = ImGui::GetWindowDrawList();

                draw->AddRectFilled(pos, pos + size, ImColor(31, 31, 31, 255), 0.0f);
                draw->AddRect(pos, pos + size, ImColor(50, 50, 50, 255), 0.0f, 0, 1.0f);

                auto& io = ImGui::GetIO();
                const char* title = "Nebul Fexecutor";

                ImVec2 title_size = ImGui::CalcTextSize(title, NULL, true, -1.0f);
                ImVec2 text_pos = pos + ImVec2((size.x - title_size.x) * 0.5f, 15);

                int vtx_start = draw->VtxBuffer.Size;
                draw->AddText(io.Fonts->Fonts[1], 26.0f, text_pos, IM_COL32(255, 255, 255, 255), title);
                int vtx_end = draw->VtxBuffer.Size;

                float time = ImGui::GetTime();
                float wave_speed = 3.0f;
                float wave_height = 3.0f;

                for (int i = vtx_start; i < vtx_end; i++) {
                    ImDrawVert& vtx = draw->VtxBuffer[i];
                    float local_x = vtx.pos.x - text_pos.x;

                    vtx.pos.y += sinf((local_x * 0.05f) + (time * wave_speed)) * wave_height;

                    float factor = (sinf(time * 2.0f + local_x * 0.02f) * 0.5f) + 0.5f;
                    ImVec4 base = ImVec4(0.2f, 0.5f, 1.0f, 1.0f);  
                    ImVec4 glow = ImVec4(0.9f, 0.9f, 1.0f, 1.0f);  
                    ImVec4 final_col = ImLerp(base, glow, factor);

                    vtx.col = ImColor(final_col);
                }
                ImGui::Dummy(ImVec2(0.0f, 40.0f));
                if (ImGui::BeginTabBar(xorstr_("MainTab"), ImGuiTabBarFlags_None))
                {
                    var->editor.executor.Render(xorstr_("Lua Editor"), ImVec2(ImGui::GetContentRegionAvail().x, 350));

                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();

                    ImGui::Columns(2, nullptr, false);

                    {
                        CCheckbox(xorstr_("Enable Dumping"), &element->dump.dump_enabled);
                        ImGui::SameLine();
                        CCheckbox(xorstr_("DebugHook Spoofer"), &element->info.break_debughook);
                        ImGui::SameLine();
                        CCheckbox(xorstr_("Bypass AntiCheats"), &element->info.bypass_lua_anticheats);

                        ImGui::Spacing();

                        if (ImGui::Button(xorstr_(ICON_FA_PLAY " Execute"), ImVec2(ImGui::GetContentRegionAvail().x / 3, 25)))
                        {
                            std::string lua_code = var->editor.executor.GetText();
                            if (element->content.loaded_client && !lua_code.empty())
                            {
                                if (element->executor.item_current >= 0 && element->executor.item_current < element->executor.resources_list.size())
                                {
                                    const auto& selected_exec = element->executor.resources_list[element->executor.item_current];
                                    var->Send_Script_Packet = true;
                                    client->load_code(selected_exec.resource_name.c_str(), lua_code.c_str(), lua_code.length());
                                }
                            }
                        }

                        ImGui::SameLine();
                        if (ImGui::Button(xorstr_(ICON_FA_BROOM " Clear"), ImVec2(ImGui::GetContentRegionAvail().x / 3, 25)))
                        {
                            var->editor.executor.SetText("");
                        }

                        ImGui::SameLine();
                        if (ImGui::Button(xorstr_(ICON_FA_STOP " Stop"), ImVec2(ImGui::GetContentRegionAvail().x / 3, 25)))
                        {
                            if (element->executor.item_current >= 0 && element->executor.item_current < element->executor.resources_list.size())
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
                        ImGui::Combo("##Select a Resource", &element->executor.item_current, items.data(), (int)items.size(), -1);


                        ImGui::Columns(1);
                    }
                    ImGui::EndTabBar();

                }
            }


            ImGui::End();
        }

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
    ImGuiStyle* style = &ImGui::GetStyle();

    style->WindowPadding = ImVec2(8, 8);
    style->FramePadding = ImVec2(6, 4);
    style->ItemSpacing = ImVec2(8, 6);
    style->ScrollbarSize = 12.0f;
    style->GrabMinSize = 8.0f;

    style->WindowRounding = 2.0f;
    style->FrameRounding = 3.0f;
    style->PopupRounding = 2.0f;
    style->ScrollbarRounding = 2.0f;
    style->GrabRounding = 2.0f;
    style->TabRounding = 3.0f;


    style->WindowBorderSize = 0.0f;
    style->FrameBorderSize = 0.0f;
    style->PopupBorderSize = 0.0f;
    style->TabBorderSize = 0.0f;

    ImVec4* colors = style->Colors;

    colors[ImGuiCol_WindowBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.0f); 
    colors[ImGuiCol_ChildBg] = ImVec4(0.12f, 0.12f, 0.12f, 1.0f); 
    colors[ImGuiCol_Button] = ImVec4(0.18f, 0.18f, 0.18f, 1.0f); 
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.25f, 0.25f, 0.25f, 1.0f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.30f, 0.30f, 0.30f, 1.0f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.15f, 0.15f, 0.15f, 1.0f); 

    colors[ImGuiCol_Text] = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f); 
    colors[ImGuiCol_ChildBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);

    colors[ImGuiCol_Button] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);

    colors[ImGuiCol_FrameBg] = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);

    colors[ImGuiCol_Tab] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_TabActive] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);

    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);

    colors[ImGuiCol_Header] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);

    colors[ImGuiCol_PlotLines] = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.40f, 0.40f, 1.00f);
}


bool c_menu::release()
{
    var->winapi.mh_status = MH_CreateHook(&::SetCursorPos, &h_cursor, reinterpret_cast<LPVOID*>(&o_cursor));
    if (var->winapi.mh_status != MH_OK)
    {
        return false;
    }

    var->winapi.mh_status = MH_EnableHook(&::SetCursorPos);
    if (var->winapi.mh_status != MH_OK)
    {
        return false;
    }

    /* ============ */

    void* dw_present = reinterpret_cast<void*>(utilities::c_device::get_address(17));

    var->winapi.mh_status = MH_CreateHook(dw_present, &h_present, reinterpret_cast<LPVOID*>(&o_present));
    if (var->winapi.mh_status != MH_OK)
    {
        return false;
    }

    var->winapi.mh_status = MH_EnableHook(dw_present);
    if (var->winapi.mh_status != MH_OK)
    {
        return false;
    }

    /* ============ */

    void* dw_reset = reinterpret_cast<void*>(utilities::c_device::get_address(16));

    var->winapi.mh_status = MH_CreateHook(dw_reset, &h_reset, reinterpret_cast<LPVOID*>(&o_reset));
    if (var->winapi.mh_status != MH_OK)
    {
        return false;
    }

    var->winapi.mh_status = MH_EnableHook(dw_reset);
    if (var->winapi.mh_status != MH_OK)
    {
        return false;
    }

    return true;
}

