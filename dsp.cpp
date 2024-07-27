#include <stdio.h>
//include "SoundStream.h"
#include "dsp.h"

//extern SoundStream soundStream;

#define CELP_MODE 1
#define WORD_MODE 2
#define PCM_MODE 4

const int LSPsearchPtr[ORDER] = { 0, 8, 24, 40, 56, 72, 80, 88, 96, 104 } ;

const int InterWeight[8] = {  //1.15
	3,2,1,0, // {0.75,0.5,0.25,0.},
	1,2,3,4  // {0.25,0.5,0.75,1.}
};

const short wsinc[16] = { //1.15
	-235,-383,
	1050,1629,
	-3571,-5216,
	12714,26674,
	26674,12714,
	-5216,-3571,
	1629,1050,
	-383,-235
};

const short PGain[]={       //Q14
	0x0000, 0x04cc, 0x0999, 0x0e66, 0x1333, 0x1800, 0x1ccc, 0x2199,
	0x2666, 0x2b33, 0x3000, 0x34cc, 0x3999, 0x3e66, 0x4333, 0x4800
};

const short CGain[]={       //Q0
	//   1.,  3., 13., 35., 64., 98.,136., 178.,
	// 224.,278.,340.,418.,520.,660.,870.,1330.
	0x0000, 0x0001, 0x0003, 0x000D, 0x0023, 0x0040, 0x0062, 0x0088,
	0x00B2, 0x00E0, 0x0116, 0x0154, 0x01A2, 0x0208, 0x0294, 0x0366
};

const short LSPINDEX[112] = { // Q8, angle resolution = 31.25
	0x0333, 0x0570, 0x0733, 0x0800, 0x08f5, 0x0ae1, 0x0d70, 0x1000,
	0x06b8, 0x0785, 0x087a, 0x0970, 0x0a66, 0x0b85, 0x0ccc, 0x0e14,
	0x0f5c, 0x10a3, 0x11eb, 0x1385, 0x1570, 0x17ae, 0x19eb, 0x1c28,
	0x0d70, 0x0eb8, 0x1000, 0x1147, 0x12b8, 0x147a, 0x168f, 0x18cc,
	0x1b33, 0x1e66, 0x2199, 0x24cc, 0x2800, 0x2b33, 0x2e66, 0x3199,
	0x13d7, 0x151e, 0x170a, 0x1970, 0x1c28, 0x1f0a, 0x228f, 0x2570,
	0x28a3, 0x2bd7, 0x2f0a, 0x323d, 0x3570, 0x38a3, 0x3bd7, 0x3f0a,
	0x2000, 0x2199, 0x2428, 0x26b8, 0x291e, 0x2b33, 0x2dc2, 0x3051,
	0x32e1, 0x3570, 0x3800, 0x3b33, 0x3e66, 0x4199, 0x44cc, 0x4800,
	0x2f0a, 0x323d, 0x3614, 0x3a8f, 0x4000, 0x4666, 0x4ccc, 0x5333,
	0x3999, 0x3c28, 0x3eb8, 0x4333, 0x4999, 0x4f5c, 0x5666, 0x5ccc,
	0x4733, 0x4ccc, 0x50cc, 0x54cc, 0x5999, 0x5e66, 0x64cc, 0x6b33,
	0x5851, 0x5c28, 0x6000, 0x6333, 0x6666, 0x69eb, 0x6dc2, 0x7199,
	0x6614, 0x68a3, 0x6b33, 0x6d70, 0x6fae, 0x72e1, 0x76b8, 0x7a8f
};

