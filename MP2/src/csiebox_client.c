#define _XOPEN_SOURCE 700

#define _LARGEFILE64_SOURCE
#define _FILE_OFFSET_BITS 64 
#ifndef USE_FDS
#define USE_FDS 15
#endif

#include "csiebox_client.h"
#include "csiebox_common.h"
#include "connect.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <ftw.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <linux/inotify.h> //header for inotify

#define EVENT_SIZE (sizeof(struct inotify_event))
#define EVENT_BUF_LEN (4096 * (EVENT_SIZE + 16))
/* --treewalk --*/

typedef struct{
	char *filepath;
	// char *target;
	struct stat info;
	int typeflag;
	uint8_t md5[MD5_DIGEST_LENGTH];
}File_entry;

typedef struct{
	char *filepath;
	int wd;
}Inotify_wd;

// hard link files
int fileCount = 0;
File_entry files[512];

// inotify wd & filepath pairs
int wdCount = 0;
Inotify_wd watch[512];

// global socket fd
csiebox_client* mClient;

// for counting lopngest filepath
int base = 0,level = 0;
char longest_path[512];

// inotify
int ifd;
char inotifyBuffer[EVENT_BUF_LEN];

void insert_file(File_entry *file,  char *filepath,  struct stat *info,  int typeflag){
	file->info = *info;
  	file->filepath =  strdup(filepath);     
	file->typeflag = typeflag;
	// its a file, compute MD5
	if(!typeflag){		
		md5_file(filepath, file->md5);
	}
	fileCount++;
}

void insert_wd(Inotify_wd *watch, char* filepath, int wd){
	watch->filepath = strdup(filepath);
	watch->wd = wd;
	wdCount++;
}



int remove_file(char* targetPath){
	csiebox_protocol_rm req;
	memset(&req, 0, sizeof(req));
	req.message.header.req.magic = CSIEBOX_PROTOCOL_MAGIC_REQ;
	req.message.header.req.op = CSIEBOX_PROTOCOL_OP_RM;
	req.message.header.req.datalen = sizeof(req) - sizeof(req.message.header);
	req.message.body.pathlen = strlen(targetPath);

	if (!send_message(mClient->conn_fd, &req, sizeof(req))) {
		fprintf(stderr, "send fail\n");
		return 0;
	}
	// send srcPath
	if (!send_message(mClient->conn_fd, targetPath, strlen(targetPath))) {
		fprintf(stderr, "send fail\n");
		return 0;
	}

	// recv ok to see its done
	//receive csiebox_protocol_header from server
	csiebox_protocol_header header;
	memset(&header, 0, sizeof(header));
	if (recv_message(mClient->conn_fd, &header, sizeof(header))) {
		if (header.res.magic == CSIEBOX_PROTOCOL_MAGIC_RES &&
			header.res.op == CSIEBOX_PROTOCOL_OP_RM ){
			if(header.res.status == CSIEBOX_PROTOCOL_STATUS_OK){
				fprintf(stderr, "Receive RM OK from server\n");
				return 1;
			}
		}
	}
	
}

int sync_link(char* srcPath, char* targetPath, struct stat *info, int typeflag ){
	csiebox_protocol_hardlink req;
	memset(&req, 0, sizeof(req));
	req.message.header.req.magic = CSIEBOX_PROTOCOL_MAGIC_REQ;
	req.message.header.req.op = CSIEBOX_PROTOCOL_OP_SYNC_HARDLINK;
	// hardlink
	if(typeflag == FTW_F){
		req.message.header.req.status = CSIEBOX_PROTOCOL_STATUS_OK;
	}
	// softlink
	else if(typeflag == FTW_SL){
		req.message.header.req.status = CSIEBOX_PROTOCOL_STATUS_MORE;
	}
	req.message.header.req.datalen = sizeof(req) - sizeof(req.message.header);
	req.message.body.srclen = strlen(srcPath);
	req.message.body.targetlen = strlen(targetPath);
	req.message.body.stat = *info;

	if (!send_message(mClient->conn_fd, &req, sizeof(req))) {
		fprintf(stderr, "send fail\n");
		return 0;
	}
	// send srcPath
	if (!send_message(mClient->conn_fd, srcPath, strlen(srcPath))) {
		fprintf(stderr, "send fail\n");
		return 0;
	}
	// send targetPath
	if (!send_message(mClient->conn_fd, targetPath, strlen(targetPath))) {
			fprintf(stderr, "send fail\n");
			return 0;
	}
}

