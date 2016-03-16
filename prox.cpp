#include "prox.h"
#include <sys/resource.h>

/***********************************
 *File generator program
 *Creates a .SecKey, .CtxtBase and .Ctxt file
 *to form the basis of a FHE scheme.
 ************************************/
int main(int argc, char * argv[]) {

    ServerLink op;
    UserPackage me;

    string f1 = "Prox.CtxtBase";
    string f2 = "Prox.Ctxt";
    string f3 = "Prox.PubKey";
    string f4 = "Prx.Loc";
    string f5 = "Prx.Dis";
    vector<long> them;

    if (!(prepare_socket(&op, argv))) {
        cerr << "fuck";
        return 0;
    }

    install_upkg(&me, f1, f2, f3);

    while(send_location(&me, f4) == 1) {
        them = get_distances(f5, &me);
        display_positions(them);
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

    fs.open(&ctxfile[0], fstream::in);
    fs >> *upk->context;
    fs.close();

    fs.open(&pkfile[0], fstream::in);
    upk->publicKey = new FHEPubKey(*upk->context);
    fs >> *upk->publicKey;
    fs.close();

    upk->secretKey = new FHESecKey(*upk->context);
    G = upk->context->alMod.getFactorsOverZZ()[0];
    upk->secretKey->GenSecKey(64);
    addSome1DMatrices(*upk->secretKey);
    upk->ea = new EncryptedArray(*upk->context, G);
    upk->nslots = upk->ea->size();
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

	for (int i = 2; i < upk->publicKey->getContext().ea->size(); i++) {
		loc.push_back(0);
	}

	Ctxt cloc(*upk->publicKey);
	upk->publicKey->getContext().ea->encrypt(cloc, *upk->publicKey, loc);

	return cloc;
}

/***********************
 *This should exit with
 *status 1 if it has successfully
 *written the encrypted
 *gps coords to file.
 **********************/
int send_location(UserPackage * upk, string outfile) {
    fstream fs;
    fs.open(&outfile[0], fstream::out | fstream::trunc);
    pair<int, int> me = get_gps_x();
    Ctxt output = encrypt_location_x(me.first, me.second, upk);
    fs << output;
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
    for (i=0; i=d.size(); i++) {
        cout << "User " << i << " is ";
        cout << sqrt(d[i]) << "m ";
        cout << "from your position";

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
