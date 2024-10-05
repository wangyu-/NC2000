#ifdef HANDYPSP

extern "C" {
#include "w65c02.h"
#include "w65c02macro.h"
}


// Read/Write Cycle definitions
#define CPU_RDWR_CYC    1
//#define DMA_RDWR_CYC    4
//#define SPR_RDWR_CYC    3

void xILLEGAL(void);

DWORD CpuExecute(void)
{
    DWORD cycle = 0;
//
// NMI is currently unused by the lynx so lets save some time
//
    // Check NMI & IRQ status, prioritise NMI then IRQ
//     if(g_nmi)
//     {
//         // Mark the NMI as services
//         g_nmi=FALSE;
//         // FIXME: cycle
//         //mProcessingInterrupt++;
//
//         // Push processor status
//         CPU_POKE(0x0100+mSP--,mPC>>8);
//         CPU_POKE(0x0100+mSP--,mPC&0x00ff);
//         CPU_POKE(0x0100+mSP--,PS());
//
//         // Pick up the new PC
//         mPC=CPU_PEEKW(NMI_VECTOR);
//     }
//
//     if (g_irq && !mI) {
//         TRACE_CPU1("Update() IRQ taken at PC=%04x", mPC);
//         // IRQ signal clearance is handled by CMikie::Update() as this
//         // is the only source of interrupts
//
//         // Push processor status
//         PUSH(mPC >> 8);
//         PUSH(mPC & 0xff);
//         PUSH(PS() & 0xef);      // Clear B flag on stack
//
//         mI = TRUE;              // Stop further interrupts
//         mD = FALSE;             // Clear decimal mode
//
//         // Pick up the new PC
//         mPC = CPU_PEEKW(IRQ_VECTOR);
//
//         // Save the sleep state as an irq has possibly woken the processor
//         g_wai_saved = g_wai;
//         g_wai = FALSE;
//
//         // Log the irq entry time
//         // FIXME: cc800 wakeup IRQ
//         //gIRQEntryCycle = gSystemCycleCount;
//
//         // Clear the interrupt status line
//         g_irq = FALSE;
//     }

    //
    // If the CPU is asleep then skip to the next timer event
    //
    if (g_wai) return cycle;

    // Fetch opcode
    mOpcode = CPU_PEEK(mPC);
    TRACE_CPU2("Update() PC=$%04x, Opcode=%02x", mPC, mOpcode);
    mPC++;

    // Execute Opcode

    switch (mOpcode) {

//
// 0x00
//
    case 0x00:
        cycle = (1 + (6));
        // IMPLIED
        xBRK();
        break;
    case 0x01:
        cycle = (1 + (5));
        xINDIRECT_X();
        xORA();
        break;
    case 0x02:
        cycle = (1 + (1));
        // *** ILLEGAL ***
        xILLEGAL();
        break;
    case 0x03:
        cycle = (1 + (1));
        // *** ILLEGAL ***
        xILLEGAL();
        break;
    case 0x04:
        cycle = (1 + (4));
        xZEROPAGE();
        xTSB();
        break;
    case 0x05:
        cycle = (1 + (2));
        xZEROPAGE();
        xORA();
        break;
    case 0x06:
        cycle = (1 + (4));
        xZEROPAGE();
        xASL();
        break;
    case 0x07:
        cycle = (1 + (1));
        // *** ILLEGAL ***
        xILLEGAL();
        break;

    case 0x08:
        cycle = (1 + (2));
        // IMPLIED
        xPHP();
        break;
    case 0x09:
        cycle = 2;
        xIMMEDIATE();
        xORA();
        break;
    case 0x0A:
        cycle = (1 + (1));
        // IMPLIED
        xASLA();
        break;
    case 0x0B:
        cycle = (1 + (1));
        // *** ILLEGAL ***
        xILLEGAL();
        break;
    case 0x0C:
        cycle = (1 + (5));
        xABSOLUTE();
        xTSB();
        break;
    case 0x0D:
        cycle = (1 + (3));
        xABSOLUTE();
        xORA();
        break;
    case 0x0E:
        cycle = (1 + (5));
        xABSOLUTE();
        xASL();
        break;
    case 0x0F:
        cycle = (1 + (4));
        // *** ILLEGAL ***
        xILLEGAL();
        break;

//
// 0x10
//
    case 0x10:
        cycle = (1 + (1));
        // RELATIVE (IN FUNCTION)
        xBPL();
        break;
    case 0x11:
        cycle = 5;
        xINDIRECT_Y2();
        xORA();
        break;
    case 0x12:
        cycle = (1 + (4));
        xINDIRECT();
        xORA();
        break;
    case 0x13:
        cycle = (1 + (1));
        // *** ILLEGAL ***
        xILLEGAL();
        break;
    case 0x14:
        cycle = (1 + (4));
        xZEROPAGE();
        xTRB();
        break;
    case 0x15:
        cycle = (1 + (3));
        xZEROPAGE_X();
        xORA();
        break;
    case 0x16:
        cycle = (1 + (5));
        xZEROPAGE_X();
        xASL();
        break;
    case 0x17:
        cycle = (1 + (4));
        // *** ILLEGAL ***
        xILLEGAL();
        break;

    case 0x18:
        cycle = (1 + (1));
        // IMPLIED
        xCLC();
        break;
    case 0x19:
        cycle = (1 + (3));
        xABSOLUTE_Y();
        xORA();
        break;
    case 0x1A:
        cycle = (1 + (1));
        // IMPLIED
        xINCA();
        break;
    case 0x1B:
        cycle = (1 + (1));
        // *** ILLEGAL ***
        xILLEGAL();
        break;
    case 0x1C:
        cycle = (1 + (5));
        xABSOLUTE();
        xTRB();
        break;
    case 0x1D:
        cycle = 4;
        xABSOLUTE_X();
        xORA();
        break;
    case 0x1E:
        cycle = (1 + (6));
        xABSOLUTE_X();
        xASL();
        break;
    case 0x1F:
        cycle = (1 + (4));
        // *** ILLEGAL ***
        xILLEGAL();
        break;

//
// 0x20
//
    case 0x20:
        cycle = (1 + (5));
        xABSOLUTE();
        xJSR();
        break;
    case 0x21:
        cycle = (1 + (5));
        xINDIRECT_X();
        xAND();
        break;
    case 0x22:
        cycle = (1 + (1));
        // *** ILLEGAL ***
        xILLEGAL();
        break;
    case 0x23:
        cycle = (1 + (1));
        // *** ILLEGAL ***
        xILLEGAL();
        break;
    case 0x24:
        cycle = (1 + (2));
        xZEROPAGE();
        xBIT();
        break;
    case 0x25:
        cycle = (1 + (2));
        xZEROPAGE();
        xAND();
        break;
    case 0x26:
        cycle = (1 + (4));
        xZEROPAGE();
        xROL();
        break;
    case 0x27:
        cycle = (1 + (4));
        // *** ILLEGAL ***
        xILLEGAL();
        break;

    case 0x28:
        cycle = (1 + (3));
        // IMPLIED
        xPLP();
        break;
    case 0x29:
        cycle = (1 + (1));
        xIMMEDIATE();
        xAND();
        break;
    case 0x2A:
        cycle = (1 + (1));
        // IMPLIED
        xROLA();
        break;
    case 0x2B:
        cycle = (1 + (1));
        // *** ILLEGAL ***
        xILLEGAL();
        break;
    case 0x2C:
        cycle = (1 + (3));
        xABSOLUTE();
        xBIT();
        break;
    case 0x2D:
        cycle = (1 + (3));
        xABSOLUTE();
        xAND();
        break;
    case 0x2E:
        cycle = (1 + (5));
        xABSOLUTE();
        xROL();
        break;
    case 0x2F:
        cycle = (1 + (4));
        // *** ILLEGAL ***
        xILLEGAL();
        break;

//
// 0x30
//
    case 0x30:
        cycle = (1 + (1));
        // RELATIVE (IN FUNCTION)
        xBMI();
        break;
    case 0x31:
        cycle = 5;
        xINDIRECT_Y2();
        xAND();
        break;
    case 0x32:
        cycle = (1 + (4));
        xINDIRECT();
        xAND();
        break;
    case 0x33:
        cycle = (1 + (1));
        // *** ILLEGAL ***
        xILLEGAL();
        break;
    case 0x34:
        cycle = (1 + (3));
        xZEROPAGE_X();
        xBIT();
        break;
    case 0x35:
        cycle = (1 + (3));
        xZEROPAGE_X();
        xAND();
        break;
    case 0x36:
        cycle = (1 + (5));
        xZEROPAGE_X();
        xROL();
        break;
    case 0x37:
        cycle = (1 + (4));
        // *** ILLEGAL ***
        xILLEGAL();
        break;

    case 0x38:
        cycle = (1 + (1));
        // IMPLIED
        xSEC();
        break;
    case 0x39:
        cycle = (1 + (3));
        xABSOLUTE_Y();
        xAND();
        break;
    case 0x3A:
        cycle = (1 + (1));
        // IMPLIED
        xDECA();
        break;
    case 0x3B:
        cycle = (1 + (1));
        // *** ILLEGAL ***
        xILLEGAL();
        break;
    case 0x3C:
        cycle = (1 + (3));
        xABSOLUTE_X();
        xBIT();
        break;
    case 0x3D:
        cycle = (1 + (3));
        xABSOLUTE_X();
        xAND();
        break;
    case 0x3E:
        cycle = (1 + (6));
        xABSOLUTE_X();
        xROL();
        break;
    case 0x3F:
        cycle = (1 + (4));
        // *** ILLEGAL ***
        xILLEGAL();
        break;

//
// 0x40
//
    case 0x40:
        cycle = (1 + (5));
        // Only clear IRQ if this is not a BRK instruction based RTI

        // B flag is on the stack cant test the flag
        int tmp;
        PULL(tmp);
        PUSH(tmp);
        if (!(tmp & 0x10)) {
            g_wai = g_wai_saved;

            // If were in sleep mode then we need to push the
            // wakeup counter along by the same number of cycles
            // we have used during the sleep period
            if (g_wai) {
                // FIXME: CC800 wakeup
                //gCPUWakeupTime += gSystemCycleCount - gIRQEntryCycle;
            }
        }
        // IMPLIED
        xRTI();
        break;
    case 0x41:
        cycle = (1 + (5));
        xINDIRECT_X();
        xEOR();
        break;
    case 0x42:
        cycle = (1 + (1));
        // *** ILLEGAL ***
        xILLEGAL();
        break;
    case 0x43:
        cycle = (1 + (1));
        // *** ILLEGAL ***
        xILLEGAL();
        break;
    case 0x44:
        cycle = (1 + (2));
        // *** ILLEGAL ***
        xILLEGAL();
        break;
    case 0x45:
        cycle = (1 + (2));
        xZEROPAGE();
        xEOR();
        break;
    case 0x46:
        cycle = (1 + (4));
        xZEROPAGE();
        xLSR();
        break;
    case 0x47:
        cycle = (1 + (4));
        // *** ILLEGAL ***
        xILLEGAL();
        break;

    case 0x48:
        cycle = (1 + (2));
        // IMPLIED
        xPHA();
        break;
    case 0x49:
        cycle = (1 + (1));
        xIMMEDIATE();
        xEOR();
        break;
    case 0x4A:
        cycle = (1 + (1));
        // IMPLIED
        xLSRA();
        break;
    case 0x4B:
        cycle = (1 + (1));
        // *** ILLEGAL ***
        xILLEGAL();
        break;
    case 0x4C:
        cycle = (1 + (2));
        xABSOLUTE();
        xJMP();
        break;
    case 0x4D:
        cycle = (1 + (3));
        xABSOLUTE();
        xEOR();
        break;
    case 0x4E:
        cycle = (1 + (5));
        xABSOLUTE();
        xLSR();
        break;
    case 0x4F:
        cycle = (1 + (4));
        // *** ILLEGAL ***
        xILLEGAL();
        break;

//
// 0x50
//
    case 0x50:
        cycle = (1 + (1));
        // RELATIVE (IN FUNCTION)
        xBVC();
        break;
    case 0x51:
        cycle = 5;
        xINDIRECT_Y2();
        xEOR();
        break;
    case 0x52:
        cycle = (1 + (4));
        xINDIRECT();
        xEOR();
        break;
    case 0x53:
        cycle = (1 + (1));
        // *** ILLEGAL ***
        xILLEGAL();
        break;
    case 0x54:
        cycle = (1 + (3));
        // *** ILLEGAL ***
        xILLEGAL();
        break;
    case 0x55:
        cycle = (1 + (3));
        xZEROPAGE_X();
        xEOR();
        break;
    case 0x56:
        cycle = (1 + (5));
        xZEROPAGE_X();
        xLSR();
        break;
    case 0x57:
        cycle = (1 + (4));
        // *** ILLEGAL ***
        xILLEGAL();
        break;

    case 0x58:
        cycle = (1 + (1));
        // IMPLIED
        xCLI();
        break;
    case 0x59:
        cycle = (1 + (3));
        xABSOLUTE_Y();
        xEOR();
        break;
    case 0x5A:
        cycle = (1 + (2));
        // IMPLIED
        xPHY();
        break;
    case 0x5B:
        cycle = (1 + (1));
        // *** ILLEGAL ***
        xILLEGAL();
        break;
    case 0x5C:
        cycle = (1 + (7));
        // *** ILLEGAL ***
        xILLEGAL();
        break;
    case 0x5D:
        cycle = (1 + (3));
        xABSOLUTE_X();
        xEOR();
        break;
    case 0x5E:
        cycle = (1 + (6));
        xABSOLUTE_X();
        xLSR();
        break;
    case 0x5F:
        cycle = (1 + (4));
        // *** ILLEGAL ***
        xILLEGAL();
        break;

//
// 0x60
//
    case 0x60:
        cycle = (1 + (5));
        // IMPLIED
        xRTS();
        break;
    case 0x61:
        cycle = (1 + (5));
        xINDIRECT_X();
        xADC();
        break;
    case 0x62:
        cycle = (1 + (1));
        // *** ILLEGAL ***
        xILLEGAL();
        break;
    case 0x63:
        cycle = (1 + (1));
        // *** ILLEGAL ***
        xILLEGAL();
        break;
    case 0x64:
        cycle = (1 + (2));
        xZEROPAGE();
        xSTZ();
        break;
    case 0x65:
        cycle = (1 + (2));
        xZEROPAGE();
        xADC();
        break;
    case 0x66:
        cycle = (1 + (4));
        xZEROPAGE();
        xROR();
        break;
    case 0x67:
        cycle = (1 + (4));
        // *** ILLEGAL ***
        xILLEGAL();
        break;

    case 0x68:
        cycle = (1 + (3));
        // IMPLIED
        xPLA();
        break;
    case 0x69:
        cycle = (1 + (1));
        xIMMEDIATE();
        xADC();
        break;
    case 0x6A:
        cycle = (1 + (1));
        // IMPLIED
        xRORA();
        break;
    case 0x6B:
        cycle = (1 + (1));
        // *** ILLEGAL ***
        xILLEGAL();
        break;
    case 0x6C:
        cycle = 5;
        xINDIRECT_ABSOLUTE();
        xJMP();
        break;
    case 0x6D:
        cycle = (1 + (3));
        xABSOLUTE();
        xADC();
        break;
    case 0x6E:
        cycle = (1 + (5));
        xABSOLUTE();
        xROR();
        break;
    case 0x6F:
        cycle = (1 + (4));
        // *** ILLEGAL ***
        xILLEGAL();
        break;

//
// 0x70
//
    case 0x70:
        cycle = (1 + (1));
        // RELATIVE (IN FUNCTION)
        xBVS();
        break;
    case 0x71:
        cycle = 5;
        xINDIRECT_Y2();
        xADC();
        break;
    case 0x72:
        cycle = (1 + (4));
        xINDIRECT();
        xADC();
        break;
    case 0x73:
        cycle = (1 + (1));
        // *** ILLEGAL ***
        xILLEGAL();
        break;
    case 0x74:
        cycle = (1 + (3));
        xZEROPAGE_X();
        xSTZ();
        break;
    case 0x75:
        cycle = (1 + (3));
        xZEROPAGE_X();
        xADC();
        break;
    case 0x76:
        cycle = (1 + (5));
        xZEROPAGE_X();
        xROR();
        break;
    case 0x77:
        cycle = (1 + (4));
        // *** ILLEGAL ***
        xILLEGAL();
        break;

    case 0x78:
        cycle = (1 + (1));
        // IMPLIED
        xSEI();
        break;
    case 0x79:
        cycle = (1 + (3));
        xABSOLUTE_Y();
        xADC();
        break;
    case 0x7A:
        cycle = (1 + (3));
        // IMPLIED
        xPLY();
        break;
    case 0x7B:
        cycle = (1 + (1));
        // *** ILLEGAL ***
        xILLEGAL();
        break;
    case 0x7C:
        cycle = (1 + (5));
        xINDIRECT_ABSOLUTE_X();
        xJMP();
        break;
    case 0x7D:
        cycle = (1 + (3));
        xABSOLUTE_X();
        xADC();
        break;
    case 0x7E:
        cycle = (1 + (6));
        xABSOLUTE_X();
        xROR();
        break;
    case 0x7F:
        cycle = (1 + (4));
        // *** ILLEGAL ***
        xILLEGAL();
        break;

//
// 0x80
//
    case 0x80:
        cycle = (1 + (2));
        // RELATIVE (IN FUNCTION)
        xBRA();
        break;
    case 0x81:
        cycle = (1 + (5));
        xINDIRECT_X();
        xSTA();
        break;
    case 0x82:
        cycle = (1 + (1));
        // *** ILLEGAL ***
        xILLEGAL();
        break;
    case 0x83:
        cycle = (1 + (1));
        // *** ILLEGAL ***
        xILLEGAL();
        break;
    case 0x84:
        cycle = (1 + (2));
        xZEROPAGE();
        xSTY();
        break;
    case 0x85:
        cycle = (1 + (2));
        xZEROPAGE();
        xSTA();
        break;
    case 0x86:
        cycle = (1 + (2));
        xZEROPAGE();
        xSTX();
        break;
    case 0x87:
        cycle = (1 + (4));
        // *** ILLEGAL ***
        xILLEGAL();
        break;

    case 0x88:
        cycle = (1 + (1));
        // IMPLIED
        xDEY();
        break;
    case 0x89:
        cycle = (1 + (1));
        xIMMEDIATE();
        xBIT();
        break;
    case 0x8A:
        cycle = (1 + (1));
        // IMPLIED
        xTXA();
        break;
    case 0x8B:
        cycle = (1 + (1));
        // *** ILLEGAL ***
        xILLEGAL();
        break;
    case 0x8C:
        cycle = (1 + (3));
        xABSOLUTE();
        xSTY();
        break;
    case 0x8D:
        cycle = (1 + (3));
        xABSOLUTE();
        xSTA();
        break;
    case 0x8E:
        cycle = (1 + (3));
        xABSOLUTE();
        xSTX();
        break;
    case 0x8F:
        cycle = (1 + (4));
        // *** ILLEGAL ***
        xILLEGAL();
        break;

//
// 0x90
//
    case 0x90:
        cycle = (1 + (1));
        // RELATIVE (IN FUNCTION)
        xBCC();
        break;
    case 0x91:
        cycle = 6;
        xINDIRECT_Y();
        xSTA();
        break;
    case 0x92:
        cycle = (1 + (4));
        xINDIRECT();
        xSTA();
        break;
    case 0x93:
        cycle = (1 + (1));
        // *** ILLEGAL ***
        xILLEGAL();
        break;
    case 0x94:
        cycle = (1 + (3));
        xZEROPAGE_X();
        xSTY();
        break;
    case 0x95:
        cycle = (1 + (3));
        xZEROPAGE_X();
        xSTA();
        break;
    case 0x96:
        cycle = (1 + (3));
        xZEROPAGE_Y();
        xSTX();
        break;
    case 0x97:
        cycle = (1 + (4));
        // *** ILLEGAL ***
        xILLEGAL();
        break;

    case 0x98:
        cycle = (1 + (1));
        // IMPLIED
        xTYA();
        break;
    case 0x99:
        cycle = (1 + (4));
        xABSOLUTE_Y();
        xSTA();
        break;
    case 0x9A:
        cycle = (1 + (1));
        // IMPLIED
        xTXS();
        break;
    case 0x9B:
        cycle = (1 + (1));
        // *** ILLEGAL ***
        xILLEGAL();
        break;
    case 0x9C:
        cycle = (1 + (3));
        xABSOLUTE();
        xSTZ();
        break;
    case 0x9D:
        cycle = (1 + (4));
        xABSOLUTE_X();
        xSTA();
        break;
    case 0x9E:
        cycle = (1 + (4));
        xABSOLUTE_X();
        xSTZ();
        break;
    case 0x9F:
        cycle = (1 + (4));
        // *** ILLEGAL ***
        xILLEGAL();
        break;

//
// 0xA0
//
    case 0xA0:
        cycle = (1 + (1));
        xIMMEDIATE();
        xLDY();
        break;
    case 0xA1:
        cycle = (1 + (5));
        xINDIRECT_X();
        xLDA();
        break;
    case 0xA2:
        cycle = (1 + (1));
        xIMMEDIATE();
        xLDX();
        break;
    case 0xA3:
        cycle = (1 + (1));
        // *** ILLEGAL ***
        xILLEGAL();
        break;
    case 0xA4:
        cycle = (1 + (2));
        xZEROPAGE();
        xLDY();
        break;
    case 0xA5:
        cycle = (1 + (2));
        xZEROPAGE();
        xLDA();
        break;
    case 0xA6:
        cycle = (1 + (2));
        xZEROPAGE();
        xLDX();
        break;
    case 0xA7:
        cycle = (1 + (4));
        // *** ILLEGAL ***
        xILLEGAL();
        break;

    case 0xA8:
        cycle = (1 + (1));
        // IMPLIED
        xTAY();
        break;
    case 0xA9:
        cycle = (1 + (1));
        xIMMEDIATE();
        xLDA();
        break;
    case 0xAA:
        cycle = (1 + (1));
        // IMPLIED
        xTAX();
        break;
    case 0xAB:
        cycle = (1 + (1));
        // *** ILLEGAL ***
        xILLEGAL();
        break;
    case 0xAC:
        cycle = (1 + (3));
        xABSOLUTE();
        xLDY();
        break;
    case 0xAD:
        cycle = (1 + (3));
        xABSOLUTE();
        xLDA();
        break;
    case 0xAE:
        cycle = (1 + (3));
        xABSOLUTE();
        xLDX();
        break;
    case 0xAF:
        cycle = (1 + (4));
        // *** ILLEGAL ***
        xILLEGAL();
        break;

//
// 0xB0
//
    case 0xB0:
        cycle = (1 + (1));
        // RELATIVE (IN FUNCTION)
        xBCS();
        break;
    case 0xB1:
        cycle = 5;
        xINDIRECT_Y2();
        xLDA();
        break;
    case 0xB2:
        cycle = (1 + (4));
        xINDIRECT();
        xLDA();
        break;
    case 0xB3:
        cycle = (1 + (1));
        // *** ILLEGAL ***
        xILLEGAL();
        break;
    case 0xB4:
        cycle = (1 + (3));
        xZEROPAGE_X();
        xLDY();
        break;
    case 0xB5:
        cycle = (1 + (3));
        xZEROPAGE_X();
        xLDA();
        break;
    case 0xB6:
        cycle = (1 + (3));
        xZEROPAGE_Y();
        xLDX();
        break;
    case 0xB7:
        cycle = (1 + (4));
        // *** ILLEGAL ***
        xILLEGAL();
        break;

    case 0xB8:
        cycle = (1 + (1));
        // IMPLIED
        xCLV();
        break;
    case 0xB9:
        cycle = (1 + (3));
        xABSOLUTE_Y();
        xLDA();
        break;
    case 0xBA:
        cycle = (1 + (1));
        // IMPLIED
        xTSX();
        break;
    case 0xBB:
        cycle = (1 + (1));
        // *** ILLEGAL ***
        xILLEGAL();
        break;
    case 0xBC:
        cycle = 4;
        xABSOLUTE_X();
        xLDY();
        break;
    case 0xBD:
        cycle = 4;
        xABSOLUTE_X();
        xLDA();
        break;
    case 0xBE:
        cycle = 4;
        xABSOLUTE_Y();
        xLDX();
        break;
    case 0xBF:
        cycle = (1 + (3));
        // *** ILLEGAL ***
        xILLEGAL();
        break;

//
// 0xC0
//
    case 0xC0:
        cycle = (1 + (1));
        xIMMEDIATE();
        xCPY();
        break;
    case 0xC1:
        cycle = (1 + (5));
        xINDIRECT_X();
        xCMP();
        break;
    case 0xC2:
        cycle = (1 + (1));
        // *** ILLEGAL ***
        xILLEGAL();
        break;
    case 0xC3:
        cycle = (1 + (1));
        // *** ILLEGAL ***
        xILLEGAL();
        break;
    case 0xC4:
        cycle = (1 + (2));
        xZEROPAGE();
        xCPY();
        break;
    case 0xC5:
        cycle = (1 + (2));
        xZEROPAGE();
        xCMP();
        break;
    case 0xC6:
        cycle = (1 + (4));
        xZEROPAGE();
        xDEC();
        break;
    case 0xC7:
        cycle = (1 + (4));
        // *** ILLEGAL ***
        xILLEGAL();
        break;

    case 0xC8:
        cycle = (1 + (1));
        // IMPLIED
        xINY();
        break;
    case 0xC9:
        cycle = (1 + (1));
        xIMMEDIATE();
        xCMP();
        break;
    case 0xCA:
        cycle = (1 + (1));
        // IMPLIED
        xDEX();
        break;
    case 0xCB:
        cycle = (1 + (1));
        // IMPLIED
        xWAI();
        break;
    case 0xCC:
        cycle = (1 + (3));
        xABSOLUTE();
        xCPY();
        break;
    case 0xCD:
        cycle = (1 + (3));
        xABSOLUTE();
        xCMP();
        break;
    case 0xCE:
        cycle = (1 + (5));
        xABSOLUTE();
        xDEC();
        break;
    case 0xCF:
        cycle = (1 + (4));
        // *** ILLEGAL ***
        xILLEGAL();
        break;

//
// 0xD0
//
    case 0xD0:
        cycle = 2;
        // RELATIVE (IN FUNCTION)
        xBNE();
        break;
    case 0xD1:
        cycle = 5;
        xINDIRECT_Y2();
        xCMP();
        break;
    case 0xD2:
        cycle = (1 + (4));
        xINDIRECT();
        xCMP();
        break;
    case 0xD3:
        cycle = (1 + (1));
        // *** ILLEGAL ***
        xILLEGAL();
        break;
    case 0xD4:
        cycle = (1 + (3));
        // *** ILLEGAL ***
        xILLEGAL();
        break;
    case 0xD5:
        cycle = (1 + (3));
        xZEROPAGE_X();
        xCMP();
        break;
    case 0xD6:
        cycle = (1 + (5));
        xZEROPAGE_X();
        xDEC();
        break;
    case 0xD7:
        cycle = (1 + (4));
        // *** ILLEGAL ***
        xILLEGAL();
        break;

    case 0xD8:
        cycle = (1 + (1));
        // IMPLIED
        xCLD();
        break;
    case 0xD9:
        cycle = (1 + (3));
        xABSOLUTE_Y();
        xCMP();
        break;
    case 0xDA:
        cycle = (1 + (2));
        // IMPLIED
        xPHX();
        break;
    case 0xDB:
        cycle = (1 + (1));
        // IMPLIED
        xSTP();
        break;
    case 0xDC:
        cycle = (1 + (3));
        // *** ILLEGAL ***
        xILLEGAL();
        break;
    case 0xDD:
        cycle = (1 + (3));
        xABSOLUTE_X();
        xCMP();
        break;
    case 0xDE:
        cycle = (1 + (6));
        xABSOLUTE_X();
        xDEC();
        break;
    case 0xDF:
        cycle = (1 + (4));
        // *** ILLEGAL ***
        xILLEGAL();
        break;

//
// 0xE0
//
    case 0xE0:
        cycle = (1 + (1));
        xIMMEDIATE();
        xCPX();
        break;
    case 0xE1:
        cycle = (1 + (5));
        xINDIRECT_X();
        xSBC();
        break;
    case 0xE2:
        cycle = (1 + (1));
        // *** ILLEGAL ***
        xILLEGAL();
        break;
    case 0xE3:
        cycle = (1 + (1));
        // *** ILLEGAL ***
        xILLEGAL();
        break;
    case 0xE4:
        cycle = (1 + (2));
        xZEROPAGE();
        xCPX();
        break;
    case 0xE5:
        cycle = (1 + (2));
        xZEROPAGE();
        xSBC();
        break;
    case 0xE6:
        cycle = (1 + (4));
        xZEROPAGE();
        xINC();
        break;
    case 0xE7:
        cycle = (1 + (4));
        // *** ILLEGAL ***
        xILLEGAL();
        break;

    case 0xE8:
        cycle = (1 + (1));
        // IMPLIED
        xINX();
        break;
    case 0xE9:
        cycle = (1 + (1));
        xIMMEDIATE();
        xSBC();
        break;
    case 0xEA:
        cycle = (1 + (1));
        // IMPLIED
        xNOP();
        break;
    case 0xEB:
        cycle = (1 + (1));
        // *** ILLEGAL ***
        xILLEGAL();
        break;
    case 0xEC:
        cycle = (1 + (3));
        xABSOLUTE();
        xCPX();
        break;
    case 0xED:
        cycle = (1 + (3));
        xABSOLUTE();
        xSBC();
        break;
    case 0xEE:
        cycle = (1 + (5));
        xABSOLUTE();
        xINC();
        break;
    case 0xEF:
        cycle = (1 + (4));
        // *** ILLEGAL ***
        xILLEGAL();
        break;

//
// 0xF0
//
    case 0xF0:
        cycle = 2;
        // RELATIVE (IN FUNCTION)
        xBEQ();
        break;
    case 0xF1:
        cycle = 5;
        xINDIRECT_Y2();
        xSBC();
        break;
    case 0xF2:
        cycle = (1 + (4));
        xINDIRECT();
        xSBC();
        break;
    case 0xF3:
        cycle = (1 + (1));
        // *** ILLEGAL ***
        xILLEGAL();
        break;
    case 0xF4:
        cycle = (1 + (3));
        // *** ILLEGAL ***
        xILLEGAL();
        break;
    case 0xF5:
        cycle = (1 + (3));
        xZEROPAGE_X();
        xSBC();
        break;
    case 0xF6:
        cycle = (1 + (5));
        xZEROPAGE_X();
        xINC();
        break;
    case 0xF7:
        cycle = (1 + (4));
        // *** ILLEGAL ***
        xILLEGAL();
        break;

    case 0xF8:
        cycle = (1 + (1));
        // IMPLIED
        xSED();
        break;
    case 0xF9:
        cycle = (1 + (3));
        xABSOLUTE_Y();
        xSBC();
        break;
    case 0xFA:
        cycle = (1 + (3));
        // IMPLIED
        xPLX();
        break;
    case 0xFB:
        cycle = (1 + (1));
        // *** ILLEGAL ***
        xILLEGAL();
        break;
    case 0xFC:
        cycle = (1 + (3));
        // *** ILLEGAL ***
        xILLEGAL();
        break;
    case 0xFD:
        cycle = (1 + (3));
        xABSOLUTE_X();
        xSBC();
        break;
    case 0xFE:
        cycle = (1 + (6));
        xABSOLUTE_X();
        xINC();
        break;
    case 0xFF:
        cycle = (1 + (4));
        // *** ILLEGAL ***
        xILLEGAL();
        break;
    }


    // FIXME: GET Latest GGV Simulator
    // NC3000 have same behavior
    if (g_nmi) {
        // Mark the NMI as services
        g_nmi = FALSE;
        cycle += 6; // NMI?
        //mProcessingInterrupt++;

        // Push processor status
        CPU_POKE(0x0100 + mSP, mPC >> 8); // don't use ++ in poke or will inc/dec 4 times
        mSP--;
        CPU_POKE(0x0100 + mSP, mPC & 0x00ff);
        mSP--;
#ifdef MERGEGGVSIM
        mI = TRUE; // FIXME: MERGE
#endif
        CPU_POKE(0x0100 + mSP, PS());
        mSP--;

        // Pick up the new PC
        mPC = CPU_PEEKW(NMI_VECTOR);
    }

    if (g_irq && !mI) {
        TRACE_CPU1("Update() IRQ taken at PC=%04x", mPC);
        // IRQ signal clearance is handled by CMikie::Update() as this
        // is the only source of interrupts

        // Push processor status
        PUSH(mPC >> 8);
        PUSH(mPC & 0xff);
#ifdef MERGEGGVSIM
        mB = FALSE; // MERGE
        PUSH(PS());
#else
        PUSH(PS() & 0xef);      // Clear B flag on stack
#endif

        mI = TRUE;              // Stop further interrupts
        mD = FALSE;             // Clear decimal mode

        // Pick up the new PC
        mPC = CPU_PEEKW(IRQ_VECTOR);

        // Save the sleep state as an irq has possibly woken the processor
        g_wai_saved = g_wai;
        g_wai = FALSE;

        // Log the irq entry time
        // FIXME: cc800 wakeup IRQ
        //gIRQEntryCycle = gSystemCycleCount;

        // Clear the interrupt status line
        g_irq = FALSE;
        cycle += 6;
    }

#ifdef _LYNXDBG

    // Trigger breakpoint if required

    for (int loop = 0; loop < MAX_CPU_BREAKPOINTS; loop++) {
        if (mPcBreakpoints[loop] == mPC) {
            gBreakpointHit = TRUE;
            mSystem.DebugTrace(0);
        }
    }

    // Check code level debug features
    // back to back CPX ($Absolute)
    // on the 2nd Occurance we do some debug
    if (mOpcode == 0xec) {
        if (mDbgFlag) {
            // We shoud do some debug now
            if (!mOperand) {
                // Trigger a breakpoint
                gBreakpointHit = TRUE;
                // Generate a debug trail output
                mSystem.DebugTrace(0);
            } else {
                // Generate a debug trail output
                mSystem.DebugTrace(mOperand);
            }
            mDbgFlag = 0;
        } else {
            if (mOperand == 0x5aa5) mDbgFlag = 1;
            else mDbgFlag = 0;
        }
    } else {
        mDbgFlag = 0;
    }
#endif
    return cycle;
}

#endif