const short COSTABLE[128] = { // Q13, angle resolution = 31.25
	16384,16379,16364,16339,16305,16260,16206,16142,
	16069,15985,15892,15790,15678,15557,15426,15286,
	15136,14978,14810,14634,14449,14255,14053,13842,
	13622,13395,13159,12916,12665,12406,12139,11866,
	11585,11297,11002,10701,10393,10079,9759,9434,
	9102,8765,8423,8075,7723,7366,7005,6639,
	6269,5896,5519,5139,4756,4369,3980,3589,
	3196,2801,2404,2005,1605,1205,803,402,
	0,-402,-803,-1205,-1605,-2005,-2404,-2801,
	-3196,-3589,-3980,-4369,-4756,-5139,-5519,-5896,
	-6269,-6639,-7005,-7366,-7723,-8075,-8423,-8765,
	-9102,-9434,-9759,-10079,-10393,-10701,-11002,-11297,
	-11585,-11866,-12139,-12406,-12665,-12916,-13159,-13395,
	-13622,-13842,-14053,-14255,-14449,-14634,-14810,-14978,
	-15136,-15286,-15426,-15557,-15678,-15790,-15892,-15985,
	-16069,-16142,-16206,-16260,-16305,-16339,-16364,-16379
};

/*void printfTable() {
	printf("{\n\t");
	for (int i=0;i<16;i++) {
		printf("%d,",wsinc[i]);
		if ((i&1)==1)
			printf("\n\t");
	}
	printf("}\n");
}*/

