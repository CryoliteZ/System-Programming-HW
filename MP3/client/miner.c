#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <linux/limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "../include/common.h"
#include <openssl/md5.h>
int input_fd, output_fd;
int MAXZEROS = 0;
int FINDTARGET = 0;
int BYTES = 0;
// unsigned char newmine[PATH_MAX];
// unsigned char nmine[PATH_MAX];
// unsigned char working[PATH_MAX];
unsigned char* newmine;
unsigned char* nmine;
unsigned char working[40];
char* treaure;
//used to receive complete header
int complete_message_with_header(int conn_fd, AIO_protocol_header* header, void* result) {
  memcpy(result, header->bytes, sizeof(AIO_protocol_header));
  read(conn_fd, result + sizeof(AIO_protocol_header), header->datalen);
  return 1;
}

int pMD5(const unsigned char * msg){
    unsigned char *c = malloc(sizeof(unsigned char) *MD5_DIGEST_LENGTH );
    MD5_CTX mdContext;
    MD5_Init (&mdContext);
    MD5_Update (&mdContext,  msg, strlen((const char*)msg));        
    MD5_Final (c,&mdContext);  
    for(int i = 0; i < MD5_DIGEST_LENGTH; i++){
        fprintf(stdout, "%02x", c[i]); 
    }
    fprintf(stdout,"\n");
}

int mMD5(const unsigned char * msg, int level){
    unsigned char *c = malloc(sizeof(unsigned char) *MD5_DIGEST_LENGTH );
    MD5_CTX mdContext;
    MD5_Init (&mdContext);
    MD5_Update (&mdContext,  msg, strlen(msg));        
    MD5_Final (c,&mdContext);  
    int count = 0;
    char md5string[33];
    // copy to working
    for(int i = 0; i < 16; ++i){
        sprintf(&working[i*2], "%02x", c[i]);
    }
    for(int i = 0; i < 16; ++i){
        sprintf(&md5string[i*2], "%02x", c[i]);
        if(md5string[i*2] == '0'){
            count++;
        }
        else{
            break;
        }
        if(md5string[i*2+1] == '0'){
            count++;
        }
        else{
            break;
        }
    }
    if(count ==  MAXZEROS){
        // FIND!
        MAXZEROS = count;
        
        // update global var
        FINDTARGET = 1;
        AIO_protocol_findmine fm;
        fm.header.op = FIND_MINE_OP;
        fm.header.datalen = sizeof(fm) - sizeof(fm.header);
        fm.body.minelen = strlen((const char*)msg);
        fm.body.findzero = MAXZEROS;
        fm.body.find = FINDTARGET;
        
        write(output_fd, &fm, sizeof(fm));
        write(output_fd, msg, fm.body.minelen);
       
        BYTES = strlen((const char*)msg);

        // for(int i = 0; i < MD5_DIGEST_LENGTH; i++){
        //     fprintf(stderr, "%02x", c[i]); 
        // }
        // fprintf(stderr,"\n");

    } 
    else{
        return;
    }
    // fprintf(stderr,"%d Bytes: ", strlen(msg));
    // for (int j = 0; j <  strlen(msg); j ++) {
    //     fprintf(stderr, "%02x ", msg[j]);    
    // }
    // fprintf(stdout,"\n");
    
}

void recursiveFind(unsigned char* x, int basesize, int level, int maxlevel, int key){
    if(FINDTARGET)
        return;
    x[(basesize)+level] = key;
    ++level;
    if(level == maxlevel){
        // for (int j = 0; j < basesize + level; j ++) {
        //    fprintf(stderr, "%02x ", x[j]);    
        // }
        // fprintf(stderr,": Len %d\n", basesize + level); 
        // const char* result = malloc(sizeof(unsigned char) *MD5_DIGEST_LENGTH );
        mMD5(x, basesize + level);
        return;
    }
    else{
        for(int i = 0; i < 256; ++i){
            recursiveFind(x, basesize, level, maxlevel,i);
        }
    }
}

