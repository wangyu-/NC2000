#pragma once

#include "comm.h"

extern string udp_msg;
extern std::mutex g_mutex;

void handle_cmd(string str);
void clear_cmds();

void push_message(string msg);
string get_message();
char* peek_message();



// compile below with 6502 macroassembler and simulator,
// "save code" as intel-hex format, then use convert.sh 
/*=======
nc2600
=======*/
//nc2600 put:
/*
INT:.MACRO INT_PARAM
    .DB $00
    .DW INT_PARAM
    .ENDM
 .ORG $3000
   INT $051C
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
   CLV
   BVC WRITE
PREEND:
     INT $0516
     INT $C001
;END: INT $0527
;     JMP END   
*/

//nc2600 get:
/*
INT:.MACRO INT_PARAM
    .DB $00
    .DW INT_PARAM
    .ENDM
 .ORG $3000
OPEN:   
   LDA #$80 ; open mode
   STA $0912
   LDA #$EF ; not really needed??
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
   CLV
   BVC READ
PREEND:
     LDA #$0
     STA $3FFF  ;indicate dummy close
     INT $0516  ;close file
END:
     INT $C001
;END: INT $0527  ;open file manager
;     JMP END  
*/


/*=======
nc2000
=======*/
//nc2000 put
/*
INT:.MACRO INT_PARAM
    .DB $00
    .DW INT_PARAM
    .ENDM
 .ORG $3000
   INT $051D
CREATE:   
   LDA #$70
   STA $08fa
   LDA #$EF
   STA $08fb
   STA $08fc 
   INT $0515
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
   STA $08f7
   LDA #$0
   STA $08f8  
   STA $08f9  ;not really needed maybe
   INT $0518
   CLV
   BVC WRITE
PREEND:
     INT $0517
     INT $C001
;END: INT $0528
;     JMP END 
*/

//nc2000 get:
/*
INT:.MACRO INT_PARAM
    .DB $00
    .DW INT_PARAM
    .ENDM
 .ORG $3000
OPEN:   
   LDA #$80 ; open mode
   STA $08fa
   LDA #$EF ; file mode for create not really needed
   STA $08fb 
   STA $08fc 
   INT $0515
READ:
   LDA #$00
   STA $3f6 ;prevent auto shutdown
   LDA #$00
   STA $DD
   LDA #$32
   STA $DE
   LDA #$1   ; read 1 byte
   STA $08f7
   LDA #$0   ; read 1 byte (high value 0)
   STA $08f8  
   STA $09f9
   INT $0516  ; read
   LDA $08f7   ; actual read byte here
   BEQ PREEND  ;
   LDA #$1
   STA $3FFF
   LDA $3200
   STA $3FFF
   CLV
   BVC READ
PREEND:
   LDA #$0
   STA $3FFF  ;indicate dummy close
   INT $0516  ;close file
END:
   INT $C001
;END: INT $0528  ;open file manager
;     JMP END  
*/

/*=======
nc2000tw
=======*/
//nc2000tw put
/*
INT:.MACRO INT_PARAM
    .DB $00
    .DW INT_PARAM
    .ENDM
 .ORG $3000
   INT $051D
CREATE:   
   LDA #$70
   STA $08e8
   LDA #$EF
   STA $08e9
   STA $08ea 
   INT $0515
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
   STA $08e5
   LDA #$0
   STA $08e6  
   STA $08e7  ;not really needed maybe
   INT $0518
   CLV
   BVC WRITE
PREEND:
     INT $0517
     INT $C001
;END: INT $0528
;     JMP END 
*/



/*=======
nc3000
========*/
// nc3000 put
/*
INT:.MACRO INT_PARAM
    .DB $00
    .DW INT_PARAM
    .ENDM
 .ORG $3000
CREATE:   
   LDA #$70
   STA $08c9
   LDA #$EF
   STA $08ca
   STA $08cb
   INT $0515
WRITE:
   LDA #$00
   STA $3f6
   LDA $3FFF
   CMP #$00
   BEQ PREEND
   LDA $3FFF
   STA $3200
   LDA #$00
   STA $e0
   LDA #$32
   STA $e1
   LDA #$1
   STA $08c6
   LDA #$0
   STA $08c7  
   STA $08c8
   INT $0518
   JMP WRITE
PREEND:
     INT $0517
     INT $051C
END: INT $0528
     JMP END  
*/

