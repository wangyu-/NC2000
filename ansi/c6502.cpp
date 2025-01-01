#include "c6502.h"
#include <cassert>
#include <cstdio>
#include <cstring>

static int znTbl[] = {
    002, 000, 000, 000, 000, 000, 000, 000, 000, 000,
    000, 000, 000, 000, 000, 000, 000, 000, 000, 000,
    000, 000, 000, 000, 000, 000, 000, 000, 000, 000,
    000, 000, 000, 000, 000, 000, 000, 000, 000, 000,
    000, 000, 000, 000, 000, 000, 000, 000, 000, 000,
    000, 000, 000, 000, 000, 000, 000, 000, 000, 000,
    000, 000, 000, 000, 000, 000, 000, 000, 000, 000,
    000, 000, 000, 000, 000, 000, 000, 000, 000, 000,
    000, 000, 000, 000, 000, 000, 000, 000, 000, 000,
    000, 000, 000, 000, 000, 000, 000, 000, 000, 000,
    000, 000, 000, 000, 000, 000, 000, 000, 000, 000,
    000, 000, 000, 000, 000, 000, 000, 000, 000, 000,
    000, 000, 000, 000, 000, 000, 000, 000, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128
};

C6502::C6502(IBus6502* bus) {
	this->bus = bus;
}

void C6502::reset() {
	A = 0;
    X = 0;
    Y = 0;
    P = 4;
    SP = 0xff;
    PC = readAddress(0xfffc);
    clk = 0;
    irqPending = false;
    nmiPending = false;
    nmiRequest = false;

    total_cycles = 0;
}

long long C6502::getTotalCycles() {
	return total_cycles + clk;
}

void C6502::exec(int cycle) {
    if (clk >= cycle) {
        lineclk += cycle;
        total_cycles += cycle;
        clk -= cycle;
        return;
    }
    if (irqPending && (P & 4) == 0) {
        irqPending = false;
        doIRQ();
    }
    while (clk < cycle) {
        if (nmiPending) {
            nmiPending = false;
            nmiRequest = true;
        }
        doCode(getCode());
        if (nmiRequest) {
            nmiRequest = false;
            doNMI();
        }
    }
    lineclk += cycle;
    total_cycles += cycle;
    clk -= cycle;
}

int C6502::exec2(int max_cycles) {
    //this can be false since IRQ() can be called externally
    //assert(clk==0);

    if (irqPending && (P & 4) == 0) {
        irqPending = false;
        doIRQ();
    }
    if (nmiPending) {
        nmiPending = false;
        doNMI();
    }
    do{
        doCode(getCode());//allow one cycle anyway
    }while(clk<max_cycles);
    
    int res=clk;
    lineclk += clk;
    total_cycles += clk;
    clk=0;
    return res;
}


void C6502::NMI() {
    nmiPending = true;
}

void C6502::doNMI() {
    pushW(PC);
    push(P | 0x20);
    PC = readAddress(0xfffa);
	toSei();
    P |= 4;
    clk += 84;
}

void C6502::IRQ() {
    if ((P & 4) != 0)
        irqPending = true;
    else
        doIRQ();
}

void C6502::doIRQ() {
    pushW(PC);
    push(P | 0x20);
    PC = readAddress(0xfffe);
	toSei();
    P |= 4;
    clk += 84;
}

int C6502::getCode() {
    return bus->read(PC++);
}

int C6502::getCodeW() {
    int t = bus->read(PC) | (bus->read(PC + 1) << 8);
    PC += 2;
    return t;
}

int C6502::readAddress(int address) {
    return bus->read(address) | (bus->read(address + 1) << 8);
}

void C6502::XXX_xx() {
	printf("不支持的指令 %x at %x\n",bus->read(PC - 1),PC-1);
	///////////throw bus->read(PC - 1);
    //throw new Error("不支持的指令 " + Integer.toHexString(bus->read(PC - 1)));
}

void C6502::toSei() {
}

void C6502::toCli() {
}

void C6502::BRK_00() {
    clk += 84;
    pushW(PC + 1);
    push(P | 0x30);
    PC = readAddress(0xfffe);
	toSei();
    P |= 4;
}

void C6502::NOP_ea() {
    clk += 24;
}

void C6502::BIT_24() {
    clk += 36;
    int dt = bus->read(getCode());
    P = P & 0x3d | (dt & 0xc0);
    P |= ((A & dt) != 0) ? 0 : 2;
}

void C6502::BIT_2c() {
    clk += 48;
    int dt = bus->read(getCodeW());
    P = P & 0x3d | (dt & 0xc0);
    P |= ((A & dt) != 0) ? 0 : 2;
}

void C6502::BPL_10() {
    clk += 24;
    rel_jmp(0x80, false);
}

void C6502::BMI_30() {
    clk += 24;
    rel_jmp(0x80, true);
}

void C6502::BCC_90() {
    clk += 24;
    rel_jmp(0x01, false);
}

void C6502::BCS_b0() {
    clk += 24;
    rel_jmp(0x01, true);
}

void C6502::BNE_d0() {
    clk += 24;
    rel_jmp(0x02, false);
}

void C6502::BEQ_f0() {
    clk += 24;
    rel_jmp(0x02, true);
}

