#ifndef FHEOPS_H
#define FHEOPS_H

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
#include <sstream>
#include <fstream>
#include <iostream>

struct UserPackage {
    unsigned long m;
    unsigned long p;
    unsigned long r;
    vector <long> gens;
    vector <long> ords;
	FHEcontext *context;
	FHESecKey *secretKey;
	EncryptedArray *ea;
	const FHEPubKey *publicKey;
	long nslots;
};

struct ServerData {
    FHEcontext *context;
	FHESecKey *secretKey;
	const FHEPubKey *publicKey;
	EncryptedArray *ea;
	long nslots;
	int users;
};

struct ServerLink {
    int sockFD;
    int port;
    vector <int> links;
    int blocklen;
    socklen_t len;
    struct sockaddr_in clientAddr;
    struct sockaddr_in servAddr;
    int link;
};

int generate_scheme(ServerData * sd);
int generate_upkg(ServerData * sd);
Ctxt generate_output(vector<Ctxt> locs, Ctxt input, ServerData * sd, const FHEPubKey &pk);
Ctxt compute(Ctxt c1, Ctxt c2, const FHEPubKey &pk);
vector<Ctxt> handle_user(vector<Ctxt>  locs, ServerData * sd, Ctxt newusr, string outname, string keyfile);
vector<Ctxt> handle_new_user(vector<Ctxt>  locs, ServerData * sd, Ctxt newusr, string outname, string keyfile);
int prepare_server_socket(ServerLink * sl, char * argv[]);
int stream_from_socket(char ** buffer, int blocksize, ServerLink * sl);
int write_to_socket(char ** buffer, int blocksize, ServerLink * sl);
bool send_ack(ServerLink * sl);
bool recv_ack(ServerLink * sl);

#endif
