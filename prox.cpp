#include "prox.h"
#include <sys/resource.h>

#define DEBUG
#define PC
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
- ANDROID mode is for when the exe is part of an
  android application as a resource. No files used
  in this mode, as the standard I/O for the program is
  used to stream information to the wrapping application.
 ************************************/
int main(int argc, char * argv[]) {

#ifdef PC
    string f1 = "Prox.Base";
    string f2 = "Prox.Ctxt";
    string f3 = "Prox.PubKey";
    string f4 = "Prx.Loc";
    string f5 = "Prx.Dis";
    string f6 = "Cli.PubKey";
#endif  //Filenames for PC mode usage.

    ServerLink op;
    UserPackage me;
    vector<long> them;
    struct stat checkFile;
    char * buffer = new char[256];
    op.xfer = 0;

    //Check the arguments
    if (argc != 3) {
        cerr << "./prox_x portnum localhost(hostname)" << endl;
        return 0;
    }

    //Try to create a link to the server,
    //This will fail if the server is not ready.
    if (!(prepare_socket(&op, argv))) {
        cout << "Server Unavailable.";
        return 0;
    }

    bzero(buffer, sizeof(buffer));
#ifdef DEBUG
    cout << "Connection Established." << endl;
#endif // DEBUG

#ifdef PC
    buffer[0] = 'P';
    buffer[1] = 'C';
    if (write_to_socket(&buffer, sizeof(buffer), &op)) {
        if (!recv_ack(&op)) {
            cout << "No ACK received." << endl;
            exit(0);
        }
        install_upkg(&me, f1, f2, f3);
    }
#elif ANDROID
    buffer[0] = 'A';
    buffer[1] = 'D';
    if (write_to_socket(&buffer, 256, &op)) {
        if (!recv_ack(&op)) {
            cout << "No ACK received." << endl;
            exit(0);
        }
        install_upkg_android(&op, &me);
    }
#endif // ANDROID

     if (!send_ack(&op)) {
#ifdef DEBUG
        cout << "Socket buffer error." << endl;
        exit(0);
#endif
    }

#ifdef DEBUG
    cout << "FHE Scheme installed." << endl;
#endif // DEBUG

#ifdef PC
    while(send_location(&me, f4, f6) == 1) {
        if (!recv_ack(&op)) {
            cout << "No ACK received." << endl;
            exit(0);
        }
#elif ANDROID
    while (send_location_android(&me, &op) == 1) {
#endif // ANDROID

#ifdef DEBUG
    cout << "Location Sent." << endl;
#endif // DEBUG

#ifdef PC
        //Wait for the server to reply with confirmation that
        //the location has been processed.
        while(buffer[0] != 'K') {
            bzero(buffer, sizeof(buffer));
            while ((op.xfer = read(op.sockFD, buffer, sizeof(buffer))) < 1);
            op.xfer = 0;
        }

        //Panic button for first user. The file will not exist.
        if ((stat(&f5[0], &checkFile) == 0) == true) {

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
        them = get_distances_android(&op, &me);
        display_positions(them, 10);
#endif // ANDROID

    }

    return 0;
}

/*******************************
 *Build an encryption/decryption
 *data structure for a user.
 *******************************/
void install_upkg(UserPackage * upk, string basefile, string ctxfile, string pkfile) {

    fstream fs;
    fs.open(&basefile[0], fstream::in);
    readContextBase(fs, upk->m, upk->p, upk->r, upk->gens, upk->ords);
    upk->context = new FHEcontext(upk->m, upk->p, upk->r, upk->gens, upk->ords);
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
 *Afterwards streams the
 *****************************/
void install_upkg_android(ServerLink * sl, UserPackage * upk)
{
    stringstream ss;
    char * buffer = new char[4096];
    int blk = 4096*sizeof(char);

    //Read the context base from the socket, then
    //push it into a stream to be used with readContextBase()
    while ((stream_from_socket(&buffer, blk, sl)) > 0) {
        ss << buffer;
    }

    readContextBase(ss, upk->m, upk->p, upk->r, upk->gens, upk->ords);
    upk->context = new FHEcontext(upk->m, upk->p, upk->r, upk->gens, upk->ords);

    if (!send_ack(sl)) {
#ifdef DEBUG
        cout << "Socket buffer error." << endl;
        exit(0);
#endif
    }

    //Stream the context information into the context structure.
    while ((stream_from_socket(&buffer, blk, sl)) > 0) {
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

    if (!send_ack(sl)) {
#ifdef DEBUG
        cout << "Socket buffer error." << endl;
        exit(0);
#endif
    }

    //Stream the server's public key from the socket.
    while ((stream_from_socket(&buffer, blk, sl)) > 0) {
        ss << buffer;
        ss >> *upk->serverKey;
    }

    upk->ea = new EncryptedArray(*upk->context, upk->G);
    upk->nslots = upk->ea->size();

    if (!send_ack(sl)) {
#ifdef DEBUG
        cout << "Socket buffer error." << endl;
        exit(0);
#endif
    }

    delete [] buffer;
}

/*****
Writes a specific length of data from a socket to a buffer.
This could possibly be made more efficient by making it
into a stream of some sort.
BOTH THIS FUNCTION AND IT'S COMPLEMENT WIPE THE BUFFER AFTER
OPERATIONS.
*****/
int stream_from_socket(char ** buffer, int blocksize, ServerLink * sl)
{
    int p;
    bzero(*buffer, sizeof(*buffer));
    sl->xfer = read(sl->sockFD, buffer, blocksize);
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
    sl->xfer = write(sl->sockFD, buffer, blocksize);
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
    if (write_to_socket(&buffer, sizeof(buffer), sl) == sizeof(buffer)) {
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
bool recv_ack(ServerLink * sl) {
    char * buffer = new char[3];
    char * ack = new char[3];
    ack[0] = 'A';
    ack[1] = 'C';
    ack[2] = 'K';
    int blk = sizeof(ack);
    while (stream_from_socket(&buffer, sizeof(buffer), sl) == blk) {
        if (strcmp(ack, buffer) == 0) {
            return true;
            delete [] buffer;
            delete [] ack;
        }
    }
    delete [] buffer;
    delete [] ack;
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
 *I wonder what this does...
***************/
Ctxt encrypt_location_x(int x, int y, UserPackage * upk)
{
	vector<long> loc;

	loc.push_back((long)x);
	loc.push_back((long)y);

	for (int i = 2; i < upk->serverKey->getContext().ea->size(); i++) {
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
 *Send's your public key immediately afterward
 *************************************/
int send_location_android(UserPackage * upk, ServerLink * sl)
{
    stringstream stream;
    char * buffer = new char[4096];
    int blk = sizeof(buffer);

    pair<int, int> me = get_gps_x();
    Ctxt output = encrypt_location_x(me.first, me.second, upk);

    stream << output;
    stream.read(buffer, 4096);

    while (stream) {
        write_to_socket(&buffer, blk, sl);
        stream.read(buffer, 4096);
    }

    bzero(buffer, sizeof(buffer));

    if (!recv_ack(sl)) {
#ifdef DEBUG
        cout << "Socket buffer error, no ACK." << endl;
        exit(0);
#endif
    }

    stream << *upk->publicKey;
    stream.read(buffer, 4096);

    while (stream) {
        write_to_socket(&buffer, blk, sl);
        stream.read(buffer, 4096);
    }

    bzero(buffer, sizeof(buffer));

    if (!recv_ack(sl)) {
#ifdef DEBUG
        cout << "Socket buffer error, no ACK." << endl;
        exit(0);
#endif
    }

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
vector<long> get_distances_android(ServerLink * sl, UserPackage * upk)
{
    vector<long> d;
    Ctxt encrypted_distances(*upk->publicKey);
    stringstream stream;
    char * buffer = new char[4096];
    int blk = 4096*sizeof(char);

    while ((stream_from_socket(&buffer, blk, sl)) > 0) {
        stream << buffer;
        stream >> encrypted_distances;
    }

    if (!send_ack(sl)) {
#ifdef DEBUG
        cout << "Socket buffer error." << endl;
        exit(0);
#endif
    }

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
    for (i=0; i < cnt; i++) {
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
	memcpy((char *)&sl->serv->h_addr, (char *)&sl->servAddr.sin_addr.s_addr,
			sl->serv->h_length);
	sl->servAddr.sin_port = htons(sl->port);
	if ((sl->xfer = connect(sl->sockFD, (struct sockaddr *)&sl->servAddr,
			sizeof(sl->servAddr))) < 0)
     {
        cerr << "Failure connecting to server." << endl;
		return -1;
     }
     return 1;
}
