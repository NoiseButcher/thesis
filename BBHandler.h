#ifndef BBHANDLER_H
#define BBHANDLER_H

#include <unistd.h>
#include <NTL/lzz_pXFactoring.h>
#include <NTL/matrix.h>
#include "FHE.h"
#include "EncryptedArray.h"
#include <stdlib.h>
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
#include <iostream>

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
/*
void get_gps_handler(int infd, int outfd, char ** buffer,
                       int blocksize);
void display_positions_handler(int infd, int outfd, char ** buffer,
                               int blocksize);
*/
void get_gps_handler(int infd, int outfd, char ** buffer,
                       int blocksize, stringstream &stream);
void display_positions_handler(int infd, int outfd, char ** buffer,
                               int blocksize, stringstream &stream);
void install_upkg_handler(int infd, int outfd, char ** buffer,
                          ServerLink * sl, int blocksize);
void send_location_handler(int infd, int outfd, char ** buffer,
                          ServerLink * sl, int blocksize);
void get_distance_handler(int infd, int outfd, char ** buffer,
                          ServerLink * sl, int blocksize);

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
void handler_to_pipe(istream &stream, int infd, int outfd,
                     char** buffer, int blocksize);
void pipe_to_handler(ostream &stream, int infd, int outfd,
                     char ** buffer, int blocksize);
void pipe_to_socket(int infd, int outfd, char ** buffer,
                    ServerLink * sl, int blocksize);
void socket_to_pipe(int infd, int outfd, char ** buffer,
                    ServerLink * sl, int blocksize);
bool send_ack_pipe(int infd);
bool send_nak_pipe(int infd);
bool recv_ack_pipe(int outfd);
bool send_ack_socket(ServerLink * sl);
bool recv_ack_socket(ServerLink * sl);
void terminate_pipe_msg(int infd);
#endif
