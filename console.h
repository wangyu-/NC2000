#pragma once

#include "comm.h"

extern string udp_msg;
extern std::mutex g_mutex;

void handle_cmd(string & str);
void push_message(char* msg);

string get_message();


// compile below with 6502 macroassembler and simulator,
// "save code" as intel-hex format, then use convert.sh 

//for put:
/*
INT:.MACRO INT_PARAM
    .DB $00
    .DW INT_PARAM
    .ENDM
 .ORG $3000
CREATE:   
   LDA #$70
   STA $0912
   LDA #$EF
   STA $0913
   STA $0914 
   INT $0514
WRITE:
   LDA #$00
   STA $3f6
   LDA $3FFF
   CMP #$00
   BEQ PREEND
   LDA $3FFF
   STA $3200
   LDA #$00
   STA $DD
   LDA #$32
   STA $DE
   LDA #$1
   STA $090F
   LDA #$0
   STA $0910  
   STA $0911
   INT $0517
   JMP WRITE
PREEND:
     INT $0516
END: INT $0527
     JMP END   
*/

//for get:
/*
INT:.MACRO INT_PARAM
    .DB $00
    .DW INT_PARAM
    .ENDM
 .ORG $3000
CREATE:   
   LDA #$80 ; open mode
   STA $0912
   LDA #$EF ; file mode for create not really needed
   STA $0913 
   STA $0914 
   INT $0514
READ:
   LDA #$00
   STA $3f6 ;prevent auto shutdown
   LDA #$00
   STA $DD
   LDA #$32
   STA $DE
   LDA #$1   ; read 1 byte
   STA $090F
   LDA #$0   ; read 1 byte (high value 0)
   STA $0910  
   STA $0911
   INT $0515  ; read
   LDA $090F   ; actual read byte here
   BEQ PREEND  ;
   LDA #$1
   STA $3FFF
   LDA $3200
   STA $3FFF
   JMP READ
PREEND:
     LDA #$0
     STA $3FFF  ;indicate dummy close
     INT $0516  ;close file
END: INT $0527  ;open file manager
     JMP END  
*/