void C6502::BVC_50() {
    clk += 24;
    rel_jmp(0x40, false);
}

void C6502::BVS_70() {
    clk += 48;
    rel_jmp(0x40, true);
}

void C6502::CLC_18() {
    clk += 24;
    P &= 0xfe;
}

void C6502::CLD_d8() {
    clk += 24;
    P &= 0xf7;
}

void C6502::CLI_58() {
    clk += 24;
	toCli();
    P &= 0xfb;
    if (irqPending) {
        irqPending = false;
        doIRQ();
    }
}

void C6502::CLV_b8() {
    clk += 24;
    P &= 0xbf;
}

void C6502::SEC_38() {
    clk += 24;
    P |= 1;
}

void C6502::SED_f8() {
    clk += 24;
    P |= 8;
}

void C6502::SEI_78() {
    clk += 24;
	toSei();
    P |= 4;
}

void C6502::JMP_4c() {
    clk += 36;
    PC = getCodeW();
}

void C6502::JMP_6c() {
    clk += 60;
    int t = getCodeW();
    if ((t & 0xff) != 0xff)
        PC = readAddress(t);
    else
        PC = bus->read(t) | (bus->read(t & 0xff00) << 8);
}

void C6502::JSR_20() {
    clk += 72;
    pushW(PC + 1);
    PC = getCodeW();
}

void C6502::RTI_40() {
    clk += 72;
	int p = pop();
	if (p&4) {
		toSei();
	} else {
		toCli();
	}
    P = p & 0xcf; //P只有6位!!!
    PC = popW();
    if (irqPending && (P & 4) == 0) {
        irqPending = false;
        doIRQ();
    }
}

void C6502::RTS_60() {
    clk += 72;
    PC = popW() + 1;
}

void C6502::PHA_48() {
    clk += 36;
    push(A);
}

void C6502::PHP_08() {
    clk += 36;
    push(P | 0x30);
}

void C6502::PLA_68() {
    clk += 48;
    A = pop();
    P = (P & 0x7d) | znTbl[A];
}

void C6502::PLP_28() {
    clk += 48;
	int p = pop();
	if (p&4) {
		toSei();
	} else {
		toCli();
	}
    P = p & 0xcf; //P只有6位!!!
}

void C6502::LDA_a9() {
    clk += 24;
    A = getCode();
    P = (P & 0x7d) | znTbl[A];
}

void C6502::LDA_a5() {
    clk += 36;
    A = bus->read(getCode());
    P = (P & 0x7d) | znTbl[A];
}

void C6502::LDA_b5() {
    clk += 48;
    A = bus->read((getCode() + X) & 0xff);
    P = (P & 0x7d) | znTbl[A];
}

void C6502::LDA_ad() {
    clk += 48;
    A = bus->read(getCodeW());
    P = (P & 0x7d) | znTbl[A];
}

void C6502::LDA_bd() {
    clk += 48;
    A = bus->read(EA_AX());
    P = (P & 0x7d) | znTbl[A];
}

void C6502::LDA_b9() {
    clk += 48;
    A = bus->read(EA_AY());
    P = (P & 0x7d) | znTbl[A];
}

void C6502::LDA_a1() {
    clk += 72;
    A = bus->read(EA_IX());
    P = (P & 0x7d) | znTbl[A];
}

void C6502::LDA_b1() {
    clk += 60;
    A = bus->read(EA_IY());
    P = (P & 0x7d) | znTbl[A];
}

void C6502::LDX_a2() {
    clk += 24;
    X = getCode();
    P = (P & 0x7d) | znTbl[X];
}

void C6502::LDX_a6() {
    clk += 36;
    X = bus->read(getCode());
    P = (P & 0x7d) | znTbl[X];
}

void C6502::LDX_b6() {
    clk += 48;
    X = bus->read((getCode() + Y) & 0xff);
    P = (P & 0x7d) | znTbl[X];
}

void C6502::LDX_ae() {
    clk += 48;
    X = bus->read(getCodeW());
    P = (P & 0x7d) | znTbl[X];
}

void C6502::LDX_be() {
    clk += 48;
    X = bus->read(EA_AY());
    P = (P & 0x7d) | znTbl[X];
}

void C6502::LDY_a0() {
    clk += 24;
    Y = getCode();
    P = (P & 0x7d) | znTbl[Y];
}

void C6502::LDY_a4() {
    clk += 36;
    Y = bus->read(getCode());
    P = (P & 0x7d) | znTbl[Y];
}

void C6502::LDY_b4() {
    clk += 48;
    Y = bus->read((getCode() + X) & 0xff);
    P = (P & 0x7d) | znTbl[Y];
}

void C6502::LDY_ac() {
    clk += 48;
    Y = bus->read(getCodeW());
    P = (P & 0x7d) | znTbl[Y];
}

void C6502::LDY_bc() {
    clk += 48;
    Y = bus->read(EA_AX());
    P = (P & 0x7d) | znTbl[Y];
}

void C6502::STA_85() {
    clk += 36;
    bus->write(getCode(), A);
}

void C6502::STA_95() {
    clk += 48;
    bus->write((getCode() + X) & 0xff, A);
}

void C6502::STA_8d() {
    clk += 48;
    bus->write(getCodeW(), A);
}

