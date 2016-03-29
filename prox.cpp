#include "prox.h"
#include <sys/resource.h>

#define DEBUG
#define PC
//#define SOCKETMODE
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
    struct stat checkFile;
    char * buffer = new char[4096];

    /**
    Sanity-check buffer & transfer volume zeroing
    **/
    bzero(buffer, sizeof(buffer));
    op.xfer = 0;

#ifdef PC
    /**
    Filenames for file-based
    PC mode operations.
    **/
    string f1 = "Prox.Base";
    string f2 = "Prox.Ctxt";
    string f3 = "Prox.PubKey";
    string f4 = "Prx.Loc";
    string f5 = "Prx.Dis";
    string f6 = "Cli.PubKey";
#endif
    /**
    Check input arguments.
    This only requires server information
    for the PC version. The android version uses
    the i/o to talk to the Java software.
    **/
    if (argc != 3)
    {
        cout << "./prox_x portnum localhost(hostname)" << endl;
        return 0;
    }

    /**
    Connect to the server or throw an error if it can't.
    **/
    if (!(prepare_socket(&op, argv)))
    {
        cout << "Server Unavailable.";
        return 0;
    }

#ifdef DEBUG
    cout << "Connection Established." << endl;
#endif // DEBUG

#ifdef PC
    install_upkg_socket(&op, &me);
#elif ANDROID
    install_upkg_android(&me);
#endif

    if (!send_ack(&op))
    {
#ifdef DEBUG
        cout << "Socket buffer error." << endl;
        exit(0);
#endif
    }

#ifdef DEBUG
    cout << "FHE Scheme installed." << endl;
#endif // DEBUG

#ifdef PC
    while (send_location_socket(&me, &op) == 1)
#elif SOCKETMODE
    while (send_location_android(&me) == 1)
#endif // SOCKETMODE
    {
        if (!recv_ack(&op))
        {
            cout << "No ACK received." << endl;
            exit(0);
        }

#ifdef DEBUG
    cout << "Location Sent." << endl;
#endif // DEBUG

#ifdef PC
        //Wait for the server to reply with confirmation that
        //the location has been processed.
        while(buffer[0] != 'K')
        {
            bzero(buffer, sizeof(buffer));
            while ((op.xfer = read(op.sockFD,
                                   buffer,
                                   sizeof(buffer))) < 1);
            op.xfer = 0;
        }

        //Panic button for first user. The file will not exist.
        if ((stat(&f5[0], &checkFile) == 0) == true)
        {

            them = get_distances(f5, &me);
#ifdef DEBUG
    cout << "Distances Received." << endl;
#endif // DEBUG

            display_positions(them, 10);
#ifdef DEBUG
    cout << "Distances Decoded." << endl;
#endif // DEBUG
        }

#elif ANDROID
    while (send_location_android(&me) == 1)
    {
        them = get_distances_android(&me);
        display_positions(them, 10);
#endif // ANDROID

    }

    return 0;
}

/*******************************
 *Build an encryption/decryption
 *data structure for a user.
 *******************************/
void install_upkg(UserPackage * upk, string basefile,
                  string ctxfile, string pkfile)
{
    fstream fs;
    fs.open(&basefile[0], fstream::in);
    readContextBase(fs, upk->m, upk->p, upk->r, upk->gens, upk->ords);
    upk->context = new FHEcontext(upk->m, upk->p, upk->r,
                                  upk->gens, upk->ords);
    fs.close();

#ifdef DEBUG
    cout << "Base built." << endl;
#endif // DEBUG

    fs.open(&ctxfile[0], fstream::in);
    fs >> *upk->context;
    fs.close();

#ifdef DEBUG
    cout << "Context copied." << endl;
#endif // DEBUG

    //Generate my key pair and switching matrix.
    upk->secretKey = new FHESecKey(*upk->context);
    upk->publicKey = upk->secretKey;
    upk->G = upk->context->alMod.getFactorsOverZZ()[0];
    upk->secretKey->GenSecKey(64);
    addSome1DMatrices(*upk->secretKey);

    //Get the server's public key.
    fs.open(&pkfile[0], fstream::in);
    upk->serverKey = new FHEPubKey(*upk->context);
    fs >> *upk->serverKey;
    fs.close();

#ifdef DEBUG
    cout << "Server Public Key Received." << endl;
#endif // DEBUG

    upk->ea = new EncryptedArray(*upk->context, upk->G);
    upk->nslots = upk->ea->size();
#ifdef DEBUG
    cout << "Scheme Install Complete." << endl;
#endif // DEBUG
}

/*****************************
 *Android installation package.
 *Downloads the context file from a socket, then
 *installs it.
 *****************************/
