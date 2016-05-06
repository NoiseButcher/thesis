#include "FHEcli.h"
#include <sys/resource.h>

#define BUFFSIZE    1024
//#define MEMTEST
//#define TRANSFER
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
    ServerLink op;
    UserPackage me;
    vector<long> them;
    char * buffer = new char[BUFFSIZE+1];
    bzero(buffer, sizeof(buffer));
    if (argc != 3)
    {
        cout << "./FHEcli_x portnum localhost(hostname)" << endl;
        return 0;
    }
    if (prepare_socket(&op, argv) != 1)
    {
        exit(2);
    }
    cout << "Connection Established." << endl;
    install_upkg_socket(&op, &me, &buffer);

#ifdef MEMTEST
    fstream fs;
    fs.open("upkg.mem", fstream::out | fstream::app);
    fs << "Size of FHE scheme = " << sizeof(me) << "B" << endl;
    fs.close();
#endif // MEMTEST

    cout << "Enter co-ordinates:" << endl;
    pair <int, int> loc = get_gps();
    send_location_socket(&me, &op, loc.first, loc.second, &buffer);
    them = get_distances_socket(&op, &me, &buffer);
    display_positions(them);
    while (true)
    {
        cout << "Enter co-ordinates:" << endl;
        loc = get_gps();
        send_ack(&op);
        send_location_socket(&me, &op, loc.first,
                             loc.second, &buffer);
        them = get_distances_socket(&op, &me, &buffer);
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
	cout << "X:";
	cin >> lat;
	cout << "Y:";
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

#ifdef MEMTEST
    fstream fs;
    fs.open("upkg.mem", fstream::out | fstream::app);
    fs << "Size of Encrypted Location = " << sizeof(cloc);
    fs << "B" << endl;
    fs.close();
#endif // MEMTEST

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

/*******************SOCKET FUNCTIONS************************/
/*****************************
 *Socket installation package.
 *Downloads the context data from
 *a socket, then builds the local
 *scheme and key pairs.
 *****************************/
void install_upkg_socket(ServerLink * sl, UserPackage * upk,
                         char ** buffer)
{
    stringstream ss;
    bzero(*buffer, sizeof(*buffer));
    cout << "Streaming base from server..." << endl;
    socket_to_stream(ss, buffer, sl, BUFFSIZE);
    readContextBase(ss, upk->m, upk->p, upk->r, upk->gens, upk->ords);
    ss.str("");
    ss.clear();
    upk->context = new FHEcontext(upk->m, upk->p, upk->r,
                                  upk->gens, upk->ords);
    send_ack(sl);
    cout << "Streaming context data from server..." << endl;
    socket_to_stream(ss, buffer, sl, BUFFSIZE);
    ss >> *upk->context;
    ss.str("");
    ss.clear();
    upk->secretKey = new FHESecKey(*upk->context);
    upk->publicKey = upk->secretKey;
    upk->G = upk->context->alMod.getFactorsOverZZ()[0];
    upk->secretKey->GenSecKey(64);
    addSome1DMatrices(*upk->secretKey);
    upk->ea = new EncryptedArray(*upk->context, upk->G);
    upk->nslots = upk->ea->size();
    send_ack(sl);
    cout << "Streaming my public key to the server." << endl;
    ss << *upk->publicKey;
    stream_to_socket(ss, buffer, sl, BUFFSIZE);
    ss.str("");
    ss.clear();
    recv_ack(sl);
    cout << "Preliminary install complete." << endl;
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
        cerr << "Socket error: socket()" << endl;
		return 2;
    }
    if((sl->serv=gethostbyname(argv[2]))==NULL)
    {
        cerr << "Socket error: gethostbyname()" << endl;
		return 2;
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
        cerr << "Socket error: connect()" << endl;
		return 2;
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
                         int y, char ** buffer)
{
    stringstream stream;
    bzero(*buffer, sizeof(*buffer));
    int k = 0;
    cout << "Sending my encrypted position." << endl;
    while (sock_handshake(sl))
    {
        FHEPubKey * pk = new FHEPubKey(*upk->context);
        socket_to_stream(stream, buffer, sl, BUFFSIZE);
        stream >> *pk;
        stream.str("");
        stream.clear();
        Ctxt output(*pk);
        output = encrypt_location(x, y, *pk);
        stream << output;
        stream_to_socket(stream, buffer, sl, BUFFSIZE);
        stream.str("");
        stream.clear();
        delete pk;
        k++;
    }
    cout << k << " positions transferred." << endl;
    recv_ack(sl);
    return 1;
}

/***********************
 *Decrypt a list of
 *Euclidean distances of other users
 *from your position.
 *Sends ACK after all the file has been
 *obtained.
 ***********************/
vector<long> get_distances_socket(ServerLink * sl, UserPackage * upk,
                                  char ** buffer)
{
    vector<long> d;
    Ctxt encrypted_distances(*upk->publicKey);
    stringstream stream;
    bzero(*buffer, sizeof(*buffer));
    socket_to_stream(stream, buffer, sl, BUFFSIZE);
    stream >> encrypted_distances;
    stream.str("");
    stream.clear();

#ifdef MEMTEST
    fstream fs;
    fs.open("upkg.mem", fstream::out | fstream::app);
    fs << "Size of Encrypted Distances = ";
    fs << sizeof(encrypted_distances) << "B" << endl;
    fs.close();
#endif // MEMTEST

    send_ack(sl);
    upk->ea->decrypt(encrypted_distances, *upk->secretKey, d);
    return d;
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
    bzero(*buffer, sizeof(*buffer));
    return sl->xfer = read(sl->sockFD, *buffer, blocksize);
}

/*****
Complementary writing function to stream_from_socket().
Pushes the contents of a buffer to the port.
This probably requires some sort of guarantee that data has not
been lost.
*****/
int write_to_socket(char ** buffer, int blocksize, ServerLink * sl)
{
    sl->xfer = write(sl->sockFD, *buffer, blocksize);
    bzero(*buffer, sizeof(*buffer));
    return sl->xfer;
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
            delete [] buffer;
            delete [] ack;
            return true;
        }
    }
    delete [] buffer;
    delete [] ack;
    return false;
}

/********************
 *Awaits a received ACK before then sending
 *one in return.
 *Necessary to break up data transmission and
 *prevent the FHBB pipes from flooding.
 *******************/
bool sock_handshake(ServerLink * sl)
{
    if (!recv_ack(sl)) return false;
    if (!send_ack(sl)) return false;
    return true;
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

#ifdef TRANSFER
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

#ifdef TRANSFER
        tx += k;
        totalloops++;
#endif // TRANSFER

    }
    while (k == blocksize);
    if (!recv_ack(sl))
    {
        cerr << "ACK error: Server to Client" << endl;
        exit(4);
    }
#ifdef TRANSFER
        cout << totalloops << " : " << tx << endl;
#endif // TRANSFER
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

#ifdef TRANSFER
    int rx, totalloops;
    totalloops = 0;
    rx = 0;
#endif

    do
    {
        k = 0;
        k = stream_from_socket(buffer, blocksize, sl);
        stream.write(*buffer, k);

#ifdef TRANSFER
        rx += k;
        totalloops++;
#endif // TRANSFER

    }
    while (k == blocksize);
    stream.clear();
    send_ack(sl);
#ifdef TRANSFER
        cout << totalloops << " : " << rx;
        cout << " : " << stream.tellp() << endl;
#endif // TRANSFER
}