void C6502::STA_9d() {
    clk += 60;
    bus->write(EA_AX(), A);
}

void C6502::STA_99() {
    clk += 60;
    bus->write(EA_AY(), A);
}

void C6502::STA_81() {
    clk += 72;
    bus->write(EA_IX(), A);
}

void C6502::STA_91() {
    clk += 72;
    bus->write(EA_IY(), A);
}

void C6502::STX_86() {
    clk += 36;
    bus->write(getCode(), X);
}

void C6502::STX_96() {
    clk += 48;
    bus->write((getCode() + Y) & 0xff, X);
}

void C6502::STX_8e() {
    clk += 48;
    bus->write(getCodeW(), X);
}

void C6502::STY_84() {
    clk += 36;
    bus->write(getCode(), Y);
}

void C6502::STY_94() {
    clk += 48;
    bus->write((getCode() + X) & 0xff, Y);
}

void C6502::STY_8c() {
    clk += 48;
    bus->write(getCodeW(), Y);
}

void C6502::TAX_aa() {
    clk += 24;
    X = A;
    P = (P & 0x7d) | znTbl[X];
}

void C6502::TAY_a8() {
    clk += 24;
    Y = A;
    P = (P & 0x7d) | znTbl[Y];
}

void C6502::TSX_ba() {
    clk += 24;
    X = SP;
    P = (P & 0x7d) | znTbl[X];
}

void C6502::TXA_8a() {
    clk += 24;
    A = X;
    P = (P & 0x7d) | znTbl[A];
}

void C6502::TXS_9a() {
    clk += 24;
    SP = X;
}

void C6502::TYA_98() {
    clk += 24;
    A = Y;
    P = (P & 0x7d) | znTbl[A];
}

void C6502::INX_e8() {
    clk += 24;
    X = (X + 1) & 0xff;
    P = (P & 0x7d) | znTbl[X];
}

void C6502::INY_c8() {
    clk += 24;
    Y = (Y + 1) & 0xff;
    P = (P & 0x7d) | znTbl[Y];
}

void C6502::DEX_ca() {
    clk += 24;
    X = (X - 1) & 0xff;
    P = (P & 0x7d) | znTbl[X];
}

void C6502::DEY_88() {
    clk += 24;
    Y = (Y - 1) & 0xff;
    P = (P & 0x7d) | znTbl[Y];
}

void C6502::DEC_c6() {
    clk += 60;
    int z = getCode();
    int dt = (bus->read(z) - 1) & 0xff;
    bus->write(z, dt);
    P = (P & 0x7d) | znTbl[dt];
}

void C6502::DEC_d6() {
    clk += 72;
    int z = (getCode() + X) & 0xff;
    int dt = (bus->read(z) - 1) & 0xff;
    bus->write(z, dt);
    P = (P & 0x7d) | znTbl[dt];
}

void C6502::DEC_ce() {
    clk += 72;
    int address = getCodeW();
    int dt = (bus->read(address) - 1) & 0xff;
    bus->write(address, dt);
    P = (P & 0x7d) | znTbl[dt];
}

void C6502::DEC_de() {
    clk += 84;
    int address = EA_AX();
    int dt = (bus->read(address) - 1) & 0xff;
    bus->write(address, dt);
    P = (P & 0x7d) | znTbl[dt];
}

void C6502::INC_e6() {
    clk += 60;
    int z = getCode();
    int dt = (bus->read(z) + 1) & 0xff;
    bus->write(z, dt);
    P = (P & 0x7d) | znTbl[dt];
}

void C6502::INC_f6() {
    clk += 72;
    int z = (getCode() + X) & 0xff;
    int dt = (bus->read(z) + 1) & 0xff;
    bus->write(z, dt);
    P = (P & 0x7d) | znTbl[dt];
}

void C6502::INC_ee() {
    clk += 72;
    int address = getCodeW();
    int dt = (bus->read(address) + 1) & 0xff;
    bus->write(address, dt);
    P = (P & 0x7d) | znTbl[dt];
}

void C6502::INC_fe() {
    clk += 84;
    int address = EA_AX();
    int dt = (bus->read(address) + 1) & 0xff;
    bus->write(address, dt);
    P = (P & 0x7d) | znTbl[dt];
}

void C6502::CMP_c9() {
    clk += 24;
    cmp(A, getCode());
}

void C6502::CMP_c5() {
    clk += 36;
    cmp(A, bus->read(getCode()));
}

void C6502::CMP_d5() {
    clk += 48;
    cmp(A, bus->read((getCode() + X) & 0xff));
}

void C6502::CMP_cd() {
    clk += 48;
    cmp(A, bus->read(getCodeW()));
}

void C6502::CMP_dd() {
    clk += 48;
    cmp(A, bus->read(EA_AX()));
}

void C6502::CMP_d9() {
    clk += 48;
    cmp(A, bus->read(EA_AY()));
}

void C6502::CMP_c1() {
    clk += 72;
    cmp(A, bus->read(EA_IX()));
}

void C6502::CMP_d1() {
    clk += 60;
    cmp(A, bus->read(EA_IY()));
}

void C6502::CPX_e0() {
    clk += 24;
    cmp(X, getCode());
}

