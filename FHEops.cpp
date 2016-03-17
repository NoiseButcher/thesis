#include "FHEops.h"
#include <sys/resource.h>

#define DEBUG

/***********************************
 *File generator program
 *Creates a .SecKey, .CtxtBase and .Ctxt file
 *to form the basis of a FHE scheme.
 ************************************/
int main(int argc, char * argv[]) {

    string filename4 = "Prx.Loc";
    string filename5 = "Prx.Dis";
    string filename6 = "Cli.PubKey";
    vector<Ctxt> them;
    fstream fs;
    ServerData primary;
    ServerLink sl;

    sl.links.push_back(0);
    sl.link = 0;
    char buffer[256];

    if (argc < 2) {
        cout << "./FHEops_x portnum ... moron." << endl;
        return 0;
    }

    prepare_server_socket(&sl, argv);
#ifdef DEBUG
    cout << "Socket Prepped." << endl;
#endif // DEBUG
    generate_scheme(&primary);
#ifdef DEBUG
    cout << "FHE Scheme generated." << endl;
#endif // DEBUG
    generate_upkg(&primary);
#ifdef DEBUG
    cout << "FHE user package generated." << endl;
#endif // DEBUG
    int i = 0;


    while(true) {

        if (primary.users > 0) {

            for (i = 0; i < primary.users; i++) {

                //Wait to hear from the client that they have sent their location.
                bzero(buffer, sizeof(buffer));
                while (buffer[0] != 'X') {
                    bzero(buffer, sizeof(buffer));
                    while ((sl.link = read(sl.links[i], buffer, sizeof(buffer))) < 1);
                    sl.link = 0;
                }

                Ctxt input(*primary.publicKey);
#ifdef DEBUG
    cout << "User " << i << " being processed." << endl;
#endif // DEBUG

                fs.open(&filename4[0], fstream::in);
                fs >> input;
                fs.close();

#ifdef DEBUG
    cout << "New Co-ords recieved." << endl;
#endif // DEBUG

                them = handle_user(them, &primary, input, filename5, filename6);
#ifdef DEBUG
    cout << "New Output Generated." << endl;
#endif // DEBUG
                //Inform the client that the location has been processed.
                bzero(buffer, sizeof(buffer));
                buffer[0] = 'K';
                while ((sl.link = write(sl.links[i], buffer, sizeof(buffer))) < 1);
                sl.link = 0;
            }
        }

        if ((sl.links[primary.users] = accept(sl.sockFD, (struct sockaddr *)&sl.clientAddr, &sl.len)) > 0) {
#ifdef DEBUG
    cout << "New User Accepted." << endl;
#endif // DEBUG
            //Wait to hear from the client that they have sent their location.
            bzero(buffer, sizeof(buffer));
            while (buffer[0] != 'X') {
                bzero(buffer, sizeof(buffer));
                while ((sl.link = read(sl.links[primary.users], buffer, sizeof(buffer))) < 1);
                sl.link = 0;
            }

            Ctxt input(*primary.publicKey);

            fs.open(&filename4[0], fstream::in);
            fs >> input;
            fs.close();
#ifdef DEBUG
    cout << "New Co-ords recieved." << endl;
#endif // DEBUG

            them = handle_new_user(them, &primary, input, filename5, filename6);

            //Inform the client that the location has been processed.
            bzero(buffer, sizeof(buffer));
            buffer[0] = 'K';
            while ((sl.link = write(sl.links[primary.users - 1], buffer, sizeof(buffer))) < 1);
            sl.link = 0;

            sl.links.push_back(0);

        }

    }

    return 0;
}

/*******************************
 *Generate the FHE scheme with the specified
 *parameters. Writes the scheme to the ServerData
 *structure.
 *******************************/
int generate_scheme(ServerData * sd) {
    long long int p = 257;
    long r = 1;
    long L = 25;
    long security = 128;
    long m = 0;
    long c = 3;
    long w = 64;
    long d = 0;
    ZZX G;

    m = FindM(security, L, c, p, d, 3, 0);
    sd->context = new FHEcontext(m, p, r);
    buildModChain(*sd->context, L, c);

    /*******************
     *This requires work
     *******************/
    sd->secretKey = new FHESecKey(*sd->context);
    sd->publicKey = sd->secretKey;
    G = sd->context->alMod.getFactorsOverZZ()[0];
    sd->secretKey->GenSecKey(w);
    addSome1DMatrices(*sd->secretKey);
    /*******************
     *This requires work
     *******************/

    sd->ea = new EncryptedArray(*sd->context, G);
    sd->nslots = sd->ea->size();

    sd->users = 0;

    cout << "Init Complete." << endl;

    return 1;
}

/**************************************
 *Generates files for the clients to use
 *so they can have access to the FHE scheme
 **************************************/
