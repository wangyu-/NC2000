#pragma once

#include "comm.h"

extern string udp_msg;
extern std::mutex g_mutex;

void handle_cmd(string & str);
void push_message(char* msg);

string get_message();
