#ifndef FHBB_H
#define FHBB_H

#include "FHinc.h"
#include <NTL/lzz_pXFactoring.h>
#include <NTL/matrix.h>
#include "EncryptedArray.h"

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
pair<int, int> get_gps(char ** buffer);
Ctxt encrypt_location(int x, int y, FHEPubKey &pk);
void display_positions(vector<long> d);

/**
ANDROID FUNCTIONS
**/
void install_upkg_android(UserPackage * upk, char ** buffer);
void send_location_android(UserPackage * upk, int x, int y,
                          char ** buffer);
vector<long> get_distances_android(UserPackage * upk, char ** buffer);

bool recv_ack_android();
void send_ack_android();
void send_nok_android();
void purge_nulls();

void pipe_out(istream &stream, char** buffer, int blocksize);
void pipe_in(ostream &stream, char ** buffer, int blocksize);
#endif
