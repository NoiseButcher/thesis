#ifndef FHESRV_H
#define FHESRV_H

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
#include <semaphore.h>

struct Cluster {
    FHEPubKey *thisKey;
    vector<Ctxt> thisLoc;
    vector <Ctxt> theirLocs;
};

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
	vector<Cluster> cluster;
	int users;
	int currentuser;
	vector<pthread_t> threadID;
	pthread_mutex_t mutex;
	pthread_cond_t myturn;
	pthread_barrier_t barrier;
	sem_t kickittome;
};

struct ServerLink {
    int sockFD;
    int port;
    int blocklen;
    socklen_t len;
    struct sockaddr_in clientAddr;
    struct sockaddr_in servAddr;
};

struct ClientLink {
    pthread_t id;
    int thisClient;
    ServerData * server;
    int sockFD;
    int xfer;
};

/**
GENERIC FUNCTIONS
FHE OPERATION FUNCTIONS
**/
int generate_scheme(ServerData * sd);
Ctxt generate_output(Ctxt input, vector<Ctxt> locs,
                     const FHEPubKey &pk);
Ctxt compute(Ctxt c1, Ctxt c2, const FHEPubKey &pk);
void *handle_client(void *param);

/**
SOCKET-BASED FUNCTIONS
**/
void generate_upkg_android(ServerData * sd, ClientLink * sl);
int prepare_server_socket(ServerLink * sl, char * argv[]);
int stream_from_socket(char ** buffer, int blocksize,
                       ClientLink * sl);
int write_to_socket(char ** buffer, int blocksize, ClientLink * sl);
bool send_ack(ClientLink * sl);
bool send_nak(ClientLink * sl);
bool recv_ack(ClientLink * sl);
void handle_user_socket(ServerData * sd, ClientLink * sl, int id);
void handle_new_user_socket(ServerData * sd, ClientLink * sl,
                            int id);
void stream_to_socket(istream &stream, char ** buffer,
                      ClientLink * sl, int blocksize);
void socket_to_stream(ostream &stream, char ** buffer,
                      ClientLink * sl, int blocksize);
#endif
