
#include "comm.h"
#include "state.h"
#include <cassert>
#include <cstdio>

extern nc1020_states_t nc1020_states;
static uint8_t* ram_buff = nc1020_states.ram;
static uint8_t* ram_io = ram_buff;

static uint64_t last_tick=0;
static deque<uint8_t> nand_cmd;
static int nand_read_cnt=0;
//char nand_ori[65536][528];
char nand[65536+64][528];
//char nand_spare[65536+64][16];

void read_nand_file(){
    memset(nand,0xff,sizeof(nand));
    char *p0= &nand[64][0];
    FILE *f = fopen("./nand.bin", "rb");
    if(f==0) {
        printf("file ./nand.bin not exist!\n");
        exit(-1);
    }
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);  /* same as rewind(f); */
    fread(p0, fsize, 1, f);
    fclose(f);
    printf("<nand_file_size=%lu>\n",fsize);

    //the value inside the real 0 nand 32k page. But it doesn't really matter
    memcpy(&nand[0][0]+0x200+0x10 /*512+16=258*/,"ggv nc2000",strlen("ggv nc2000"));
}

void write_nand_file(){
    FILE *f = fopen("./nand.bin", "wb");
    fwrite(&nand[0][0]+ 64*528, sizeof(nand)-64*528, 1 , f);
    fclose(f);
}

uint8_t read_nand(){
    //printf("tick=%lld, read %x  %02x\n",tick, addr, ram_io[addr]);
    uint8_t roa_bbs=ram_io[0x0a];
    uint8_t ramb_vol=ram_io[0x0d];
    uint8_t bs=ram_io[0x00];
   ///////// uint16_t p=nc1020_states.cpu.reg_pc-4;

     if(enable_debug_nand) printf("tick=%llu read $29\n",tick%10000);

    if(nand_cmd.size()==0) {
        printf("oops! no nand cmd\n");
        return 0xff;
    }
    assert(nand_cmd.size()>0);

    /*
        special handle of read status after a long time
    */
    if(nand_cmd[0]==0x70 && nand_cmd.size()==1) {
        nand_cmd.clear();
        nand_read_cnt=0;
        return 0x40;
    }

    /*
        robust check
    */
    if(nand_cmd[0]!=0x0 && nand_cmd[0]!=0x1 &&nand_cmd[0]!=0x60 &&nand_cmd[0]!=0x50){
        printf("<<%x>>!!!\n",(unsigned char)nand_cmd[0]);
        for(int i=0;i<nand_cmd.size();i++){
            printf("<%x>",(unsigned char)nand_cmd[i]);
        }
        printf("\n");
        assert(false);
        return 0;
    }

    
    /*
        read low/high and read spare
    */
    unsigned char cmd=nand_cmd[0];
    if(cmd ==0 ||cmd==1||cmd==0x50){
        if(nand_cmd.size()!=5 ){
            printf("oops cmd size!=5\n");
            for(int i=0;i<nand_cmd.size();i++){
                printf("<%x>",(unsigned char)nand_cmd[i]);
            }
            printf("\n");

            /*if(nand_cmd.size()==1 && nand_cmd[0]==0x0){
                printf("oops!! nand_cmd=[0] but trying to read\n");
                return 0xff;
            }*/
            assert(false);
            return 0xff;
        }

        unsigned char low=nand_cmd[1];
        unsigned char mid=nand_cmd[2];
        unsigned char high=nand_cmd[3];

        uint32_t pos=high*256u+mid;

        /*
        if(nand_cmd.size()==5&&false){
            printf("[%x %x]",low,high);
            
            for(int i=0;i<nand_cmd.size();i++){
                printf("<%x>",(unsigned char)nand_cmd[i]);
            }
            printf("\n");
            exit(-1);
            //printf("<%x;%x,%x:%x,%d>", final, pos, low,cmd,nand_read_cnt);
        }*/
        
        unsigned int x=pos;
        unsigned int y=low;
        //unsigned int pre_final= pos*528u+ y +nand_read_cnt;
        //assert(pre_final%528==0);
        if(cmd==0x1) y+=256u;
        if(cmd==0x50) y+=512u;
        unsigned int final= pos*528u+ y +nand_read_cnt;
        if(nand_read_cnt!=0||cmd!=0){
            assert(final%528!=0);
        }


        //final-=32*1024;
        if(nand_read_cnt==0 && enable_debug_nand){
            printf("[%x %x]",low,high);
            
            for(int i=0;i<nand_cmd.size();i++){
                printf("<%x>",(unsigned char)nand_cmd[i]);
            }
            printf("<%x;%x,%x:%x,%d>\n", final, pos, low,cmd,nand_read_cnt);
        }
        char *p=&nand[0][0];
        uint8_t result=p[final];
        //if(final<0) return 0x00;
        //uint8_t result=nand[pos][low+off+nand_read_cnt];
        nand_read_cnt++;
        //printf("<<%02x>>",result);
        return result;
    }

    /*
        erase
    */
    //printf("bs=%x roa_bbs=%x pc=%x  %x %x %x %x \n",ram_io[0x00], ram_io[0x0a], reg_pc,  Peek16(p), Peek16(p+1),Peek16(p+2),Peek16(p+3));
    if(cmd==0x60){
        unsigned char low=nand_cmd[1];
        unsigned char mid=nand_cmd[2];
        unsigned char high=nand_cmd[3];

        //assert(false);
        //return 0x40;
        //if(xxx==1) return 0x40;
        unsigned int final= (mid*256u+low)*528u;
        /*
        if(nand_read_cnt%100==0){
            printf("<cmd60> %d\n",nand_read_cnt);
            for(int i=0;i<nand_cmd.size();i++){
                printf("<%x>",(unsigned char)nand_cmd[i]);
            }
        }*/

        nand_read_cnt++;
        char *p=&nand[0][0];
        //printf("final=%x\n",final);
        //printf("erase!!! %x tick=%lld pc=%x %x\n",final,tick,nc1020_states.cpu.reg_pc, ram_io[0x00]);
        //assert(false);

        //final-=32*1024;
        assert(final%(32*528)==0);
        memset(p+final,0xff,32*528);
        nand_cmd.clear();
        nand_read_cnt=0;
        return 0x40;
    }

    assert(false);
}

