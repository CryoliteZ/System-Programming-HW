#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <linux/limits.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <vector>
#include <iomanip>  
#include <openssl/md5.h>
#include "boss.h"
#include "../include/common.h"
#define MAXLINESIZE 100
using namespace std;

// string MINE;
int TARGETZEROS = 0;
int MINELENS = 9;
int MINERNUMBERS = 0;
int FINDMINE = 0;
unsigned char* NEWMINE;

int idx(int* x, int length){
    for(int i = 0; i < length; ++i){
        if(x[i] == 0)
        return i;
    }
    return 0;
}

int sum(int* x, int length){
    int sum = 0;
    for(int i = 0; i < length; ++i){
        sum += x[i];
    }
    return sum;
}

string MD5(string msg){
    unsigned char *c = new unsigned char [MD5_DIGEST_LENGTH]; 
    MD5_CTX mdContext;
    MD5_Init (&mdContext);
    // cout << msg << endl;
    MD5_Update (&mdContext, (const unsigned char *) msg.c_str(), msg.length());   
    // MD5_Update (&mdContext, (const unsigned char *) msg, strlen(msg)-1);        
         
    MD5_Final (c,&mdContext);  
    char buffer[48];
    string result = "";       
    for(int i = 0; i < MD5_DIGEST_LENGTH; i++){
        sprintf (buffer, "%02x", c[i]);
        result.append( buffer );
    }
    return result;
}

int mMD5(const unsigned char * msg){
    unsigned char *c = new unsigned char [MD5_DIGEST_LENGTH]; 
    MD5_CTX mdContext;
    MD5_Init (&mdContext);
    MD5_Update (&mdContext,  msg, strlen((const char*)msg));        
    MD5_Final (c,&mdContext);  
    // output  server stdout
    for (int i = 0; i <  MD5_DIGEST_LENGTH; i ++) {
        char buffer[48];
        printf("%02x", c[i]);
        // sprintf(buffer, "%02x", c[i]);  
        // cout << buffer;
    }
    delete c;
}

int getZeros(const unsigned char * msg){
    unsigned char *c = new unsigned char [MD5_DIGEST_LENGTH]; 
    MD5_CTX mdContext;
    MD5_Init (&mdContext);
    MD5_Update (&mdContext,  msg, strlen((const char*)msg));        
    MD5_Final (c,&mdContext);  
    int count = 0;
    char md5string[33];
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
    return count;
}

int load_config_file(struct server_config *config, char *path)
{
    /* TODO finish your own config file parser */
    ifstream inFile(path);
    string line;
    vector <pipe_pair> pipe_pairs;
    int miner_count = 0;
    while(getline(inFile, line)){
        vector <string> v;
        istringstream iss(line);
        do{
            string subs;
            iss >> subs;
            if(subs.length() > 0) v.push_back(subs);
            // cout << "Substring: " << subs << subs.length() <<endl;
        } while (iss);
        if(v[0].compare("MINE:") == 0){
            config->mine_file = strdup(v[1].c_str());
            FILE* fp = fopen(config->mine_file, "rb");
            fseek(fp, 0L, SEEK_END);
            int sz = ftell(fp);
            rewind(fp);


            // ifstream mineFile(config->mine_file);
            // string MINE;
            // mineFile >> MINE;
            NEWMINE = new unsigned char(sz+1);
            fread(NEWMINE, sz, 1, fp);
            fclose(fp);

            // for(int i = 0; i < MINE.length(); ++i){
            //     NEWMINE[i] = MINE[i];
            // }
            // NEWMINE[MINE.length()]  = '\0';
            // FILE *fp = fopen ("../tmp/mine_path", "wb");
            // ofstream myfile;
            // myfile.open ("../tmp/mine_tmp");
            // myfile << MINE;
            // myfile.close();
               
        }
        else if(v[0].compare("MINER:") == 0){
            assert(v.size()==3);
            pipe_pair p;
            p.input_pipe = strdup(v[1].c_str());
            p.output_pipe = strdup(v[2].c_str());
            pipe_pairs.push_back(p);
            ++miner_count;
        }
    }
    config->num_miners = miner_count;
    config->pipes = new pipe_pair[miner_count];
    for(int i = 0; i < miner_count; ++i){
        config->pipes[i] = pipe_pairs[i];
    }
    // cout << config->num_miners << endl;
    // config->mine_file  = /* path to mine file */;
    // config->pipes      = /* array of pipe pairs */;
    // config->num_miners = /* number of miners */;

    return 0;
}

