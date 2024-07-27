#pragma once

#include <vector>
using std::vector;

#define byte unsigned char

#define ORDER 10

typedef struct {
	int D[ORDER+1];
	int ai[ORDER+1];
} fixfilter;

typedef struct {
	int Lindex;
	int Gindex;
	int Grid;
	int Pos[2];
	int Sign[2];
} tagGainShape;

class Dsp {
	byte clpBuf[18];
	int dspCelp[15];
	int dspCelpOff;
	int dspMode;
public:
	Dsp();
	void reset();
	void write(int high,int low);
private:
	void dspCelpToCelp();
	void writeSample8000(int val);
	void writePcm(int val);

	void dspStart();
	void subFrame(int subframe_NUM,int s0,int s1,int fixpos);
	void dsp(byte* celp);
	int SamplePitch(int Pos,int F,int s);
	void DecodePitch(int P,int k);
	void InterpLSP(int k);
	void fixlsptopc();
	void root_rs(int *HXrootInPo, int *HXpcoef);
	int fixpolefilter(int S);
	int rounding(int mul,int P);
private:
	int oldindex[ORDER];
	int newindex[ORDER];
	int lsp[ORDER];
	int r[240];
	int LTP[148] ;
	short Sout[240];
	int recursive[60];
	int Period,Frac,_SubFrameSize;
	int PreP;
	int pcount;
	int EX;
	int PM;
	bool lspflag;
	tagGainShape Pitch,Inno;
	fixfilter decoder;

	int delsp[10];
	int pc[ORDER+1];
    int tr[5];
	int p[5], q[5] , RX[5] , SX[5];
	int id;

	vector<signed short> buf_frame;
	vector<signed short> buf_syllable;
	int c2=0;
	int c4=0;
	int c8=0;
	int cnt=0;

public:
	void (*callback) (unsigned char *, int);

};