void C6502::CPX_e4() {
    clk += 36;
    cmp(X, bus->read(getCode()));
}

void C6502::CPX_ec() {
    clk += 48;
    cmp(X, bus->read(getCodeW()));
}

void C6502::CPY_c0() {
    clk += 24;
    cmp(Y, getCode());
}

void C6502::CPY_c4() {
    clk += 36;
    cmp(Y, bus->read(getCode()));
}

void C6502::CPY_cc() {
    clk += 48;
    cmp(Y, bus->read(getCodeW()));
}

void C6502::AND_29() {
    clk += 24;
    A &= getCode();
    P = (P & 0x7d) | znTbl[A];
}

void C6502::AND_25() {
    clk += 36;
    A &= bus->read(getCode());
    P = (P & 0x7d) | znTbl[A];
}

void C6502::AND_35() {
    clk += 48;
    A &= bus->read((getCode() + X) & 0xff);
    P = (P & 0x7d) | znTbl[A];
}

void C6502::AND_2d() {
    clk += 48;
    A &= bus->read(getCodeW());
    P = (P & 0x7d) | znTbl[A];
}

void C6502::AND_3d() {
    clk += 48;
    A &= bus->read(EA_AX());
    P = (P & 0x7d) | znTbl[A];
}

void C6502::AND_39() {
    clk += 48;
    A &= bus->read(EA_AY());
    P = (P & 0x7d) | znTbl[A];
}

void C6502::AND_21() {
    clk += 72;
    A &= bus->read(EA_IX());
    P = (P & 0x7d) | znTbl[A];
}

void C6502::AND_31() {
    clk += 60;
    A &= bus->read(EA_IY());
    P = (P & 0x7d) | znTbl[A];
}

void C6502::EOR_49() {
    clk += 24;
    A ^= getCode();
    P = (P & 0x7d) | znTbl[A];
}

void C6502::EOR_45() {
    clk += 36;
    A ^= bus->read(getCode());
    P = (P & 0x7d) | znTbl[A];
}

void C6502::EOR_55() {
    clk += 48;
    A ^= bus->read((getCode() + X) & 0xff);
    P = (P & 0x7d) | znTbl[A];
}

void C6502::EOR_4d() {
    clk += 48;
    A ^= bus->read(getCodeW());
    P = (P & 0x7d) | znTbl[A];
}

void C6502::EOR_5d() {
    clk += 48;
    A ^= bus->read(EA_AX());
    P = (P & 0x7d) | znTbl[A];
}

void C6502::EOR_59() {
    clk += 48;
    A ^= bus->read(EA_AY());
    P = (P & 0x7d) | znTbl[A];
}

void C6502::EOR_41() {
    clk += 72;
    A ^= bus->read(EA_IX());
    P = (P & 0x7d) | znTbl[A];
}

void C6502::EOR_51() {
    clk += 60;
    A ^= bus->read(EA_IY());
    P = (P & 0x7d) | znTbl[A];
}

void C6502::ORA_09() {
    clk += 24;
    A |= getCode();
    P = (P & 0x7d) | znTbl[A];
}

void C6502::ORA_05() {
    clk += 36;
    A |= bus->read(getCode());
    P = (P & 0x7d) | znTbl[A];
}

void C6502::ORA_15() {
    clk += 48;
    A |= bus->read((getCode() + X) & 0xff);
    P = (P & 0x7d) | znTbl[A];
}

void C6502::ORA_0d() {
    clk += 48;
    A |= bus->read(getCodeW());
    P = (P & 0x7d) | znTbl[A];
}

void C6502::ORA_1d() {
    clk += 48;
    A |= bus->read(EA_AX());
    P = (P & 0x7d) | znTbl[A];
}

void C6502::ORA_19() {
    clk += 48;
    A |= bus->read(EA_AY());
    P = (P & 0x7d) | znTbl[A];
}

void C6502::ORA_01() {
    clk += 72;
    A |= bus->read(EA_IX());
    P = (P & 0x7d) | znTbl[A];
}

void C6502::ORA_11() {
    clk += 60;
    A |= bus->read(EA_IY());
    P = (P & 0x7d) | znTbl[A];
}

void C6502::ROL_26() {
    clk += 60;
    int z = getCode();
    bus->write(z, rol(bus->read(z)));
}

void C6502::ROL_36() {
    clk += 72;
    int z = (getCode() + X) & 0xff;
    bus->write(z, rol(bus->read(z)));
}

void C6502::ROL_2e() {
    clk += 72;
    int address = getCodeW();
    bus->write(address, rol(bus->read(address)));
}

void C6502::ROL_3e() {
    clk += 84;
    int address = EA_AX();
    bus->write(address, rol(bus->read(address)));
}

void C6502::ROL_2a() {
    clk += 24;
    A = rol(A);
}

void C6502::ROR_66() {
    clk += 60;
    int z = getCode();
    bus->write(z, ror(bus->read(z)));
}

void C6502::ROR_76() {
    clk += 72;
    int z = (getCode() + X) & 0xff;
    bus->write(z, ror(bus->read(z)));
}

void C6502::ROR_6e() {
    clk += 72;
    int address = getCodeW();
    bus->write(address, ror(bus->read(address)));
}

