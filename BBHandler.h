#ifndef BBHANDLER_H
#define BBHANDLER_H

#include <unistd.h>
#include <stdlib.h>
#include <argp.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <netdb.h>
#include <math.h>
#include <fstream>
#include <sstream>

/**********
 *Data structure to handle server connections.
 *Not valid in android mode as all communications
 *are done by the Java software at user level.
 *********/
struct ServerLink
{
    int sockFD;
    int port;
    int xfer;
    struct hostent *serv;
    struct sockaddr_in servAddr;
};

/**
ALL MODE FUNCTIONS
**/
pair<int, int> get_gps();
void display_positions(vector<long> d);

/**
COMMUNICATION FUNCTIONS
**/
int prepare_socket(ServerLink * sl, char * argv[]);
int stream_from_socket(char ** buffer, int blocksize, ServerLink * sl);
int write_to_socket(char ** buffer, int blocksize, ServerLink * sl);
void handler_to_socket(istream &stream, char ** buffer,
                      ServerLink * sl, int blocksize);
void socket_to_handler(ostream &stream, char ** buffer,
                      ServerLink * sl, int blocksize);
void handler_to_pipe(istream &stream, int fd, char** buffer,
                     int blocksize);
void pipe_to_handler(ostream &stream, int fd, char ** buffer,
                     int blocksize);
void pipe_to_socket(int fd, char ** buffer, ServerLink * sl,
                    int blocksize);
void socket_to_pipe((int fd, char ** buffer, ServerLink * sl,
                    int blocksize);
#endif
