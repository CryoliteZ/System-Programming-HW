#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int count[256] = {0};

// count characters from a line
void count_line(char* CHARSET, int charset_len){
    int sum = 0;
    for(int i = 0; i < charset_len; ++i){
        sum += count[CHARSET[i]];
    }
    printf("%d\n", sum);           
    memset(count, 0 ,sizeof(count));  
    return;
}

int main(int argc,char* argv[]){
    FILE *fp;
    char* CHARSET;
    /* read file input */
    if(argc < 2){
        fprintf(stderr, "error\n");
        return -1;
    }
    else if(argc == 2){
        CHARSET = argv[1];        
        fp = stdin;
    }
    else if(argc == 3){
        CHARSET = argv[1];
        fp = fopen(argv[2],"rb"); 
        if(fp == NULL){
            fprintf(stderr, "error\n");
            return -1;
        }        
    }
    else{
        fprintf(stderr, "error\n");
        return -1;
    }
    /* process count */
    int charset_len = strlen(CHARSET);
    if(charset_len > 0){
        while(1){   
            char c = fgetc(fp);     
            if(c == '\n'){
                count_line(CHARSET, charset_len);
                continue;
            }
            else if(c == EOF){
                // end of file
                break;
            }
            else{            
                count[c] += 1;
            }
        }
    }
    else{
        while(1){   
            char c = fgetc(fp);     
            if(c == '\n'){
                printf("0\n");
                continue;
            }
            else if(c == EOF){                
                break;
            }           
        }

    }
    return 0;   
}