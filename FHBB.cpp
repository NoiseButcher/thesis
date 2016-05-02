#include "FHBB.h"
#include <sys/resource.h>

//#define INTEGCHK
/***********************************
 *Client side black box program designed for mobile
 *systems. Operates as an I/O function box that pipes
 *GPS data in and outputs an encrypted position. Needs
 *to be executed as a child process to something interfacing
 *with the FHE server. All I/O is done via the cin/cout streams.
 ************************************/
int main(int argc, char * argv[])
{
    UserPackage me;
    vector<long> them;
    if (argc != 1)
    {
        cerr << "./FHBB_x" << endl;
        return 0;
    }

#ifdef INTEGCHK
    stringstream ts;
    char * test = new char[1026];
    ts.str("");
    ts.clear();
    bzero(test, sizeof(test));
    fstream wtf;
    wtf.open("integ.txt", fstream::out | fstream::trunc);
    wtf << "INTEGRITY CHECK FILE" << endl;
    wtf.close();
    pipe_in_dbg(ts, &test, 1025);
    pipe_out_dbg(ts, &test, 1024);
    ts.str("");
    ts.clear();
    bzero(test, sizeof(test));
    pipe_in_dbg(ts, &test, 1025);
    pipe_out_dbg(ts, &test, 1024);
    ts.str("");
    ts.clear();
    bzero(test, sizeof(test));
    delete [] test;
    cin.sync();
    cout.flush();
#endif

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
	stringstream input;
	char * buffer = new char[1026];
	bzero(buffer, sizeof(buffer));
	input.str("");
	input.clear();
	cout << "X:";
	cout.flush();

#ifndef INTEGCHK
    pipe_in(input, &buffer, 1025);
    sleep(0.1);
#else
    pipe_in_dbg(input, &buffer, 1025);
    sleep(0.1);
#endif // INTEGCHK

	input >> lat;
    input.str("");
	input.clear();
	cout << "Y:";
	cout.flush();

#ifndef INTEGCHK
    pipe_in(input, &buffer, 1025);
    sleep(0.1);
#else
    pipe_in_dbg(input, &buffer, 1025);
    sleep(0.1);
#endif // INTEGCHK

	input >> lng;
    input.str("");
	input.clear();
	delete [] buffer;
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
    recv_ack_android();
}

/*************ANDROID FUNCTIONS***************/
/*******************
 *Android mode install.
 *Reads from standard i/o.
 *******************/
void install_upkg_android(UserPackage * upk)
{
    stringstream stream;
    char * buffer = new char[1026];
    stream.str("");
    stream.clear();
    bzero(buffer, sizeof(buffer));

#ifndef INTEGCHK
    pipe_in(stream, &buffer, 1025);
#else
    pipe_in_dbg(stream, &buffer, 1025);
#endif // INTEGCHK

    readContextBase(stream, upk->m, upk->p, upk->r,
                    upk->gens, upk->ords);
    upk->context = new FHEcontext(upk->m, upk->p, upk->r,
                                  upk->gens, upk->ords);
    stream.str("");
    stream.clear();
    send_ack_android();

#ifndef INTEGCHK
    pipe_in(stream, &buffer, 1025);
#else
    pipe_in_dbg(stream, &buffer, 1025);
#endif // INTEGCHK

    stream >> *upk->context;
    stream.str("");
    stream.clear();
    upk->secretKey = new FHESecKey(*upk->context);
    upk->publicKey = upk->secretKey;
    upk->G = upk->context->alMod.getFactorsOverZZ()[0];
    upk->secretKey->GenSecKey(64);
    addSome1DMatrices(*upk->secretKey);
    upk->ea = new EncryptedArray(*upk->context, upk->G);
    upk->nslots = upk->ea->size();
    send_ack_android();
    stream << *upk->publicKey;

#ifndef INTEGCHK
    pipe_out(stream, &buffer, 1024);
#else
    pipe_out_dbg(stream, &buffer, 1024);
#endif // INTEGCHK

    stream.str("");
    stream.clear();
    delete [] buffer;
    if (!recv_ack_android())
    {
        cerr << "ABORT ABORT" << endl;
        exit(0);
    }
}

/*****************
 *Sends my location, encrypted with all of the public keys,
 *to the FHBB cout file descriptor.
 *****************/
