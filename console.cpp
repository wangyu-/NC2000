#include "comm.h"
#include "font.h"
#include "SDL.h"
#include "cmd.h"
#include <SDL_keycode.h>
#include "display.h"

string promot=">";
string console_input="";

bool console_on=false;
int cursor=0;

const int FONT_START = 0;
const int FONT_WIDTH = 8; //need to <=8
const int FONT_HEIGHT = 16;

const int DRAW_HEIGHT_START = 1;
const int DRAW_HEIGHT = FONT_HEIGHT-2;

void draw_console(){
	string to_draw = promot+console_input+" ";
	memset(lcd_buf,0, sizeof(lcd_buf));
	for(int i=0;i<to_draw.length();i++){
		int row_size=SCREEN_WIDTH/8;
		int line= i/row_size;
		if(line>= SCREEN_HEIGHT/(DRAW_HEIGHT)) break; //no more space to draw
		for(int row=DRAW_HEIGHT_START;row-DRAW_HEIGHT_START<DRAW_HEIGHT;row++){
			char c=to_draw[i]-FONT_START;
			lcd_buf[row_size*(row+line* (DRAW_HEIGHT)) +i]= font8x16[c*FONT_HEIGHT+row];
			if(row-DRAW_HEIGHT_START >= DRAW_HEIGHT -2){
				if(i==cursor+promot.length()){
					lcd_buf[row_size*(row+line* (DRAW_HEIGHT)) +i]^=0xff; //set the last bit to 1
				}
			}
		}
	}

}

void input_text(string a){
	if(console_on && !(a[0]=='`')){
		console_input.insert(cursor,a);
		cursor+= a.length();
	}
}

deque<string> history;
int history_index=0;
string save_of_current;
extern SDL_Window* window;

void on_enter_consoel(){
	SDL_StartTextInput();
	//SDL_SetWindowTitle(window, "Console");
}

void on_exit_console(){
	SDL_StopTextInput();
	//SDL_SetWindowTitle(window, get_title().c_str());
}

void handle_console(signed int sym, bool key_down){
	if(!key_down) return;
	if(!shift_down && sym==SDLK_BACKQUOTE){
		console_on^= 0x1;
		if(console_on) on_enter_consoel();
		else on_exit_console();
		//printf("console %s\n", console_on ? "on" : "off");
	}
	if(!console_on) return;
	if(sym!=SDLK_UP &&sym!=SDLK_DOWN && sym!=SDLK_LEFT && sym!=SDLK_RIGHT){
		history_index=0;
	}
	if(sym== SDLK_BACKSPACE){
		if(!console_input.empty()) {
			if(cursor>0){
				console_input.erase(cursor-1,1);
				cursor--;
			}
		}
	}
	if(sym==SDLK_LEFT){
		cursor--;
		if(shift_down) cursor-=9;

	}
	if(sym==SDLK_RIGHT){
		cursor++;
		if(shift_down) cursor+=9;
	}
	if(sym==SDLK_UP){
		if(history_index==0){
			save_of_current=console_input;
		}
		if(history_index<history.size()){
			history_index++;
			if(history_index>0){
				bool cursor_at_end= (cursor==console_input.length());
				console_input=history[history.size()-history_index];
				if(cursor_at_end) cursor=console_input.length();
			}
		}

	}
	if(sym==SDLK_DOWN){
		if(history_index>0){
			history_index--;
			bool cursor_at_end= (cursor==console_input.length());
			if(history_index>0){
				console_input=history[history.size()-history_index];
			}else{
				console_input=save_of_current;
			}
			if(cursor_at_end) cursor=console_input.length();
		}

	}
	if(sym==SDLK_ESCAPE){
		console_input.clear();
		cursor=0;
		console_on=false;
		on_exit_console();	
	}
	if(sym==SDLK_RETURN){
		if(!console_input.empty()){
			push_message(console_input);
			history.push_back(console_input);
			if(history.size()>100) history.pop_front();
			console_input.clear();
			cursor=0;
		}
		console_on=false;
		on_exit_console();
	}
	if(cursor<0) cursor=0;
	if(cursor>console_input.length()) cursor=console_input.length();
}