void C6502::ROR_7e() {
    clk += 84;
    int address = EA_AX();
    bus->write(address, ror(bus->read(address)));
}

void C6502::ROR_6a() {
    clk += 24;
    A = ror(A);
}

void C6502::ASL_06() {
    clk += 60;
    int z = getCode();
    bus->write(z, asl(bus->read(z)));
}

void C6502::ASL_16() {
    clk += 72;
    int z = (getCode() + X) & 0xff;
    bus->write(z, asl(bus->read(z)));
}

void C6502::ASL_0e() {
    clk += 72;
    int address = getCodeW();
    bus->write(address, asl(bus->read(address)));
}

void C6502::ASL_1e() {
    clk += 84;
    int address = EA_AX();
    bus->write(address, asl(bus->read(address)));
}

void C6502::ASL_0a() {
    clk += 24;
    A = asl(A);
}

void C6502::LSR_46() {
    clk += 60;
    int z = getCode();
    bus->write(z, lsr(bus->read(z)));
}

void C6502::LSR_56() {
    clk += 72;
    int z = (getCode() + X) & 0xff;
    bus->write(z, lsr(bus->read(z)));
}

void C6502::LSR_4e() {
    clk += 72;
    int address = getCodeW();
    bus->write(address, lsr(bus->read(address)));
}

void C6502::LSR_5e() {
    clk += 84;
    int address = EA_AX();
    bus->write(address, lsr(bus->read(address)));
}

void C6502::LSR_4a() {
    clk += 24;
    A = lsr(A);
}

void C6502::ADC_69() {
    clk += 24;
    add(getCode());
}

void C6502::ADC_65() {
    clk += 36;
    add(bus->read(getCode()));
}

void C6502::ADC_75() {
    clk += 48;
    add(bus->read((getCode() + X) & 0xff));
}

void C6502::ADC_6d() {
    clk += 48;
    add(bus->read(getCodeW()));
}

void C6502::ADC_7d() {
    clk += 48;
    add(bus->read(EA_AX()));
}

void C6502::ADC_79() {
    clk += 48;
    add(bus->read(EA_AY()));
}

void C6502::ADC_61() {
    clk += 72;
    add(bus->read(EA_IX()));
}

void C6502::ADC_71() {
    clk += 60;
    add(bus->read(EA_IY()));
}

void C6502::SBC_e9() {
    clk += 24;
    sub(getCode());
}

void C6502::SBC_e5() {
    clk += 36;
    sub(bus->read(getCode()));
}

void C6502::SBC_f5() {
    clk += 48;
    sub(bus->read((getCode() + X) & 0xff));
}

void C6502::SBC_ed() {
    clk += 48;
    sub(bus->read(getCodeW()));
}

void C6502::SBC_fd() {
    clk += 48;
    sub(bus->read(EA_AX()));
}

void C6502::SBC_f9() {
    clk += 48;
    sub(bus->read(EA_AY()));
}

void C6502::SBC_e1() {
    clk += 72;
    sub(bus->read(EA_IX()));
}

void C6502::SBC_f1() {
    clk += 60;
    sub(bus->read(EA_IY()));
}

int C6502::EA_AX() {
    int et = getCodeW();
    int ea = (et + X) & 0xffff; //地址必须限制为16位!!!
    if (((ea ^ et) & 0x100) != 0)
        clk += 12;
    return ea;
}

int C6502::EA_AY() {
    int et = getCodeW();
    int ea = (et + Y) & 0xffff; //地址必须限制为16位!!!
    if (((ea ^ et) & 0x100) != 0)
        clk += 12;
    return ea;
}

int C6502::EA_IX() {
    int z = (getCode() + X) & 0xff;
    return bus->read(z) | (bus->read((z + 1) & 0xff) << 8); //z+1必须限制在0页!!!
}

int C6502::EA_IY() {
    int z = getCode();
    int et = bus->read(z) | (bus->read((z + 1) & 0xff) << 8); //z+1必须限制在0页!!!
    int ea = (et + Y) & 0xffff; //地址必须限制为16位!!!
    if (((ea ^ et) & 0x100) != 0)
        clk += 12;
    return ea;
}

int C6502::asl(int dt) {
    P = (P & 0x7c) | (dt >> 7);
    dt = (dt << 1) & 0xff;
    P |= znTbl[dt];
    return dt;
}

int C6502::lsr(int dt) {
    P = (P & 0x7c) | (dt & 1);
    dt >>= 1;
    P |= znTbl[dt];
    return dt;
}

int C6502::rol(int dt) {
    dt = (dt << 1) | (P & 1);
    P = (P & 0x7c) | (dt >> 8);
    dt &= 0xff;
    P |= znTbl[dt];
    return dt;
}

int C6502::ror(int dt) {
    int j = P & 1;
    P = (P & 0x7c) | (dt & 1);
    dt = (dt >> 1) | (j << 7);
    P |= znTbl[dt];
    return dt;
}

void C6502::add(int dt) {
    int j = A + dt + (P & 1);
    P = (P & 0x3c) | ((~(A ^ dt) & (A ^ j) & 0x80) >> 1) | (j >> 8);
    A = j & 0xff;
    P |= znTbl[A];
}

