#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "keytab.h"
#include "convert.h"
#include <vector>

//this file is adapted from ftplink, original author: lee

static char passwd[256];
static char randm_pwd[256];

int get_value(int ch,int cv)
{
	int	i;
	ch&=0xff;
	cv&=0xff;

	for (i=0;i<256;i++) {
		if (keytab[ch][i]==cv)
			break;
	}
	return i;
}

int bin_dec(string name,string outname)
{
	FILE *fpin,*fpout;
	int i,j;
	int jm_len,addr,addr2,chr,value;

	//打开文件
	memset(passwd,0xff,sizeof(passwd));
	if ((fpin=fopen(name.c_str(),"rb"))==NULL) {
		printf("Failed to open input file: %s\n", name.c_str());
		return -1;
	}
	jm_len=fgetc(fpin);
	if(jm_len==EOF){
		printf("file %s is empty\n",name.c_str());
		return -1;
	}
	for (i=0;i<jm_len;i++)
		randm_pwd[i]=(char)fgetc(fpin);
	addr=fgetc(fpin);
	addr+=fgetc(fpin)*256+78;
	addr2=addr+jm_len+3;
	fseek (fpin,addr2,SEEK_SET);
	for (i=0;i<jm_len;i++) {
		chr=fgetc(fpin)^randm_pwd[i];
		passwd[i]=(char)chr;
	}

	if ((fpout=fopen(outname.c_str(),"wb"))==NULL) {
		printf("Failed to open output file: %s\n", outname.c_str());
		fclose(fpin);
		return -1;
	}

	//加密区
	fseek(fpin,jm_len+3,SEEK_SET);
	for (j=0,i=0;j<addr;j++,i++)
	{
		if (i==jm_len) i=0;
		value=(unsigned)passwd[i];
		fread(randm_pwd,1,1,fpin);
		chr=randm_pwd[0];
		randm_pwd[0]=(unsigned)get_value(value,chr);
		if (j>=78) fwrite(randm_pwd,1,1,fpout);
	}


	//保留区
	for (;j<addr+jm_len;j++)
	{
		fread(randm_pwd,1,1,fpin);
		fwrite(randm_pwd,1,1,fpout);
	}

	//加密区

	fread(randm_pwd,1,1,fpin);
	chr=randm_pwd[0];

	for (;!feof(fpin);j++,i++)
	{
		if (i==jm_len) i=0;
		value=(unsigned)passwd[i];

		randm_pwd[0]=(unsigned)get_value(value,chr);
		fwrite(randm_pwd,1,1,fpout);

		fread(randm_pwd,1,1,fpin);
		chr=randm_pwd[0];
	}

	fclose(fpin);
	fclose(fpout);
	return 0;
}

static char txt_head[24]={0,0,0,0,0,1,0,24,0,0,0,0,0,0,0,0,24,0,0,0,0,0,0,0};

inline void write_uint32_le(char* p, uint32_t val) {
    auto* u = reinterpret_cast<uint8_t*>(p);
    u[0] = static_cast<uint8_t>(val);
    u[1] = static_cast<uint8_t>(val >> 8);
    u[2] = static_cast<uint8_t>(val >> 16);
    u[3] = static_cast<uint8_t>(val >> 24);
}

int add_txt_head(string name,string outname)
{
	FILE *fpin,*fpout;
	int filelen;
	std::vector<char> buf;
	char *p;
	if ((fpin=fopen(name.c_str(),"rb"))==NULL) {
		printf("Failed to open input file: %s\n", name.c_str());
		return -1;
	}
	fseek(fpin,0,SEEK_END);
	filelen=ftell(fpin);
	fseek(fpin,0,SEEK_SET);
	buf.resize(filelen+24);
	p=&buf[0];
	fread(p+24,1,filelen,fpin);
	fclose(fpin);
	memcpy(p,txt_head,24);
	write_uint32_le(p+10, filelen+24);
	write_uint32_le(p+19, filelen+24);

	if ((fpout=fopen(outname.c_str(),"wb"))==NULL) {
		printf("Failed to open output file: %s\n", outname.c_str());
		return -1;
	}

	fwrite(p,1,filelen+24,fpout);
	fclose(fpout);
	return 0;
}
