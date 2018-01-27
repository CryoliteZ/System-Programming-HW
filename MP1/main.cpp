#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <openssl/md5.h>
#include <iostream>
#include <algorithm>
#include <map>
#include <string>
#include <vector>
#include <unistd.h>
#include "mStrings.h"
using namespace std;

map<string,string> name2MD5;
map<string,string> MD52name;

bool compareFilename(char* c1, char* c2) {
    return strcmp(c1, c2) <= 0;
}

string MD5(char *filename){
    // unsigned char c[MD5_DIGEST_LENGTH];
    unsigned char *c = new unsigned char [MD5_DIGEST_LENGTH];
    unsigned char data[1024];    
    FILE *inFile = fopen (filename, "rb");
    MD5_CTX mdContext;
    int bytes;
    if (inFile == NULL) {
        printf ("%s can't be opened.\n", filename);
        return 0;
    }
    MD5_Init (&mdContext);
    while ((bytes = fread (data, 1, 1024, inFile)) != 0){
        MD5_Update (&mdContext, data, bytes);        
    }
    MD5_Final (c,&mdContext);  
    char buffer[48];
    string result = "";       
    for(int i = 0; i < MD5_DIGEST_LENGTH; i++){
        sprintf (buffer, "%02x", c[i]);
        result.append( buffer );
    }
    fclose (inFile);
    return result;
}

vector <char*> loadFileNames(){
    // dir names
    vector <char*> dir_names;
    bool find = false;    
    DIR *d;
    struct dirent *dir;
    d = opendir(".");
    if(d){
        while ((dir = readdir(d)) != NULL){
            if(!find && strcmp(dir->d_name, LOSER_RECORD) == 0){
                find = true;
                continue;
            }
            if(strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0){
                char* a = strdup(dir->d_name);
                dir_names.push_back(a);
                // cout << a << endl;
            }
            
        }
        closedir(d);
    }
    sort(dir_names.begin(), dir_names.end(), compareFilename);
    return dir_names;
}

int mapMD5SUM(){
    int offset = 0;
    FILE *fp;
    fp = fopen(LOSER_RECORD,"rb"); 
    string line = "";
    fseek(fp, -2, SEEK_END);       
    while(1){
        char c = fgetc(fp); 
        if(c == '\n'){
            size_t pos = line.find(' ');   
            if(pos!= string::npos){
                string md5 = line.substr(0,pos);
                string filename = line.substr(pos+1, line.length());
                reverse(md5.begin(), md5.end());
                reverse(filename.begin(), filename.end());
                // cout << md5 << "/" << filename << endl; 
                name2MD5[filename] = md5;
                MD52name[md5] = filename;
                line = "";               
            }
            else{
                break;
            }                    
        }else{
            line += c;   
            offset++;
        }
        fseek(fp, -2, SEEK_CUR);
    }
    fclose(fp);
    return offset;
}

int getCommitNum(int offset){
    int num = 1;
    string line = "";    
    FILE *fp;
    fp = fopen(LOSER_RECORD,"rb");
    fseek(fp, -offset, SEEK_END);  
    while(1){
        char c = fgetc(fp); 
        if(c == '\n'){
            // find commit line, get number
            if(line.find(" timmoc #")!=string::npos){
                num = line[0] - '0';
                break;
            }
            else{
                line = "";
            }
                      
        }else{
            line += c;   
            offset++;
        }
        if(line.find("1 timmoc #")!=string::npos){
            return 2;
        }
        fseek(fp, -2, SEEK_CUR);
    }
    fclose(fp);
    return num + 1;
}


void loser_update(int method){
    vector <char*> dir_names;
    dir_names = loadFileNames();
    
    vector <string> dir_filesMD5;
    vector< string > newFile;
    vector< string > modified;
    vector< string > copiedFrom;
    vector< string > copiedTo;
    // map MD5Sum from loser record, get current offset
    int offset = mapMD5SUM();
    for(int i = 0; i < dir_names.size();  ++i){
        bool newFileName = false;
        // save MD5Sum
        string curMD5 = MD5(dir_names[i]);
        dir_filesMD5.push_back(curMD5);

        // find if new /modified file        
        map<string, string>::iterator it;
        string d_name(dir_names[i]);
        it = name2MD5.find(d_name);    
          
        // filename exists
        if(it != name2MD5.end()){
            // cout << it->second << " :prevMD5" << endl;
            // file modified
            if(curMD5.compare(it->second)!=0){
                modified.push_back(d_name);
            }
        }
        // filename doesn't exists, new file name
        else{
            newFileName = true;
        }

        // find if copied file 
        if(newFileName){
             it = MD52name.find(curMD5); 
            // find same MD5 
            if(it != MD52name.end()){
                // filename diff -> copied
                if(d_name.compare(it->second)!=0){
                    copiedFrom.push_back(it->second);
                    copiedTo.push_back(d_name);
                }
            }
            // no same MD5, must be newfile
            else{
                newFile.push_back(d_name);
            }
        }
       
    }
    if(method == 1){
        printf("%s\n", NEWFILE);
        for(int i = 0; i < newFile.size(); ++i){
            cout << newFile[i] << endl;
        }
        printf("%s\n", MODIFIED);
        for(int i = 0; i < modified.size(); ++i){
            cout << modified[i] << endl;
        }
        printf("%s\n", COPIED);
        for(int i = 0; i < copiedFrom.size(); ++i){
            cout << copiedFrom[i] << " => " << copiedTo[i] << endl;
        }
    }
    else if (method == 2){
        // return;
        if((newFile.size() + modified.size() + copiedFrom.size()) == 0 ) {
            // nothing new, do nothing
            return;
        }
        int num = getCommitNum(offset);
        FILE *fp;
        fp = fopen(LOSER_RECORD, "ab+");
        fprintf (fp, "\n# commit %d\n",num);
        fprintf(fp, "%s\n", NEWFILE);
        for(int i = 0; i < newFile.size(); ++i){
            fprintf(fp, "%s\n", newFile[i].c_str());
        }
        fprintf(fp, "%s\n", MODIFIED);
        for(int i = 0; i < modified.size(); ++i){
            fprintf(fp, "%s\n",  modified[i].c_str());
        }
        fprintf(fp, "%s\n", COPIED);
        for(int i = 0; i < copiedFrom.size(); ++i){
            fprintf(fp, "%s => %s\n",  copiedFrom[i].c_str(), copiedTo[i].c_str());
        }
        fprintf(fp, "%s\n", MD5LABEL);
        for(int i = 0; i < dir_names.size(); ++i){
            fprintf(fp, "%s %s\n",  dir_names[i], dir_filesMD5[i].c_str());
        }
    }
    return;
}

