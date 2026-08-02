#pragma once
#include <includes.h>
#include <netsdk/CNet.h>
struct s_executor {
    std::string resource_name;
    void* lua_vm;

    bool operator==(const s_executor& other) const {
        return lua_vm == other.lua_vm;
    }
};

struct s_resource {
    std::string resource_name;
    void* resource_ptr;

    bool operator==(const s_resource& other) const {
        return resource_ptr == other.resource_ptr;
    }
};

struct s_events {
    std::string event_name;
    bool is_blocked = false;
};

struct s_dumps {
    std::string script_name;
    std::vector<char> buffer;
    size_t size;
    bool is_compiled;
};

class c_elements {
public:
    struct {
        bool debug_mode{ true };
        bool loaded_client{ false };
        bool enabled_console{ false };
        bool injected_code{ false };
    } content;

    struct {
        int menu_bind{ 45 };
    } binds;

    struct {
        std::vector<s_executor> resources_list;
        int item_current = 0;
    } executor;

    struct {
        std::vector<s_resource> resources_list;
        int item_current = 0;
        char search_buffer[128] = "";
    } resource;

    struct {
        std::vector<s_events> events_list;
        int item_current = 0;
        char search_buffer[1028] = "";
    } event;

    struct {
        bool dump_enabled{ false };
        bool deobfuscate{ true };
        std::vector<s_dumps> dumps_list;
        int item_current = 0;
    } dump;

    struct {
        bool break_debughook{ false };
        bool break_onClientPaste{ true };
        bool break_onClientResourceStop{ true };
        bool bypass_lua_anticheats{ true };
        char dll_buffer[1028] = "";
    } info;
};

inline c_elements* element = new c_elements();
