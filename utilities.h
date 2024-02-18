#ifndef UTILITIES_H
#define UTILITIES_H

#define LIST 0x0
#define DOWNLOAD 0x1
#define UPLOAD 0x2
#define DELETE 0X4
#define MOVE 0x8
#define UPDATE 0x10
#define SEARCH 0x20

#define EXIT 0x80 

#define SIZE 1024
#define SUCCESS 0x0
#define FILE_NOT_FOUND 0x1 
#define PERMISSION_DENIED 0x2
#define OUT_OF_MEMORY 0x4
#define SERVER_BUSY 0x8
#define UNKNOWN_OPERATION 0x10
#define BAD_ARGUMENTS 0x20
#define OTHER_ERROR 0x40

#define MAX_FILE_PATH 1024 // cati octeti poate avea un fisier in calea sa
#define PORT 8080
#define CLIENT_DIRECTORY "client-files"
#define SERVER_DIRECTORY "server-files"


#include <sys/epoll.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <signal.h>
#include <time.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <libgen.h> // pt. functia basename() cu care extrag numele fisierului dintr-o cale
#include <dirent.h>
#include <stdio.h>
#include <sys/sendfile.h>


void createDirectories(char dir[128]) {
    char tmp[128];
    char *p = NULL;
    size_t len;

    snprintf(tmp, sizeof(tmp),"%s",dir);
    len = strlen(tmp);
    if (tmp[len - 1] == '/')
        tmp[len - 1] = '\0';
    for (p = tmp + 1; *p; p++)
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, 0777);
            *p = '/';
        }
    mkdir(tmp, 0777);
}


#endif 
