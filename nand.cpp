
#include <SDL.h>
#include "comm.h"
#include "state.h"
#include <cassert>
#include <cstdio>
#include "nand.h"

extern WqxRom nc2k_rom;
extern nc2k_states_t nc2k_states;
static uint8_t* ram_buff = nc2k_states.ram;
static uint8_t* ram_io = nc2k_states.ram_io;

const int MAX_NAND_ADDR_SIZE = 4+10;
const int MAX_NAND_DATA_SIZE = 528+100;

static char nand[65536*2][528];
char * nand_magic= &nand[0][0]+0x210;

void read_nand0_file(){
    memset(nand,0xff, 64*528);
    char *p0= &nand[0][0];
    FILE *f = fopen(nc2k_rom.nand0Path.c_str(), "rb");
    if(f==0) {
        printf("file %s not exist!\n",nc2k_rom.nand0Path.c_str());
        exit(-1);
    }
    fseek(f, 0, SEEK_END);
    long long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);  /* same as rewind(f); */
    printf("<nand0_file_size=%llu>\n",fsize);
    assert(fsize<= 64*528);
    fread(p0, fsize, 1, f);
    fclose(f);

    char tmp_nand_magic[11];
    for(int i=0;i<sizeof(tmp_nand_magic)-1;i++){
        tmp_nand_magic[i]=p0[0x200+0x10+i];
    }
    tmp_nand_magic[sizeof(tmp_nand_magic)-1]=0;
    printf("nand magic: %s\n",tmp_nand_magic);
}

void read_nand_file(){
    char *p0= &nand[64][0];
    memset(p0,0xff,sizeof(nand)-64*528);
    FILE *f = fopen(nc2k_rom.nandFlashPath.c_str(), "rb");
    if(f==0) {
        printf("file %s not exist!\n",nc2k_rom.nandFlashPath.c_str());
        exit(-1);
    }
    fseek(f, 0, SEEK_END);
    long long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);  /* same as rewind(f); */
    printf("<nand_file_size=%llu>\n",fsize);
    long long expected_size=num_nand_pages*528-64*528;
    if(fsize<expected_size){
        printf("%s expected_size=%lld, actual_size=%lld; pad with 0xFF\n",nc2k_rom.nandFlashPath.c_str(),expected_size,fsize);
    }
    if(fsize>expected_size){
        printf("%s expected_size=%lld, actual_size=%lld; ignore tailing bytes\n",nc2k_rom.nandFlashPath.c_str(),expected_size,fsize);
        fsize=expected_size;
    }
    assert(fsize + 64*528 <= (int)sizeof(nand));
    fread(p0, fsize, 1, f);
    fclose(f);

#if 0
    if(nc2000mode){
        if(!nc2000_use_2600_rom){
            //if it's not 2600 rom, then it should always be this
            memcpy(&nand[0][0]+0x200+0x10 /*512+16=528*/,"ggv nc2000",strlen("ggv nc2000"));
        }
        else{
            if(!nc2600_rom_use_ggvsim){
                //2600 physical nor expect this to be "ggv nc2010"
                //nc2kutil dump shows there is a '\n' or 0x0A. But this doesn't matter. It boots either with or without `\n`
                memcpy(&nand[0][0]+0x200+0x10 /*512+16=528*/,"ggv nc2010\n",strlen("ggv nc2010\n"));
            }else{
                memcpy(&nand[0][0]+0x200+0x10 /*512+16=528*/,"ggv nc2000",strlen("ggv nc2000"));
            }
        }
    }
    if(nc3000mode){
        memcpy(&nand[0][0]+0x200+0x10 /*512+16=528*/,"ggv nc3000",strlen("ggv nc3000"));
    }
#endif

}

void write_nand0_file(string file){
    if(!nc2000mode &&!nc3000mode) return;
    if(file.empty()) file=nc2k_rom.nand0Path;
    else file+=".nand0";
     FILE *f = fopen(file.c_str(), "wb");
    fwrite(&nand[0][0], 64*528 , 1 , f);
    fclose(f);
}

