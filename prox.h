#ifndef PROX_H
#define PROX_H

#include <fstream>
#include <unistd.h>
#include <NTL/lzz_pXFactoring.h>
#include "FHE.h"
#include "EncryptedArray.h"
#include <stdlib.h>
#include <argp.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <netdb.h>
#include <math.h>
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
};

struct ServerLink {
    int sockFD;
    int port;
    int link;
    struct hostent *serv;
    struct sockaddr_in servAddr;
};

pair<int, int> get_gps_x();
Ctxt encrypt_location_x(int x, int y, UserPackage * upk);
void install_upkg(UserPackage * upk, string basefile, string ctxfile, string pkfile);
int send_location(UserPackage * upk, string outfile, string keyfile);
vector<long> get_distances(string infile, UserPackage * upk);
void display_positions(vector<long> d);
int prepare_socket(ServerLink * sl, char * argv[]);

#endif