const short FIXCODEBOOK[435] = {
	0x0001, 0x0002, 0x0003, 0x0004, 0x0005, 0x0006, 0x0007, 0x0008, 0x0009, 0x000a,
	0x000b, 0x000c, 0x000d, 0x000e, 0x000f, 0x0010, 0x0011, 0x0012, 0x0013, 0x0014,
	0x0015, 0x0016, 0x0017, 0x0018, 0x0019, 0x001a, 0x001b, 0x001c, 0x001d, 0x0102,
	0x0103, 0x0104, 0x0105, 0x0106, 0x0107, 0x0108, 0x0109, 0x010a, 0x010b, 0x010c,
	0x010d, 0x010e, 0x010f, 0x0110, 0x0111, 0x0112, 0x0113, 0x0114, 0x0115, 0x0116,
	0x0117, 0x0118, 0x0119, 0x011a, 0x011b, 0x011c, 0x011d, 0x0203, 0x0204, 0x0205,
	0x0206, 0x0207, 0x0208, 0x0209, 0x020a, 0x020b, 0x020c, 0x020d, 0x020e, 0x020f,
	0x0210, 0x0211, 0x0212, 0x0213, 0x0214, 0x0215, 0x0216, 0x0217, 0x0218, 0x0219,
	0x021a, 0x021b, 0x021c, 0x021d, 0x0304, 0x0305, 0x0306, 0x0307, 0x0308, 0x0309,
	0x030a, 0x030b, 0x030c, 0x030d, 0x030e, 0x030f, 0x0310, 0x0311, 0x0312, 0x0313,
	0x0314, 0x0315, 0x0316, 0x0317, 0x0318, 0x0319, 0x031a, 0x031b, 0x031c, 0x031d,
	0x0405, 0x0406, 0x0407, 0x0408, 0x0409, 0x040a, 0x040b, 0x040c, 0x040d, 0x040e,
	0x040f, 0x0410, 0x0411, 0x0412, 0x0413, 0x0414, 0x0415, 0x0416, 0x0417, 0x0418,
	0x0419, 0x041a, 0x041b, 0x041c, 0x041d, 0x0506, 0x0507, 0x0508, 0x0509, 0x050a,
	0x050b, 0x050c, 0x050d, 0x050e, 0x050f, 0x0510, 0x0511, 0x0512, 0x0513, 0x0514,
	0x0515, 0x0516, 0x0517, 0x0518, 0x0519, 0x051a, 0x051b, 0x051c, 0x051d, 0x0607,
	0x0608, 0x0609, 0x060a, 0x060b, 0x060c, 0x060d, 0x060e, 0x060f, 0x0610, 0x0611,
	0x0612, 0x0613, 0x0614, 0x0615, 0x0616, 0x0617, 0x0618, 0x0619, 0x061a, 0x061b,
	0x061c, 0x061d, 0x0708, 0x0709, 0x070a, 0x070b, 0x070c, 0x070d, 0x070e, 0x070f,
	0x0710, 0x0711, 0x0712, 0x0713, 0x0714, 0x0715, 0x0716, 0x0717, 0x0718, 0x0719,
	0x071a, 0x071b, 0x071c, 0x071d, 0x0809, 0x080a, 0x080b, 0x080c, 0x080d, 0x080e,
	0x080f, 0x0810, 0x0811, 0x0812, 0x0813, 0x0814, 0x0815, 0x0816, 0x0817, 0x0818,
	0x0819, 0x081a, 0x081b, 0x081c, 0x081d, 0x090a, 0x090b, 0x090c, 0x090d, 0x090e,
	0x090f, 0x0910, 0x0911, 0x0912, 0x0913, 0x0914, 0x0915, 0x0916, 0x0917, 0x0918,
	0x0919, 0x091a, 0x091b, 0x091c, 0x091d, 0x0a0b, 0x0a0c, 0x0a0d, 0x0a0e, 0x0a0f,
	0x0a10, 0x0a11, 0x0a12, 0x0a13, 0x0a14, 0x0a15, 0x0a16, 0x0a17, 0x0a18, 0x0a19,
	0x0a1a, 0x0a1b, 0x0a1c, 0x0a1d, 0x0b0c, 0x0b0d, 0x0b0e, 0x0b0f, 0x0b10, 0x0b11,
	0x0b12, 0x0b13, 0x0b14, 0x0b15, 0x0b16, 0x0b17, 0x0b18, 0x0b19, 0x0b1a, 0x0b1b,
	0x0b1c, 0x0b1d, 0x0c0d, 0x0c0e, 0x0c0f, 0x0c10, 0x0c11, 0x0c12, 0x0c13, 0x0c14,
	0x0c15, 0x0c16, 0x0c17, 0x0c18, 0x0c19, 0x0c1a, 0x0c1b, 0x0c1c, 0x0c1d, 0x0d0e,
	0x0d0f, 0x0d10, 0x0d11, 0x0d12, 0x0d13, 0x0d14, 0x0d15, 0x0d16, 0x0d17, 0x0d18,
	0x0d19, 0x0d1a, 0x0d1b, 0x0d1c, 0x0d1d, 0x0e0f, 0x0e10, 0x0e11, 0x0e12, 0x0e13,
	0x0e14, 0x0e15, 0x0e16, 0x0e17, 0x0e18, 0x0e19, 0x0e1a, 0x0e1b, 0x0e1c, 0x0e1d,
	0x0f10, 0x0f11, 0x0f12, 0x0f13, 0x0f14, 0x0f15, 0x0f16, 0x0f17, 0x0f18, 0x0f19,
	0x0f1a, 0x0f1b, 0x0f1c, 0x0f1d, 0x1011, 0x1012, 0x1013, 0x1014, 0x1015, 0x1016,
	0x1017, 0x1018, 0x1019, 0x101a, 0x101b, 0x101c, 0x101d, 0x1112, 0x1113, 0x1114,
	0x1115, 0x1116, 0x1117, 0x1118, 0x1119, 0x111a, 0x111b, 0x111c, 0x111d, 0x1213,
	0x1214, 0x1215, 0x1216, 0x1217, 0x1218, 0x1219, 0x121a, 0x121b, 0x121c, 0x121d,
	0x1314, 0x1315, 0x1316, 0x1317, 0x1318, 0x1319, 0x131a, 0x131b, 0x131c, 0x131d,
	0x1415, 0x1416, 0x1417, 0x1418, 0x1419, 0x141a, 0x141b, 0x141c, 0x141d, 0x1516,
	0x1517, 0x1518, 0x1519, 0x151a, 0x151b, 0x151c, 0x151d, 0x1617, 0x1618, 0x1619,
	0x161a, 0x161b, 0x161c, 0x161d, 0x1718, 0x1719, 0x171a, 0x171b, 0x171c, 0x171d,
	0x1819, 0x181a, 0x181b, 0x181c, 0x181d, 0x191a, 0x191b, 0x191c, 0x191d, 0x1a1b,
	0x1a1c, 0x1a1d, 0x1b1c, 0x1b1d, 0x1c1d
};

Dsp::Dsp() {
	id = 0;
	lspflag = true;
}

void Dsp::reset() {
	dspStart();
	dspMode=CELP_MODE;
}