// nc3000 get
/*
INT:.MACRO INT_PARAM
    .DB $00
    .DW INT_PARAM
    .ENDM
 .ORG $3000
OPEN:   
   LDA #$80 ; open mode
   STA $08c9
   LDA #$EF ; not really needed??
   STA $08ca 
   STA $08cb 
   INT $0515
READ:
   LDA #$00
   STA $3f6 ;prevent auto shutdown
   LDA #$00
   STA $e0
   LDA #$32
   STA $e1
   LDA #$1   ; read 1 byte
   STA $08c6
   LDA #$0   ; read 1 byte (high value 0)
   STA $08c7  
   STA $08c8
   INT $0516  ; read
   LDA $08c6   ; actual read byte here
   BEQ PREEND  ;
   LDA #$1
   STA $3FFF
   LDA $3200
   STA $3FFF
   CLV
   BVC READ
PREEND:
    LDA #$0
    STA $3FFF  ;indicate dummy close
    INT $0517  ;close file
END: 
    INT $0528  ;open file manager
    CLV
    BVC END
*/


/*=======
nc1020
=======*/
/*
nc1020 put:

INT:.MACRO INT_PARAM
    .DB $00
    .DW INT_PARAM
    .ENDM
 .ORG $3000
   ;INT $9327    ;defrag, but this overwrite some ram
CREATE:   
   INT $9301     ;create file
WRITE:
   LDA #$00
   STA $46E  ;prevent auto shutdown
   STA $4AE  ;prevent auto shutdown
   LDA $3FFF
   CMP #$00
   BEQ PREEND
   LDA $3FFF
   STA $3200
   LDA #$00
   STA $120D
   LDA #$32
   STA $120E
   LDA #$1
   STA $120F
   LDA #$0
   STA $1210  
   INT $9305       ;do write
   CLV
   BVC WRITE
PREEND:
     INT $9307     ;close
END: INT $C001     ;restart 
*/

/*
nc1020 get:

INT:.MACRO INT_PARAM
    .DB $00
    .DW INT_PARAM
    .ENDM
 .ORG $3000
 
OPEN:
   LDA #$00
   STA $1214
   INT $9302
   BCS PREEND
READ:
   LDA #$00
   STA $46E  ;prevent auto shutdown
   STA $4AE  ;prevent auto shutdown
   LDA #$00 
   STA $120D
   LDA #$32  ;3200--->addr to read
   STA $120E
   LDA #$01
   STA $120F
   LDA #$00  ;0001--->read 1byte each time
   STA $1210
   INT $9304
   LDA $120F ;load acutal read byte
   BEQ PREEND
   LDA #$1
   STA $3FFF
   LDA $3200
   STA $3FFF
   CLV
   BVC READ
PREEND:
   LDA #$0
   STA $3FFF
   INT $9307
END: 
   INT $C001 ;restart

*/


/*=======
nc1020tw
=======*/
/*
nc1020tw put:

INT:.MACRO INT_PARAM
    .DB $00
    .DW INT_PARAM
    .ENDM
 .ORG $3000
   ;INT $1027    ;defrag, but this overwrite some ram
CREATE:   
   INT $1001     ;create file
WRITE:
   LDA #$00
   STA $46E  ;prevent auto shutdown
   STA $4AE  ;prevent auto shutdown
   LDA $3FFF
   CMP #$00
   BEQ PREEND
   LDA $3FFF
   STA $3200
   LDA #$00
   STA $120D
   LDA #$32
   STA $120E
   LDA #$1
   STA $120F
   LDA #$0
   STA $1210  
   INT $1005       ;do write
   CLV
   BVC WRITE
PREEND:
     INT $1007     ;close
END: INT $C001     ;restart
*/

/*
nc1020tw get:

INT:.MACRO INT_PARAM
    .DB $00
    .DW INT_PARAM
    .ENDM
 .ORG $3000
OPEN:
   LDA #$00
   STA $1214
   INT $1002
   BCS PREEND
READ:
   LDA #$00
   STA $46E  ;prevent auto shutdown
   STA $4AE  ;prevent auto shutdown
   LDA #$00 
   STA $120D
   LDA #$32  ;3200--->addr to read
   STA $120E
   LDA #$01
   STA $120F
   LDA #$00  ;0001--->read 1byte each time
   STA $1210
   INT $1004
   LDA $120F ;load acutal read byte
   BEQ PREEND
   LDA #$1
   STA $3FFF
   LDA $3200
   STA $3FFF
   CLV
   BVC READ
PREEND:
   LDA #$0
   STA $3FFF
   INT $1007
END: 
   INT $C001 ;restart

*/