// sync file
int sync_file(char *filepath,  struct stat *info, int typeflag){
	csiebox_protocol_meta req;
	memset(&req, 0, sizeof(req));
	req.message.header.req.magic = CSIEBOX_PROTOCOL_MAGIC_REQ;
	req.message.header.req.op = CSIEBOX_PROTOCOL_OP_SYNC_META;
	req.message.header.req.datalen = sizeof(req) - sizeof(req.message.header);
	req.message.body.pathlen = strlen(filepath);
	req.message.body.stat = *info;
	// lstat(filepath, &req.message.body.stat);
	// regular file, compute md5
	if(typeflag == FTW_F)
		md5_file(filepath, req.message.body.hash);
	if (!send_message(mClient->conn_fd, &req, sizeof(req))) {
		fprintf(stderr, "send fail\n");
		return 0;
	}
	//send path
	send_message(mClient->conn_fd, filepath, strlen(filepath));

	//receive csiebox_protocol_header from server
	csiebox_protocol_header header;
	memset(&header, 0, sizeof(header));
	if (recv_message(mClient->conn_fd, &header, sizeof(header))) {
		if (header.res.magic == CSIEBOX_PROTOCOL_MAGIC_RES &&
			header.res.op == CSIEBOX_PROTOCOL_OP_SYNC_META ){
				if(header.res.status == CSIEBOX_PROTOCOL_STATUS_OK){
					fprintf(stderr, "Receive OK from server\n");
					return 1;
				}
				else if(header.res.status == CSIEBOX_PROTOCOL_STATUS_MORE){
					fprintf(stderr, "Receive MORE from server\n");
					// sync file to server
					long datalen;
					char* buffer;
					size_t result;
					FILE * pFile = fopen ( filepath , "rb" );;
					if (pFile!=NULL){
						// obtain file size:
						fseek (pFile , 0 , SEEK_END);
						datalen = ftell (pFile);
						rewind (pFile);
						// allocate memory to contain the whole file:
						buffer = (char*) malloc (sizeof(char)*datalen);
						// copy the file into the buffer:
						result = fread (buffer,1,datalen,pFile);
						fclose(pFile);
					}
					// setup request
					csiebox_protocol_file req;
					memset(&req, 0, sizeof(req));
					req.message.header.req.magic = CSIEBOX_PROTOCOL_MAGIC_REQ;
					req.message.header.req.op = CSIEBOX_PROTOCOL_OP_SYNC_FILE;
					req.message.header.req.datalen = sizeof(req) - sizeof(req.message.header);
					req.message.body.datalen = datalen;
					strcpy(req.message.body.filename, filepath);
					req.message.body.stat = *info;
					// printf("Data length %d\n", req.message.body.datalen);
					if (!send_message(mClient->conn_fd, &req, sizeof(req))) {
						fprintf(stderr, "send fail\n");
						return 0;
					}
					send_message(mClient->conn_fd, buffer, datalen);
					return 1;
				}
				return 0;		
		}	
		else{
			return 0;
		}
	}
	return 0;
}

int sync_meta(char *filepath,  struct stat *info, int typeflag){
	csiebox_protocol_meta req;
	memset(&req, 0, sizeof(req));
	req.message.header.req.magic = CSIEBOX_PROTOCOL_MAGIC_REQ;
	req.message.header.req.op = CSIEBOX_PROTOCOL_OP_SYNC_META;
	req.message.header.req.datalen = sizeof(req) - sizeof(req.message.header);
	req.message.body.pathlen = strlen(filepath);
	req.message.body.stat = *info;
	// lstat(filepath, &req.message.body.stat);
	// regular file, compute md5
	if(typeflag == FTW_F)
		md5_file(filepath, req.message.body.hash);
	if (!send_message(mClient->conn_fd, &req, sizeof(req))) {
		fprintf(stderr, "send fail\n");
		return 0;
	}
	//send path
	send_message(mClient->conn_fd, filepath, strlen(filepath));
	//receive csiebox_protocol_header from server
	csiebox_protocol_header header;
	memset(&header, 0, sizeof(header));
	if (recv_message(mClient->conn_fd, &header, sizeof(header))) {
		if (header.res.magic == CSIEBOX_PROTOCOL_MAGIC_RES &&
			header.res.op == CSIEBOX_PROTOCOL_OP_SYNC_META ){
			if(header.res.status == CSIEBOX_PROTOCOL_STATUS_OK){
				fprintf(stderr, "Receive OK from server\n");
				return 1;
			}else{
				fprintf(stderr, "Failed sync\n");
			}
	
		
		}
	}
}

