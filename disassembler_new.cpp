// originally NekoDriverDebug.cpp

#include <comm.h>
#include <cstdio>
#include <mem.h>
#include <cpu.h>

#define  INVALID1        1
#define  INVALID2        2
#define  INVALID3        3
#define  ADDR_IMM        4
#define  ADDR_ABS        5
#define  ADDR_ZPG        6
#define  ADDR_ABSX       7
#define  ADDR_ABSY       8
#define  ADDR_ZPGX       9
#define  ADDR_ZPGY       10
#define  ADDR_REL        11
#define  ADDR_BBREL      12
#define  ADDR_INDX       13
#define  ADDR_ABSIINDX   14
#define  ADDR_INDY       15
#define  ADDR_IZPG       16
#define  ADDR_IABS       17


typedef struct _addrrec {
    char format[12];
    int  bytes;
} addrrec, *addrptr;


typedef struct _instrec {
    char mnemonic[5];
    int  addrmode;
} instrec, *instptr;



addrrec addressmode[19]  = { {""         ,1},        // (implied)
{""         ,1},        // INVALID1
{""         ,2},        // INVALID2
{""         ,3},        // INVALID3
{"#$%02X"   ,2},        // ADDR_IMM
{"%s"       ,3},        // ADDR_ABS
{"%s"       ,2},        // ADDR_ZPG
{"%s,X"     ,3},        // ADDR_ABSX
{"%s,Y"     ,3},        // ADDR_ABSY
{"%s,X"     ,2},        // ADDR_ZPGX
{"%s,Y"     ,2},        // ADDR_ZPGY
{"%s"       ,2},        // ADDR_REL
{"%s %s"    ,3},        // ADDR_BBREL
{"(%s,X)"   ,2},        // ADDR_INDX
{"(%s,X)"   ,3},        // ADDR_ABSIINDX
{"(%s),Y"   ,2},        // ADDR_INDY
{"(%s)"     ,2},        // ADDR_IZPG
{"(%s)"     ,3},        // ADDR_IABS
{"IO PAGE"  ,1} };      // ADDR_IO PAGE