void install_upkg_socket(ServerLink * sl, UserPackage * upk)
{
    stringstream ss;
    fstream fs;
    string filey = "Cli.Base";

    char * buffer = new char[256];
    int blk = 1024*sizeof(char);

#ifdef DEBUG
        cout << "Creating the file." << endl;
#endif

    fs.open(&filey[0], fstream::out | fstream::trunc);

#ifdef DEBUG
    cout << "Streaming from server..." << endl;
#endif

    stream_from_socket(&buffer, 1024, sl);
    fs.write(buffer, sl->xfer);
    fs.close();

#ifdef DEBUG
    cout << "Base File streaming complete." << endl;
#endif

    fs.open(&filey[0], fstream::in);

    readContextBase(fs, upk->m, upk->p, upk->r, upk->gens, upk->ords);
    upk->context = new FHEcontext(upk->m, upk->p, upk->r,
                                  upk->gens, upk->ords);

    fs.close();

#ifdef DEBUG
        cout << "Context built." << endl;
#endif

    if (!send_ack(sl))
    {
#ifdef DEBUG
        cout << "Socket buffer error." << endl;
        exit(0);
#endif
    }

    //Stream the context information into the context structure.
    while ((stream_from_socket(&buffer, 1024, sl)) > 0)
    {
        ss << buffer;
        ss >> *upk->context;
    }

    //Generate my key pair and switching matrix.
    upk->secretKey = new FHESecKey(*upk->context);
    upk->publicKey = upk->secretKey;
    upk->G = upk->context->alMod.getFactorsOverZZ()[0];
    upk->secretKey->GenSecKey(64);
    addSome1DMatrices(*upk->secretKey);
    upk->serverKey = new FHEPubKey(*upk->context);

    if (!send_ack(sl))
    {
#ifdef DEBUG
        cout << "Socket buffer error." << endl;
        exit(0);
#endif
    }

    //Stream the server's public key from the socket.
    while ((stream_from_socket(&buffer, 1024, sl)) > 0)
    {
        ss << buffer;
        ss >> *upk->serverKey;
    }

    upk->ea = new EncryptedArray(*upk->context, upk->G);
    upk->nslots = upk->ea->size();

    if (!send_ack(sl))
    {
#ifdef DEBUG
        cout << "Socket buffer error." << endl;
        exit(0);
#endif
    }

    delete [] buffer;
}

/*******************
 *Android mode install.
 *Reads from standard i/o.
 *******************/
