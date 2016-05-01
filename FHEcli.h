#ifndef FHECLI_H
#define FHECLI_H

#include <unistd.h>
#include <NTL/lzz_pXFactoring.h>
#include <NTL/matrix.h>
#include "FHE.h"
#include "EncryptedArray.h"
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
#include <iostream>

struct UserPackage
{
    unsigned long m;
    unsigned long p;
    unsigned long r;
    vector <long> gens;
    vector <long> ords;
	FHEcontext *context;
	FHESecKey *secretKey;
	EncryptedArray *ea;
	FHEPubKey *publicKey;
	long nslots;
	ZZX G;
};

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
Ctxt encrypt_location(int x, int y, FHEPubKey &pk);
void display_positions(vector<long> d);

/**
SOCKET FUNCTIONS
**/
int prepare_socket(ServerLink * sl, char * argv[]);
int stream_from_socket(char ** buffer, int blocksize, ServerLink * sl);
int write_to_socket(char ** buffer, int blocksize, ServerLink * sl);
bool send_ack(ServerLink * sl);
bool recv_ack(ServerLink * sl);
bool sock_handshake(ServerLink * sl);
int send_location_socket(UserPackage * upk, ServerLink * sl, int x,
                         int y);
void install_upkg_socket(ServerLink * sl, UserPackage * upk);
vector<long> get_distances_socket(ServerLink * sl, UserPackage * upk);
void stream_to_socket(istream &stream, char ** buffer,
                      ServerLink * sl, int blocksize);
void socket_to_stream(ostream &stream, char ** buffer,
                      ServerLink * sl, int blocksize);
#endif