void Dsp::write(int high,int low) {
	if (dspMode==PCM_MODE) {
		if (high==0xff) {
			dspMode=CELP_MODE;
		} else {
			writePcm((high<<8)|low);
		}
		return;
	}
	if (high<0x60) {
		int id=high>>4;
		if (id<3) {
			dspCelpOff=id;
			dspCelp[dspCelpOff++]=(high<<8)|low;
		} else if (id<6 && (dspCelpOff%3)+3==id) {
			dspCelp[dspCelpOff++]=(high<<8)|low;
			if (dspCelpOff==15) {
				//满1帧数据
				dspCelpToCelp();
			}
		}
	} else if (high==0xa0) {
		dspMode=low;
		printf("DSP_MODE %d\n",low);
	} else {
		printf("DSP_CMD %04X\n",(high<<8)|low);
	}
}

void Dsp::dspCelpToCelp() {
    if ((dspCelp[0] & 0x8000) != 0)
        clpBuf[0] = 0x20;
    else
        clpBuf[0] = 0x10;
    clpBuf[0] |= dspCelp[0] & 0xf; //Areg0 bit 0--3 = celp_tmp 0--4

    clpBuf[1] = (dspCelp[0] >> 4) & 0xf; //Areg1 bit 0--3 = dataL bit 4--7
    clpBuf[1] |= ((dspCelp[0] >> 8) & 7) << 4; //Areg1 bit 4--6 = dataH bit 0--2
    clpBuf[1] |= (dspCelp[1] & 1) << 7; //Areg1 bit 7 = celp_tmp bit 0

    clpBuf[2] = (dspCelp[1] >> 1) & 0x7f; //Areg2 bit 0--6 = dataL bit 1--7
    clpBuf[2] |= ((dspCelp[1] >> 8) & 1) << 7; //Areg2 bit 7 = celp_tmp 0

    clpBuf[3] = (dspCelp[1] >> 9) & 3; //Areg3 bit 0--1 = dataH bit 1--2
    clpBuf[3] |= (dspCelp[2] & 0x3f) << 2; //Areg3 bit 2--7 = celp_tmp bit 0--5

    clpBuf[4] = (dspCelp[2] >> 6) & 3; //Areg4 bit 0--1 = dataL bit 6--7
    clpBuf[4] |= ((dspCelp[2] >> 8) & 0xf) << 2; //Areg4 bit 2--5 = dataH bit 0--3
    clpBuf[4] |= (dspCelp[3] & 3) << 6; //Areg4 bit 6--7 = celp_tmp bit 0--1

    clpBuf[5] = (dspCelp[3] >> 2) & 0x3f; //Areg5 bit 0--5 = dataL bit 2--7
    clpBuf[5] |= (dspCelp[4] & 3) << 6; //Areg5 bit 6--7 = celp_tmp bit 0--1

    clpBuf[6] = (dspCelp[4] >> 2) & 0x3f; //Areg6 bit 0--5 = dataL bit 2--7
    clpBuf[6] |= ((dspCelp[4] >> 8) & 3) << 6; //Areg6 bit 6--7 = dataH 0--1

    clpBuf[7] = dspCelp[5] & 0xff; //Areg7 bit 0--7 = dataL bit 0--7

    clpBuf[8] = (dspCelp[5] >> 8) & 3; //Areg8 bit 0--1 = dataH bit 0--1
    clpBuf[8] |= (dspCelp[6] & 0x1f) << 2; //Areg8 bit 2--7 = celp_tmp bit 0--5
    clpBuf[8] |= (dspCelp[7] & 1) << 7;

    clpBuf[9] = (dspCelp[7] >> 1) & 0x7f; //Areg9 bit 0--6 = dataL bit 1--7
    clpBuf[9] |= (dspCelp[7] >> 1) & 0x80; //Areg9 bit 7 = celp_tmp bit 0

    clpBuf[10] = (dspCelp[7] >> 9) & 1; //Areg10 bit 0 = dataH bit 1
    clpBuf[10] |= (dspCelp[8] & 0x7f) << 1; //Areg10 bit 1--7 = celp_tmp 0--6

    clpBuf[11] = (dspCelp[8] >> 7) & 1; //Areg11 bit 0 = dataL bit 7
    clpBuf[11] |= ((dspCelp[8] >> 8) & 3) << 1; //Areg11 bit 1-2 = dataH bit 0--1
    clpBuf[11] |= (dspCelp[9] & 0x1f) << 3; //Areg11 bit 3--7 = celp_tmp 0--4

    clpBuf[12] = (dspCelp[9] >> 5) & 7; //Areg12 bit 0--2 = dataL bit 5--7
    clpBuf[12] |= (dspCelp[10] & 0x1f) << 3; //Areg12 bit 3--7 = celp_tmp 0--4

    clpBuf[13] = (dspCelp[10] >> 5) & 7; //Areg13 bit 0--2 = dataL bit 5--7
    clpBuf[13] |= ((dspCelp[10] >> 8) & 3) << 3; //Areg13 bit 3--4 = dataH bit 0--1
    clpBuf[13] |= (dspCelp[11] & 7) << 5; //Areg13 bit 5--7 = celp_tmp 0--2

    clpBuf[14] = (dspCelp[11] >> 3) & 0x1f; //Areg14 bit 0--4 = dataL bit 3--7
    clpBuf[14] |= ((dspCelp[11] >> 8) & 3) << 5; //Areg14 bit 5--6 = dataH 0--1
    clpBuf[14] |= (dspCelp[12] & 1) << 7; //Areg14 bit 7 = celp_tmp bit 0

    clpBuf[15] = (dspCelp[12] >> 1) & 0xf; //Areg15 bit 0--3 = dataL bit 1--4
    clpBuf[15] |= (dspCelp[13] & 0xf) << 4; //Areg15 bit 4--7 = celp_tmp bit 0--3

    clpBuf[16] = (dspCelp[13] >> 4) & 0xf; //Areg16 bit 0--3 = dataL bit 4--7
    clpBuf[16] |= ((dspCelp[13] >> 8) & 3) << 4; //Areg16 bit 4--5 = dataH 0--1
    clpBuf[16] |= (dspCelp[14] & 3) << 6; //Areg16 bit 6--7 = celp_tmp 0--1

    clpBuf[17] = (dspCelp[14] >> 2) & 0x1f; //Areg17 bit 0--5 = dataL 2--7
    clpBuf[17] |= ((dspCelp[14] >> 8) & 3) << 6; //Areg17 bit 6--7 = dataH bit 0--1

    dsp(clpBuf);
    int len = (dspCelp[0] & 0x8000) != 0 ? 160 : 240;
    for (int i = 0; i < len; i++) {
        writeSample8000(Sout[i]);
    }
}