void C6502::sub(int dt) {
    int j = A - dt - (~P & 1);
    P = (P & 0x3c) | (((A ^ dt) & (A ^ j) & 0x80) >> 1) | (j >= 0 ? 1 : 0);
    A = j & 0xff;
    P |= znTbl[A];
}

void C6502::cmp(int i, int j) {
    int k = i - j;
    P = (P & 0x7c) | (k >= 0 ? 1 : 0);
    P |= znTbl[k & 0xff];
}

void C6502::rel_jmp(int flagNum, boolean flagVal) {
    int offset = getCode();
    if (((P & flagNum) != 0) == flagVal) {
        if ((offset & 0x80) != 0)
            offset -= 256;
        if (((PC ^ (PC + offset)) & 0x100) != 0)
            clk += 12;
        PC = (PC + offset) & 0xffff; //防止pc超过16位限制!!!
        clk += 12;
    }
}

void C6502::push(int value) {
    bus->write(SP + 0x100, value);
    SP = (SP - 1) & 0xff;
}

void C6502::pushW(int value) {
    push(value >> 8);
    push(value & 0xff);
}

int C6502::pop() {
    SP = (SP + 1) & 0xff;
    return bus->read(SP + 0x100);
}

int C6502::popW() {
	//防止不正确的优化，改变两个pop的执行顺序
	int w = pop();
    return /*pop()*/w | (pop() << 8);
}

void C6502::doCode(int code) {
    switch (code) {
        case 0:
            BRK_00();
            break;
        case 1:
            ORA_01();
            break;
        case 5:
            ORA_05();
            break;
        case 6:
            ASL_06();
            break;
        case 8:
            PHP_08();
            break;
        case 9:
            ORA_09();
            break;
        case 10:
            ASL_0a();
            break;
        case 13:
            ORA_0d();
            break;
        case 14:
            ASL_0e();
            break;
        case 16:
            BPL_10();
            break;
        case 17:
            ORA_11();
            break;
        case 21:
            ORA_15();
            break;
        case 22:
            ASL_16();
            break;
        case 24:
            CLC_18();
            break;
        case 25:
            ORA_19();
            break;
        case 29:
            ORA_1d();
            break;
        case 30:
            ASL_1e();
            break;
        case 32:
            JSR_20();
            break;
        case 33:
            AND_21();
            break;
        case 36:
            BIT_24();
            break;
        case 37:
            AND_25();
            break;
        case 38:
            ROL_26();
            break;
        case 40:
            PLP_28();
            break;
        case 41:
            AND_29();
            break;
        case 42:
            ROL_2a();
            break;
        case 44:
            BIT_2c();
            break;
        case 45:
            AND_2d();
            break;
        case 46:
            ROL_2e();
            break;
        case 48:
            BMI_30();
            break;
        case 49:
            AND_31();
            break;
        case 53:
            AND_35();
            break;
        case 54:
            ROL_36();
            break;
        case 56:
            SEC_38();
            break;
        case 57:
            AND_39();
            break;
        case 61:
            AND_3d();
            break;
        case 62:
            ROL_3e();
            break;
        case 64:
            RTI_40();
            break;
        case 65:
            EOR_41();
            break;
        case 69:
            EOR_45();
            break;
        case 70:
            LSR_46();
            break;
        case 72:
            PHA_48();
            break;
        case 73:
            EOR_49();
            break;
        case 74:
            LSR_4a();
            break;
        case 76:
            JMP_4c();
            break;
        case 77:
            EOR_4d();
            break;
        case 78:
            LSR_4e();
            break;
        case 80:
            BVC_50();
            break;
        case 81:
            EOR_51();
            break;
        case 85:
            EOR_55();
            break;
        case 86:
            LSR_56();
            break;
        case 88:
            CLI_58();
            break;
        case 89:
            EOR_59();
            break;
        case 93:
            EOR_5d();
            break;
        case 94:
            LSR_5e();
            break;
        case 96:
            RTS_60();
            break;
        case 97:
            ADC_61();
            break;
        case 101:
            ADC_65();
            break;
        case 102:
            ROR_66();
            break;
        case 104:
            PLA_68();
            break;
        case 105:
            ADC_69();
            break;
        case 106:
            ROR_6a();
            break;
        case 108:
            JMP_6c();
            break;
        case 109:
            ADC_6d();
            break;
        case 110:
            ROR_6e();
            break;
        case 112:
            BVS_70();
            break;
        case 113:
            ADC_71();
            break;
        case 117:
            ADC_75();
            break;
        case 118:
            ROR_76();
            break;
        case 120:
            SEI_78();
            break;
        case 121:
            ADC_79();
            break;
        case 125:
            ADC_7d();
            break;
        case 126:
            ROR_7e();
            break;
        case 129:
            STA_81();
            break;
        case 132:
            STY_84();
            break;
        case 133:
            STA_85();
            break;
        case 134:
            STX_86();
            break;
        case 136:
            DEY_88();
            break;
        case 138:
            TXA_8a();
            break;
        case 140:
            STY_8c();
            break;
        case 141:
            STA_8d();
            break;
        case 142:
            STX_8e();
            break;
        case 144:
            BCC_90();
            break;
        case 145:
            STA_91();
            break;
        case 148:
            STY_94();
            break;
        case 149:
            STA_95();
            break;
        case 150:
            STX_96();
            break;
        case 152:
            TYA_98();
            break;
        case 153:
            STA_99();
            break;
        case 154:
            TXS_9a();
            break;
        case 157:
            STA_9d();
            break;
        case 160:
            LDY_a0();
            break;
        case 161:
            LDA_a1();
            break;
        case 162:
            LDX_a2();
            break;
        case 164:
            LDY_a4();
            break;
        case 165:
            LDA_a5();
            break;
        case 166:
            LDX_a6();
            break;
        case 168:
            TAY_a8();
            break;
        case 169:
            LDA_a9();
            break;
        case 170:
            TAX_aa();
            break;
        case 172:
            LDY_ac();
            break;
        case 173:
            LDA_ad();
            break;
        case 174:
            LDX_ae();
            break;
        case 176:
            BCS_b0();
            break;
        case 177:
            LDA_b1();
            break;
        case 180:
            LDY_b4();
            break;
        case 181:
            LDA_b5();
            break;
        case 182:
            LDX_b6();
            break;
        case 184:
            CLV_b8();
            break;
        case 185:
            LDA_b9();
            break;
        case 186:
            TSX_ba();
            break;
        case 188:
            LDY_bc();
            break;
        case 189:
            LDA_bd();
            break;
        case 190:
            LDX_be();
            break;
        case 192:
            CPY_c0();
            break;
        case 193:
            CMP_c1();
            break;
        case 196:
            CPY_c4();
            break;
        case 197:
            CMP_c5();
            break;
        case 198:
            DEC_c6();
            break;
        case 200:
            INY_c8();
            break;
        case 201:
            CMP_c9();
            break;
        case 202:
            DEX_ca();
            break;
        case 204:
            CPY_cc();
            break;
        case 205:
            CMP_cd();
            break;
        case 206:
            DEC_ce();
            break;
        case 208:
            BNE_d0();
            break;
        case 209:
            CMP_d1();
            break;
        case 213:
            CMP_d5();
            break;
        case 214:
            DEC_d6();
            break;
        case 216:
            CLD_d8();
            break;
        case 217:
            CMP_d9();
            break;
        case 221:
            CMP_dd();
            break;
        case 222:
            DEC_de();
            break;
        case 224:
            CPX_e0();
            break;
        case 225:
            SBC_e1();
            break;
        case 228:
            CPX_e4();
            break;
        case 229:
            SBC_e5();
            break;
        case 230:
            INC_e6();
            break;
        case 232:
            INX_e8();
            break;
        case 233:
            SBC_e9();
            break;
        case 234:
            NOP_ea();
            break;
        case 236:
            CPX_ec();
            break;
        case 237:
            SBC_ed();
            break;
        case 238:
            INC_ee();
            break;
        case 240:
            BEQ_f0();
            break;
        case 241:
            SBC_f1();
            break;
        case 245:
            SBC_f5();
            break;
        case 246:
            INC_f6();
            break;
        case 248:
            SED_f8();
            break;
        case 249:
            SBC_f9();
            break;
        case 253:
            SBC_fd();
            break;
        case 254:
            INC_fe();
            break;

        default:
            XXX_xx();
    }
}

