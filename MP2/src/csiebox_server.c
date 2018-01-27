#include "csiebox_server.h"

#include "csiebox_common.h"
#include "connect.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdlib.h>
#include <utime.h>
#include <sys/time.h>



static int parse_arg(csiebox_server* server, int argc, char** argv);
static void handle_request(csiebox_server* server, int conn_fd);
static int get_account_info(
  csiebox_server* server,  const char* user, csiebox_account_info* info);
static void login(
  csiebox_server* server, int conn_fd, csiebox_protocol_login* login);
static void logout(csiebox_server* server, int conn_fd);
static char* get_user_homedir(
  csiebox_server* server, csiebox_client_info* info);

#define DIR_S_FLAG (S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH)//permission you can use to create new file
#define REG_S_FLAG (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)//permission you can use to create new directory



char* username;

int file_exist (char *filename){
  struct stat   buffer;   
  return (stat (filename, &buffer) == 0);
}


//read config file, and start to listen
void csiebox_server_init(
  csiebox_server** server, int argc, char** argv) {
  csiebox_server* tmp = (csiebox_server*)malloc(sizeof(csiebox_server));
  if (!tmp) {
    fprintf(stderr, "server malloc fail\n");
    return;
  }
  memset(tmp, 0, sizeof(csiebox_server));
  if (!parse_arg(tmp, argc, argv)) {
    fprintf(stderr, "Usage: %s [config file]\n", argv[0]);
    free(tmp);
    return;
  }
  int fd = server_start();
  if (fd < 0) {
    fprintf(stderr, "server fail\n");
    free(tmp);
    return;
  }
  tmp->client = (csiebox_client_info**)
      malloc(sizeof(csiebox_client_info*) * getdtablesize());
  if (!tmp->client) {
    fprintf(stderr, "client list malloc fail\n");
    close(fd);
    free(tmp);
    return;
  }
  memset(tmp->client, 0, sizeof(csiebox_client_info*) * getdtablesize());
  tmp->listen_fd = fd;
  *server = tmp;
}

//wait client to connect and handle requests from connected socket fd
int csiebox_server_run(csiebox_server* server) {
  int conn_fd, conn_len;
  struct sockaddr_in addr;
  while (1) {
    memset(&addr, 0, sizeof(addr));
    conn_len = 0;
    // waiting client connect
    conn_fd = accept(
      server->listen_fd, (struct sockaddr*)&addr, (socklen_t*)&conn_len);
    if (conn_fd < 0) {
      if (errno == ENFILE) {
          fprintf(stderr, "out of file descriptor table\n");
          continue;
        } else if (errno == EAGAIN || errno == EINTR) {
          continue;
        } else {
          fprintf(stderr, "accept err\n");
          fprintf(stderr, "code: %s\n", strerror(errno));
          break;
        }
    }
    // handle request from connected socket fd
    handle_request(server, conn_fd);
  }
  return 1;
}

void csiebox_server_destroy(csiebox_server** server) {
  csiebox_server* tmp = *server;
  *server = 0;
  if (!tmp) {
    return;
  }
  close(tmp->listen_fd);
  int i = getdtablesize() - 1;
  for (; i >= 0; --i) {
    if (tmp->client[i]) {
      free(tmp->client[i]);
    }
  }
  free(tmp->client);
  free(tmp);
}