int send_location_android(UserPackage * upk, int x, int y)
{
    stringstream stream;
    char * buffer = new char[1026];
    stream.str("");
    stream.clear();
    bzero(buffer, sizeof(buffer));
    while (recv_ack_android() == true)
    {
        FHEPubKey * pk = new FHEPubKey(*upk->context);

#ifndef INTEGCHK
        pipe_in(stream, &buffer, 1025);
#else
        pipe_in_dbg(stream, &buffer, 1025);
#endif // INTEGCHK

        stream >> *pk;
        stream.str("");
        stream.clear();
        Ctxt output(*pk);
        output = encrypt_location(x, y, *pk);
        stream << output;

#ifndef INTEGCHK
        pipe_out(stream, &buffer, 1024);
#else
        pipe_out_dbg(stream, &buffer, 1024);
#endif // INTEGCHK

        stream.str("");
        stream.clear();
        delete pk;
    }
    if (!recv_ack_android())
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
    char * buffer = new char[1026];
    stream.str("");
    stream.clear();
    bzero(buffer, sizeof(buffer));

#ifndef INTEGCHK
    pipe_in(stream, &buffer, 1025);
#else
    pipe_in_dbg(stream, &buffer, 1025);
#endif // INTEGCHK

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
    purge_nulls();
    char * ack = new char [4];
    char * chk = new char[4];
    bzero(ack, sizeof(ack));
    bzero(chk, sizeof(chk));
    chk[0]='A';
    chk[1]='C';
    chk[2]='K';
    chk[3]='\0';
    cin.get(ack, 4);
    if (cin.fail()) cin.clear();
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

/*****************
 *Function to check the
 *stdin buffer for null characters
 *and remove them.
 *****************/
void purge_nulls()
{
    int pk;
    pk = cin.peek();
    while (pk < 32)
    {
        cin.ignore(1);
        pk = cin.peek();
    }
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
        cout.flush();
        bzero(*buffer, sizeof(*buffer));
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
    int x, y, pk, charlim;
    charlim = blocksize - 1;
    do
    {
        x = 0;
        do
        {
            y = 0;
            pk = 0;
            purge_nulls();
            cin.getline(*buffer, blocksize - x);
            if (cin.fail()) cin.clear();
            y = cin.gcount() - 1;
            stream.write(*buffer, y);
            x += y;
            pk = cin.peek();
            if ((pk == 10) && (x == (charlim-1)))
            {
                cin.ignore(2);
                stream << endl;
                x++;
                pk = cin.peek();
            }
            if ((x < charlim) && (pk > 31))
            {
                stream << endl;
                x++;
            }
            bzero(*buffer, sizeof(*buffer));
        }
        while ((x < charlim) && (pk > 31) && (pk < 127));
        send_ack_android();
    }
    while (x == charlim);
    stream.clear();
}

/**DEBUG PIPE THING**/
void pipe_in_dbg(ostream &stream, char ** buffer, int blocksize)
{
    bzero(*buffer, sizeof(*buffer));
    int x, y, pk, charlim;
    charlim = blocksize - 1;
    fstream wtf;
    wtf.open("integ.txt", fstream::out | fstream::app);
    wtf << "<<<INPUT>>>" << endl;

    do
    {
        x = 0;
        do
        {
            pk = 0;
            y = 0;
            purge_nulls();
            cin.getline(*buffer, blocksize - x);
            if (cin.fail()) cin.clear();
            y = cin.gcount() - 1;
            stream.write(*buffer, y);
            if (y < charlim) wtf.write(*buffer, y);
            x += y;
            pk = cin.peek();
            if ((pk == 10) && (x == (charlim-1)))
            {
                cin.ignore(2);
                stream << endl;
                wtf << endl;
                x++;
                pk = cin.peek();
            }
            if ((x < charlim) && (pk > 31))
            {
                stream << endl;
                wtf << endl;
                x++;
            }
            bzero(*buffer, sizeof(*buffer));
        }
        while ((x < charlim) && (pk > 31) && (pk < 127));
        send_ack_android();
    }
    while (x == charlim);
    stream.clear();
    wtf << endl;
    wtf << x << " Bytes DL on exit, pk val: " << pk << endl;
    wtf.close();
}

/**PIPE OUTPUT DEBUG**/
void pipe_out_dbg(istream &stream, char** buffer, int blocksize)
{
    bzero(*buffer, sizeof(*buffer));
    int x;
    cin.sync();
    fstream wtf;
    wtf.open("integ.txt", fstream::out | fstream::app);
    wtf << "<<<OUTPUT>>>" << endl;
    do
    {
        x = 0;
        stream.read(*buffer, blocksize);
        x = stream.gcount();
        cout.write(*buffer, x);
        cout.flush();
        if (x < blocksize)
        {
            wtf.write(*buffer, x);
            wtf << endl;
            wtf << x << "B << OUT" << endl;
        }
        bzero(*buffer, sizeof(*buffer));
    }
    while (x == blocksize);
    wtf.close();
}
