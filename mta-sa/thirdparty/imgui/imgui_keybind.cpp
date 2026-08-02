#include "imgui_keybind.h"

bool c_keybind::add(std::string_view label, int* keystate)
{
    struct key_select_state
    {
        float alpha{ 0 };
    };

    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    ImGuiIO& io = g.IO;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label.data());
    const ImVec2 pos = window->DC.CursorPos;

    const ImVec2 label_size = ImGui::CalcTextSize(label.data());
    const ImRect label_rect(pos, pos + ImVec2(label_size.x + 5, 20));

    const ImVec2 button_pos = pos + ImVec2(label_size.x + 5, 0);
    const ImRect key_rect(button_pos, button_pos + ImVec2(40, 20));

    ImGui::ItemSize(key_rect, style.FramePadding.y);
    if (!ImGui::ItemAdd(key_rect, id))
        return false;

    const bool hovered = ImGui::ItemHoverable(key_rect, id, 0);
    const bool pressed = hovered && io.MouseClicked[0];

    if (pressed)
    {
        if (g.ActiveId != id)
        {
            memset(io.MouseDown, 0, sizeof(io.MouseDown));
            memset(io.KeysDown, 0, sizeof(io.KeysDown));
            *keystate = 0;
        }
        ImGui::SetActiveID(id, window);
        ImGui::FocusWindow(window);
    }
    else if (io.MouseClicked[0])
    {
        if (g.ActiveId == id)
            ImGui::ClearActiveID();
    }

    bool value_changed = false;
    int key = *keystate;

    if (g.ActiveId == id)
    {
        for (auto i = 0; i < 5; i++)
        {
            if (io.MouseDown[i])
            {
                switch (i)
                {
                case 0:
                    key = 0x01;
                    break;
                case 1:
                    key = 0x02;
                    break;
                case 2:
                    key = 0x04;
                    break;
                case 3:
                    key = 0x05;
                    break;
                case 4:
                    key = 0x06;
                    break;
                }
                value_changed = true;
                ImGui::ClearActiveID();
            }
        }

        if (!value_changed) 
        {
            for (auto i = 0x08; i <= 0xA5; i++)
            {
                if (io.KeysDown[i]) 
                {
                    if (i == 0x10 || i == 0x11 || i == 0x12 || i == 0xA0 || i == 0xA1 || i == 0xA2 || i == 0xA3 || i == 0xA4 || i == 0xA5)
                        continue;

                    key = i;
                    value_changed = true;
                    ImGui::ClearActiveID();
                }
            }
        }

        if (ImGui::IsKeyPressed(ImGuiKey_Escape))
        {
            *keystate = 0;
            ImGui::ClearActiveID();
        }
        else 
        {
            *keystate = key;
        }
    }

    std::string buf_display = "NONE";

    if (*keystate != 0 && g.ActiveId != id)
    {
        buf_display = keys[*keystate];
    }
    else if (g.ActiveId == id)
    {
        buf_display = "...";
    }

    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    draw_list->AddText(label_rect.Min, ImColor(255, 255, 255), label.data());
    draw_list->AddRectFilled(key_rect.Min, key_rect.Max, ImColor(28, 42, 60), 5.0f);
    draw_list->AddText(key_rect.Min, ImColor(255, 255, 255), buf_display.data());

    return value_changed;
}