int assign_jobs(vector <fd_pair> fd_pairs)
{
    /* TODO design your own (1) communication protocol (2) job assignment algorithm */
    for(int i = 0; i < fd_pairs.size(); ++i){
        // unsigned char* num = new unsigned char(MINE.length()+1);
        // for(int i = 0; i < MINE.length(); ++i){
        //     num[i] = MINE[i];
        // }
        // num[MINE.length()] = '\0';
        AIO_protocol_initjob req;
        req.header.op = INIT_JOB_OP;
        req.header.datalen = sizeof(req) - sizeof(req.header);
        req.body.start = 1;
        req.body.end = 4;
        req.body.nodestart =  (i * (256/MINERNUMBERS))% 256;
        req.body.nodeend = ((i+1) * (256/MINERNUMBERS)-1)% 256;
        req.body.targetzeros = TARGETZEROS+1;
        req.body.datalen = strlen((const char *)NEWMINE);
        // send request
        write(fd_pairs[i].input_fd, &req, sizeof(req));
        write(fd_pairs[i].input_fd, NEWMINE, req.body.datalen);
      
        

    }
    

}

int handle_command()
{
    /* TODO parse user commands here */
    char *cmd; /* command string */

    
}

int string2hex(char* str){
    unsigned char* arr = (unsigned char *) str;
    char buffer[48];
    for (int i = 0; i <  strlen(str); i ++) {
        // arr[i] = arr[i] + 256;感覺
        printf(" %02x", arr[i]);    
    }
    putchar('\n');
    return 1;
}

//used to receive complete header
int complete_message_with_header(int conn_fd, AIO_protocol_header* header, void* result) {
  memcpy(result, header->bytes, sizeof(AIO_protocol_header));
  read(conn_fd, result + sizeof(AIO_protocol_header), header->datalen);
  return 1;

}