void Dsp::writeSample8000(int val) {
    //通过重复5.5次，把8000Hz变成44000Hz
    ////soundStream.write(val);
    ////soundStream.write(val);
    ////soundStream.write(val);
    ////soundStream.write(val);
    ////soundStream.write(val);
    if ((id++ & 1) > 0){
        ////soundStream.write(val);
    }
}

void Dsp::writePcm(int val) {
	writeSample8000(val - 0x8000);
}

void Dsp::dspStart() {
	for (int i=0;i<148;i++)
		LTP[i] = 0;
	EX = 147 ;
	PM = 0 ;
}

void Dsp::subFrame(int subframe_NUM,int s0,int s1,int fixpos) {
    int op, Amp1, Amp2;
    int Expos, Pitpos;
    int base;

    fixpos = FIXCODEBOOK[fixpos];
    Inno.Pos[0] = ((fixpos >> 8) & 0xff) * 2 + Inno.Grid;
    Inno.Pos[1] = (fixpos & 0xff) * 2 + Inno.Grid;
    if (s0 != 0)
        Inno.Sign[0] = -1;
    else
        Inno.Sign[0] = 1;
    if (s1 != 0)
        Inno.Sign[1] = -1;
    else
        Inno.Sign[1] = 1;

    Amp1 = CGain[Inno.Gindex] * Inno.Sign[0];
    Amp2 = CGain[Inno.Gindex] * Inno.Sign[1];
    DecodePitch(Pitch.Lindex, subframe_NUM);

    InterpLSP(subframe_NUM);
    fixlsptopc(); //lsp Q6, lpc Q11

    base = subframe_NUM * _SubFrameSize;
    //printf("base %d\n",base);
    //getch();
    Pitpos = PM + Period - 1;
    if (Pitpos > 147)
        Pitpos -= 148;
    Expos = EX;
    pcount = Period - 1;
/* ===============  Synthesis Signal ============ */
    for (int i = 0; i < _SubFrameSize; i++, --pcount) {   // Synthesis signal
        op = SamplePitch(Pitpos, Frac, i);
        op = (PGain[Pitch.Gindex] * op) >> 14;
        // special case: pos1 is 3, pos2 is 29, and Period = 26.
        // if i = 29, then op = op + Amp1 + Amp2 .
        if (i == Inno.Pos[0]) {
            op += Amp1;
            Inno.Pos[0] += Period;
        }
        if (i == Inno.Pos[1]) {
            op += Amp2;
            Inno.Pos[1] += Period;
        }
        LTP[Expos] = op;
        r[i + base] = fixpolefilter(op);
        int t = r[i + base] * 16;
        Sout[i + base] = (short) (t > 32767 ? 32767 : (t < -32768 ? -32768 : t));

        if (Expos > 0)
            --Expos;
        else
            Expos = 147;
        if (Pitpos > 0)
            --Pitpos;
        else
            Pitpos = 147;

        if (i == 1 && subframe_NUM == 3) {
            for (int j = 0; j < ORDER; j++)
                oldindex[j] = newindex[j];
        }
    }
    ////// Update LTP //////
    EX -= _SubFrameSize;
    if (EX < 0)
        EX += 148;
    PM = EX + 1;
    if (PM > 147)
        PM -= 148;
}

