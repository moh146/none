#include "console.h"
#include <data/elements.h>

void c_console::initialize()
{
	if (element->content.enabled_console)
		return;

	AllocConsole();

	freopen_s(reinterpret_cast<_iobuf**>(__acrt_iob_func(0)), "conin$", "r", static_cast<_iobuf*>(__acrt_iob_func(0)));
	freopen_s(reinterpret_cast<_iobuf**>(__acrt_iob_func(1)), "conout$", "w", static_cast<_iobuf*>(__acrt_iob_func(1)));
	freopen_s(reinterpret_cast<_iobuf**>(__acrt_iob_func(2)), "conout$", "w", static_cast<_iobuf*>(__acrt_iob_func(2)));
}

void c_console::destroy()
{
	if (!element->content.enabled_console)
		return;

	fclose(static_cast<_iobuf*>(__acrt_iob_func(0)));
	fclose(static_cast<_iobuf*>(__acrt_iob_func(1)));
	fclose(static_cast<_iobuf*>(__acrt_iob_func(2)));

	FreeConsole();
}

void c_console::color(WORD color)
{
	HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
	if (!hStdout)
		return;

	SetConsoleTextAttribute(hStdout, color);
}

void c_console::print(const char* message, ...)
{
	if (!element->content.enabled_console && !element->content.debug_mode)
		return;

	char buffer[1024];
	va_list args;
	va_start(args, message);
	vsnprintf(buffer, sizeof(buffer), message, args);
	va_end(args);

	printf(xorstr_("[INFO]: %s"), buffer);
	color(7);
}

void c_console::error(const char* message, ...)
{
	if (!element->content.enabled_console && !element->content.debug_mode)
		return;

	color(4);

	char buffer[1024];
	va_list args;
	va_start(args, message);
	vsnprintf(buffer, sizeof(buffer), message, args);
	va_end(args);

	printf(xorstr_("[ERROR]: %s"), buffer);
	color(7);
}

void c_console::warning(const char* message, ...)
{
	if (!element->content.enabled_console && !element->content.debug_mode)
		return;

	color(6);

	char buffer[1024];
	va_list args;
	va_start(args, message);
	vsnprintf(buffer, sizeof(buffer), message, args);
	va_end(args);

	printf(xorstr_("[DEBUG]: %s"), buffer);
	color(7);
}

void c_console::success(const char* message, ...)
{
	if (!element->content.enabled_console && !element->content.debug_mode)
		return;

	color(10);

	char buffer[1024];
	va_list args;
	va_start(args, message);
	vsnprintf(buffer, sizeof(buffer), message, args);
	va_end(args);

	printf(xorstr_("[INPUT]: %s"), buffer);
	color(7);
}