int traverse_entry(const char *filepath, const struct stat *info,
                const int typeflag, struct FTW *pathinfo){
	if (pathinfo->level == 1) base = pathinfo->base;
    if(pathinfo->level > level) {
        level = pathinfo->level;
        memset(longest_path,0,strlen(longest_path));
        strcpy(longest_path,filepath+base);
	}
	char* path = strdup(filepath+base);
	if(strcmp(path,".") == 0) {
		int wd = inotify_add_watch(ifd, ".", IN_CREATE | IN_DELETE | IN_ATTRIB | IN_MODIFY);
		insert_wd(&watch[wdCount], filepath, wd);
		return 0;
	}
    if (typeflag == FTW_SL) {
		char *target;
        size_t  maxlen = 1023;
        ssize_t len;
        while (1) {
            target = malloc(maxlen + 1);
            if (target == NULL)
                return ENOMEM;
            len = readlink(filepath, target, maxlen);
            if (len == (ssize_t)-1) {
                const int saved_errno = errno;
                free(target);
                return saved_errno;
            }
            if (len >= (ssize_t)maxlen) {
                free(target);
                maxlen += 1024;
                continue;
            }
            target[len] = '\0';
            break;
        }
		// printf("[softlink]\t%s -> %s\n", filepath, target);
		sync_link(path, target, info, typeflag);
        free(target);
    } 
    else if (typeflag == FTW_SLN){
		// printf(" %s (dangling symlink)\n", filepath);
	}   
	// Files     
    else if (typeflag == FTW_F){
		// regular file
		if(info->st_nlink == 1){
			// printf("[file] %s\n", path);
			insert_file(&files[fileCount],path, info, typeflag);
			sync_file(path, info, typeflag);
		}
		// hard link
		else{
			// printf("[file] %s (links) %d\n", path, info->st_nlink);
			int exists = 0;
			for(int i = 0; i < fileCount; ++i){
				// printf("%s %d : %s %d\n", path, info->st_ino, files[i].filepath, files[i].info.st_ino);
				if(files[i].info.st_ino == info->st_ino){
					// this file links to someone else!
					// printf("oldname: %s newname %s\n", files[i].filepath, path);
					sync_link(path, files[i].filepath, info, typeflag );
					exists = 1;
					break;
				}
			}
			if(exists == 0){
				insert_file(&files[fileCount],path, info, typeflag);
				sync_file(path, info, typeflag);
			}
		}
	}
    else if (typeflag == FTW_D || typeflag == FTW_DP){
		// printf("[dir] %s\n", path);
		char* name = strdup(filepath+base);
		int wd = inotify_add_watch(ifd, path, IN_CREATE | IN_DELETE | IN_ATTRIB | IN_MODIFY);
		insert_wd(&watch[wdCount], name, wd);
		sync_file(path, info, typeflag);
	}        
    else if (typeflag == FTW_DNR){
		// printf(" %s/ (unreadable)\n", filepath);
	}        
    else{
		// printf(" %s (unknown)\n", filepath);
	}
    return 0;
}


int traverse_directory_tree(const char *const dirpath){
    int result;
    /* Invalid directory path? */
    if (dirpath == NULL || *dirpath == '\0')
        return errno = EINVAL;
    result = nftw(dirpath, traverse_entry, 1, FTW_PHYS);
    if (result >= 0)
        errno = result;
    return errno;
}

/* -- end of treewalk -- */

static int parse_arg(csiebox_client* client, int argc, char** argv);
static int login(csiebox_client* client);

//read config file, and connect to server
void csiebox_client_init(
  csiebox_client** client, int argc, char** argv) {
  csiebox_client* tmp = (csiebox_client*)malloc(sizeof(csiebox_client));
  if (!tmp) {
    fprintf(stderr, "client malloc fail\n");
    return;
  }
  memset(tmp, 0, sizeof(csiebox_client));
  if (!parse_arg(tmp, argc, argv)) {
    fprintf(stderr, "Usage: %s [config file]\n", argv[0]);
    free(tmp);
    return;
  }
  int fd = client_start(tmp->arg.name, tmp->arg.server);
  if (fd < 0) {
    fprintf(stderr, "connect fail\n");
    free(tmp);
    return;
  }
  tmp->conn_fd = fd;
  *client = tmp;
}