void Dsp::dsp(byte celp[]) {
	int fixpos;
	
    if ((celp[0] & 0xF0) == 0x10)
        _SubFrameSize = 60;
    else
        _SubFrameSize = 40;

    delsp[0] = celp[0] & 0x7; //3位
    delsp[1] = (celp[0] & 0x08) >> 3; //4位
    delsp[1] += (celp[1] & 0x7) << 1;
    delsp[2] = (celp[1] & 0x78) >> 3; //4位

    delsp[3] = (celp[1] & 0x80) >> 7; //4位
    delsp[3] += (celp[2] & 0x7) << 1;
    delsp[4] = (celp[2] & 0x78) >> 3; //4位

    delsp[5] = (celp[2] & 0x80) >> 7; //3位
    delsp[5] += (celp[3] & 0x3) << 1;
    delsp[6] = (celp[3] & 0x1c) >> 2; //3位
    delsp[7] = (celp[3] & 0xe0) >> 5; //3位
    delsp[8] = (celp[4] & 0x7);      //3位
    delsp[9] = (celp[4] & 0x38) >> 3; //3位

    for (int i = 0; i < ORDER; i++)
        newindex[i] = LSPINDEX[delsp[i] + LSPsearchPtr[i]]; //Q6

    if (lspflag) { /* initial q_oldindex */
        for (int i = 0; i < ORDER; i++)
            oldindex[i] = newindex[i];
        lspflag = false;
    }

    Pitch.Lindex = (celp[4] & 0xc0) >> 6;
    Pitch.Lindex += (celp[5] & 0x3f) << 2;  //6
    fixpos = (celp[5] & 0xc0) >> 6;
    fixpos += (celp[6] & 0x7f) << 2;        //7
    Inno.Grid = (celp[6] & 0x80) >> 7;
    Pitch.Gindex = (celp[7] & 0xf);          //8
    Inno.Gindex = (celp[7] & 0xf0) >> 4;
    subFrame(0, celp[7] & 0x2, celp[7] & 0x1, fixpos);

    Pitch.Lindex = (celp[8] & 0x7c) >> 2;   //9
    fixpos = (celp[8] & 0x80) >> 7;
    fixpos += celp[9] << 1;           //10
    Inno.Grid = (celp[10] & 0x1);           //11
    Pitch.Gindex = (celp[10] & 0x1e) >> 1;
    Inno.Gindex = (celp[10] & 0xe0) >> 5;
    Inno.Gindex += (celp[11] & 0x1) << 3;    //12
    subFrame(1, celp[11] & 0x4, celp[11] & 0x2, fixpos);

    Pitch.Lindex = (celp[11] & 0xf8) >> 3; //12
    Pitch.Lindex += (celp[12] & 0x7) << 5;   //13
    fixpos = (celp[12] & 0xf8) >> 3;
    fixpos += (celp[13] & 0xf) << 5;         //14
    Inno.Grid = (celp[13] & 0x10) >> 4;
    Pitch.Gindex = (celp[13] & 0xe0) >> 5;
    Pitch.Gindex += (celp[14] & 0x1) << 3;   //15
    Inno.Gindex = (celp[14] & 0x1e) >> 1;
    subFrame(2, celp[14] & 0x40, celp[14] & 0x20, fixpos);

    Pitch.Lindex = (celp[14] & 0x80) >> 7; //15
    Pitch.Lindex += (celp[15] & 0xf) << 1;   //16
    fixpos = (celp[15] & 0xf0) >> 4;
    fixpos += (celp[16] & 0x1f) << 4;        //17
    Inno.Grid = (celp[16] & 0x20) >> 5;
    Pitch.Gindex = (celp[16] & 0xc0) >> 6;
    Pitch.Gindex += (celp[17] & 0x3) << 2;   //18
    Inno.Gindex = (celp[17] & 0x3c) >> 2;
    subFrame(3, celp[17] & 0x80, celp[17] & 0x40, fixpos);
}

