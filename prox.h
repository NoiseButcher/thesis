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

struct UserPackage {
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

struct ServerLink {
    int sockFD;
    int port;
    int xfer;
    struct hostent *serv;
    struct sockaddr_in servAddr;
};

pair<int, int> get_gps_x();
Ctxt encrypt_location_x(int x, int y, UserPackage * upk);
void install_upkg(UserPackage * upk, string basefile, string ctxfile, string pkfile);
int send_location(UserPackage * upk, string outfile, string keyfile);
vector<long> get_distances(string infile, UserPackage * upk);
void display_positions(vector<long> d, int cnt);
int prepare_socket(ServerLink * sl, char * argv[]);
int stream_from_socket(char ** buffer, int blocksize, ServerLink * sl);
int write_to_socket(char ** buffer, int blocksize, ServerLink * sl);
bool send_ack(ServerLink * sl);
bool recv_ack(ServerLink * sl);

//ANDROID SPECIFIC
int send_location_android(UserPackage * upk, ServerLink * sl);
void install_upkg_android(ServerLink * sl, UserPackage * upk);
vector<long> get_distances_android(ServerLink * sl, UserPackage * upk);

#endif
