#ifndef PROX_H
#define PROX_H

#include <unistd.h>
#include <NTL/lzz_pXFactoring.h>
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
	FHEPubKey *serverKey;
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
UNIVERSAL FUNCTIONS
**/
pair<int, int> get_gps_x();
Ctxt encrypt_location_x(int x, int y, UserPackage * upk);
void display_positions(vector<long> d, int cnt);

/**
FILE BASED FUNCTIONS
**/
void install_upkg(UserPackage * upk, string basefile,
                  string ctxfile, string pkfile);
int send_location(UserPackage * upk, string outfile, string keyfile);
vector<long> get_distances(string infile, UserPackage * upk);

/**
SOCKET FUNCTIONS
**/
int prepare_socket(ServerLink * sl, char * argv[]);
int stream_from_socket(char ** buffer, int blocksize, ServerLink * sl);
int write_to_socket(char ** buffer, int blocksize, ServerLink * sl);
bool send_ack(ServerLink * sl);
bool recv_ack(ServerLink * sl);
int send_location_socket(UserPackage * upk, ServerLink * sl);
void install_upkg_socket(ServerLink * sl, UserPackage * upk);
vector<long> get_distances_socket(ServerLink * sl, UserPackage * upk);

/**
ANDROID FUNCTIONS
**/
vector<long> get_distances_android(UserPackage * upk);
int send_location_android(UserPackage * upk);
void install_upkg_android(UserPackage * upk);
bool recv_ack_android();
#endif
