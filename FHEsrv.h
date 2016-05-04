#ifndef FHESRV_H
#define FHESRV_H

#include <unistd.h>
#include <NTL/lzz_pXFactoring.h>
#include <NTL/matrix.h>
#include "FHE.h"
#include "EncryptedArray.h"
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <netdb.h>
#include <math.h>
#include <sstream>
#include <fstream>
#include <iostream>
#include <exception>
/**DATA STRUCTURES**/
/*******************************************************************
*Class for storing client specific information, the amount of these
*will be equal to the amount of clients on the system at any one time.
*********************************************************************/
class Cluster {
public:
    /*Client X's public key.*/
    FHEPubKey *thisKey;
    /*Client X's location encrypted with every client's public key*/
    vector<Ctxt> thisLoc;
    /*Every client's location encrypted with Client X's public key.
     *With the exception of Client X's own location*/
    vector <Ctxt> theirLocs;
    /*Constructor for this class and public key*/
    Cluster(FHEcontext& context);
};
Cluster::Cluster(FHEcontext& context)
{
    thisKey = new FHEPubKey(context);
}

/**************************************************************
*Primary server data structure, contains all necessary shared
*information; FHE scheme data, client keys and positions, thread
*id and synchronisation data, user tracking data.
***************************************************************/
struct ServerData {
    FHEcontext *context;
	vector<Cluster> cluster;
	vector<pthread_t> threadID;
    int users;
	int maxthreads;
	pthread_mutex_t mutex;
	pthread_barrier_t popcap;
};

/*******************************************************************
*Data structure to handle the server side connection to all of the
*clients, one instance should exist per server. "listen()ing" struct.
*********************************************************************/
struct ServerLink {
    int sockFD;
    int port;
    int blocklen;
    socklen_t len;
    struct sockaddr_in clientAddr;
    struct sockaddr_in servAddr;
};

/******************************************************************
*Data structure to handle client side connection within each thread.
*Each client is allocated one of these "accept()ing" struct.
*******************************************************************/
struct ClientLink {
    pthread_t id;
    int thisClient;
    ServerData * server;
    int sockFD;
    int xfer;
};
/**FUNCTION PRIMITIVES**/
/**LOGISTICS**/
void generate_upkg(ServerData * sd, ClientLink * sl, char ** buffer);
void *handle_client(void *param);
void *handle_population(void *param);
void get_client_position(ServerData * sd, ClientLink * sl, int id,
                         char ** buffer);
void calculate_distances(ServerData * sd, ClientLink * sl, int id,
                         char ** buffer);
/**FHE**/
int generate_scheme(ServerData * sd);
Ctxt generate_output(Ctxt input, vector<Ctxt> locs,
                     const FHEPubKey &pk);
Ctxt compute(Ctxt c1, Ctxt c2, const FHEPubKey &pk);
/**NETWORKING**/
int prepare_server_socket(ServerLink * sl, char * argv[]);
int stream_from_socket(char ** buffer, int blocksize,
                       ClientLink * sl);
int write_to_socket(char ** buffer, int blocksize, ClientLink * sl);
bool send_ack(ClientLink * sl);
bool send_nak(ClientLink * sl);
bool recv_ack(ClientLink * sl);
bool sock_handshake(ClientLink * sl);
void stream_to_socket(istream &stream, char ** buffer,
                      ClientLink * sl, int blocksize);
void socket_to_stream(ostream &stream, char ** buffer,
                      ClientLink * sl, int blocksize);
#endif