void C6502::getInfo(char info[]) {
	sprintf(info,"PC:%04X SP:%03X P:%02X A:%02X X:%02X Y:%02X",PC,SP+0x100,P,A,X,Y);
}

const char *codeName[256]={
	"BRK","ORA","   ","   ","   ","ORA","ASL","   ", //00
	"PHP","ORA","ASL","   ","   ","ORA","ASL","   ",
	"BPL","ORA","   ","   ","   ","ORA","ASL","   ", //10
	"CLC","ORA","   ","   ","   ","ORA","ASL","   ",
	"JSR","AND","   ","   ","BIT","AND","ROL","   ", //20
	"PLP","AND","ROL","   ","BIT","AND","ROL","   ",
	"BMI","AND","   ","   ","   ","AND","ROL","   ", //30
	"SEC","AND","   ","   ","   ","AND","ROL","   ",
	"RTI","EOR","   ","   ","   ","EOR","LSR","   ", //40
	"PHA","EOR","LSR","   ","JMP","EOR","LSR","   ",
	"BVC","EOR","   ","   ","   ","EOR","LSR","   ", //50
	"CLI","EOR","   ","   ","   ","EOR","LSR","   ",
	"RTS","ADC","   ","   ","   ","ADC","ROR","   ", //60
	"PLA","ADC","ROR","   ","JMP","ADC","ROR","   ",
	"BVS","ADC","   ","   ","   ","ADC","ROR","   ", //70
	"SEI","ADC","   ","   ","   ","ADC","ROR","   ",
	"   ","STA","   ","   ","STY","STA","STX","   ", //80
	"DEY","   ","TXA","   ","STY","STA","STX","   ",
	"BCC","STA","   ","   ","STY","STA","STX","   ", //90
	"TYA","STA","TXS","   ","   ","STA","   ","   ",
	"LDY","LDA","LDX","   ","LDY","LDA","LDX","   ", //a0
	"TAY","LDA","TAX","   ","LDY","LDA","LDX","   ",
	"BCS","LDA","   ","   ","LDY","LDA","LDX","   ", //b0
	"CLV","LDA","TSX","   ","LDY","LDA","LDX","   ",
	"CPY","CMP","   ","   ","CPY","CMP","DEC","   ", //c0
	"INY","CMP","DEX","   ","CPY","CMP","DEC","   ",
	"BNE","CMP","   ","   ","   ","CMP","DEC","   ", //d0
	"CLD","CMP","   ","   ","   ","CMP","DEC","   ",
	"CPX","SBC","   ","   ","CPX","SBC","INC","   ", //e0
	"INX","SBC","NOP","   ","CPX","SBC","INC","   ",
	"BEQ","SBC","   ","   ","   ","SBC","INC","   ", //f0
	"SED","SBC","   ","   ","   ","SBC","INC","   "
};