int Dsp::SamplePitch(int Pos,int F,int s) {
    if (F == 0) { // f = 0 is sampling 1
        if (pcount < 0) {
            recursive[s] = recursive[-pcount - 1];
        } else {
            recursive[s] = LTP[Pos];
        }
    } else {   // f = 1 is sampling 1/3 and f = 2 is sampling 2/3
        int fpos = Pos + 4;
        if (fpos > 147)
            fpos -= 148;
        int fcount = pcount + 5;
        short Acc = 0;
        for (int i = 0; i < 8; i++) {
            --fcount;
            if (fcount < 0)  // check fcount < 0
                //1.15*16.0 -> 16.16
                Acc += (recursive[-fcount - 1] * wsinc[(i << 1) + F - 1]) >> 15;
            else {
                Acc += (LTP[fpos] * wsinc[(i << 1) + F - 1]) >> 15;
                if (fpos > 0)
                    --fpos;
                else
                    fpos += 147;
            }
        }
        recursive[s] = Acc;
    }
    return recursive[s];
}

void Dsp::DecodePitch(int P,int k) {
    if ((k & 1) == 0)
        PreP = P;
    else
        P += PreP - 16;

    if (P <= 191) {
        Period = P / 3 + 13;
        Frac = P % 3;
    } else {
        Period = P - 115;
        Frac = 0;
    }
}

void Dsp::InterpLSP(int k) {
    for (int j = 0; j < ORDER; j++) {
        // lsp[j] = InterWeight[0][k]*oldindex[j] + InterWeight[1][k]*newindex[j];
        //InterWeight Q15,oldindex Q8
        int mul = InterWeight[k] * oldindex[j] + InterWeight[k + 4] * newindex[j];
        int index = mul >> 10;
        int frac = mul & 0x3ff;
        lsp[j] = COSTABLE[index] + (((COSTABLE[index + 1] - COSTABLE[index]) * frac) >> 10);
    }
}