instrec instruction[257] = { {"BRK",ADDR_ABS},              // 00h
{"ORA",ADDR_INDX},      // 01h
{"NOP",INVALID2},       // 02h
{"NOP",INVALID1},       // 03h
{"TSB",ADDR_ZPG},       // 04h
{"ORA",ADDR_ZPG},       // 05h
{"ASL",ADDR_ZPG},       // 06h
{"RMB0",ADDR_ZPG},      // 07h
{"PHP",0},              // 08h
{"ORA",ADDR_IMM},       // 09h
{"ASL",0},              // 0Ah
{"NOP",INVALID1},       // 0Bh
{"TSB",ADDR_ABS},       // 0Ch
{"ORA",ADDR_ABS},       // 0Dh
{"ASL",ADDR_ABS},       // 0Eh
{"BBR0",ADDR_BBREL},    // 0Fh
{"BPL",ADDR_REL},       // 10h
{"ORA",ADDR_INDY},      // 11h
{"ORA",ADDR_IZPG},      // 12h
{"NOP",INVALID1},       // 13h
{"TRB",ADDR_ZPG},       // 14h
{"ORA",ADDR_ZPGX},      // 15h
{"ASL",ADDR_ZPGX},      // 16h
{"RMB1",ADDR_ZPG},      // 17h
{"CLC",0},              // 18h
{"ORA",ADDR_ABSY},      // 19h
{"INA",0},              // 1Ah
{"NOP",INVALID1},       // 1Bh
{"TRB",ADDR_ABS},       // 1Ch
{"ORA",ADDR_ABSX},      // 1Dh
{"ASL",ADDR_ABSX},      // 1Eh
{"BBR1",ADDR_BBREL},    // 1Fh
{"JSR",ADDR_ABS},       // 20h
{"AND",ADDR_INDX},      // 21h
{"NOP",INVALID2},       // 22h
{"NOP",INVALID1},       // 23h
{"BIT",ADDR_ZPG},       // 24h
{"AND",ADDR_ZPG},       // 25h
{"ROL",ADDR_ZPG},       // 26h
{"RMB2",ADDR_ZPG},      // 27h
{"PLP",0},              // 28h
{"AND",ADDR_IMM},       // 29h
{"ROL",0},              // 2Ah
{"NOP",INVALID1},       // 2Bh
{"BIT",ADDR_ABS},       // 2Ch
{"AND",ADDR_ABS},       // 2Dh
{"ROL",ADDR_ABS},       // 2Eh
{"BBR2",ADDR_BBREL},    // 2Fh
{"BMI",ADDR_REL},       // 30h
{"AND",ADDR_INDY},      // 31h
{"AND",ADDR_IZPG},      // 32h
{"NOP",INVALID1},       // 33h
{"BIT",ADDR_ZPGX},      // 34h
{"AND",ADDR_ZPGX},      // 35h
{"ROL",ADDR_ZPGX},      // 36h
{"RMB3",ADDR_ZPG},      // 37h
{"SEC",0},              // 38h
{"AND",ADDR_ABSY},      // 39h
{"DEA",0},              // 3Ah
{"NOP",INVALID1},       // 3Bh
{"BIT",ADDR_ABSX},      // 3Ch
{"AND",ADDR_ABSX},      // 3Dh
{"ROL",ADDR_ABSX},      // 3Eh
{"BBR3",ADDR_BBREL},    // 3Fh
{"RTI",0},              // 40h
{"EOR",ADDR_INDX},      // 41h
{"NOP",INVALID2},       // 42h
{"NOP",INVALID1},       // 43h
{"NOP",INVALID2},       // 44h
{"EOR",ADDR_ZPG},       // 45h
{"LSR",ADDR_ZPG},       // 46h
{"RMB4",ADDR_ZPG},      // 47h
{"PHA",0},              // 48h
{"EOR",ADDR_IMM},       // 49h
{"LSR",0},              // 4Ah
{"NOP",INVALID1},       // 4Bh
{"JMP",ADDR_ABS},       // 4Ch
{"EOR",ADDR_ABS},       // 4Dh
{"LSR",ADDR_ABS},       // 4Eh
{"BBR4",ADDR_BBREL},    // 4Fh
{"BVC",ADDR_REL},       // 50h
{"EOR",ADDR_INDY},      // 51h
{"EOR",ADDR_IZPG},      // 52h
{"NOP",INVALID1},       // 53h
{"NOP",INVALID2},       // 54h
{"EOR",ADDR_ZPGX},      // 55h
{"LSR",ADDR_ZPGX},      // 56h
{"RMB5",ADDR_ZPG},      // 57h
{"CLI",0},              // 58h
{"EOR",ADDR_ABSY},      // 59h
{"PHY",0},              // 5Ah
{"NOP",INVALID1},       // 5Bh
{"NOP",INVALID3},       // 5Ch
{"EOR",ADDR_ABSX},      // 5Dh
{"LSR",ADDR_ABSX},      // 5Eh
{"BBR5",ADDR_BBREL},    // 5Fh
{"RTS",0},              // 60h
{"ADC",ADDR_INDX},      // 61h
{"NOP",INVALID2},       // 62h
{"NOP",INVALID1},       // 63h
{"STZ",ADDR_ZPG},       // 64h
{"ADC",ADDR_ZPG},       // 65h
{"ROR",ADDR_ZPG},       // 66h
{"RMB6",ADDR_ZPG},      // 67h
{"PLA",0},              // 68h
{"ADC",ADDR_IMM},       // 69h
{"ROR",0},              // 6Ah
{"NOP",INVALID1},       // 6Bh
{"JMP",ADDR_IABS},      // 6Ch
{"ADC",ADDR_ABS},       // 6Dh
{"ROR",ADDR_ABS},       // 6Eh
{"BBR6",ADDR_BBREL},    // 6Fh
{"BVS",ADDR_REL},       // 70h
{"ADC",ADDR_INDY},      // 71h
{"ADC",ADDR_IZPG},      // 72h
{"NOP",INVALID1},       // 73h
{"STZ",ADDR_ZPGX},      // 74h
{"ADC",ADDR_ZPGX},      // 75h
{"ROR",ADDR_ZPGX},      // 76h
{"RMB7",ADDR_ZPG},      // 77h
{"SEI",0},              // 78h
{"ADC",ADDR_ABSY},      // 79h
{"PLY",0},              // 7Ah
{"NOP",INVALID1},       // 7Bh
{"JMP",ADDR_ABSIINDX},  // 7Ch
{"ADC",ADDR_ABSX},      // 7Dh
{"ROR",ADDR_ABSX},      // 7Eh
{"BBR7",ADDR_BBREL},    // 7Fh
{"BRA",ADDR_REL},       // 80h
{"STA",ADDR_INDX},      // 81h
{"NOP",INVALID2},       // 82h
{"NOP",INVALID1},       // 83h
{"STY",ADDR_ZPG},       // 84h
{"STA",ADDR_ZPG},       // 85h
{"STX",ADDR_ZPG},       // 86h
{"SMB0",ADDR_ZPG},      // 87h
{"DEY",0},              // 88h
{"BIT",ADDR_IMM},       // 89h
{"TXA",0},              // 8Ah
{"NOP",INVALID1},       // 8Bh
{"STY",ADDR_ABS},       // 8Ch
{"STA",ADDR_ABS},       // 8Dh
{"STX",ADDR_ABS},       // 8Eh
{"BBS0",ADDR_BBREL},    // 8Fh
{"BCC",ADDR_REL},       // 90h
{"STA",ADDR_INDY},      // 91h
{"STA",ADDR_IZPG},      // 92h
{"NOP",INVALID1},       // 93h
{"STY",ADDR_ZPGX},      // 94h
{"STA",ADDR_ZPGX},      // 95h
{"STX",ADDR_ZPGY},      // 96h
{"SMB1",ADDR_ZPG},      // 97h
{"TYA",0},              // 98h
{"STA",ADDR_ABSY},      // 99h
{"TXS",0},              // 9Ah
{"NOP",INVALID1},       // 9Bh
{"STZ",ADDR_ABS},       // 9Ch
{"STA",ADDR_ABSX},      // 9Dh
{"STZ",ADDR_ABSX},      // 9Eh
{"BBS1",ADDR_BBREL},    // 9Fh
{"LDY",ADDR_IMM},       // A0h
{"LDA",ADDR_INDX},      // A1h
{"LDX",ADDR_IMM},       // A2h
{"NOP",INVALID1},       // A3h
{"LDY",ADDR_ZPG},       // A4h
{"LDA",ADDR_ZPG},       // A5h
{"LDX",ADDR_ZPG},       // A6h
{"SMB2",ADDR_ZPG},      // A7h
{"TAY",0},              // A8h
{"LDA",ADDR_IMM},       // A9h
{"TAX",0},              // AAh
{"NOP",INVALID1},       // ABh
{"LDY",ADDR_ABS},       // ACh
{"LDA",ADDR_ABS},       // ADh
{"LDX",ADDR_ABS},       // AEh
{"BBS2",ADDR_BBREL},    // AFh
{"BCS",ADDR_REL},       // B0h
{"LDA",ADDR_INDY},      // B1h
{"LDA",ADDR_IZPG},      // B2h
{"NOP",INVALID1},       // B3h
{"LDY",ADDR_ZPGX},      // B4h
{"LDA",ADDR_ZPGX},      // B5h
{"LDX",ADDR_ZPGY},      // B6h
{"SMB3",ADDR_ZPG},      // B7h
{"CLV",0},              // B8h
{"LDA",ADDR_ABSY},      // B9h
{"TSX",0},              // BAh
{"NOP",INVALID1},       // BBh
{"LDY",ADDR_ABSX},      // BCh
{"LDA",ADDR_ABSX},      // BDh
{"LDX",ADDR_ABSY},      // BEh
{"BBS3",ADDR_BBREL},    // BFh
{"CPY",ADDR_IMM},       // C0h
{"CMP",ADDR_INDX},      // C1h
{"NOP",INVALID2},       // C2h
{"NOP",INVALID1},       // C3h
{"CPY",ADDR_ZPG},       // C4h
{"CMP",ADDR_ZPG},       // C5h
{"DEC",ADDR_ZPG},       // C6h
{"SMB4",ADDR_ZPG},      // C7h
{"INY",0},              // C8h
{"CMP",ADDR_IMM},       // C9h
{"DEX",0},              // CAh
{"WAI",INVALID1},       // CBh
{"CPY",ADDR_ABS},       // CCh
{"CMP",ADDR_ABS},       // CDh
{"DEC",ADDR_ABS},       // CEh
{"BBS4",ADDR_BBREL},    // CFh
{"BNE",ADDR_REL},       // D0h
{"CMP",ADDR_INDY},      // D1h
{"CMP",ADDR_IZPG},      // D2h
{"NOP",INVALID1},       // D3h
{"NOP",INVALID2},       // D4h
{"CMP",ADDR_ZPGX},      // D5h
{"DEC",ADDR_ZPGX},      // D6h
{"SMB5",ADDR_ZPG},      // D7h
{"CLD",0},              // D8h
{"CMP",ADDR_ABSY},      // D9h
{"PHX",0},              // DAh
{"STP",INVALID1},       // DBh
{"NOP",INVALID3},       // DCh
{"CMP",ADDR_ABSX},      // DDh
{"DEC",ADDR_ABSX},      // DEh
{"BBS5",ADDR_BBREL},    // DFh
{"CPX",ADDR_IMM},       // E0h
{"SBC",ADDR_INDX},      // E1h
{"NOP",INVALID2},       // E2h
{"NOP",INVALID1},       // E3h
{"CPX",ADDR_ZPG},       // E4h
{"SBC",ADDR_ZPG},       // E5h
{"INC",ADDR_ZPG},       // E6h
{"SMB6",ADDR_ZPG},      // E7h
{"INX",0},              // E8h
{"SBC",ADDR_IMM},       // E9h
{"NOP",0},              // EAh
{"NOP",INVALID1},       // EBh
{"CPX",ADDR_ABS},       // ECh
{"SBC",ADDR_ABS},       // EDh
{"INC",ADDR_ABS},       // EEh
{"BBS6",ADDR_BBREL},    // EFh
{"BEQ",ADDR_REL},       // F0h
{"SBC",ADDR_INDY},      // F1h
{"SBC",ADDR_IZPG},      // F2h
{"NOP",INVALID1},       // F3h
{"NOP",INVALID2},       // F4h
{"SBC",ADDR_ZPGX},      // F5h
{"INC",ADDR_ZPGX},      // F6h
{"SMB7",ADDR_ZPG},      // F7h
{"SED",0},              // F8h
{"SBC",ADDR_ABSY},      // F9h
{"PLX",0},              // FAh
{"NOP",INVALID1},       // FBh
{"NOP",INVALID3},       // FCh
{"SBC",ADDR_ABSX},      // FDh
{"INC",ADDR_ABSX},      // FEh
{"BBS7",ADDR_BBREL},    // FFh
{"   ",18} };           // 100h