int main(int argc, char **argv)
{
    /* sanity check on arguments */
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s CONFIG_FILE\n", argv[0]);
        exit(1);
    }
  
   
    // cout << MD5(s) << endl;
    /* load config file */
    struct server_config config;
    load_config_file(&config, argv[1]);
    MINERNUMBERS = config.num_miners;
    // NEWMINE = new unsigned char[MINE.length()+1];
    // for(int k = 0; k < MINE.length(); ++k){
    //     NEWMINE[k] = MINE[k];
    // }
    // NEWMINE[MINE.length()] = '\0';
    int z = getZeros(NEWMINE);
    // fprintf(stderr, "z %d\n", z);
    TARGETZEROS = z;

     /* open the named pipes */
    struct fd_pair client_fds[config.num_miners];
    vector <fd_pair> fd_pairs;
    vector <string> miner_names;
    ofstream miner_names_file;
    miner_names_file.open ("../tmp/miner_names");
    for (int ind = 0; ind < config.num_miners; ind += 1)
    {
        struct fd_pair *fd_ptr = &client_fds[ind];
        struct pipe_pair *pipe_ptr = &config.pipes[ind];
        fd_ptr->input_fd = open(pipe_ptr->input_pipe, O_WRONLY);
        assert (fd_ptr->input_fd >= 0);
        string mine_file(config.mine_file);
        string hello = "BOSS is mindful.";
        string helloworld = hello + mine_file;
        // cout << helloworld << endl;
        write(fd_ptr->input_fd, "BOSS is mindful.", sizeof("BOSS is mindful."));
        fd_ptr->output_fd = open(pipe_ptr->output_pipe, O_RDONLY);
        char buf[1024];
        read(fd_ptr->output_fd, buf, 1024);
        // fprintf(stderr, "%s is initialized\n", buf);
        assert (fd_ptr->output_fd >= 0);
        string str(strdup(buf));
        miner_names.push_back(str);
        miner_names_file << str << endl;
        fd_pairs.push_back(*fd_ptr);
    }
    miner_names_file.close();


    /* initialize data for select() */
    int maxfd = 0;
    fd_set readset;
    fd_set working_readset;

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    FD_ZERO(&readset);
    FD_SET(STDIN_FILENO, &readset);
    // TODO add input pipes to readset, setup maxfd
    for(int i = 0; i < fd_pairs.size(); ++i){
        if(fd_pairs[i].input_fd > maxfd) maxfd = fd_pairs[i].input_fd;
        if(fd_pairs[i].output_fd > maxfd) maxfd = fd_pairs[i].output_fd;
        FD_SET(fd_pairs[i].output_fd, &readset);
    }

    /* assign jobs to clients */
    assign_jobs(fd_pairs);

    // for notifications
    int* notification = new int[MINERNUMBERS];
    memset(notification, 0, MINERNUMBERS);
    // current treasure
    while (1){
        memcpy(&working_readset, &readset, sizeof(readset)); // why we need memcpy() here?
        int err = select(maxfd + 1, &working_readset, NULL, NULL, &timeout);
        // cout << err <<endl;
        if (FD_ISSET(STDIN_FILENO, &working_readset)){
            /* TODO handle user input here */
            string cmd ="";
            cin >> cmd;
            // fprintf(stderr,"C: %s\n",cmd);
            if (cmd.compare("status") == 0){
                /* TODO show status */
                if(TARGETZEROS == 0){
                   printf("best 0-treasure in 0 bytes\n");
                }
                else{
                    printf("best %d-treasure ",TARGETZEROS);
                    // cout << "best "<< TARGETZEROS  <<"-treasure ";
                    mMD5(NEWMINE);
                    printf(" in %d bytes\n",strlen((const char *)NEWMINE));
                    // cout << " in "<< MINELENS <<" bytes\n" << std::flush;;
                }
                for(int i = 0; i < MINERNUMBERS; ++i){
                    AIO_protocol_cmd req;
                    req.header.op = USER_CMD_OP;
                    req.header.datalen = sizeof(req) - sizeof(req.header);
                    req.body.cmd = 1;
                    write(fd_pairs[i].input_fd, &req, sizeof(req));
                }
            }
            else if (cmd.compare("dump") == 0){
                char dest[PATH_MAX] ="";
                scanf("%s",dest);
                int dump_fd = open(dest,O_CREAT|O_WRONLY|O_NONBLOCK|O_TRUNC,0644);
                write(dump_fd,NEWMINE,strlen((const char *)NEWMINE));
                close(dump_fd);
            }
            else if (cmd.compare("quit") == 0){
                for(int i = 0; i < MINERNUMBERS; ++i){
                    AIO_protocol_cmd req;
                    req.header.op = USER_CMD_OP;
                    req.header.datalen = sizeof(req) - sizeof(req.header);
                    req.body.cmd = 2;
                    write(fd_pairs[i].input_fd, &req, sizeof(req));
                }
                exit(0);
                
                
                /* TODO tell clients to cease their jobs and exit normally */
            }
        }

        /* TODO check if any client send me some message
           you may re-assign new jobs to clients*/
        for(int i = 0; i < fd_pairs.size(); ++i){
            if (FD_ISSET(fd_pairs[i].output_fd,&working_readset)){
                AIO_protocol_header header;
                int bytes = read(fd_pairs[i].output_fd, &header, sizeof(AIO_protocol_header));
                if(header.op == FIND_MINE_OP){
                    AIO_protocol_findmine fm;
                    complete_message_with_header(fd_pairs[i].output_fd, &header, &fm);

                    if(!fm.body.find){
                        continue;
                    }
                     // init mine block
                    delete NEWMINE;
                    NEWMINE = new unsigned char[fm.body.minelen+1];
                    // newmine[fm.body.minelen] = '\0';
                    read(fd_pairs[i].output_fd, NEWMINE, fm.body.minelen);
                    if(fm.body.findzero > TARGETZEROS){
                        // NEWMINE = new unsigned char[fm.body.minelen+1];
                        // for(int k = 0; k < fm.body.minelen; ++k){
                        //     NEWMINE[k] = newmine[k];
                        // }
                        
                        TARGETZEROS++;
                        FINDMINE = 1;
                    }
                    else{
                        continue;
                    }
                    
                    // fprintf(stderr, "%s %d", newmine, strlen((const char*)newmine));
                    // fprintf(stderr, "find%d,minelen:%d, findzero:%d\n",fm.body.find,fm.body.minelen, fm.body.findzero);
                   
                    // write to mine.bin
                    FILE *fp = fopen(config.mine_file, "wb");
                    fprintf(fp, "%s", NEWMINE);
                    fclose(fp);
                    
                    // update global vars
                    // MAXZEROS = fm.body.maxzero;
                    MINELENS = fm.body.minelen;
                    
                    fprintf(stdout, "A %d-treasure discovered! ", fm.body.findzero);
                    mMD5((const unsigned char *)NEWMINE);
                    fprintf(stdout, "\n");

                    // prepare for notifications
                    memset(notification,0,MINERNUMBERS);
                    for(int k = 0; k < MINERNUMBERS; ++k){
                        if(k!=i){
                            notification[k] = 1;
                        }
                    }
                
                       
                    // tell other miners who won
                    for(int t = 0; t < MINERNUMBERS; ++t){
                        AIO_protocol_verify x;
                        x.header.op = SHARE_VERIFY_OP;
                        x.header.datalen = sizeof(x) - sizeof(x.header);
                        x.body.ac = (t == i);
                        x.body.minerid = i;
                        x.body.minelen = MINELENS;
                        x.body.targetzeros = TARGETZEROS;
                        x.body.minerlen = miner_names[t].length();
                        write(fd_pairs[t].input_fd, &x, sizeof(x));
                        write(fd_pairs[t].input_fd, NEWMINE, x.body.minelen);
                        notification[t] = 0;
                    }
                    
                    // fprintf(stderr, "! %s %d\n", NEWMINE, MINELENS);
                    for(int j = 0; j < MINERNUMBERS; ++j){
                        unsigned char* num = new unsigned char(MINELENS+1);
                        for(int k = 0; k < MINELENS; ++k){
                            num[k] = NEWMINE[k];
                        }
                        num[strlen((const char*)NEWMINE)] = '\0';
                        AIO_protocol_initjob req;
                        req.header.op = INIT_JOB_OP;
                        req.header.datalen = sizeof(req) - sizeof(req.header);
                        req.body.start = 1;
                        req.body.end = 3;
                        req.body.nodestart =  (j * (256/MINERNUMBERS)) % 256;
                        req.body.nodeend = ((j+1) * (256/MINERNUMBERS))-1 % 256;
                        req.body.targetzeros = TARGETZEROS+1;
                        req.body.datalen = MINELENS;
                        // send request
                        write(fd_pairs[j].input_fd, &req, sizeof(req));
                        write(fd_pairs[j].input_fd, num, MINELENS);
                    }
                    FINDMINE = 0;
                }
            }
        }
        if(TARGETZEROS < 4)
            usleep(200);
    }

    return 0;
}