//read config file
static int parse_arg(csiebox_server* server, int argc, char** argv) {
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
  int accept_config_total = 2;
  int accept_config[2] = {0, 0};
  while ((keylen = getdelim(&key, &keysize, '=', file) - 1) > 0) {
    key[keylen] = '\0';
    vallen = getline(&val, &valsize, file) - 1;
    val[vallen] = '\0';
    fprintf(stderr, "config (%d, %s)=(%d, %s)\n", keylen, key, vallen, val);
    if (strcmp("path", key) == 0) {
      if (vallen <= sizeof(server->arg.path)) {
        strncpy(server->arg.path, val, vallen);
        accept_config[0] = 1;
      }
    } else if (strcmp("account_path", key) == 0) {
      if (vallen <= sizeof(server->arg.account_path)) {
        strncpy(server->arg.account_path, val, vallen);
        accept_config[1] = 1;
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

//It is a sample function
//you may remove it after understanding
int sampleFunction(int conn_fd, csiebox_protocol_meta* meta) {
  
  printf("In sampleFunction:)\n");
  uint8_t hash[MD5_DIGEST_LENGTH];
  memset(&hash, 0, sizeof(hash));
  md5_file(".gitignore", hash);
  // for(int i = 0; i < MD5_DIGEST_LENGTH; i++){
  //     fprintf (stderr ,"%02x", hash[i]);      
  // }
  // printf("%d %d\n", MD5_DIGEST_LENGTH, strlen(hash));
  printf("pathlen: %d\n", meta->message.body.pathlen);
  // for(int i = 0; i < MD5_DIGEST_LENGTH; i++){
  //   fprintf (stderr ,"%02x",  meta->message.body.hash[i]);      
  // }
  // cout << meta->message.body.hash << "!!!" << hash << endl;
  if (memcmp(hash, meta->message.body.hash, MD5_DIGEST_LENGTH) == 0) {
    printf("hashes are equal!\n");
  }

  //use the pathlen from client to recv path 
  char buf[400];
  memset(buf, 0, sizeof(buf));
  recv_message(conn_fd, buf, meta->message.body.pathlen);
  printf("This is the path from client:%s\n", buf);

  //send OK to client
  csiebox_protocol_header header;
  memset(&header, 0, sizeof(header));
  header.res.magic = CSIEBOX_PROTOCOL_MAGIC_RES;
  header.res.op = CSIEBOX_PROTOCOL_OP_SYNC_META;
  header.res.datalen = 0;
  header.res.status = CSIEBOX_PROTOCOL_STATUS_MORE;
  if(!send_message(conn_fd, &header, sizeof(header))){
    fprintf(stderr, "send fail\n");
    return 0;
  }

  return 1;
}

int syncMeta(int conn_fd, csiebox_protocol_meta* meta) {
	
	// printf("[Read meta]\n");
	
	//use the pathlen from client to recv path 
	char path[512];
	memset(path, 0, sizeof(path));
	recv_message(conn_fd, path, meta->message.body.pathlen);
	// fprintf(stderr,"{%s}  ",path);
	uint8_t hash[MD5_DIGEST_LENGTH];
	memset(&hash, 0, sizeof(hash));

	csiebox_protocol_header header;
	memset(&header, 0, sizeof(header));
	header.res.magic = CSIEBOX_PROTOCOL_MAGIC_RES;
	header.res.op = CSIEBOX_PROTOCOL_OP_SYNC_META;
	header.res.datalen = 0;
	
	
	if (file_exist (path)){
		
		if(S_ISREG(meta->message.body.stat.st_mode)){
			fprintf(stderr,"file exists and ");
			md5_file(path, hash);
			if (memcmp(hash, meta->message.body.hash, MD5_DIGEST_LENGTH) == 0) {
				fprintf(stderr,"hashes are equal!\n");
				//status OK to client
				header.res.status = CSIEBOX_PROTOCOL_STATUS_OK;

				// update stat
				if (chmod(path, meta->message.body.stat.st_mode)){
					perror("chmod");
				}
				struct utimbuf new_times;
				new_times.actime = meta->message.body.stat.st_atime; /* keep atime unchanged */
				new_times.modtime = meta->message.body.stat.st_mtime;    /* set mtime to current time */
				
				if(utime(path, &new_times) == 0){
					fprintf(stderr, "sync regular file time\n");
				}
			}
			else{
				fprintf(stderr,"hashes not equal!\n");
				header.res.status = CSIEBOX_PROTOCOL_STATUS_MORE;
			}
		}
		else if(S_ISDIR(meta->message.body.stat.st_mode)){
			fprintf(stderr,"dir exists\n ");
			header.res.status = CSIEBOX_PROTOCOL_STATUS_OK;
			// update stat
			if (chmod(path, meta->message.body.stat.st_mode)){
				perror("chmod");
			}
			struct utimbuf new_times;
			new_times.actime = meta->message.body.stat.st_atime; /* keep atime unchanged */
			new_times.modtime = meta->message.body.stat.st_mtime;    /* set mtime to current time */
			if(utime(path, &new_times) == 0){
				fprintf(stderr, "sync dir time\n");
			}
		}	
		else if(S_ISLNK(meta->message.body.stat.st_mode)){
			fprintf(stderr,"soft link exists\n ");

			struct utimbuf new_times;
			new_times.actime = meta->message.body.stat.st_atime; /* keep atime unchanged */
			new_times.modtime = meta->message.body.stat.st_mtime;    /* set mtime to current time */
			if(utime(path, &new_times) == 0){
				fprintf(stderr, "sync soft link time\n");
			}

		}

	}
	else{
		if(S_ISDIR(meta->message.body.stat.st_mode)){
			fprintf(stderr, "is dir and it doesn't exist\n");
			header.res.status = CSIEBOX_PROTOCOL_STATUS_OK;
			// create dir
			mkdir(path, meta->message.body.stat.st_mode);
			if (chmod(path, meta->message.body.stat.st_mode)){
				perror("chmod");
      }
			struct utimbuf new_times;
			new_times.actime = meta->message.body.stat.st_atime; /* keep atime unchanged */
			new_times.modtime = meta->message.body.stat.st_mtime;    /* set mtime to current time */
			if(utime(path, &new_times) == 0){
				fprintf(stderr, "sync dir time\n");
			}
			
			
		}
	
		else if(S_ISREG(meta->message.body.stat.st_mode)){
			fprintf (stderr,"is file and dont exists\n");
			// status send more

			header.res.status = CSIEBOX_PROTOCOL_STATUS_MORE;
		}
		
	}
	if(!send_message(conn_fd, &header, sizeof(header))){
		fprintf(stderr, "send fail\n");
		return 0;
  }else{
    fprintf(stderr, "sent OK!\n");
  }

	printf("\n");
	return 1;
  }

//this is where the server handle requests, you should write your code here
static void handle_request(csiebox_server* server, int conn_fd) {
  
  csiebox_protocol_header header;
  memset(&header, 0, sizeof(header));
  
  while (recv_message(conn_fd, &header, sizeof(header))) {
    if (header.req.magic != CSIEBOX_PROTOCOL_MAGIC_REQ) {
      continue;
    }
    switch (header.req.op) {
	  case CSIEBOX_PROTOCOL_OP_LOGIN:
		// chdir("../../bin");
        fprintf(stderr, "[login]\n");
		csiebox_protocol_login req;
        if (complete_message_with_header(conn_fd, &header, &req)) {
          login(server, conn_fd, &req);
		}
		char sdir_user_dir[512];
        strcpy(sdir_user_dir, server->arg.path);
        strcat(sdir_user_dir, "/");
		strcat(sdir_user_dir, username);
		// printf("%s !", sdir_user_dir);
		
		// change directory to sdir/user		
        chdir(sdir_user_dir);
        break;
	case CSIEBOX_PROTOCOL_OP_SYNC_META:
        ;
      
        // printf("%s !", username);
        fprintf(stderr, "[sync meta]\n");
        csiebox_protocol_meta meta;
        if (complete_message_with_header(conn_fd, &header, &meta)) {
          
        //This is a sample function showing how to send data using defined header in common.h
        //You can remove it after you understand
		// sampleFunction(conn_fd, &meta);
			syncMeta(conn_fd, &meta);
			//====================
			//        TODO
			//====================
        }
        break;
	case CSIEBOX_PROTOCOL_OP_SYNC_FILE:
        fprintf(stderr, "[sync file]\n");
        csiebox_protocol_file file;
        if (complete_message_with_header(conn_fd, &header, &file)) {
			//====================
			//        TODO
			//====================
			// wait for recv datalen header
			// fprintf(stderr, "file: {%s} len: %d\n\n", file.message.body.filename, file.message.body.datalen);
			int datalen = file.message.body.datalen;
			char* buffer;
			buffer = (char*) malloc (sizeof(char)*datalen);
			recv_message(conn_fd, buffer, datalen);
			// write file
			FILE * f = fopen(file.message.body.filename, "wb");
			fwrite(buffer, datalen, 1, f);
			fclose(f);

			// chmod sync st_mode
			if (chmod(file.message.body.filename, file.message.body.stat.st_mode)){
				perror("chmod");
			}
			// sync time
			struct utimbuf new_times;
			new_times.actime = file.message.body.stat.st_atime; /* keep atime unchanged */
			new_times.modtime = file.message.body.stat.st_mtime;    /* set mtime to current time */
			if(utime(file.message.body.filename, &new_times) == 0){
				fprintf(stderr, "success update time\n");
			}
			
			
        }
        break;
	case CSIEBOX_PROTOCOL_OP_SYNC_HARDLINK:
        fprintf(stderr, "[sync link]\n");
        csiebox_protocol_hardlink hardlink;
        if (complete_message_with_header(conn_fd, &header, &hardlink)) {
			// fprintf(stderr, "srclen: %d targetlen: %d ", hardlink.message.body.srclen, hardlink.message.body.targetlen);
		}
		char srcPath[512];
		char targetPath[512];
		memset(srcPath, 0, sizeof(srcPath));
		memset(targetPath, 0, sizeof(targetPath));
		recv_message(conn_fd, srcPath, hardlink.message.body.srclen);
		recv_message(conn_fd, targetPath, hardlink.message.body.targetlen);
		// fprintf(stderr, "src: {%s} target: {%s}\n\n", srcPath, targetPath);
		// create link (oldname, newname)
		// HARDLINK
		if(header.req.status == CSIEBOX_PROTOCOL_STATUS_OK){
			link(targetPath, srcPath);
		}
		else if(header.req.status == CSIEBOX_PROTOCOL_STATUS_MORE){
      symlink(targetPath, srcPath);
			// sync time
			struct timeval new_times[2];

			new_times[0].tv_sec = (long)hardlink.message.body.stat.st_atime; /* keep atime unchanged */
			new_times[0].tv_usec = 0;
			new_times[1].tv_sec = (long)hardlink.message.body.stat.st_mtime;    /* set mtime to current time */
			new_times[1].tv_usec = 0;
			if(lutimes(srcPath, new_times) == 0){
				fprintf(stderr, "soft link sync time\n");
			}
			
		}
		 

        break;
      case CSIEBOX_PROTOCOL_OP_SYNC_END:
        fprintf(stderr, "[sync end]\n");
        csiebox_protocol_header end;
          //====================
          //        TODO
          //====================
        break;
      case CSIEBOX_PROTOCOL_OP_RM:
        fprintf(stderr, "[rm]\n");
        csiebox_protocol_rm rm;
        if (complete_message_with_header(conn_fd, &header, &rm)) {
			// fprintf(stderr, "targetlen: %d ", rm.message.body.pathlen);
			char targetPath[512];		
			memset(targetPath, 0, sizeof(targetPath));
			recv_message(conn_fd, targetPath, rm.message.body.pathlen);
			// fprintf(stderr, "target: {%s} len: {%d}\n", targetPath, rm.message.body.pathlen);
			if(remove(targetPath) == 0){
				csiebox_protocol_header header;
				memset(&header, 0, sizeof(header));
				header.res.magic = CSIEBOX_PROTOCOL_MAGIC_RES;
				header.res.op = CSIEBOX_PROTOCOL_OP_RM;
				header.res.datalen = 0;
				header.res.status = CSIEBOX_PROTOCOL_STATUS_OK;
				if(!send_message(conn_fd, &header, sizeof(header))){
					fprintf(stderr, "send fail\n");
					return 0;
				}
			}
			
    
        }
        break;
      default:
        fprintf(stderr, "[unknown op] %x\n", header.req.op);
        break;
    }
  }
  fprintf(stderr, "[end of connection]\n\n");
  logout(server, conn_fd);
}

//open account file to get account information
static int get_account_info(
  csiebox_server* server,  const char* user, csiebox_account_info* info) {
  FILE* file = fopen(server->arg.account_path, "r");
  if (!file) {
    return 0;
  }
  size_t buflen = 100;
  char* buf = (char*)malloc(sizeof(char) * buflen);
  memset(buf, 0, buflen);
  ssize_t len;
  int ret = 0;
  int line = 0;
  while ((len = getline(&buf, &buflen, file) - 1) > 0) {
    ++line;
    buf[len] = '\0';
    char* u = strtok(buf, ",");
    if (!u) {
      fprintf(stderr, "illegal form in account file, line %d\n", line);
      continue;
    }
    if (strcmp(user, u) == 0) {
      memcpy(info->user, user, strlen(user));
      char* passwd = strtok(NULL, ",");
      if (!passwd) {
        fprintf(stderr, "illegal form in account file, line %d\n", line);
        continue;
      }
      md5(passwd, strlen(passwd), info->passwd_hash);
      ret = 1;
      break;
    }
  }
  free(buf);
  fclose(file);
  return ret;
}

//handle the login request from client
static void login(
  csiebox_server* server, int conn_fd, csiebox_protocol_login* login) {
  int succ = 1;
  csiebox_client_info* info =
    (csiebox_client_info*)malloc(sizeof(csiebox_client_info));
  memset(info, 0, sizeof(csiebox_client_info));
  username = login->message.body.user;
  if (!get_account_info(server, login->message.body.user, &(info->account))) {
    fprintf(stderr, "cannot find account\n");
    succ = 0;
  }
  if (succ &&
      memcmp(login->message.body.passwd_hash,
             info->account.passwd_hash,
             MD5_DIGEST_LENGTH) != 0) {
    fprintf(stderr, "passwd miss match\n");
    succ = 0;
  }

  csiebox_protocol_header header;
  memset(&header, 0, sizeof(header));
  header.res.magic = CSIEBOX_PROTOCOL_MAGIC_RES;
  header.res.op = CSIEBOX_PROTOCOL_OP_LOGIN;
  header.res.datalen = 0;
  if (succ) {
    if (server->client[conn_fd]) {
      free(server->client[conn_fd]);
    }
    info->conn_fd = conn_fd;
    server->client[conn_fd] = info;
    header.res.status = CSIEBOX_PROTOCOL_STATUS_OK;
    header.res.client_id = info->conn_fd;
    char* homedir = get_user_homedir(server, info);
    mkdir(homedir, DIR_S_FLAG);
    free(homedir);
  } else {
    header.res.status = CSIEBOX_PROTOCOL_STATUS_FAIL;
    free(info);
  }
  send_message(conn_fd, &header, sizeof(header));
}

static void logout(csiebox_server* server, int conn_fd) {
  free(server->client[conn_fd]);
  server->client[conn_fd] = 0;
  close(conn_fd);
}

static char* get_user_homedir(
  csiebox_server* server, csiebox_client_info* info) {
  char* ret = (char*)malloc(sizeof(char) * PATH_MAX);
  memset(ret, 0, PATH_MAX);
  sprintf(ret, "%s/%s", server->arg.path, info->account.user);
  return ret;
}

