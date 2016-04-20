#ifndef FHBB_H
#define FHBB_H

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

/**
ALL MODE FUNCTIONS
**/
pair<int, int> get_gps();
Ctxt encrypt_location(int x, int y, FHEPubKey &pk);
void display_positions(vector<long> d);

/**
ANDROID FUNCTIONS
**/
void install_upkg_android(UserPackage * upk);
int send_location_android(UserPackage * upk);
vector<long> get_distances_android(UserPackage * upk);
bool recv_ack_android();
bool send_ack_android();
void pipe_out(istream &stream, char** buffer, int blocksize);
void pipe_in(ostream &stream, char ** buffer, int blocksize);
#endif
