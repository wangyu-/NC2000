#pragma once

#include <stdint.h>

class IBus6502 {
public:
	virtual int read(int address)=0;
    virtual void write(int address, int value)=0;
	virtual ~IBus6502() = default;
};

struct BusWrapper:IBus6502{
	BusWrapper(){
	}
	int read(int address){
        uint8_t Load(uint16_t addr);
		return Load(address);
	}
    void write(int address, int value){
        void Store(uint16_t addr, uint8_t value);
		Store(address,value);
	}
};