void Dsp::fixlsptopc() {
	int rx1, sx1, rx2, sx2, rx3, sx3, rx4, sx4;

    for (int i = 0; i < 5; i++) {
        p[i] = lsp[2 * i];
        q[i] = lsp[2 * i + 1];
    }
    root_rs(p, RX); // input: p's format is Q13, output: RX's format is Q11
    root_rs(q, SX); // input: q's format is Q13, output: SX's format is Q11

    //Use <AccB> to calculate the lpc coefficient
    //pc[0] = 1. ;
    pc[0] = 0x0800;
    pc[1] = (RX[0] + SX[0]) >> 1;
    rx1 = 0x2800 + RX[0] + RX[1];
    rx2 = rx1 + RX[2] + RX[0] * 3;
    rx3 = rx2 + 0x2800 + RX[3] + (RX[1] << 1);
    rx4 = rx3 + RX[4] + RX[2] + (RX[0] << 1);
    sx1 = 0x2800 + SX[1] - SX[0];
    sx2 = SX[2] + SX[0] * 3 - sx1;
    sx3 = SX[3] + 0x2800 + (SX[1] << 1) - sx2;
    sx4 = SX[4] + SX[2] + (SX[0] << 1) - sx3;

    pc[2] = (rx1 + sx1) >> 1;
    pc[3] = (rx2 + sx2) >> 1;
    pc[4] = (rx3 + sx3) >> 1;
    pc[5] = (rx4 + sx4) >> 1;
    pc[6] = (rx4 - sx4) >> 1;
    pc[7] = (rx3 - sx3) >> 1;
    pc[8] = (rx2 - sx2) >> 1;
    pc[9] = (rx1 - sx1) >> 1;
    //pc[10] = (RX[0]-SX[0]+2.)/2. ;
    pc[10] = (RX[0] - SX[0] + 0x1000) >> 1;

    for (int i = 0; i <= 10; i++)
        decoder.ai[i] = pc[i];
}

void Dsp::root_rs(int HXrootInPo[], int HXpcoef[]) {
    int Alu, Blu, a1, a2, temp1, temp2;

    Alu = 0;
    for (int i = 0; i < 5; i++)
        Alu -= HXrootInPo[i]; //Q13
    HXpcoef[0] = rounding(Alu, 2); //Q13->Q11

    Alu = 0;
    Blu = 0;
    for (int i = 0; i < 4; i++) {
        for (int j = i + 1; j < 5; j++) {
            Alu += (HXrootInPo[i] * HXrootInPo[j]) << 1; //Q27
            Blu += (HXrootInPo[i] * HXrootInPo[j]) >> 15; //Q27
        }
    }
    HXpcoef[1] = Blu + 5;

    for (int i = 0; i < 5; i++) {
        tr[i] = HXrootInPo[i] << 1;
    }
    Alu = 0;
    Blu = 0;
    for (int i = 0; i < 3; i++) {
        for (int j = i + 1; j < 4; j++) {
            for (int k = j + 1; k < 5; k++) {
                temp1 = (HXrootInPo[i] * HXrootInPo[j]) << 1; //Q27
                Alu -= (HXrootInPo[k] * rounding(temp1, 14)) << 1; //Q27
                a1 = (tr[i] * tr[j]) >> 15;
                Blu -= (a1 * HXrootInPo[k]) >> 15;
            }
        }
    }
    HXpcoef[2] = Blu;

    Alu = 0;
    Blu = 0;
    for (int i = 0; i < 2; i++)
        for (int j = i + 1; j < 3; j++)
            for (int k = j + 1; k < 4; k++)
                for (int l = k + 1; l < 5; l++) {
                    temp1 = (HXrootInPo[i] * HXrootInPo[j]) << 1;//Q27
                    temp2 = (HXrootInPo[k] * HXrootInPo[l]) << 1;//Q27
                    Alu += (rounding(temp1, 14) * rounding(temp2, 14)) << 1;//Q27
                    a1 = (tr[i] * tr[j]) >> 15;
                    a2 = (tr[k] * tr[l]) >> 15;
                    Blu += (a1 * a2) >> 15;
                }
    HXpcoef[3] = Blu;

    a1 = (tr[0] * tr[1]) >> 15;
    a2 = (tr[3] * tr[4]) >> 15;
    a1 = (a1 * a2) >> 15;
    Blu = (a1 * tr[2]) >> 15;
    Blu = Blu << 1;
    HXpcoef[4] = -Blu;
}

int Dsp::fixpolefilter(int S) {
    int Sum = 0;
    for (int i = 10; i > 0; i--) {
        Sum += decoder.D[i] * decoder.ai[i];
        decoder.D[i] = decoder.D[i - 1];
    }
    decoder.D[1] = S - rounding(Sum, 11);
    return decoder.D[1];
}

//mul截掉末尾P位并四舍五入
int Dsp::rounding(int mul,int P)
{
    return (mul + (1 << (P - 1))) >> P;
}

