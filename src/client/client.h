#pragma once
#include <includes.h>
#include <netsdk/utilities/SString.h>

namespace EDebugHook
{
	enum EDebugHookType
	{
		PRE_EVENT,
		POST_EVENT,
		PRE_FUNCTION,
		POST_FUNCTION,
		PRE_EVENT_FUNCTION,
		POST_EVENT_FUNCTION,
		MAX_DEBUG_HOOK_TYPE
	};
}
using EDebugHook::EDebugHookType;

namespace EEventPriority
{
	enum EEventPriorityType
	{
		LOW,
		NORMAL,
		HIGH,
	};
}
using EEventPriority::EEventPriorityType;

class c_client {
private:
	typedef void* (__thiscall* get_resource_name_t)(void* ecx, SString* rslt, void* state);
	typedef void* (__thiscall* resource_manager_t)(void* ecx, unsigned short usNetID, const char* szResourceName, void* pResourceEntity, void* pResourceDynamicEntity, const std::string& strMinServerReq, const std::string& strMinClientReq, bool bEnableOOP);
	typedef void* (__thiscall* resource_constructor_t)(void* ecx, unsigned short usNetID, const char* szResourceName, void* pResourceEntity, void* pResourceDynamicEntity, const std::string& strMinServerReq, const std::string& strMinClientReq, bool bEnableOOP);
	typedef void(__thiscall* stop_resource_t)(void* resource_manager, void* resource);
	typedef void* (__thiscall* get_virtual_machine_t)(void* ecx, void* lua_vm);
	typedef bool(__thiscall* load_script_t)(void* lua_vm, const char* cpInBuffer, unsigned int uiInSize, const char* szFileName);
	typedef bool(__cdecl* trigger_server_event_t)(const char* szName, void* CallWithEntity, void* Arguments);
	typedef bool(__thiscall* add_debug_hook_t)(void* ecx, EDebugHook::EDebugHookType hookType, const void* functionRef, const std::vector<SString>* allowedNameList);
	typedef int(__cdecl* lua_load_buffer_t)(void* lua_vm, const char* buff, unsigned int sz, const char* name);
	typedef bool( __cdecl* add_event_handler_t)(void* lua_vm, const char* szName, void* Entity, const void* iLuaFunction, bool bPropagated, EEventPriority::EEventPriorityType eventPriority, float fPriorityMod);
public:
	void* c_resource_manager = nullptr;
	void* c_lua_manager = nullptr;

	get_resource_name_t o_get_resource_name;
	resource_manager_t o_resource_manager;
	resource_constructor_t o_resource_constructor;
	stop_resource_t o_stop_resource;
	get_virtual_machine_t o_get_virtual_machine;
	load_script_t o_load_script;
	trigger_server_event_t o_trigger_server_event;
	add_debug_hook_t o_add_debug_hook;
	lua_load_buffer_t o_lua_load_buffer;
	add_event_handler_t o_add_event_handler;

	void release();
	void stop_resource(void* p_resource);
	bool load_code(const char* resourceName, const char* buffer, unsigned int size);
};

inline c_client* client = new c_client();