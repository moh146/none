#pragma once
#include <includes.h>

static const char* keys[] = { "None", "Mouse 1", "Mouse 2", "CN", "Mouse 3", "Mouse 4", "Mouse 5", "-", "Back", "Tab", "-", "-", "CLR", "Enter", "-", "-", "Shift", "Ctrl", "Alt", "Pause", "Caps", "KAN", "-", "JUN", "FIN", "KAN", "-", "Escape", "CON", "NCO", "ACC", "MAD", "Space", "PGU", "PGD", "End", "Home", "Left", "Up", "Right", "Down", "SEL", "PRI", "EXE", "PRI", "INS", "Delete", "HEL", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "-", "-", "-", "-", "-", "-", "-", "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z", "WIN", "WIN", "APP", "-", "SLE", "Num 0", "Num 1", "Num 2", "Num 3", "Num 4", "Num 5", "Num 6", "Num 7", "Num 8", "Num 9", "MUL", "ADD", "SEP", "MIN", "Delete", "DIV", "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12", "F13", "F14", "F15", "F16", "F17", "F18", "F19", "F20", "F21", "F22", "F23", "F24", "-", "-", "-", "-", "-", "-", "-", "NUM", "SCR", "EQU", "MAS", "TOY", "OYA", "OYA", "-", "-", "-", "-", "-", "-", "-", "-", "-", "Shift", "Shift", "Ctrl", "Ctrl", "Alt", "Alt" };

class c_keybind {
public:
    bool add(std::string_view label, int* keystate);
};

inline c_keybind* keybind = new c_keybind();