const char* GetSymbol(uint16_t address, int bytes);
const char* byte_to_binary(unsigned char c, char* b);


unsigned logpos = 0;

char logbuff [102400 * 50]; // 5M buffer. no need initialize

#define WORD uint16_t
#define BYTE uint8_t

uint8_t GetByte(uint16_t addr){
    uint8_t & Peek16Debug(uint16_t addr);
    return Peek16Debug(addr);
}

uint16_t GetWord(uint16_t addr){
    return (GetByte(addr) | (GetByte(addr + 1) << 8));
}


string LogDisassembly(WORD offset)
{
    static long long debug_inst_cnt=0;

    char addresstext[40] = "";
    char bytestext[10]   = "";
    char output[100]    = "";
    WORD  inst            = GetByte(offset);

    /*if (offset < iorange) {
        // Register address can't be executed
        inst = 0x100;
    }*/

    int   addrmode        = instruction[inst].addrmode;
    WORD  bytes           = addressmode[addrmode].bytes;

    int ptr=-1;

    // Build a string containing the target address or symbol
    if (addressmode[addrmode].format[0]) {

        WORD address = GetWord((offset + 1) & 0xffff);

        if (bytes == 2)
            address &= 0xFF;

        if (addrmode == ADDR_IMM) {
            WORD address = GetWord((offset + 1) & 0xffff) & 0xFF;
            sprintf(addresstext,
                    addressmode[addrmode].format,
                    (unsigned)address);
        }
        if (addrmode == ADDR_REL) {
            address = (offset + 2 + (int)(signed char)address) & 0xffff;
            sprintf(addresstext,
                    addressmode[addrmode].format,
                    (LPSTR)GetSymbol(address, bytes));
            ptr=address;
        }
        if (addrmode == ADDR_BBREL) {
            address &= 0xFF;
            WORD zpaddr = address;
            address = GetWord((offset + 2) & 0xffff); //*(LPWORD)(mem+((offset+2) & 0xffff));
            address &= 0xFF;
            address = (offset + 3 + (int)(signed char)address) & 0xffff;

            char zptxt[14];
            sprintf(zptxt, "%s", (LPSTR)GetSymbol(zpaddr, 2));
            sprintf(addresstext,
                    addressmode[addrmode].format,
                    zptxt,
                    (LPSTR)GetSymbol(address, 3));
            ptr=address;
        }

        if (addresstext[0] == 0) {//else
            sprintf(addresstext,
                    addressmode[addrmode].format,
                    (LPSTR)GetSymbol(address, bytes));
            ptr=address;
        }
    }

    // Build a string containing the actual bytes that make up this
    // instruction
    /*{
        int loop = 0;
        while (loop < bytes)
            sprintf(bytestext + strlen(bytestext),
                    "%02X",
                    GetByte(offset + (loop++)));
        while (strlen(bytestext) < 6)
            strcat(bytestext, " ");
    }*/
    string res;

    sprintf(output,"idx=%03lld,%03lld,%03lld; ",debug_inst_cnt/(1000*1000),debug_inst_cnt/1000%1000,debug_inst_cnt%1000);
    debug_inst_cnt++;
    res+=output;

    sprintf(output,"bs=%02x roa_bbs=%02x vol=%02x zp=%02x reg=%02x,%02x,%02x,%03o sp=%02d; ",ram_io[0x00], ram_io[0x0a], ram_io[0x0d], ram_io[0x0f],cpu->A,cpu->X,cpu->Y,cpu->P(), 256-cpu->SP);
    res+=output;

    sprintf(output,
            "pc=%04x: %-4s %-8s; ",
            (unsigned)offset,                   // 4000
            instruction[inst].mnemonic,         // ORA_
            addresstext                         // ($49,X)
    );
    res+=output;

    res+="pc-> ";
    for(int i=0;i<4;i++){
        sprintf(output,"%02x ", GetByte((offset + i) & 0xffff));
        res+=output;
    }
    sprintf(output,"(%d); ",bytes);
    res+=output;

    if(ptr!=-1){
        sprintf(output, "%04x -> %02x %02x %02x %02x ; ", ptr, GetByte(ptr),GetByte((ptr+1)&0xffff), GetByte((ptr+2)&0xffff), GetByte((ptr+3)&0xffff));
    }else{
        sprintf(output, "                    ; ");
    }
    res+=output;

    return res;
}

const char* GetSymbol(WORD address, int bytes)
{
    // If there is no symbol for this address, then just return a string
    // containing the address number
    static char buffer[8];
    switch (bytes) {
    case 2:
        sprintf(buffer, "$%02x", (unsigned)address);
        break;
    case 3:
        sprintf(buffer, "$%04x", (unsigned)address);
        break;
    default:
        buffer[0] = 0;
        break;
    }
    return buffer;
}

const char* byte_to_binary(unsigned char c, char* b)
{
    //static char b[9]; // TODO: threadsafe

    int i = 0;
    for (unsigned char z = 128; z > 0; z >>= 1) {
        b[i] = ((c & z) == z ? '1' : '0');
        i++;
    }
    b[i] = '\0';

    return b;
}

string disassemble2(uint16_t pc){
    return LogDisassembly(pc);
}