void write_nand_file(string file){
    if(!nc2000mode &&!nc3000mode) return;
    if(file.empty()) file=nc2k_rom.nandFlashPath;
    else file+=".nand";
    FILE *f = fopen(file.c_str(), "wb");
    assert(num_nand_pages*528<= sizeof(nand));
    fwrite(&nand[0][0]+ 64*528, num_nand_pages*528- 64*528 , 1 , f);
    fclose(f);
}

#define NLOG_R(fmt, ...) \
    do { \
        const uint32_t interval_ms = 3000; \
        static uint64_t last_log_ms = 0; \
        static uint64_t total_count = 0; \
        static uint64_t suppressed_count = 0; \
        uint64_t now = SDL_GetTicks64(); \
        total_count++; \
        if (total_count <= 5 || (now - last_log_ms >= (uint64_t)(interval_ms)) ) { \
            printf(fmt "  #%llu", \
                   ##__VA_ARGS__, total_count); \
            if(suppressed_count>0) printf(" (suppressed %llu)\n", suppressed_count); \
            else printf("\n"); \
            last_log_ms = now; \
            suppressed_count = 0; \
        } else { \
            suppressed_count++; \
        } \
    } while (0)

enum NAND_POINTER_LATCH{
    AREA1=0,
    AREA2=256u,
    AREA3=512u
};

enum NAND_CMD_LATCH{
    WAIT_FOR_NEXT=0,
    READ1_2=0x01,
    ERASE=0x60,
    READ_STATUS=0x70,
    PROGRAM=0x80,
    READ_ID=0x90
};

static deque<uint8_t> nand_addr;
static deque<uint8_t> nand_data;
static uint32_t nand_read_offset=0;
static NAND_POINTER_LATCH nand_pointer_latch = AREA1;
static NAND_CMD_LATCH nand_cmd_latch = READ1_2;
void clear_nand_status(){
    nand_addr.clear();
    nand_data.clear();
    nand_read_offset = 0;
    
    nand_pointer_latch = NAND_POINTER_LATCH::AREA1;
    nand_cmd_latch = NAND_CMD_LATCH::WAIT_FOR_NEXT; 
}

static uint8_t & nand_peek(int off){
    static uint8_t dummy;
    if(off<0 || off>=num_nand_pages*528){
        dummy=0xff;
        NLOG_R("[nand] oops, access out of bound %d",off);
        return dummy;
    }
    uint8_t *p=(uint8_t*)&nand[0][0];
    return p[off];
}

#define nand_read_assert(expr)  \
    do {  \
        if(enable_assert_for_wqx_software) assert(expr); \
        if(!(expr)) { \
            printf("unexpected nand read %s:%d\n",__FILE__, __LINE__); \
            clear_nand_status(); \
            return 0xff; \
        } \
    } while(0)

uint8_t read_nand(){
    bool CLE;
    bool ALE;
    bool CEB;
    assert(nc3000mode || nc2000mode);

    if(nc3000mode){
        CLE = ram_io[0x18]&0x20;
        ALE = ram_io[0x18]&0x10;
        CEB = ram_io[0x18]&0x04;
        if(CEB) {
            NLOG_R("[nand] read while no CE");
            //return 0xff;
        }
    }
    if(nc2000mode){
        CLE = ram_io[0x18]&0x01;
        ALE = ram_io[0x18]&0x02;
        CEB = ram_io[0x18]&0x40;
        if(CEB) {
            NLOG_R("[nand] read while no CE");
            //return 0xff;
        }
    }
    if(CLE){
        NLOG_R("[nand] read while CLE is true!");
    }
    if(ALE){
        NLOG_R("[nand] read while ALE is true!");
    }

    uint8_t roa_bbs=ram_io[0x0a];
    uint8_t ramb_vol=ram_io[0x0d];
    uint8_t bs=ram_io[0x00];
    if(enable_debug_nand) printf("tick=%llu read nand\n",tick%10000);
    
    assert(nand_pointer_latch == NAND_POINTER_LATCH::AREA1 || nand_pointer_latch == NAND_POINTER_LATCH::AREA2 || nand_pointer_latch == NAND_POINTER_LATCH::AREA3);

    if(nand_cmd_latch==NAND_CMD_LATCH::WAIT_FOR_NEXT) {
        NLOG_R("[nand] no nand_cmd while read, CLE=%d ALE=%d CEB=%d",CLE,ALE,CEB);
        return 0xff;
    }

    if(nand_cmd_latch==NAND_CMD_LATCH::READ_STATUS) {
        if(nand_addr.size()>0) NLOG_R("[nand] unexpected nand_addr size %d in READ_STATUS", (int)nand_addr.size());
        if(nand_data.size()>0) NLOG_R("[nand] unexpected nand_data size %d in READ_STATUS", (int)nand_data.size());
        return 0xc0;
    }

    if(nand_cmd_latch==NAND_CMD_LATCH::READ_ID) {
        nand_read_assert(nand_addr.size()>=1 && nand_addr[0]==0x00);
        if(nand_addr.size()>1) NLOG_R("[nand] unexpected nand_addr size %d in READ_ID", (int)nand_addr.size());
        if(nand_data.size()>0) NLOG_R("[nand] unexpected nand_data size %d in READ_ID", (int)nand_data.size());
        if(nand_read_offset>=2) NLOG_R("[nand] unexpected nand_read_offset %d in READ_ID", (int)nand_read_offset);
        char id[2];
        if(nc2000mode) {id[0] = 0xec;id[1] = 0x75;}
        else if(nc3000mode) {id[0] = 0x98;id[1] = 0x76;}
        else assert(false);
        return id[(nand_read_offset++)%2];
    }

    if(nand_cmd_latch==NAND_CMD_LATCH::READ1_2){
        nand_read_assert(nand_addr.size()>=3);
        
        if(nand_addr.size()>4) NLOG_R("[nand] unexpected nand_addr size %d in READ1/READ2", (int)nand_addr.size());
        if(nand_data.size()>0) NLOG_R("[nand] unexpected nand_data size %d in READ1/READ2", (int)nand_data.size());

        bool first_read = (nand_read_offset == 0);
        //printf("nand read a0_7=%02x a9_16=%02x a17_24=%02x a25=%02x\n", a0_7, a9_16, a17_24, a25);
        if(first_read) {//only first read
            unsigned char a0_7=nand_addr[0];
            unsigned char a9_16=nand_addr[1];
            unsigned char a17_24=nand_addr[2];
            unsigned char a25=nand_addr.size()>3? nand_addr[3]&0x01:0;

            if(first_read && enable_debug_nand) {
                printf("cmd=%x,pointer=%d",nand_cmd_latch,nand_pointer_latch);
                printf("<%x %x %x %x %x>\n", nand_read_offset, a0_7, a9_16, a17_24, a25);
            }

            uint32_t inside_page_offset = a0_7 + (uint32_t)nand_pointer_latch;
            nand_read_assert(inside_page_offset < 528);
            
            nand_read_offset=a25*256u*256u*528u+a17_24*256u*528u+a9_16*528u+inside_page_offset;

            if(nand_pointer_latch == NAND_POINTER_LATCH::AREA2) {
                nand_pointer_latch = NAND_POINTER_LATCH::AREA1;
            }
        }else{
            if(nand_pointer_latch==NAND_POINTER_LATCH::AREA2) NLOG_R("[nand] unexpected pointer_latch=AREA2 in non-first read");
        }

        if(!first_read && nand_read_offset%528==0) {
            if(nand_pointer_latch==NAND_POINTER_LATCH::AREA3) {
                NLOG_R("[nand] read offset=%d across 528 boundary, while pointer_latch=%d", (int)nand_read_offset, (int)nand_pointer_latch);
            }else if(debug_level>=1) {
                NLOG_R("[nand] read offset=%d across 528 boundary, while pointer_latch=%d", (int)nand_read_offset, (int)nand_pointer_latch);
            }
        }

        if(nand_pointer_latch==NAND_POINTER_LATCH::AREA3){
            if(nand_read_offset%528 < 512){
                NLOG_R("[nand] unexpected read offset %d with AREA3", (int)nand_read_offset);
            }
        } 

        nand_read_assert(nand_read_offset < num_nand_pages*528);
        uint8_t result=nand_peek(nand_read_offset);

        nand_read_offset++;
        if(nand_read_offset % 528 == 0) {
            if(nand_pointer_latch == NAND_POINTER_LATCH::AREA3) {
                nand_read_offset+=512;
            }
        }

        //printf("nand read result=%02x nand_read_offset=%u\n", result, nand_read_offset);
        return result;
    }

    printf("[nand] unknown nand cmd %x in read, pointer_latch=%d, addr=", nand_cmd_latch, nand_pointer_latch);
    for(int i=0;i<nand_addr.size();i++) printf("%02x ", nand_addr[i]);
    printf(", data_size=%d\n", (int)nand_data.size());
    nand_read_assert(false);
}

#define nand_write_assert(expr)  \
    do {  \
        if(enable_assert_for_wqx_software) assert(expr); \
        if(!(expr)) { \
            printf("unexpected nand write %s:%d\n",__FILE__, __LINE__); \
            clear_nand_status(); \
            goto out; \
        } \
    } while(0)

void nand_write(uint8_t value){
    bool CLE;
    bool ALE;
    bool CEB;
    assert(nc3000mode || nc2000mode);
    if(nc3000mode){
        CLE = ram_io[0x18]&0x20;
        ALE = ram_io[0x18]&0x10;
        CEB = ram_io[0x18]&0x04;
    }
    if(nc2000mode){
        CLE = ram_io[0x18]&0x01;
        ALE = ram_io[0x18]&0x02;
        CEB = ram_io[0x18]&0x40;
    }
    if(CEB) {
        NLOG_R("[nand] write while no CE");
        //return;
    }

    if(CLE && ALE){
        NLOG_R("[nand] both CLE and ALE true while write");
        return;
    }

    if(enable_debug_nand) printf("tick=%llu write $29 %x  CLE=%d ALE=%d\n",tick%10000,value,CLE,ALE);
    uint8_t roa_bbs=ram_io[0x0a];
    uint8_t ramb_vol=ram_io[0x0d];
    uint8_t bs=ram_io[0x00];

    assert(nand_pointer_latch == NAND_POINTER_LATCH::AREA1 || nand_pointer_latch == NAND_POINTER_LATCH::AREA2 || nand_pointer_latch == NAND_POINTER_LATCH::AREA3);
    auto pointer_latch_old = nand_pointer_latch;

    if(CLE){
        if(value ==0x00||value==0x01||value==0x50){
            clear_nand_status();
            nand_cmd_latch = NAND_CMD_LATCH::READ1_2;
            if(value==0x00) nand_pointer_latch = NAND_POINTER_LATCH::AREA1;
            else if(value==0x01) nand_pointer_latch = NAND_POINTER_LATCH::AREA2;
            else if(value==0x50) nand_pointer_latch = NAND_POINTER_LATCH::AREA3;
            else assert(false);
            goto out;
        }

        if(value == 0x60){
            clear_nand_status();
            nand_cmd_latch = NAND_CMD_LATCH::ERASE;
            nand_pointer_latch = pointer_latch_old;
            goto out;
        }

        if(value == 0x70){
            clear_nand_status();
            nand_cmd_latch = NAND_CMD_LATCH::READ_STATUS;
            nand_pointer_latch = pointer_latch_old;
            goto out;
        }

        if(value == 0x80){
            clear_nand_status();
            nand_cmd_latch = NAND_CMD_LATCH::PROGRAM;
            nand_pointer_latch = pointer_latch_old;
            goto out;
        }

        if(value ==0x90){
            clear_nand_status();
            nand_cmd_latch = NAND_CMD_LATCH::READ_ID;
            nand_pointer_latch = pointer_latch_old;
            goto out;
        }

        if(value == 0xff){
            clear_nand_status();
            goto out;
        }

        if(value ==0x10){
            nand_write_assert(nand_cmd_latch==NAND_CMD_LATCH::PROGRAM);
            nand_write_assert(nand_addr.size()>=3);
            if(nand_addr.size()>3+1) NLOG_R("[nand] unexpected nand_addr size %d in PROGRAM", (int)nand_addr.size());
            unsigned char a0_7=nand_addr[0];
            unsigned char a9_16=nand_addr[1];
            unsigned char a17_24=nand_addr[2];
            unsigned char a25=nand_addr.size()>3? nand_addr[3]&0x01:0;

            uint32_t inside_page_offset = a0_7 + (uint32_t)nand_pointer_latch;
            nand_write_assert(inside_page_offset < 528);
            nand_write_assert(inside_page_offset + nand_data.size() <= 528);

            uint32_t offset=a25*256u*256u*528u+a17_24*256u*528u+a9_16*528u+inside_page_offset;

            bool warn=false;
            for(int i=0;i<nand_data.size();i++){
                nand_write_assert(offset+i < num_nand_pages*528);
                if(nand_peek(offset+i)!=0xff){
                    warn=true;
                    //fix byte that is not erased before write and produce a warning
                    if(forced_erase_before_write) nand_peek(offset+i)=0xff;
                }
                nand_peek(offset+i)&=nand_data[i];
            }
            if(warn){
                NLOG_R("[nand] oops, writing to non-erased byte at %x!!!!!!!!!!",offset);
            }
            if(inside_page_offset <512){
                printf("[nand] program, offset=0x%x (%u), size=%d\n",offset, offset, (int)nand_data.size());
            }else{
                printf("[nand] program spare, offset=0x%x (%u), size=%d\n",offset, offset, (int)nand_data.size());
            }
            clear_nand_status();
            nand_pointer_latch = pointer_latch_old;
            if(nand_pointer_latch == NAND_POINTER_LATCH::AREA2){
                nand_pointer_latch = NAND_POINTER_LATCH::AREA1;
            }
            goto out;
        }

        if(value==0xd0){
            nand_write_assert(nand_cmd_latch==NAND_CMD_LATCH::ERASE);
            nand_write_assert(nand_addr.size()>=2);
            if(nand_addr.size()>2+1) NLOG_R("[nand] unexpected nand_addr size %d in ERASE", (int)nand_addr.size());
            if(nand_data.size()>0) NLOG_R("[nand] unexpected nand_data size %d in ERASE", (int)nand_data.size());
            
            unsigned char a9_16=nand_addr[0];
            unsigned char a17_24=nand_addr[1];
            unsigned char a25=nand_addr.size()>2? nand_addr[2]&0x01:0;
            
            unsigned int offset= (a25*256u*256u + a17_24*256u+a9_16)*528u;

            if(offset%(32*528)!=0) {
                NLOG_R("[nand] erase offset %x not aligned to multiple of 32 pages", offset);
                offset/=(32*528);
                offset*=(32*528);
                assert(offset%(32*528)==0);
            }
            char *p=&nand[0][0];
            printf("[nand] erase, offset=0x%x (%u)\n",offset, offset);

            nand_write_assert(offset +32*528 <= sizeof(nand));
            memset(p+offset,0xff,32*528);

            clear_nand_status();
            nand_pointer_latch = pointer_latch_old;
            if(nand_pointer_latch == NAND_POINTER_LATCH::AREA2){
                nand_pointer_latch = NAND_POINTER_LATCH::AREA1;
            }
            goto out;
        }

        printf("[nand] unknown nand cmd %x in write, pointer_latch=%d, addr=", nand_cmd_latch, nand_pointer_latch);
        for(int i=0;i<nand_addr.size();i++) printf("%02x ", nand_addr[i]);
        printf(", data_size=%d\n", (int)nand_data.size());
        nand_write_assert(false);
    }

    if(ALE){
        nand_addr.push_back(value);
        if(nand_addr.size() >MAX_NAND_ADDR_SIZE){
            NLOG_R("[nand] oops, too many nand_addr %d", (int)nand_addr.size());
            nand_addr.pop_back();
        }
        goto out;
    }

    assert(!ALE&&!CLE);
    nand_data.push_back(value);
    if(nand_data.size() >MAX_NAND_DATA_SIZE){
        NLOG_R("[nand] oops, too many nand_data %d", (int)nand_data.size());
        nand_data.pop_back();
    }
    goto out;

    out:;

    // the out label here is for put some print cmd for debug
    //printf("bs=%x roa_bbs=%x pc=%x  %x %x %x %x \n",ram_io[0x00], ram_io[0x0a], reg_pc,  Peek16(p), Peek16(p+1),Peek16(p+2),Peek16(p+3));
}