void install_upkg_android(UserPackage * upk)
{
    stringstream ss;
    char * buffer = new char[4096];
    int blk = 4096*sizeof(char);

    readContextBase(cin, upk->m, upk->p, upk->r,
                    upk->gens, upk->ords);
    upk->context = new FHEcontext(upk->m, upk->p, upk->r,
                                  upk->gens, upk->ords);

    cout << "ACK" << endl;

    cin.getline(buffer, 4096);

    while (cin.gcount() > 0)
    {
        ss << buffer;
        ss >> *upk->context;
        bzero(buffer, sizeof(buffer));
        cin.getline(buffer, 4096);
    }

    //Generate my key pair and switching matrix.
    upk->secretKey = new FHESecKey(*upk->context);
    upk->publicKey = upk->secretKey;
    upk->G = upk->context->alMod.getFactorsOverZZ()[0];
    upk->secretKey->GenSecKey(64);
    addSome1DMatrices(*upk->secretKey);
    upk->serverKey = new FHEPubKey(*upk->context);

    cout << "ACK" << endl;

    cin.getline(buffer, 4096);

    while (cin.gcount() > 0)
    {
        ss << buffer;
        ss >> *upk->serverKey;
        bzero(buffer, sizeof(buffer));
        cin.getline(buffer, 4096);
    }

    upk->ea = new EncryptedArray(*upk->context, upk->G);
    upk->nslots = upk->ea->size();

    cout << "ACK" << endl;

    delete [] buffer;
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
#ifdef DEBUG
    cout << *buffer << endl;
    cout << sl->xfer << endl;
#endif
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
    char * buffer = new char[3];
    buffer[0] = 'A';
    buffer[1] = 'C';
    buffer[2] = 'K';
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
    char * ack = new char[3];
    ack[0] = 'A';
    ack[1] = 'C';
    ack[2] = 'K';
    int blk = sizeof(ack);
    while (stream_from_socket(&buffer, sizeof(buffer), sl) == blk)
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

/***************************
 *This function expects the
 *latitude and longitude of
 *the client in radians multiplied
 *by 1000.
 *As it is an integer there is no decimal precision.
 **************************/
pair<int, int> get_gps_x()
{
	int lat, lng;
	string input;

	cout << "L_Y";
	cin >> lat;
	cout << "L_X";
	cin >> lng;

	return make_pair(lat, lng);
}

/*************
 *Encrypt the client's current GPS location
 *with the server's Public Key.
***************/
Ctxt encrypt_location_x(int x, int y, UserPackage * upk)
{
	vector<long> loc;

	loc.push_back((long)x);
	loc.push_back((long)y);

	for (int i = 2; i < upk->serverKey->getContext().ea->size(); i++)
    {
		loc.push_back(0);
	}
#ifdef DEBUG
    cout << "GPS GOT." << endl;
#endif // DEBUG

	Ctxt cloc(*upk->serverKey);
#ifdef DEBUG
    cout << "Ctxt prepared." << endl;
#endif // DEBUG

	upk->serverKey->getContext().ea->encrypt(cloc, *upk->serverKey, loc);

#ifdef DEBUG
    cout << "LOCATION ENCRYPTED." << endl;
#endif // DEBUG

	return cloc;
}

/***********************
 *This should exit with
 *status 1 if it has successfully
 *written the encrypted
 *gps coords to file.
 **********************/
int send_location(UserPackage * upk, string outfile, string keyfile)
{
    fstream fs;

    pair<int, int> me = get_gps_x();
    Ctxt output = encrypt_location_x(me.first, me.second, upk);

    fs.open(&outfile[0], fstream::out | fstream::trunc);
    fs << output;
    fs.close();

    fs.open(&keyfile[0], fstream::out | fstream::trunc);
    fs << *upk->publicKey;
    fs.close();

    return 1;
}

/************************************
 *Get's GPS co-ords from the standard IO,
 *encrypts them using the server's public key,
 *then sends them via socket to the server.
 *Sends your public key immediately afterward
 *************************************/
int send_location_socket(UserPackage * upk, ServerLink * sl)
{
    stringstream stream;
    char * buffer = new char[4096];
    int blk = sizeof(buffer);

    pair<int, int> me = get_gps_x();
    Ctxt output = encrypt_location_x(me.first, me.second, upk);

    stream << output;
    stream.read(buffer, 4096);

    while (stream)
    {
        write_to_socket(&buffer, blk, sl);
        stream.read(buffer, 4096);
    }

    bzero(buffer, sizeof(buffer));

    if (!recv_ack(sl))
    {
#ifdef DEBUG
        cout << "Socket buffer error, no ACK." << endl;
        exit(0);
#endif
    }

    stream << *upk->publicKey;
    stream.read(buffer, 4096);

    while (stream)
    {
        write_to_socket(&buffer, blk, sl);
        stream.read(buffer, 4096);
    }

    bzero(buffer, sizeof(buffer));

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

/*****************
 *This seems far too simple.
 *It probably won't work, so test it.
 *****************/
int send_location_android(UserPackage * upk)
{
    string ack;

    pair<int, int> me = get_gps_x();
    Ctxt output = encrypt_location_x(me.first, me.second, upk);

    cout << output << endl;

    while (recv_ack_android() == false);

    cout << *upk->publicKey << endl;

    while (recv_ack_android() == false);

    return 1;
}

/***********************
 *Decrypt a list of
 *Euclidean distances of other users
 *from your position.
 *PC VERSION.
 ***********************/
vector<long> get_distances(string infile, UserPackage * upk)
{
    vector<long> d;
    fstream fs;
    Ctxt encrypted_distances(*upk->publicKey);

    fs.open(&infile[0], fstream::in);
    fs >> encrypted_distances;
    fs.close();

    upk->ea->decrypt(encrypted_distances, *upk->secretKey, d);
    return d;
}

/***********************
 *Decrypt a list of
 *Euclidean distances of other users
 *from your position.
 *ANDROID/SOCKET VERSION
 ***********************/
vector<long> get_distances_socket(ServerLink * sl, UserPackage * upk)
{
    vector<long> d;
    Ctxt encrypted_distances(*upk->publicKey);
    stringstream stream;
    char * buffer = new char[4096];
    int blk = 4096*sizeof(char);

    while ((stream_from_socket(&buffer, blk, sl)) > 0)
    {
        stream << buffer;
        stream >> encrypted_distances;
    }

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

/***********************
 *Android mode distance recovery.
 *Pushes
 **********************/
vector<long> get_distances_android(UserPackage * upk)
{
    vector<long> d;
    stringstream stream;
    Ctxt encrypted_distances(*upk->publicKey);
    char * buffer = new char[4096];
    int blk = 4096*sizeof(char);

    cin.getline(buffer, 4096);

    while (cin.gcount() > 0)
    {
        stream << buffer;
        stream >> encrypted_distances;
        bzero(buffer, sizeof(buffer));
        cin.getline(buffer, 4096);
    }

    cout << "ACK" << endl;

    upk->ea->decrypt(encrypted_distances, *upk->secretKey, d);

    delete [] buffer;
    return d;
}

/*********************
 *Display the proximity
 *of other users. Prints
 *to primary I/O, so only
 *one mode.
 **********************/
void display_positions(vector<long> d, int cnt)
{
    int i;
    for (i=0; i < cnt; i++)
    {
        cout << "User " << i << " is ";
        cout << sqrt(d[i]) << "m ";
        cout << "from your position." << endl;
    }
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