int codeAddr[256]={
	7,5,0,0,0,2,2,0,0,1,0,0,0,7,7,0, //00
	11,6,0,0,0,3,3,0,0,9,0,0,0,8,8,0,
	7,5,0,0,2,2,2,0,0,1,0,0,7,7,7,0, //20
	11,6,0,0,0,3,3,0,0,9,0,0,0,8,8,0,
	0,5,0,0,0,2,2,0,0,1,0,0,7,7,7,0, //40
	11,6,0,0,0,3,3,0,0,9,0,0,0,8,8,0,
	0,5,0,0,0,2,2,0,0,1,0,0,10,7,7,0, //60
	11,6,0,0,0,3,3,0,0,9,0,0,0,8,8,0,
	0,5,0,0,2,2,2,0,0,0,0,0,7,7,7,0, //80
	11,6,0,0,3,3,4,0,0,9,0,0,0,8,0,0,
	1,5,1,0,2,2,2,0,0,1,0,0,7,7,7,0, //a0
	11,6,0,0,3,3,4,0,0,9,0,0,8,8,9,0,
	1,5,0,0,2,2,2,0,0,1,0,0,7,7,7,0, //c0
	11,6,0,0,0,3,3,0,0,9,0,0,0,8,8,0,
	1,5,0,0,2,2,2,0,0,1,0,0,7,7,7,0, //e0
	11,6,0,0,0,3,3,0,0,9,0,0,0,8,8,0
};

byte CodeLen[256]={
	1,1,0,0,0,1,1,0,0,1,0,0,0,2,2,0,1,1,0,0,0,1,1,0,0,2,0,0,0,2,2,0,
	2,1,0,0,1,1,1,0,0,1,0,0,2,2,2,0,1,1,0,0,0,1,1,0,0,2,0,0,0,2,2,0,
	0,1,0,0,0,1,1,0,0,1,0,0,2,2,2,0,1,1,0,0,0,1,1,0,0,2,0,0,0,2,2,0,
	0,1,0,0,0,1,1,0,0,1,0,0,2,2,2,0,1,1,0,0,0,1,1,0,0,2,0,0,0,2,2,0,
	0,1,0,0,1,1,1,0,0,0,0,0,2,2,2,0,1,1,0,0,1,1,1,0,0,2,0,0,0,2,0,0,
	1,1,1,0,1,1,1,0,0,1,0,0,2,2,2,0,1,1,0,0,1,1,1,0,0,2,0,0,2,2,2,0,
	1,1,0,0,1,1,1,0,0,1,0,0,2,2,2,0,1,1,0,0,0,1,1,0,0,2,0,0,0,2,2,0,
	1,1,0,0,1,1,1,0,0,1,0,0,2,2,2,0,1,1,0,0,0,1,1,0,0,2,0,0,0,2,2,0};

int C6502::dasm(int address,char *s)
{
	byte code;
	char t[20];
	sprintf(s,"%04X: ",address);
	code=bus->read(address);
	sprintf(t,"%s ",codeName[code]);
	if (t[0]==' ') { //非法指令
		s[0]=0;
		return 0;
	}
	int ret=CodeLen[code]+1;
	if (code==0) {
		ret=3; //BRK在文曲星当3字节指令用
	}
	strcat(s,t);
	switch (codeAddr[code]) {
	case 1:
		sprintf(t,"#$%02X",bus->read(address+1));break;
	case 2:
		sprintf(t,"$%02X",bus->read(address+1));break;
	case 3:
		sprintf(t,"$%02X,X",bus->read(address+1));break;
	case 4:
		sprintf(t,"$%02X,Y",bus->read(address+1));break;
	case 5:
		sprintf(t,"($%02X,X)",bus->read(address+1));break;
	case 6:
		sprintf(t,"($%02X),Y",bus->read(address+1));break;
	case 7:
		sprintf(t,"$%04X",readAddress(address+1));break;
	case 8:
		sprintf(t,"$%04X,X",readAddress(address+1));break;
	case 9:
		sprintf(t,"$%04X,Y",readAddress(address+1));break;
	case 10:
		sprintf(t,"($%04X)",readAddress(address+1));break;
	case 11:
		code=bus->read(address+1);
		if (code<128)
			sprintf(t,"$%04X",address+2+code);
		else
			sprintf(t,"$%04X",address+2+code-256);
		break;
	default:
		t[0]=0;
	}
	strcat(s,t);
	return ret;
}
