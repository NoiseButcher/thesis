#include "prox.h"
#include <sys/resource.h>

#define DEBUG
/***********************************
 *File generator program
 *Creates a .SecKey, .CtxtBase and .Ctxt file
 *to form the basis of a FHE scheme.
 ************************************/
int main(int argc, char * argv[]) {

    ServerLink op;
    UserPackage me;

    string f1 = "Prox.Base";
    string f2 = "Prox.Ctxt";
    string f3 = "Prox.PubKey";
    string f4 = "Prx.Loc";
    string f5 = "Prx.Dis";
    string f6 = "Cli.PubKey";
    vector<long> them;
    struct stat checkFile;
    char buffer[256];
    op.link = 0;

    if (argc < 3) {
        cerr << "./prox_x portnum localhost(hostname)" << endl;
        return 0;
    }

    if (!(prepare_socket(&op, argv))) {
        cerr << "fuck";
        return 0;
    }
#ifdef DEBUG
    cout << "Connection Established." << endl;
#endif // DEBUG
    install_upkg(&me, f1, f2, f3);
#ifdef DEBUG
    cout << "FHE Scheme installed." << endl;
#endif // DEBUG

    while(send_location(&me, f4, f6) == 1) {
#ifdef DEBUG
    cout << "Location Sent." << endl;
#endif // DEBUG
        //Write command to the socket to inform the server
        //that the location has been sent.
        bzero(buffer, sizeof(buffer));
        buffer[0] = 'X';
        while ((op.link = write(op.sockFD, buffer, sizeof(buffer))) < 1);
        op.link = 0;

        //Wait for the server to reply with confirmation that
        //the location has been processed.
        bzero(buffer, sizeof(buffer));
        while(buffer[0] != 'K') {
            bzero(buffer, sizeof(buffer));
            while ((op.link = read(op.sockFD, buffer, sizeof(buffer))) < 1);
            op.link = 0;
            cout << buffer << endl;
        }

        //Panic button for first user. The file will not exist.
        if ((stat(&f5[0], &checkFile) == 0) == true) {

            them = get_distances(f5, &me);
#ifdef DEBUG
    cout << "Distances Received." << endl;
#endif // DEBUG
            display_positions(them);
#ifdef DEBUG
    cout << "Distances Decoded." << endl;
#endif // DEBUG
        }

    }

    return 0;
}

/*******************************
 *Build an encryption/decryption
 *data structure for a user.
 *******************************/
void install_upkg(UserPackage * upk, string basefile, string ctxfile, string pkfile) {

    ZZX G;

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
    G = upk->context->alMod.getFactorsOverZZ()[0];
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

    upk->ea = new EncryptedArray(*upk->context, G);
    upk->nslots = upk->ea->size();
#ifdef DEBUG
    cout << "Scheme Install Complete." << endl;
#endif // DEBUG
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
 *Hide yo kids,
 *Hide yo wife.
 *AND HIDE YOUR HUSBAND CAUSE
 *THEY RAPING ERR'BODY AROUND
 *HERE.
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
int send_location(UserPackage * upk, string outfile, string keyfile) {

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

/***********************
 *Decrypt a list of
 *Euclidean distances of other users
 *from your position.
 ***********************/
vector<long> get_distances(string infile, UserPackage * upk) {
    vector<long> d;
    fstream fs;
    fs.open(&infile[0], fstream::in);
    Ctxt encrypted_distances(*upk->publicKey);
    fs >> encrypted_distances;
    fs.close();

    upk->ea->decrypt(encrypted_distances, *upk->secretKey, d);
    return d;
}

/*********************
 *Display the proximity
 *of other users.
 **********************/
void display_positions(vector<long> d) {
    int i;
    for (i=0; i<10; i++) {
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
int prepare_socket(ServerLink * sl, char * argv[]) {
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
	if ((sl->link = connect(sl->sockFD, (struct sockaddr *)&sl->servAddr,
			sizeof(sl->servAddr))) < 0)
     {
        cerr << "Failure connecting to server." << endl;
		return -1;
     }
     return 1;
}
