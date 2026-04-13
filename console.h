#pragma once
#include <string>

extern std::string console_input;
extern bool console_on;

void draw_console();
void input_text(std::string a);
void handle_console(signed int sym, bool key_down);
