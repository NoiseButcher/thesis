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
#include <semaphore.h>

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
	int currentuser;
	vector<Ctxt> positions;
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
    int xfer;
};

struct ClientLink {
    pthread_t id;
    int thisClient;
    ServerLink link;
    ServerData * server;
};

struct QueueHandle {
    ServerData * server;
};

/**
GENERIC FUNCTIONS
FHE OPERATION FUNCTIONS
**/
int generate_scheme(ServerData * sd);
Ctxt generate_output(Ctxt input, ServerData * sd,
                     const FHEPubKey &pk);
Ctxt compute(Ctxt c1, Ctxt c2, const FHEPubKey &pk);
void *handle_client(void *param);
void *handle_queue(void *param);

/**
FILE BASED FUNCTIONS
**/
int generate_upkg(ServerData * sd);
vector<Ctxt> handle_user(ServerData * sd, Ctxt newusr,
                         string outname, string keyfile);
vector<Ctxt> handle_new_user(ServerData * sd, Ctxt newusr,
                             string outname, string keyfile);

/**
SOCKET-BASED FUNCTIONS
**/
void generate_upkg_android(ServerData * sd, ServerLink * sl);
int prepare_server_socket(ServerLink * sl, char * argv[]);
int stream_from_socket(char ** buffer, int blocksize, ServerLink * sl);
int write_to_socket(char ** buffer, int blocksize, ServerLink * sl);
bool send_ack(ServerLink * sl);
bool send_nak(ServerLink * sl);
bool recv_ack(ServerLink * sl);
void handle_user_socket(ServerData * sd, ServerLink * sl, int id);
void handle_new_user_socket(ServerData * sd, ServerLink * sl);
void stream_to_socket(istream &stream, char ** buffer,
                      ServerLink * sl, int blocksize);
void socket_to_stream(ostream &stream, char ** buffer,
                      ServerLink * sl, int blocksize);
#endif
