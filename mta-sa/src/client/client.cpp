#include "client.h"
#include <data/elements.h>
#include <utilities/utilities.h>
#include <console/console.h>
#include <menu/menu.h>
#include <data/variables.h>

void* __fastcall h_resource_manager(void* ecx, void* edx, unsigned short usNetID, const char* szResourceName, void* pResourceEntity, void* pResourceDynamicEntity, const std::string& strMinServerReq, const std::string& strMinClientReq, bool bEnableOOP)
{
    client->c_resource_manager = ecx;
    return client->o_resource_manager(ecx, usNetID, szResourceName, pResourceEntity, pResourceDynamicEntity, strMinServerReq, strMinClientReq, bEnableOOP);
}

void* __fastcall h_resource_constructor(void* ecx, void* edx, unsigned short usNetID, const char* szResourceName, void* pResourceEntity, void* pResourceDynamicEntity, const std::string& strMinServerReq, const std::string& strMinClientReq, bool bEnableOOP)
{
    void* result = client->o_resource_constructor(ecx, usNetID, szResourceName, pResourceEntity, pResourceDynamicEntity, strMinServerReq, strMinClientReq, bEnableOOP);

    if (result)
    {
        for (s_resource resource : element->resource.resources_list)
        {
            if (strcmp(szResourceName, resource.resource_name.c_str()) == 0)
                return result;
        }

        s_resource resource = {};
        resource.resource_name = szResourceName;
        resource.resource_ptr  = ecx;
        element->resource.resources_list.push_back(resource);
    }

    return result;
}

void* __fastcall h_get_virtual_machine(void* ecx, void*, void* lua_vm)
{
    client->c_lua_manager = ecx;
    return client->o_get_virtual_machine(ecx, lua_vm);
}

int __cdecl h_lua_load_buffer(void* lua_vm, const char* buff, unsigned int sz, const char* name)
{
    int result = client->o_lua_load_buffer(lua_vm, buff, sz, name);

    if (client->c_lua_manager != nullptr && lua_vm != nullptr && client->o_get_resource_name != nullptr && name != nullptr)
    {
        SString* resource_name = new SString;
        client->o_get_resource_name(client->c_lua_manager, resource_name, lua_vm);

        bool found = false;
        for (s_executor resource : element->executor.resources_list)
        {
            if (strcmp(resource_name->c_str(), resource.resource_name.c_str()) == 0)
            {
                found = true;
            }
        }

        if (!found)
        {
            s_executor executor = {};
            executor.resource_name = resource_name->c_str();
            executor.lua_vm        = lua_vm;
            element->executor.resources_list.push_back(executor);
        }
    }

    return result;
}

bool findStringIC(const std::string& strHaystack, const std::string& strNeedle)
{
    auto it = std::search(strHaystack.begin(), strHaystack.end(),
        strNeedle.begin(), strNeedle.end(),
        [](char ch1, char ch2) { return std::toupper(ch1) == std::toupper(ch2); });
    return (it != strHaystack.end());
}

bool __cdecl h_trigger_server_event(const char* szName, void* CallWithEntity, void* Arguments)
{
    bool skip_call = false;

    if (element->info.bypass_lua_anticheats)
    {
        if (findStringIC(szName, "anticheat:alert"))               skip_call = true;
        if (findStringIC(szName, "sac.sendPlayer"))                skip_call = true;
        if (findStringIC(szName, "anticheat"))                     skip_call = true;
        if (findStringIC(szName, "onCheatDetect"))                 skip_call = true;
        if (findStringIC(szName, "AntiCheat:Detected"))            skip_call = true;
        if (findStringIC(szName, "antiHacks:MoreEventAccess"))     skip_call = true;
        if (findStringIC(szName, "anticheat:allEvents"))           skip_call = true;
        if (findStringIC(szName, "XoopAc:outputForAll"))           skip_call = true;
        if (findStringIC(szName, "XoopAC:Ban"))                    skip_call = true;
        if (findStringIC(szName, "XoopAC:Kick"))                   skip_call = true;
        if (findStringIC(szName, "clairePunish"))                  skip_call = true;
        if (findStringIC(szName, "onBanidor"))                     skip_call = true;
        if (findStringIC(szName, "detected"))                      skip_call = true;
        if (findStringIC(szName, "onSomBAN"))                      skip_call = true;
        if (findStringIC(szName, "onBan"))                         skip_call = true;
        if (findStringIC(szName, "onkick"))                        skip_call = true;
        if (findStringIC(szName, "AntiCheat:Detect"))              skip_call = true;
        if (findStringIC(szName, "RXGuard:DetectExecuterLuaCheat")) skip_call = true;
        if (findStringIC(szName, "DetectExecuterLuaCheat"))        skip_call = true;
        if (findStringIC(szName, "BanPlayer"))                     skip_call = true;
        if (findStringIC(szName, "debughook"))                     skip_call = true;
        if (findStringIC(szName, "LuaExecutorDetected"))           skip_call = true;
        if (findStringIC(szName, "BypassDetected"))                skip_call = true;
    }

    if (skip_call)
    {
        s_events new_event = {};
        new_event.event_name = szName;
        new_event.is_blocked = true;
        element->event.events_list.push_back(new_event);
        return false;
    }

    return client->o_trigger_server_event(szName, CallWithEntity, Arguments);
}

