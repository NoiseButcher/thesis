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

    stringstream ts;
    char * test = new char[1025];

    pipe_in(ts, &test, 1024);
    pipe_out(ts, &test, 1024);

    install_upkg_android(&me);
    pair <int, int> loc = get_gps();
    send_location_android(&me, loc.first, loc.second);
    them = get_distances_android(&me);
    display_positions(them);

    while (true)
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
    fstream wtf;
    char * buffer = new char[1025];

    //Read the base from standard input
    pipe_in(stream, &buffer, 1024);

    wtf.open("cbase.txt", fstream::out | fstream::trunc);
    wtf << stream.str();
    wtf << endl;
    wtf << "Base!";
    wtf.close();
    sleep(3);

    readContextBase(stream, upk->m, upk->p, upk->r,
                    upk->gens, upk->ords);
    upk->context = new FHEcontext(upk->m, upk->p, upk->r,
                                  upk->gens, upk->ords);
    stream.str("");
    stream.clear();

    send_ack_android();

    //Get the context from standard input
    pipe_in(stream, &buffer, 1024);

    wtf.open("context.txt", fstream::out | fstream::trunc);
    wtf << stream.str();
    wtf << endl;
    wtf << "Context!";
    wtf.close();
    sleep(3);

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
    stream.str("");
    stream.clear();

    recv_ack_android();

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
    int pk = cin.peek();
    if (pk == '\0')
    {
        cin.ignore(1);
    }
    char * ack = new char [5];
    char * chk = new char[5];

    chk[0]='A';
    chk[1]='C';
    chk[2]='K';
    chk[3]='\0';

    cin.getline(ack, 5);
    cin.sync();

    if (ack == chk) {
        delete [] chk;
        delete [] ack;
        return true;
    }
    delete [] chk;
    delete [] ack;
    return false;
}

/****************
 *This is a seperate function for continuity
 *purposes only.
 ****************/
bool send_ack_android()
{
    cout << "ACK";
}

/**************************
 *Write data to standard out in blocks of
 *[blocksize]. flush stdout once the operations
 *are complete.
 ***************************/
void pipe_out(istream &stream, char** buffer, int blocksize)
{
    bzero(*buffer, sizeof(*buffer));
    int x;
    cin.sync();

    do
    {
        x = 0;
        stream.read(*buffer, blocksize);
        x = stream.gcount();
        cout.write(*buffer, x);
        bzero(*buffer, sizeof(*buffer));
        //recv_ack_android();
        sleep(0.1);
    }
    while (x == blocksize);

}

/**************************
 *Read blocks of data from stdin, ideally in [blocksize]
 *chunks. Because read() operations on cin do not register
 *the same termination flags as sockets and standard streams,
 *this function uses a nested loop with getline to extract
 *the correct amount of data.
 **************************/
void pipe_in(ostream &stream, char ** buffer, int blocksize)
{
    bzero(*buffer, sizeof(*buffer));
    int x, y, pk;

    do
    {
        pk = cin.peek();
        if (pk == '\0')
        {
            cin.ignore(1);
        }

        x = 0;
        do
        {
            y = 0;
            cin.getline(*buffer, blocksize - x);
            y = cin.gcount() - 1;
            stream.write(*buffer, y);
            bzero(*buffer, sizeof(*buffer));
            x += y;
            pk = cin.peek();

            if ((x < blocksize) && (pk > 31))
            {
                stream << endl;
                x++;
            }
        }
        while ((x <= blocksize) && (pk > 31) && (pk < 127));

        send_ack_android();
        sleep(0.1);

    }
    while (x == blocksize);
    stream.clear();
}
