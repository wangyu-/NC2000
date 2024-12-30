#pragma once

class IBus6502 {
public:
	virtual int read(int address)=0;
    virtual void write(int address, int value)=0;
};