bool __fastcall h_add_debug_hook(void* ecx, void* edx, EDebugHook::EDebugHookType hookType, const void* functionRef, const std::vector<SString>* allowedNameList)
{
    if (element->info.break_debughook)
        return false;
    return client->o_add_debug_hook(ecx, hookType, functionRef, allowedNameList);
}

bool __cdecl h_add_event_handler(void* lua_vm, const char* szName, void* Entity, const void* iLuaFunction, bool bPropagated, EEventPriority::EEventPriorityType eventPriority, float fPriorityMod)
{
    if ((element->info.break_onClientPaste && strcmp(szName, "onClientPaste") == 0) ||
        (element->info.break_onClientResourceStop && strcmp(szName, "onClientResourceStop") == 0))
    {
        return false;
    }
    return client->o_add_event_handler(lua_vm, szName, Entity, iLuaFunction, bPropagated, eventPriority, fPriorityMod);
}

void c_client::stop_resource(void* p_resource)
{
    o_stop_resource(client->c_resource_manager, p_resource);

    std::string resource_name;

    for (const auto& resource : element->resource.resources_list)
    {
        if (resource.resource_ptr == p_resource)
        {
            auto it = std::find(element->resource.resources_list.begin(), element->resource.resources_list.end(), resource);
            if (it != element->resource.resources_list.end())
            {
                resource_name = resource.resource_name;
                element->resource.resources_list.erase(it);
            }
            break;
        }
    }

    for (const auto& resource : element->executor.resources_list)
    {
        if (resource.resource_name == resource_name)
        {
            auto it = std::find(element->executor.resources_list.begin(), element->executor.resources_list.end(), resource);
            if (it != element->executor.resources_list.end())
            {
                element->executor.resources_list.erase(it);
            }
            break;
        }
    }
}

bool c_client::load_code(const char* resourceName, const char* buffer, unsigned int size)
{
    void* lua_state = element->executor.resources_list[element->executor.item_current].lua_vm;
    if (lua_state == nullptr)
        return false;

    void* c_lua_vm = client->o_get_virtual_machine(client->c_lua_manager, lua_state);
    if (c_lua_vm == nullptr)
        return false;

    var->Send_Script_Packet = true;
    return o_load_script(c_lua_vm, buffer, size, resourceName);
}