void nand_write(uint8_t value){
    if(enable_debug_nand) printf("tick=%llu write $29 %x\n",tick%10000,value);
    //printf("tick=%lld, write %x  %02x\n",tick, addr, value);
    uint8_t roa_bbs=ram_io[0x0a];
    uint8_t ramb_vol=ram_io[0x0d];
    uint8_t bs=ram_io[0x00];
   ///////////////// uint16_t p=nc1020_states.cpu.reg_pc-4;

    {
        if(nand_cmd.size()==0 && value==0xff) {
            nand_read_cnt=0;
            goto out;
        }

        /*
            ideally we should emulate the CLE ALE CE WE RE etc accodring to the datasheet
            but the blow code works pretty stable on official firmware
            so I am delaying this and going to implement other important features first
        */
        if(nand_cmd.size()>0 && tick-last_tick>=40) {    // some magic timeout,  which should be removed after the 018h IO is emulated
            if(nand_read_cnt!=0&& enable_debug_nand) {
                printf("<nand reads %d>\n",nand_read_cnt);
            }
            if(enable_debug_nand)
            {
                for(int i=0;i<nand_cmd.size();i++){
                    printf("<%02x>",(unsigned char)nand_cmd[i]);
                }
                printf("\n");
            }

            // robust check: what we dumped is indeed as expected
            //if(nand_cmd.size()==7) assert(nand_cmd[6]==0xff);
            //else 
           /* special handle of some invalid sequence in 空间整理*/
           //if(nand_cmd.size()==23) assert(nand_cmd[0]==0x50 && nand_cmd.back()==0x10);
           //else 
           
           if(nand_cmd.size()==5) assert(nand_cmd[0]==0x00||nand_cmd[0]==0x01||nand_cmd[0]==0x50);
           else {
                assert(false);
           }
            nand_cmd.clear();
            nand_read_cnt=0;
            if(value==0xff) goto out;
        }
        last_tick=tick;
        nand_cmd.push_back(value);

        if(nand_cmd.size()>6) {
            if(nand_cmd[0]==0x00) {
                assert(nand_cmd[1]==0x80); 
                assert(nand_cmd.size()<=535);
            }
            else if(nand_cmd[0]==0x50) {
                assert(nand_cmd[1]==0x80);
                assert(nand_cmd.size()<=23);
            }
            else assert(false);

            if(nand_cmd[0]!=0x00&& nand_cmd.size()==23) {
                assert(nand_cmd[nand_cmd.size()-1]==0x10);
                assert(nand_cmd[1]==0x80);
                assert(nand_cmd[0]==0x50);
                
                unsigned char low=nand_cmd[2];
                unsigned char mid=nand_cmd[3];
                unsigned char high=nand_cmd[4];

                uint32_t pos=high*256u+mid;

                unsigned int x=pos;
                unsigned int y=low;
                unsigned int final= pos*528u+ y +512;

                assert((final-512)%(528)==0);

                char *p=&nand[0][0];
                for(int i=0;i<16;i++){
                    p[final+i]=nand_cmd[6+i];
                }
                printf("program spare!!!! final=%x\n",final);

                nand_cmd.clear();
                nand_read_cnt=0;
            }
            if(nand_cmd.size()==535){
                assert(nand_cmd[nand_cmd.size()-1]==0x10);
                assert(nand_cmd[0]==0 && nand_cmd[1]==0x80);
                unsigned char low=nand_cmd[2];
                unsigned char mid=nand_cmd[3];
                unsigned char high=nand_cmd[4];

                uint32_t pos=high*256u+mid;

                unsigned int x=pos;
                unsigned int y=low;
                unsigned int final= pos*528u+ y;
                assert(final%(528)==0);
                char *p=&nand[0][0];
                printf("program!!!! final=%x\n",final);

                for(int i=0;i<528;i++){
                    p[final+i]=nand_cmd[6+i];
                }
                nand_cmd.clear();
                nand_read_cnt=0;
            }
        }
    }
    out:;

    //if(nand_cmd.size()==1&& nand_cmd[0]==0xff) nand_cmd.clear();
    //printf("bs=%x roa_bbs=%x pc=%x  %x %x %x %x \n",ram_io[0x00], ram_io[0x0a], reg_pc,  Peek16(p), Peek16(p+1),Peek16(p+2),Peek16(p+3));
    //if(do_inject) wanna_inject=true;
}