void mine4win(unsigned char* mine, int start, int end, int nodestart, int nodeend, int initbytes){
    unsigned char* m = (unsigned char *) mine;
    // fprintf(stderr, "Original String: ");
    // for (int j = 0; j <strlen(mine); j ++) {
    //      fprintf(stderr, "%02x ", mine[j]);   
    // }
    // fprintf(stderr, " Start Mining!\n");
    for(int tail = start; tail < end; ++tail){
        unsigned char* target = malloc(initbytes + tail + 1);
        // unsigned char target[PATH_MAX];
        // memset(target,0, PATH_MAX);
        for (int j = 0; j < initbytes; j ++) {
            target[j] = m[j];
        }
        for(int key = nodestart; key <= nodeend; ++key){
            recursiveFind(target, initbytes, 0, tail, key);
        }
        free(target);
    }
}

int main(int argc, char **argv)
{
    /* parse arguments */
    if (argc != 4)
    {
        fprintf(stderr, "Usage: [name] [inputpipe] [outputpipe]\n");
        exit(1);
    }

    char *name = argv[1];
    char *input_pipe = argv[2];
    char *output_pipe = argv[3];

    /* create named pipes */
    int ret;
    ret = mkfifo(input_pipe, 0644);
    assert (!ret);

    ret = mkfifo(output_pipe, 0644);
    assert (!ret);
    while(1){
        /* open pipes from boss*/
        input_fd = open(input_pipe, O_RDONLY);
        assert (input_fd >= 0);
        char mineful[1024];
        memset(mineful,0,1024);
        read(input_fd, mineful, 1024);
        fprintf(stdout, "BOSS is mindful.\n");
        fflush(stdout);
        output_fd = open(output_pipe, O_WRONLY);
        write(output_fd, name, sizeof(name));
        assert (output_fd >= 0);
        
        /* initialize data for select() */
        int maxfd = 0;
        fd_set readset;
        fd_set working_readset;

        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 0;

        FD_ZERO(&readset);
        FD_SET(input_fd, &readset);

        // memset
        // memset(newmine,0,PATH_MAX);
        // memset(nmine,0,PATH_MAX);
        // memset(working,0,PATH_MAX);
        int start = 0;

        while (1){
            
            memcpy(&working_readset, &readset, sizeof(readset)); // why we need memcpy() here?
            int err = select(input_fd + 1, &working_readset, NULL, NULL, &timeout);
            if (FD_ISSET(input_fd, &working_readset)){
            
                AIO_protocol_header header;
                int bytes = read(input_fd, &header, sizeof(AIO_protocol_header));
                
                if(header.op == INIT_JOB_OP){
                    AIO_protocol_initjob aj;
                    complete_message_with_header(input_fd, &header, &aj);

                    // fprintf(stderr, "start:%d, end:%d, minelen:%d, maxzero:%d, nodestart:%d, nodeend:%d\n",aj.body.start, aj.body.end, aj.body.datalen, aj.body.targetzeros, aj.body.nodestart, aj.body.nodeend);
                    
                    // update global vars
                    MAXZEROS = aj.body.targetzeros;
                    FINDTARGET = 0;

                    // init mine block
                    if(!start){
                        unsigned char* initmine = malloc(sizeof(unsigned char) * (aj.body.datalen + 1));
                        read(input_fd, initmine, aj.body.datalen);
                        // fprintf(stderr, "The mine %s bytes:%d\n", initmine, strlen((const char*)initmine));
                        mine4win(initmine, aj.body.start, aj.body.end, aj.body.nodestart, aj.body.nodeend, aj.body.datalen);
                        start = 1;
                        if(!FINDTARGET){
                             AIO_protocol_findmine fm;
                            fm.header.op = FIND_MINE_OP;
                            fm.header.datalen = sizeof(fm) - sizeof(fm.header);
                            fm.body.minelen = 0;
                            fm.body.findzero = MAXZEROS;
                            fm.body.find = 0;
                            write(output_fd, &fm, sizeof(fm));
                        }
                    
                    }
                    else{
                        // fprintf(stderr, "The mine %s bytes:%d\n", nmine, strlen((const char*)nmine));
                        mine4win(nmine, aj.body.start, aj.body.end, aj.body.nodestart, aj.body.nodeend, aj.body.datalen);
                        if(!FINDTARGET){
                            AIO_protocol_findmine fm;
                            fm.header.op = FIND_MINE_OP;
                            fm.header.datalen = sizeof(fm) - sizeof(fm.header);
                            fm.body.minelen = 0;
                            fm.body.findzero = MAXZEROS;
                            fm.body.find = 0;
                            write(output_fd, &fm, sizeof(fm));
                        }
                    }
                    // fprintf(stderr, "Mine done\n");
                    // free(initmine);
                }
                // someone won
                else if(header.op == SHARE_VERIFY_OP){
                    AIO_protocol_verify av;
                    complete_message_with_header(input_fd, &header, &av);
                    // fprintf(stderr, "ac:%d, minelen:%d, minerlen:%d",av.body.ac, av.body.minelen, av.body.minerlen);
                    
                    // char* miner = malloc( sizeof(char) *(av.body.minerlen+1));
                    // read(input_fd, miner, av.body.minerlen);
                    // fprintf(stderr, "REady to read mine\n");
                    nmine = malloc(av.body.minelen + 1);
                    memset(nmine, 0,av.body.minelen + 1);
                    read(input_fd, nmine, av.body.minelen);
                    // FILE *fp_mine = fopen("../tmp/mine_tmp", "rb");
                    // FILE *fp_mine = fopen("../tmp/mine.bin", "rb");  
                
                    // fread(nmine, 1,av.body.minelen, fp_mine);
                    FILE *fp = fopen("../tmp/miner_names", "rb");
                    char line[4096];
                    int mid = 0;
                    while(fgets(line, sizeof(line), fp)) {
                        if(mid == av.body.minerid){
                            break;
                        }
                        mid++;
                    }
                    int len = strlen(line);
                    if (len > 0 && line[len-1] == '\n')
                        line[len-1] = 0;
                    if(av.body.ac){
                        fprintf(stdout, "I win a %d-treasure! ", av.body.targetzeros);
                        pMD5((const unsigned char*) nmine);
                    }
                    else{
                        fprintf(stdout, "%s wins a %d-treasure! ", line, av.body.targetzeros);
                        pMD5((const unsigned char*) nmine);
                    }
                
                }
                else if(header.op == ASSIGN_JOB_OP){
                    // fprintf(stderr, "Request job from boss\n");
                    AIO_protocol_assignjob aj;
                    complete_message_with_header(input_fd, &header, &aj);
                    // fprintf(stderr, "start:%d, end:%d, minelen:%d, maxzero:%d, nodestart:%d, nodeend:%d\n",aj.body.start, aj.body.end, strlen((const char*)nmine),aj.body.targetzeros, aj.body.nodestart, aj.body.nodeend);

                    // update global vars
                    MAXZEROS = aj.body.targetzeros;
                    FINDTARGET = 0;

                    // init mine block
                    // unsigned char* initmine = malloc((aj.body.datalen+1)* sizeof(char));
                    // read(input_fd, initmine, aj.body.datalen);
                    // fprintf(stderr, "The mine %s bytes:%d\n", nmine, strlen((const char*)nmine));
                    mine4win(nmine, aj.body.start, aj.body.end, aj.body.nodestart, aj.body.nodeend, strlen((const char*)nmine));
                    // fprintf(stderr, "Mine done %d\n", strlen((const char*)newmine));
                    AIO_protocol_findmine fm;
                    fm.header.op = FIND_MINE_OP;
                    fm.header.datalen = sizeof(fm) - sizeof(fm.header);
                    fm.body.minelen = strlen((const char*)newmine);
                    fm.body.findzero = MAXZEROS;
                    fm.body.find = FINDTARGET;
                    write(output_fd, &fm, sizeof(fm));
                }
                else if(header.op == USER_CMD_OP ){
                    AIO_protocol_cmd res;
                    complete_message_with_header(input_fd, &header, &res);
                    // fprintf(stderr, "Boss command: %d\n", cmd.body.cmd);
                    if(res.body.cmd == 1){
                        printf("I'm working on ");
                        pMD5(working);
                    }
                    else if(res.body.cmd == 2){
                        printf("BOSS is at rest.\n");
                        char buffer[4096];
                        read(input_fd, buffer, 1024);
                        close(input_fd);
                        close(output_fd);
                        break;
                    }
                
                }            
            
            }
            
        }
    }
    return 0;
}