void c_client::release()
{
    while (true)
    {
        HMODULE hClient = GetModuleHandleA(xorstr_("client.dll"));
        if (hClient != nullptr)
        {
            if (!element->content.loaded_client)
            {
                // ── load_script ──────────────────────────────────────────────
                // 6A FF cookie; 5E D0 39 10 handler; 50 81 (push eax + sub esp) suffix
                o_load_script = (load_script_t)utilities::c_pattern::find_pattern(
                    "client.dll",
                    "\x55\x8B\xEC\x6A\xFF\x68\x5E\xD0\x39\x10\x64\xA1\x00\x00\x00\x00\x50\x81",
                    "xxxxxx?xxxxx?xxxxx");
                if (o_load_script == nullptr)
                    console->error(xorstr_("load_script address was not found.\n"));

                // ── lua_load_buffer ──────────────────────────────────────────
                // EC 24 stack alloc; cookie at A1 40 5A 4A 10; 8B 45 08 53 8B 5D suffix
                o_lua_load_buffer = (lua_load_buffer_t)utilities::c_pattern::find_pattern(
                    "client.dll",
                    "\x55\x8B\xEC\x83\xEC\x24\xA1\x40\x5A\x4A\x10\x33\xC5\x89\x45\xFC\x8B\x45\x08\x53\x8B\x5D",
                    "xxxxxxx?xxxxxxxxxxxxxx");
                if (o_lua_load_buffer != nullptr)
                {
                    MH_RemoveHook(o_lua_load_buffer);
                    MH_CreateHook(o_lua_load_buffer, &h_lua_load_buffer, reinterpret_cast<LPVOID*>(&o_lua_load_buffer));
                    MH_EnableHook(MH_ALL_HOOKS);
                }
                else
                    console->error(xorstr_("lua_load_buffer address not found.\n"));

                // ── get_resource_name ─────────────────────────────────────────
                // EC 2C stack; A1 40 5A 4A 10 cookie; 8B 4D 0C = MOV ECX,[EBP+0C]
                // (arg2 = lua_vm); 8D = LEA into local SString buffer
                // Signature: void* __thiscall get_resource_name(ecx=lua_manager,
                //                                SString* out, void* lua_vm)
                o_get_resource_name = (get_resource_name_t)utilities::c_pattern::find_pattern(
                    xorstr_("client.dll"),
                    "\x55\x8B\xEC\x83\xEC\x2C\xA1\x40\x5A\x4A\x10\x33\xC5\x89\x45\xFC\x8B\x4D\x0C\x8D",
                    xorstr_("xxxxxxx?xxxxxxxxxxxx"));
                if (o_get_resource_name != nullptr)
                    console->success(xorstr_("'get_resource_name' address found.\n"));
                else
                    console->error(xorstr_("get_resource_name not found.\n"));

                // ── add_event_handler ────────────────────────────────────────
                // 8B 75 0C arg offset; 90 26 5B 10 global ptr; 13 1B FC FF call offset
                o_add_event_handler = (add_event_handler_t)utilities::c_pattern::find_pattern(
                    xorstr_("client.dll"),
                    "\x55\x8B\xEC\x56\x8B\x75\x0C\x85\xF6\x75\x06\x89\x35\x00\x00\x00\x00"
                    "\x8B\x0D\x90\x26\x5B\x10\x56\xE8\x13\x1B\xFC\xFF\x85\xC0\x74\x28\xF3",
                    xorstr_("xxxxxxxxxxxxxxxxxxxxxxxxx????xxxxx"));
                if (o_add_event_handler != nullptr)
                {
                    MH_RemoveHook(o_add_event_handler);
                    MH_CreateHook(o_add_event_handler, &h_add_event_handler, reinterpret_cast<LPVOID*>(&o_add_event_handler));
                    MH_EnableHook(MH_ALL_HOOKS);
                }
                else
                    console->error(xorstr_("add_event_handler address not found.\n"));

                // ── stop_resource ─────────────────────────────────────────────
                // EC 10 alloc; 53 56 57 8B F9 8B 4D 0C
                o_stop_resource = (stop_resource_t)utilities::c_pattern::find_pattern(
                    "client.dll",
                    "\x55\x8B\xEC\x83\xEC\x10\x53\x56\x57\x8B\xF9\x8B\x4D\x0C",
                    "xxxxxxxxxxxxxx");
                if (o_stop_resource != nullptr)
                    console->success(xorstr_("'stop_resource' address founded.\n"));

                // ── get_virtual_machine ───────────────────────────────────────
                // EC 30 alloc; 53 57 8B 7D 08 8B (mov ebx,ecx after push)
                o_get_virtual_machine = (get_virtual_machine_t)utilities::c_pattern::find_pattern(
                    "client.dll",
                    "\x55\x8B\xEC\x83\xEC\x30\x53\x57\x8B\x7D\x08\x8B",
                    "xxxxxxxxxxxx");
                if (o_get_virtual_machine != nullptr)
                {
                    MH_RemoveHook(o_get_virtual_machine);
                    MH_CreateHook(o_get_virtual_machine, &h_get_virtual_machine, reinterpret_cast<LPVOID*>(&o_get_virtual_machine));
                    MH_EnableHook(MH_ALL_HOOKS);
                }
                else
                    console->error(xorstr_("get_virtual_machine address not found.\n"));

                // ── resource_manager ──────────────────────────────────────────
                // 6A FF cookie; EC 10 alloc; 53 56 57 A1 40 5A 4A 10 structure;
                // 8B 5D 08 8D 81 2C suffix uniquely identifies this overload
                o_resource_manager = (resource_manager_t)utilities::c_pattern::find_pattern(
                    xorstr_("client.dll"),
                    "\x55\x8B\xEC\x6A\xFF\x68\x1D\xDA\x38\x10\x64\xA1\x00\x00\x00\x00\x50\x83\xEC\x10"
                    "\x53\x56\x57\xA1\x40\x5A\x4A\x10\x33\xC5\x50\x8D\x45\xF4\x64\xA3\x00\x00\x00\x00"
                    "\x8B\x5D\x08\x8D\x81\x2C",
                    xorstr_("xxxxxx?xxxxx?xxxxxxxxxxx?xxxxxxxxxxx?xxxxxxxxx"));
                if (o_resource_manager != nullptr)
                {
                    MH_RemoveHook(o_resource_manager);
                    MH_CreateHook(o_resource_manager, &h_resource_manager, reinterpret_cast<LPVOID*>(&o_resource_manager));
                    MH_EnableHook(MH_ALL_HOOKS);
                }
                else
                    console->error(xorstr_("resource_manager address not found.\n"));

                // ── resource_constructor ──────────────────────────────────────
                // 6A FF cookie; 4B 86 38 10 handler; EC 24 alloc
                o_resource_constructor = (resource_constructor_t)utilities::c_pattern::find_pattern(
                    xorstr_("client.dll"),
                    "\x55\x8B\xEC\x6A\xFF\x68\x4B\x86\x38\x10\x64\xA1\x00\x00\x00\x00\x50\x83\xEC\x24",
                    xorstr_("xxxxxx?xxxxx?xxxxxxx"));
                if (o_resource_constructor != nullptr)
                {
                    MH_RemoveHook(o_resource_constructor);
                    MH_CreateHook(o_resource_constructor, &h_resource_constructor, reinterpret_cast<LPVOID*>(&o_resource_constructor));
                    MH_EnableHook(MH_ALL_HOOKS);
                }
                else
                    console->error(xorstr_("resource_constructor address not found.\n"));

                // ── trigger_server_event ──────────────────────────────────────
                // 51 53 56 57 8B 7D 08 85 FF — unchanged, still unique
                o_trigger_server_event = (trigger_server_event_t)utilities::c_pattern::find_pattern(
                    "client.dll",
                    "\x55\x8B\xEC\x51\x53\x56\x57\x8B\x7D\x08\x85\xFF",
                    "xxxxxxxxxxxx");
                if (o_trigger_server_event != nullptr)
                {
                    MH_RemoveHook(o_trigger_server_event);
                    MH_CreateHook(o_trigger_server_event, &h_trigger_server_event, reinterpret_cast<LPVOID*>(&o_trigger_server_event));
                    MH_EnableHook(MH_ALL_HOOKS);
                    console->success(xorstr_("'trigger_server_event' address founded.\n"));
                }

                // ── add_debug_hook ────────────────────────────────────────────
                // 6A FF cookie; E6 52 3A 10 handler; EC 98 alloc
                o_add_debug_hook = (add_debug_hook_t)utilities::c_pattern::find_pattern(
                    "client.dll",
                    "\x55\x8B\xEC\x6A\xFF\x68\xE6\x52\x3A\x10\x64\xA1\x00\x00\x00\x00\x50\x81\xEC\x98",
                    "xxxxxx?xxxxx?xxxxxxx");
                if (o_add_debug_hook != nullptr)
                {
                    MH_RemoveHook(o_add_debug_hook);
                    MH_CreateHook(o_add_debug_hook, &h_add_debug_hook, reinterpret_cast<LPVOID*>(&o_add_debug_hook));
                    MH_EnableHook(MH_ALL_HOOKS);
                    console->success(xorstr_("'add_debug_hook' address founded.\n"));
                }

                element->content.loaded_client = true;
            }
        }
        else
        {
            if (element->content.loaded_client)
            {
                element->resource.resources_list.clear();
                element->executor.resources_list.clear();
                element->event.events_list.clear();
                element->dump.dumps_list.clear();

                c_resource_manager = nullptr;
                c_lua_manager      = nullptr;

                element->content.loaded_client = false;
            }
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}
