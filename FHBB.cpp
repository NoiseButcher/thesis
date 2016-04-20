#include "FHBB.h"
#include <sys/resource.h>

/***********************************
 *Client side black box program designed for mobile
 *systems. Operates as an I/O function box that pipes
 *GPS data in and outputs an encrypted position. Needs
 *to be executed as a child process to something interfacing
 *with the FHE server. All I/O is done via the cin/cout streams.
 ************************************/
int main(int argc, char * argv[])
{
    /*********************
     *Client instance data
     *structures and buffers
     *for per-instance operation.
     *********************/
    UserPackage me;
    vector<long> them;

    /**
    Check input arguments.
    This only requires server information
    for the PC version. The android version uses
    the i/o to talk to the Java software.
    **/
    if (argc != 1)
    {
        cerr << "./FHBB_x" << endl;
        return 0;
    }

    install_upkg_android(&me);
    pair <int, int> loc = get_gps();
    send_location_android(&me, loc.first, loc.second);
    them = get_distances_android(&me);
    display_positions(them);


    while (send_location_android(&me) == 1)
    {
        loc = get_gps();
        send_ack_android();
        send_location_android(&me, loc.first, loc.second);
        them = get_distances_android(&me);
        display_positions(them);
    }

    return 0;
}

/*********LOGISTICS AND FHE FUNCTIONS***********/
/***************************
 *This function expects the
 *latitude and longitude of
 *the client in radians multiplied
 *by 1000.
 *As it is an integer there is no decimal precision.
 **************************/
pair<int, int> get_gps()
{
	int lat, lng;
	string input;

	cout << "X";
	cin.ignore();
	cin >> lat;
	cout << "Y";
	cin.ignore();
	cin >> lng;

	return make_pair(lat, lng);
}

/*************
 *Encrypt the client's current GPS location
 *with the someone's public key.
***************/
Ctxt encrypt_location(int x, int y, FHEPubKey &pk)
{
	vector<long> loc;

	loc.push_back((long)x);
	loc.push_back((long)y);

	for (int i = 2; i < pk.getContext().ea->size(); i++)
    {
		loc.push_back(0);
	}

	Ctxt cloc(pk);

	pk.getContext().ea->encrypt(cloc, pk, loc);

	return cloc;
}

/*********************
 *Display the proximity
 *of other users. Prints
 *to primary I/O, so only
 *one mode.
 **********************/
void display_positions(vector<long> d)
{
    int i;

    do
    {
        cout << "User " << i << " is ";
        cout << sqrt(d[i]) << "m ";
        cout << "from your position." << endl;
        i++;
    }
    while (d[i] > 0);
}

/*************ANDROID FUNCTIONS***************/
/*******************
 *Android mode install.
 *Reads from standard i/o.
 *******************/
void install_upkg_android(UserPackage * upk)
{
    stringstream stream;
    char * buffer = new char[1025];

    //Read the base from standard input
    readContextBase(cin, upk->m, upk->p, upk->r,
                    upk->gens, upk->ords);
    upk->context = new FHEcontext(upk->m, upk->p, upk->r,
                                  upk->gens, upk->ords);

    send_ack_android();

    //Get the context from standard input
    pipe_in(stream, &buffer, 1024);
    stream >> *upk->context;
    stream.str("");
    stream.clear();

    /**Generate my key pair and switching matrix.**/
    upk->secretKey = new FHESecKey(*upk->context);
    upk->publicKey = upk->secretKey;
    upk->G = upk->context->alMod.getFactorsOverZZ()[0];
    upk->secretKey->GenSecKey(64);
    addSome1DMatrices(*upk->secretKey);
    upk->ea = new EncryptedArray(*upk->context, upk->G);
    upk->nslots = upk->ea->size();

    send_ack_android();

    //Send my public key to standard output
    stream << *upk->publicKey;
    pipe_out(stream, &buffer, 1024);
    ss.str("");
    ss.clear();

    send_ack_android();

    delete [] buffer;
}

/*****************
 *Sends my location, encrypted with all of the public keys,
 *to the FHBB cout file descriptor.
 *****************/
int send_location_android(UserPackage * upk, int x, int y)
{
    stringstream stream;
    char * buffer = new char[1025];

    //Get public key, send location
    while (recv_ack_android())
    {
        FHEPubKey * pk = new FHEPubKey(*upk->context);
        pipe_in(stream, &buffer, 1024);
        stream >> *pk;
        stream.str("");
        stream.clear();
        Ctxt output(*pk);
        output = encrypt_location(x, y, *pk);
        stream << output;
        pipe_out(stream, &buffer, 1024);
        stream.str("");
        stream.clear();
        delete pk;
    }

    recv_ack_android();

    delete [] buffer;

    return 1;
}

/***********************
 *Android mode distance recovery.
 **********************/
vector<long> get_distances_android(UserPackage * upk)
{
    vector<long> d;
    stringstream stream;
    Ctxt encrypted_distances(*upk->publicKey);
    char * buffer = new char[1025];

    pipe_in(stream, &buffer, 1024);

    stream >> encrypted_distances;
    stream.str("");
    stream.clear();

    send_ack_android();

    upk->ea->decrypt(encrypted_distances, *upk->secretKey, d);

    delete [] buffer;
    return d;
}

/****************
 *Android ACK check
 *for standard in.
 ****************/
bool recv_ack_android()
{
    string ack;
    cin >> ack;
    if (ack == "ACK") {
        return true;
    }
    return false;
}

/****************
 *Thy is this even a function.
 ****************/
bool send_ack_android()
{
    cout << "ACK";
}

void pipe_out(istream &stream, char** buffer, int blocksize)
{
    bzero(*buffer, sizeof(*buffer));
    int x;

    do
    {
        x = 0;
        stream.read(*buffer, blocksize);
        x = stream.gcount();
        cout.write(*buffer, blocksize);
        bzero(*buffer, sizeof(*buffer));
    }
    while (x == blocksize);
}

void pipe_in(ostream &stream, char ** buffer, int blocksize)
{
    bzero(*buffer, sizeof(*buffer));
    int x;

    do
    {
        x = 0;
        cin.read(*buffer, blocksize);
        x = cin.gcount();
        stream.write(*buffer, blocksize);
        bzero(*buffer, sizeof(*buffer));
    }
    while (x == blocksize);

    stream.clear();
}