void loser_commit(){
   // .loser_config doesn't exsist 
    FILE *fp;
    fp = fopen(LOSER_RECORD, "rb");
    if(fp == NULL){
        vector <char*> dir_names = loadFileNames();
        if(dir_names.size() == 0){
            // no files, do nothing
            return;
        }
        FILE *fp;
        fp = fopen(LOSER_RECORD,"wb");
        fprintf (fp, "# commit 1\n");
        fprintf (fp, "%s\n",NEWFILE);
        for(int i = 0; i < dir_names.size();  ++i){
            fprintf (fp, "%s\n",dir_names[i]);
        }
        fprintf(fp,"%s\n", MODIFIED);
        fprintf(fp, "%s\n", COPIED);
        fprintf(fp, "%s\n", MD5LABEL);
        for(int i = 0; i < dir_names.size(); ++i){
            fprintf(fp, "%s %s\n",  dir_names[i], MD5(dir_names[i]).c_str());
        }
   }
   else{
        fclose(fp);
        loser_update(2);
   }
   
}

void loser_status(){
    // .loser_config doesn't exsist 
    if(fopen(LOSER_RECORD,"rb") == NULL){
        vector <char*> dir_names = loadFileNames();
        printf("%s\n", NEWFILE);
        for(int i = 0; i < dir_names.size();  ++i){
        printf("%s\n", dir_names[i]);
        }
        printf("%s\n", MODIFIED);
        printf("%s\n", COPIED);
    }
    else{
        loser_update(1);
    }
}

void loser_log(int num){
    int count = 0;
    FILE *fp;
    fp = fopen(LOSER_RECORD,"rb"); 
    if(fp == NULL){
        return;
    }
    string line = "";
    fseek(fp, -2, SEEK_END);  
    vector <string> logs;     
    while(1){
        char c = fgetc(fp);
        if(c == '\n' || c == EOF){
            reverse(line.begin(), line.end());
            logs.push_back(line);
            if(line.find("# commit ")!=string::npos){   
                while(!logs.empty()){
                    string l = logs.back();
                    if(l.length() > 0)
                        cout << l << endl;
                    logs.pop_back();
                }
                count++;
                if(count == num){
                    break;
                }
                printf("\n");
            } 
            line = "";   
        }
        else if(line.find("1 timmoc #")!=string::npos){
            reverse(line.begin(), line.end());
            logs.push_back(line);
            while(!logs.empty()){
                string l = logs.back();
                if(l.length() > 0)
                    cout << l << endl;
                logs.pop_back();
            }
            break;
        }
        else{
            line += c;
        }
        fseek(fp, -2, SEEK_CUR);
    }
    fclose(fp);
}

map<string,string> getConfig(){
    map<string,string> CMDConfig;
    FILE *fp;
    fp = fopen(LOSER_CONFIG,"rb");
    char buffer[512];
    if(fp != NULL){
        while(fgets(buffer, 512, (FILE*) fp)) {
            if(buffer[strlen(buffer)-1] == '\n'){
                buffer[strlen(buffer)-1] = '\0';
            }
            string buf(buffer);
            size_t pos = buf.find('=');   
            if(pos!= string::npos){
                string cmd_abv = buf.substr(0,pos-1);
                string cmd = buf.substr(pos+2, buf.length());
                CMDConfig[cmd_abv] = cmd;
            }
        }
    }
    return CMDConfig;
}

int main(int argc,char* argv[]){
    if(argc <= 2){
        fprintf(stderr, "too few arguments error\n");
        return -1;
    }
    else if(argc == 3){
        char *cmd = argv[1];
        char *path = argv[2];
        int ret;
        ret = chdir (path);
        // get loser_config
        map<string,string> CMDConfig = getConfig();
        if(strcmp(cmd, STATUS) == 0 || CMDConfig[cmd].compare(STATUS) == 0){
            loser_status();
        }
        else if(strcmp(cmd, COMMIT) == 0 || CMDConfig[cmd].compare(COMMIT) == 0){
            loser_commit();
        }
    }
    else if(argc == 4){
        char *cmd = argv[1];
        int num = atoi(argv[2]);        
        char *path = argv[3];
        int ret;
        ret = chdir (path);
        // get loser_config
        map<string,string> CMDConfig = getConfig();
        if(num > 0){
            if(strcmp(cmd, LOG) == 0 || CMDConfig[cmd].compare(LOG) == 0){
                loser_log(num);
            }
        }
    }
    return 0;
}