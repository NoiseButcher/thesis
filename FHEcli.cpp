#include "FHEcli.h"
#include <sys/resource.h>

//#define DEBUG
//#define ANDROID
/***********************************
Client side program to be handled as
a stand alone executable.
- Connects to FHEops server.
- Gets a context base to build its private
  scheme.
- Gets the server's public key.
- Can be used in PC and ANDROID mode.
- PC Mode is file-based and allows for debugging
  and parameter checking, all data exchanges between
  the client and server are done with files. Client
  side must enter the GPS co-ords manually.
- ANDROID mode is for when the executable is part of an
  android application as a resource. No files used
  in this mode, as the standard I/O for the program is
  used to stream information to the wrapping application.
 ************************************/
int main(int argc, char * argv[])
{
    /*********************
     *Client instance data
     *structures and buffers
     *for per-instance operation.
     *********************/
    ServerLink op;
    UserPackage me;
    vector<long> them;

    /**
    Check input arguments.
    This only requires server information
    for the PC version. The android version uses
    the i/o to talk to the Java software.
    **/
    if (argc != 3)
    {
        cout << "./FHEcli_x portnum localhost(hostname)" << endl;
        return 0;
    }

#ifndef ANDROID
    /**
    Connect to the server or throw an error if it can't.
    **/
    if (!(prepare_socket(&op, argv)))
    {
        cout << "Server Unavailable.";
        return 0;
    }

    cout << "Connection Established." << endl;

    install_upkg_socket(&op, &me);

    cout << "FHE Scheme installed." << endl;

    cout << "Enter co-ordinates:" << endl;

    pair <int, int> loc = get_gps();

    send_location_socket(&me, &op, loc.first, loc.second);

    cout << "First position sent." << endl;

    them = get_distances_socket(&op, &me);

    cout << "First distance calcs:" << endl;

    display_positions(them, 10);
#else
    install_upkg_android(&me);
#endif

#ifndef ANDROID
    while (true)
    {
        cout << "Enter co-ordinates:" << endl;

        loc = get_gps();

        send_ack(&op);

        send_location_socket(&me, &op, loc.first, loc.second);

        cout << "Location sent." << endl;

        them = get_distances_socket(&op, &me);

        cout << "Distances received." << endl;

        display_positions(them, 10);

        cout << "Distances decoded." << endl;

#else
    while (send_location_android(&me) == 1)
    {
        if (recv_ack_android())
        {
            them = get_distances_android(&me);
            display_positions(them, 10);
        }
#endif // ANDROID
    }

    return 0;
}

/*********LOGISTICS AND FHE FUNCTIONS***********/
/***********ALL MODES***************************/
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
	cin >> lat;
	cout << "Y";
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

#ifndef ANDROID
    cout << "Location encrypted." << endl;
#endif // DEBUG

	return cloc;
}

/*********************
 *Display the proximity
 *of other users. Prints
 *to primary I/O, so only
 *one mode.
 **********************/
void display_positions(vector<long> d, int limit)
{
    int i;
    //for (i=0; i < limit; i++)
    do
    {
        cout << "User " << i << " is ";
        cout << sqrt(d[i]) << "m ";
        cout << "from your position." << endl;
        i++;
    }
    while (d[i] > 0);
}

/*******************SOCKET FUNCTIONS************************/
/*****************************
 *Socket installation package.
 *Downloads the context data from
 *a socket, then builds the local
 *scheme and key pairs.
 *****************************/
void install_upkg_socket(ServerLink * sl, UserPackage * upk)
{
    stringstream ss;
    char * buffer = new char[1025];

#ifndef ANDROID
    cout << "Streaming base from server..." << endl;
#endif

    /**
    Stream in the context base to a local file. Where it
    can be stored for rejoining if need be.
    **/
    socket_to_stream(ss, &buffer, sl, 1024);
    readContextBase(ss, upk->m, upk->p, upk->r, upk->gens, upk->ords);

    ss.str("");
    ss.clear();

#ifndef ANDROID
    cout << "Base data streaming complete." << endl;
#endif

    upk->context = new FHEcontext(upk->m, upk->p, upk->r,
                                  upk->gens, upk->ords);

    if (!send_ack(sl))
    {
#ifdef DEBUG
        cout << "Socket buffer error." << endl;
        exit(0);
#endif
    }

#ifndef ANDROID
    cout << "Streaming context data from server..." << endl;
#endif

    /**
    Stream in the context in blocks of 1KB.
    **/
    socket_to_stream(ss, &buffer, sl, 1024);
    ss >> *upk->context;
    ss.str("");
    ss.clear();

#ifndef ANDROID
    cout << "Context Built." << endl;
#endif

    /**
    Generate client's private and public key pair,
    associated switching matrices encrypted array.
    **/
    upk->secretKey = new FHESecKey(*upk->context);
    upk->publicKey = upk->secretKey;
    upk->G = upk->context->alMod.getFactorsOverZZ()[0];
    upk->secretKey->GenSecKey(64);
    addSome1DMatrices(*upk->secretKey);
    upk->ea = new EncryptedArray(*upk->context, upk->G);
    upk->nslots = upk->ea->size();

#ifndef ANDROID
    cout << "Client FHE scheme Generated." << endl;
#endif

    if (!send_ack(sl))
    {
#ifdef DEBUG
        cout << "Socket buffer error." << endl;
        exit(0);
#endif
    }

#ifndef ANDROID
    cout << "Streaming my public key to the server." << endl;
#endif

    ss << *upk->publicKey;
    stream_to_socket(ss, &buffer, sl ,1024);
    ss.str("");
    ss.clear();

    /**
    Wait for ACK.
    **/
    if (!recv_ack(sl))
    {
#ifdef DEBUG
        cout << "Socket buffer error, no ACK." << endl;
        exit(0);
#endif
    }

#ifndef ANDROID
    cout << "Preliminary install complete." << endl;
#endif

    delete [] buffer;
}

