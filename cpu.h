#pragma once

#include "comm.h"


extern double speed_multiplier;

void reset_cpu_states();
void cpu_run();

void init_udp_server();

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