int generate_upkg(ServerData * sd) {

    fstream keyFile;
    string filename1 = "Prox.Base";
    string filename2 = "Prox.Ctxt";
    string filename3 = "Prox.PubKey";


    keyFile.open(&filename1[0], fstream::out | fstream::trunc);
    writeContextBase(keyFile, *sd->context);
    keyFile.close();
    cout << "ContextBase written to " << filename1 << endl;

    keyFile.open(&filename2[0], fstream::out | fstream::trunc);
    keyFile << *sd->context;
    keyFile.close();
    cout << "Context written to " << filename2 << endl;

    keyFile.open(&filename3[0], fstream::out | fstream::trunc);
    keyFile << *sd->publicKey;
    keyFile.close();
    cout << "PubKey written to " << filename3 << endl;

    return 1;

}

/****************************
 *When a new position comes in,
 *generate an output ciphertext that
 *should correspond to the square of
 *the distance to each other user
 *****************************/
Ctxt generate_output(vector<Ctxt> locs, Ctxt input, ServerData * sd, const FHEPubKey &pk) {
    int i;

    Ctxt cx(*sd->publicKey);
    Ctxt vx(pk);
    vector<long> px;

    vector<long> pvec;

    for (int i = 0; i < sd->publicKey->getContext().ea->size(); i++) {
		pvec.push_back(0);
	}

    //Create a base Ctxt which has all 0 members
	sd->ea->getContext().ea->encrypt(cx, *sd->publicKey, pvec);

    for (i=0; i < locs.size(); i++) {
        Ctxt zx(locs[i]); //Create a copy of the i'th co-ords
        zx = compute(zx, input, *sd->publicKey); //determine their position relative to input co-ords
        zx.getContext().ea->shift(zx, i); //shift the position in the final ciphertext
        cx+=zx; //Since cx is an all zero ciphertext, adding a shifted element should work...
    }

    /**********************
     *And here we have
     *a security bottleneck.
     **********************/
    sd->ea->decrypt(cx, *sd->secretKey, px);
    pk.getContext().ea->encrypt(vx, pk, px);

    return vx;
}

/**************************
 *Creates a single element ciphertext
 *that contains the x^2 + y^2 value of
 *two co-ordinates.
 **************************/
Ctxt compute(Ctxt c1, Ctxt c2, const FHEPubKey &pk) {

    vector<long> pvec;
    Ctxt purge(pk);

    pvec.push_back(1);
    for (int i = 1; i < pk.getContext().ea->size(); i++) {
		pvec.push_back(0);
	}

    pk.getContext().ea->encrypt(purge, pk, pvec);

	c1.addCtxt(c2, true); //(x1, y1) - (x2, y2)
	c1.square(); //(x^2, y^2)

	Ctxt inv(c1);
	inv.getContext().ea->rotate(inv, -1);

	c1+=inv; //(x^2 + y^2, y^2 + x^2)
	c1*=purge; //(x^2 +y^2, 0)

	return c1;
}

/**********************************
 *Every time a user sends in co-ords,
 *return an output to them, then rebuild
 *the co-ordinate vector.
 *********************************/
vector<Ctxt> handle_user(vector<Ctxt>  locs, ServerData * sd, Ctxt newusr, string outname, string keyfile) {

    fstream fs;

    //Import the client's public key
    fs.open(&keyfile[0], fstream::in);
    FHEPubKey pk(*sd->context);
    fs >> pk;
    fs.close();

    //Generate an output using for the connected client.
    fs.open(&outname[0], fstream::out | fstream::trunc);
    Ctxt out = generate_output(locs, newusr, sd, pk);
    fs << out;
    fs.close();

    vector<Ctxt> updated(locs.begin() + 1, locs.end());
    updated.push_back(newusr);

    return updated;

}

/**********************
 *Same as the standard handle_user()
 *except does not change the current
 *vector of ciphertexts
 *********************/
vector<Ctxt> handle_new_user(vector<Ctxt>  locs, ServerData * sd, Ctxt newusr, string outname, string keyfile) {

    vector<Ctxt> updated;

    if (sd->users > 1) {

        fstream fs;
        //Import the client's public key
        fs.open(&keyfile[0], fstream::in);
        FHEPubKey pk(*sd->context);
        fs >> pk;
        fs.close();

        fs.open(&outname[0], fstream::out | fstream::trunc);
        Ctxt out = generate_output(locs, newusr, sd, pk);
        fs << out << endl;
        fs.close();
    }

    updated = locs;
    updated.push_back(newusr);
    sd->users++;

    return updated;
}

/*****************************
 *Listen on a port provided in the
 *arguments.
 *****************************/
int prepare_server_socket(ServerLink * sl, char * argv[]) {
    sl->port = atoi(argv[1]);
    if ((sl->sockFD = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
    {
        cerr << "Error opening socket." << endl;
		return -1;
    }
    memset((char*)&sl->servAddr, 0, sizeof(sl->servAddr));
    sl->servAddr.sin_family = AF_INET;
	sl->servAddr.sin_addr.s_addr = INADDR_ANY;
	sl->servAddr.sin_port = htons(sl->port);
	if (bind(sl->sockFD, (struct sockaddr *)&sl->servAddr, sizeof(sl->servAddr))
			< 0) {
		cerr << "Failed to bind server to socket." << endl;
		return -1;
	}

	listen(sl->sockFD, sl->blocklen);
	sl->len = sizeof(sl->clientAddr);
	return 1;
}