//this is where client sends request, you sould write your code here
int csiebox_client_run(csiebox_client* client) {
	// memset
	memset(watch, 0, sizeof watch);
	memset(files, 0, sizeof files);

	// point to client
	mClient = client;

	// inotify setup
	memset(inotifyBuffer, 0, EVENT_BUF_LEN);
	//create a instance and returns a file descriptor
	ifd = inotify_init();
	if (ifd < 0) {
		perror("inotify_init");
	}

	if (!login(client)) {
	fprintf(stderr, "login fail\n");
	return 0;
	}
	fprintf(stderr, "[login success]\n");

	// change dir to 'client->arg.path'
	int ret = chdir (client->arg.path);

	// travese tree
	traverse_directory_tree(".");

	// record longestpath and send
	FILE *longestfile = fopen ("longestPath.txt", "wb");
	fprintf (longestfile, longest_path);
	fclose(longestfile);
	// printf("[long]%s\n", longest_path);
	struct stat buf;
	lstat("longestPath.txt", &buf);
	sync_file("longestPath.txt", &buf, FTW_F);

	int ilength, i = 0;
	while ((ilength = read(ifd, inotifyBuffer, EVENT_BUF_LEN)) > 0) {
		i = 0;
		while (i < ilength) {
			struct inotify_event* event = (struct inotify_event*)&inotifyBuffer[i];
			// if(strlen(event->name) == 0){continue;}
			// find event trigger path, concat
			char new_file[512];
			char* cur_dir;
			for(int i = 0; i < wdCount; ++i){
				if(event->wd == watch[i].wd){
					// fprintf(stderr, "%s (get)\n", watch[i].filepath);
					cur_dir = strdup(watch[i].filepath);
					strcpy(new_file, watch[i].filepath);
					strcat(new_file, "/");
					strcat(new_file, event->name);	
					break;		
				}
			}
			// fprintf(stderr, "(Under Dir) %s (get) %s, ",cur_dir, new_file);
			// printf("event: (%d, %d, %s) type: ", event->wd, strlen(event->name), event->name);
			if(((event->mask & IN_CREATE)  || (event->mask & IN_MODIFY)) && (event->mask & IN_ISDIR)){
				
				int wd = inotify_add_watch(ifd, new_file, IN_CREATE | IN_DELETE | IN_ATTRIB | IN_MODIFY);
				insert_wd(&watch[wdCount], new_file, wd);
				struct stat buf;
				lstat(new_file, &buf);
				
				// if((event->mask & IN_CREATE)) printf("Create ");
				
				// if((event->mask & IN_MODIFY)) printf("Modify ");
				// printf("Dir\n");
				sync_file(new_file, &buf, FTW_D);
			}
			else if( (event->mask & IN_ATTRIB) &&(event->mask & IN_ISDIR) ){
				// if((event->mask & IN_ATTRIB)) fprintf(stderr, "Attr ");
				struct stat buf;
				lstat(new_file, &buf);
				sync_meta(new_file, &buf, FTW_D);

			}
			else if((event->mask & IN_DELETE) &&(event->mask & IN_ISDIR)){
				// printf("[i] Delete Dir\n");
				remove_file(new_file);
			}
			if (!(event->mask & IN_ISDIR)) {
				// if((event->mask & IN_CREATE)) printf("Create ");
				// if((event->mask & IN_MODIFY)) printf("Modify ");
				struct stat info;
				lstat(new_file, &info);
				 // create or modify
				if((event->mask & IN_CREATE) || (event->mask & IN_MODIFY)){
					// file
					if(S_ISREG(info.st_mode)){
						// regular file
						if(info.st_nlink == 1){
							// printf("[file] %s\n", new_file);
							sync_file(new_file, &info, FTW_F);
							insert_file(&files[fileCount],new_file, &info, FTW_F);
						}
						// hard link
						else{
							// printf("[file] %s (links) %d\n", new_file, info.st_nlink);
							int exists = 0;
							for(int i = 0; i < fileCount; ++i){
								// printf("%s %d : %s %d\n", path, info->st_ino, files[i].filepath, files[i].info.st_ino);
								if(files[i].info.st_ino == info.st_ino){
									// this file links to someone else!
									// printf("oldname: %s newname %s\n", files[i].filepath, new_file);
									sync_link(new_file, files[i].filepath, &info, FTW_F );
									exists = 1;
									break;
								}
							}
							if(exists == 0){
								insert_file(&files[fileCount],new_file, &info, FTW_F);
								sync_file(new_file, &info, FTW_F);
							}
						}
					}
					// softlink
					else if(S_ISLNK(info.st_mode)){
						// printf("[softlink] %s \n", new_file);
						char *target;
						size_t  maxlen = 1023;
						ssize_t len;
						while (1) {
							target = malloc(maxlen + 1);
							if (target == NULL)
								return ENOMEM;
							len = readlink(new_file, target, maxlen);
							if (len == (ssize_t)-1) {
								const int saved_errno = errno;
								free(target);
								return saved_errno;
							}
							if (len >= (ssize_t)maxlen) {
								free(target);
								maxlen += 1024;
								continue;
							}
							target[len] = '\0';
							break;
						}
						// printf("[softlink]\t%s -> %s\n", new_file, target);
						sync_link(new_file, target, &info, FTW_SL);
						free(target);
					}
				}
				// attr
				else if((event->mask & IN_ATTRIB)){
					// file
					if(S_ISREG(info.st_mode)){
						// regular file
						// if((event->mask & IN_ATTRIB)) printf("Attr ");
						// printf("%s \n", new_file);
						sync_meta(new_file, &info, FTW_F);
						
					}
					// softlink
					else if(S_ISLNK(info.st_mode)){
						sync_meta(new_file, &info, FTW_SL);
					}
				}
			}
			if((event->mask & IN_DELETE) && !(event->mask & IN_ISDIR)){
				
				// printf("delete file\n");
				remove_file(new_file);
			}
			i += EVENT_SIZE + event->len;
		}
		memset(inotifyBuffer, 0, EVENT_BUF_LEN);
	  }
	return 1;
}

