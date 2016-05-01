#include "FHBB.h"
#include <sys/resource.h>

#define INTEGCHK
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
    ts.str("");
    ts.clear();
    bzero(test, sizeof(test));

    pipe_in(ts, &test, 1024);
    pipe_out(ts, &test, 1024);
    ts.str("");
    ts.clear();
    bzero(test, sizeof(test));

    pipe_in(ts, &test, 1024);
    pipe_out(ts, &test, 1024);
    ts.str("");
    ts.clear();
    bzero(test, sizeof(test));

    delete [] test;
    cin.sync();
    cout.flush();

    install_upkg_android(&me);
    pair <int, int> loc = get_gps();
    send_location_android(&me, loc.first, loc.second);
    them = get_distances_android(&me);
    display_positions(them);

    while (true)
    {
        loc = get_gps();
        //send_ack_android();
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
	stringstream input;
	char * buffer = new char[3];
	bzero(buffer, sizeof(buffer));
	input.str("");
	input.clear();

	cout << "X:";
	cout.flush();

	pipe_in(input, &buffer, 2);
	input >> lat;
    input.str("");
	input.clear();

	cout << "Y:";
	cout.flush();

	pipe_in(input, &buffer, 2);
	input >> lng;
    input.str("");
	input.clear();

	delete [] buffer;

	cin.sync();
	cout.flush();

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

    stream.str("");
    stream.clear();
    bzero(buffer, sizeof(buffer));

    //Read the base from standard input
    pipe_in(stream, &buffer, 1024);

    readContextBase(stream, upk->m, upk->p, upk->r,
                    upk->gens, upk->ords);
    upk->context = new FHEcontext(upk->m, upk->p, upk->r,
                                  upk->gens, upk->ords);
    stream.str("");
    stream.clear();

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
    stream.str("");
    stream.clear();

    if (recv_ack_android() == false)
    {
        cerr << "ABORT ABORT" << endl;
        exit(0);
    }

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

    stream.str("");
    stream.clear();
    bzero(buffer, sizeof(buffer));

    /*Get public key, send location*/
    while (recv_ack_android() == true)
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

    if (recv_ack_android() == false)
    {
        cerr << "ABORT ABORT" << endl;
        exit(0);
    }

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
    stream.str("");
    stream.clear();
    bzero(buffer, sizeof(buffer));

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
    while (pk < 32)
    {
        cin.ignore(1);
        pk = cin.peek();
    }

    char * ack = new char [4];
    char * chk = new char[4];
    bzero(ack, sizeof(ack));
    bzero(chk, sizeof(chk));

    chk[0]='A';
    chk[1]='C';
    chk[2]='K';
    chk[3]='\0';

    cin.get(ack, 4);

#ifdef INTEGCHK
    fstream wtf;
    wtf.open("integ.txt", fstream::out | fstream::app);
    wtf << "<<<RECV-ACK>>>" << endl;
    wtf.write(ack, sizeof(ack));
    wtf << endl;
    wtf.close();
#endif

    if (strcmp(ack, chk) == 0) {
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
void send_ack_android()
{
    cout << "ACK";
    cout.flush();
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

#ifdef INTEGCHK
    fstream wtf;
    wtf.open("integ.txt", fstream::out | fstream::app);
    wtf << "<<<OUTPUT>>>" << endl;
#endif

    do
    {
        x = 0;
        stream.read(*buffer, blocksize);
        x = stream.gcount();
        cout.write(*buffer, x);
        sleep(0.1);
        cout.flush();


#ifdef INTEGCHK
        if (x < blocksize)
        {
            wtf.write(*buffer, x);
            wtf << endl;
        }
#endif // INTEGCHK

        bzero(*buffer, sizeof(*buffer));
    }
    while (x == blocksize);

#ifdef INTEGCHK
    wtf.close();
#endif // INTEGCHK

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

#ifdef INTEGCHK
    fstream wtf;
    wtf.open("integ.txt", fstream::out | fstream::app);
    wtf << "<<<INPUT>>>" << endl;
#endif

    do
    {
        /*Remove any null characters*/
        pk = cin.peek();
        while (pk < 32)
        {
            cin.ignore(1);
            pk = cin.peek();
        }

        x = 0;
        do
        {
            y = 0;
            cin.getline(*buffer, blocksize - x);
            y = cin.gcount() - 1;
            stream.write(*buffer, y);

#ifdef INTEGCHK
            if (y < blocksize)
            {
                wtf.write(*buffer, y);
                wtf << endl;
            }
#endif // INTEGCHK

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

#ifdef INTEGCHK
    wtf.close();
#endif // INTEGCHK
}

/**************************
 *Write some communication/exchanged data
 *to a file as unformatted data to check
 *transmission integrity.
 ***************************/
void integrity_check(char ** buffer, istream &stream, int numbytes,
                    int mode)
{
    fstream wtf;
    wtf.open("integ.txt", fstream::out | fstream::app);
    if (mode)
    {
        wtf << "<<<INPUT>>>" << endl;
        wtf.write(*buffer, numbytes);
        wtf << endl;
    }
    else
    {
        wtf << "<<<OUTPUT>>>" << endl;
        stream.read(*buffer, numbytes);
        wtf.write(*buffer, numbytes);
        wtf << endl;
    }
    wtf.close();
}