/***********************
 *Connection handling for
 *client side to avoid clutter
 *in the main loop.
 *************************/
int prepare_socket(ServerLink * sl, char * argv[])
{
    sl->port = atoi(argv[1]);

    if ((sl->sockFD = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
    {
        cerr << "Error opening socket." << endl;
		return -1;
    }

    if((sl->serv=gethostbyname(argv[2]))==NULL)
    {
        cerr << "Host not found." << endl;
		return -1;
    }

    memset((char*)&sl->servAddr, 0, sizeof(sl->servAddr));
    sl->servAddr.sin_family = AF_INET;
	memcpy((char *)&sl->serv->h_addr,
            (char *)&sl->servAddr.sin_addr.s_addr,
			sl->serv->h_length);
	sl->servAddr.sin_port = htons(sl->port);

	if ((sl->xfer = connect(sl->sockFD,
                            (struct sockaddr *)&sl->servAddr,
                            sizeof(sl->servAddr))) < 0)
     {
        cerr << "Failure connecting to server." << endl;
		return -1;
     }

     return 1;
}

/************************************
 *Get's GPS co-ords from the standard IO, then open
 *the socket link and do the following for each user
 *on the server:
 *-Receive ACK
 *-Obtain Public Key n
 *-Encrypt my location Enc(n)[x, y]
 *-Return Enc(n)[x, y] to server
 *Once complete, await ACK from server.
 *************************************/
int send_location_socket(UserPackage * upk, ServerLink * sl, int x,
                         int y)
{
    stringstream stream;
    char * buffer = new char[1025];
    int k = 0;

#ifndef ANDROID
    cout << "Sending my encrypted position." << endl;
#endif // DEBUG

    while (recv_ack(sl))
    {
        FHEPubKey * pk = new FHEPubKey(*upk->context);
        socket_to_stream(stream, &buffer, sl, 1024);
        stream >> *pk;
        stream.str("");
        stream.clear();
        Ctxt output(*pk);
        output = encrypt_location(x, y, *pk);
        stream << output;
        stream_to_socket(stream, &buffer, sl, 1024);
        stream.str("");
        stream.clear();
        delete pk;
        k++;
    }

#ifndef ANDROID
    cout << k << " positions transferred." << endl;
#endif // DEBUG

    if (!recv_ack(sl))
    {
#ifdef DEBUG
        cout << "Socket buffer error, no ACK." << endl;
        exit(0);
#endif
    }

    delete [] buffer;

    return 1;
}

/***********************
 *Decrypt a list of
 *Euclidean distances of other users
 *from your position.
 *Sends ACK after all the file has been
 *obtained.
 ***********************/
vector<long> get_distances_socket(ServerLink * sl, UserPackage * upk)
{
    vector<long> d;
    Ctxt encrypted_distances(*upk->publicKey);
    stringstream stream;
    char * buffer = new char[1025];

    socket_to_stream(stream, &buffer, sl, 1024);

    stream >> encrypted_distances;
    stream.str("");
    stream.clear();

    if (!send_ack(sl))
    {
#ifdef DEBUG
        cout << "Socket buffer error." << endl;
        exit(0);
#endif
    }

    upk->ea->decrypt(encrypted_distances, *upk->secretKey, d);

    delete [] buffer;
    return d;
}

/*****************
 *Function to sit on the port when new a new user
 *connects to the system. Once this client's position has
 *been uploaded, each other client must encrypt their own with
 *this new public key.
 *****************/
bool await_server_update(ServerLink * sl)
{
    char * buffer = new char[7];
    char * refbuf = new char[7];
    string n = "UPDATED";
    refbuf = &n[0];

    cout << refbuf << endl;

    stream_from_socket(&buffer, sizeof(buffer), sl);
    if (strcmp(buffer, refbuf) == 0)
    {
        delete [] buffer;
        delete [] refbuf;
        return true;
    }

    delete [] buffer;
    delete [] refbuf;
    return false;
}

/*****
 *Writes a specific length of data from a socket to a buffer.
 *This could possibly be made more efficient by making it
 *into a stream of some sort.
BOTH THIS FUNCTION AND IT'S COMPLEMENT WIPE THE BUFFER AFTER
OPERATIONS.
*****/
int stream_from_socket(char ** buffer, int blocksize, ServerLink * sl)
{
    int p;
    bzero(*buffer, sizeof(*buffer));
    sl->xfer = read(sl->sockFD, *buffer, blocksize);
    p = sl->xfer;
    return p;
}

/*****
Complementary writing function to stream_from_socket().
Pushes the contents of a buffer to the port.
This probably requires some sort of guarantee that data has not
been lost.
*****/
int write_to_socket(char ** buffer, int blocksize, ServerLink * sl)
{
    int p;
    sl->xfer = write(sl->sockFD, *buffer, blocksize);
    p = sl->xfer;
    bzero(*buffer, sizeof(*buffer));
    return p;
}

/*******************************
 *Quick function to help with timing.
 *Sends an ACK command to the server.
 *******************************/
bool send_ack(ServerLink * sl)
{
    char * buffer = new char[4];
    bzero(buffer, sizeof(buffer));
    buffer[0] = 'A';
    buffer[1] = 'C';
    buffer[2] = 'K';
    buffer[3] = '\0';
    if (write_to_socket(&buffer,
                        sizeof(buffer), sl) == sizeof(buffer))
    {
        delete [] buffer;
        return true;
    }
    delete [] buffer;
    return false;
}

/******************************
 *Return boolean value of whether
 *and ACK command has been received.
 ******************************/
bool recv_ack(ServerLink * sl)
{
    char * buffer = new char[4];
    char * ack = new char[4];
    bzero(buffer, sizeof(buffer));
    bzero(ack, sizeof(ack));
    ack[0] = 'A';
    ack[1] = 'C';
    ack[2] = 'K';
    ack[3] = '\0';
    int blk = sizeof(ack);
    if (stream_from_socket(&buffer, sizeof(buffer), sl) == blk)
    {
        if (strcmp(ack, buffer) == 0)
        {
            return true;
            delete [] buffer;
            delete [] ack;
        }
    }
    delete [] buffer;
    delete [] ack;
    return false;
}

/****************************
 *Function to allow data transfer from
 *an input stream to a socket with
 *consistent block size.
 ****************************/
void stream_to_socket(istream &stream, char ** buffer,
                      ServerLink * sl, int blocksize)
{
    bzero(*buffer, sizeof(*buffer));
    int k;

#ifdef DEBUG
    int tx, totalloops;
    totalloops = 0;
    tx = 0;
#endif

    do
    {
        k = 0;
        stream.read(*buffer, blocksize);
        k = stream.gcount();
        write_to_socket(buffer, k, sl);

#ifdef DEBUG
        tx += k;
        totalloops++;
#endif // DEBUG

    }
    while (k == blocksize);

#ifdef DEBUG
        cout << totalloops << " : " << tx << endl;
#endif // DEBUG
}

/******************************
 *Reads blocksize socket data from
 *a TCP socket and writes it to the
 *stream specified as the first argument.
 ******************************/
void socket_to_stream(ostream &stream, char ** buffer,
                      ServerLink * sl, int blocksize)
{
    bzero(*buffer, sizeof(*buffer));
    int k;

#ifdef DEBUG
    int rx, totalloops;
    totalloops = 0;
    rx = 0;
#endif

    do
    {
        k = 0;
        k = stream_from_socket(buffer, blocksize, sl);
        stream.write(*buffer, k);

#ifdef DEBUG
        rx += k;
        totalloops++;
#endif // DEBUG

    }
    while (k == blocksize);

    stream.clear();

#ifdef DEBUG
        cout << totalloops << " : " << rx;
        cout << " : " << stream.tellp() << endl;
#endif // DEBUG
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

    readContextBase(cin, upk->m, upk->p, upk->r,
                    upk->gens, upk->ords);
    upk->context = new FHEcontext(upk->m, upk->p, upk->r,
                                  upk->gens, upk->ords);

    send_ack_android();

    do
    {
        cin.getline(buffer, 1024);
        stream << buffer;
        stream >> *upk->context;

        bzero(buffer, sizeof(buffer));
        stream.str("");
        stream.clear();
    }
    while (cin.gcount() == 1024);

    /**Generate my key pair and switching matrix.**/
    upk->secretKey = new FHESecKey(*upk->context);
    upk->publicKey = upk->secretKey;
    upk->G = upk->context->alMod.getFactorsOverZZ()[0];
    upk->secretKey->GenSecKey(64);
    addSome1DMatrices(*upk->secretKey);
    upk->ea = new EncryptedArray(*upk->context, upk->G);
    upk->nslots = upk->ea->size();

    send_ack_android();

    delete [] buffer;
}

/*****************
 *Actually make this please.
 *****************/
int send_location_android(UserPackage * upk)
{
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
    char * buffer = new char[1024];

    do
    {
        cin.getline(buffer, 1024);
        stream << buffer;
        stream >> encrypted_distances;

        bzero(buffer, sizeof(buffer));
        stream.str("");
        stream.clear();
    }
    while(cin.gcount() == 1024);

    cout << "ACK" << endl;

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