void csiebox_client_destroy(csiebox_client** client) {
  csiebox_client* tmp = *client;
  *client = 0;
  if (!tmp) {
    return;
  }
  close(tmp->conn_fd);
  free(tmp);
}

//read config file
static int parse_arg(csiebox_client* client, int argc, char** argv) {
  if (argc != 2) {
    return 0;
  }
  FILE* file = fopen(argv[1], "r");
  if (!file) {
    return 0;
  }
  fprintf(stderr, "reading config...\n");
  size_t keysize = 20, valsize = 20;
  char* key = (char*)malloc(sizeof(char) * keysize);
  char* val = (char*)malloc(sizeof(char) * valsize);
  ssize_t keylen, vallen;
  int accept_config_total = 5;
  int accept_config[5] = {0, 0, 0, 0, 0};
  while ((keylen = getdelim(&key, &keysize, '=', file) - 1) > 0) {
    key[keylen] = '\0';
    vallen = getline(&val, &valsize, file) - 1;
    val[vallen] = '\0';
    fprintf(stderr, "config (%d, %s)=(%d, %s)\n", keylen, key, vallen, val);
    if (strcmp("name", key) == 0) {
      if (vallen <= sizeof(client->arg.name)) {
        strncpy(client->arg.name, val, vallen);
        accept_config[0] = 1;
      }
    } else if (strcmp("server", key) == 0) {
      if (vallen <= sizeof(client->arg.server)) {
        strncpy(client->arg.server, val, vallen);
        accept_config[1] = 1;
      }
    } else if (strcmp("user", key) == 0) {
      if (vallen <= sizeof(client->arg.user)) {
        strncpy(client->arg.user, val, vallen);
        accept_config[2] = 1;
      }
    } else if (strcmp("passwd", key) == 0) {
      if (vallen <= sizeof(client->arg.passwd)) {
        strncpy(client->arg.passwd, val, vallen);
        accept_config[3] = 1;
      }
    } else if (strcmp("path", key) == 0) {
      if (vallen <= sizeof(client->arg.path)) {
        strncpy(client->arg.path, val, vallen);
        accept_config[4] = 1;
      }
    }
  }
  free(key);
  free(val);
  fclose(file);
  int i, test = 1;
  for (i = 0; i < accept_config_total; ++i) {
    test = test & accept_config[i];
  }
  if (!test) {
    fprintf(stderr, "config error\n");
    return 0;
  }
  return 1;
}

static int login(csiebox_client* client) {
  csiebox_protocol_login req;
  memset(&req, 0, sizeof(req));
  req.message.header.req.magic = CSIEBOX_PROTOCOL_MAGIC_REQ;
  req.message.header.req.op = CSIEBOX_PROTOCOL_OP_LOGIN;
  req.message.header.req.datalen = sizeof(req) - sizeof(req.message.header);
  memcpy(req.message.body.user, client->arg.user, strlen(client->arg.user));
  md5(client->arg.passwd,
      strlen(client->arg.passwd),
      req.message.body.passwd_hash);
  if (!send_message(client->conn_fd, &req, sizeof(req))) {
    fprintf(stderr, "send fail\n");
    return 0;
  }
  csiebox_protocol_header header;
  memset(&header, 0, sizeof(header));
  if (recv_message(client->conn_fd, &header, sizeof(header))) {
    if (header.res.magic == CSIEBOX_PROTOCOL_MAGIC_RES &&
        header.res.op == CSIEBOX_PROTOCOL_OP_LOGIN &&
        header.res.status == CSIEBOX_PROTOCOL_STATUS_OK) {
      client->client_id = header.res.client_id;
      return 1;
    } else {
      return 0;
    }
  }
  return 0;
}
