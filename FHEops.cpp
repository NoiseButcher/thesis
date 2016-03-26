#include "FHEops.h"
#include <sys/resource.h>

#define DEBUG
/***********************************
 *Server program for operating on encrypted data.
 *Generates the basis for the security scheme
 *and distributes it amongst connected clients.
 ************************************/
int main(int argc, char * argv[])
{
    /**
    File-mode variables
    **/
    fstream fs;
    string filename4 = "Prx.Loc";
    string filename5 = "Prx.Dis";
    string filename6 = "Cli.PubKey";

    /**
    Data structures and buffers
    **/
    ServerData sd;
    ServerLink sl;
    int maxthreads;
    char * buffer = new char[4096];
    int blk = sizeof(buffer);

    /**
    Argument check... just in case...
    **/
    if (argc != 3)
    {
        cout << "./FHEops_x portnum maxclients" << endl;
        return 0;
    }

    maxthreads = atoi(argv[2]);

    prepare_server_socket(&sl, argv);
#ifdef DEBUG
    cout << "Socket Prepped." << endl;
#endif // DEBUG
    generate_scheme(&sd);
#ifdef DEBUG
    cout << "FHE Scheme generated." << endl;
#endif // DEBUG
    generate_upkg(&sd);
#ifdef DEBUG
    cout << "FHE user package generated." << endl;
#endif // DEBUG
    int i = 0;


    while(true)
    {
        if (sd.users > 0)
        {
            for (i = 0; i < sd.users; i++)
            {
                //Wait to hear from the client that they have sent their location.
                bzero(buffer, sizeof(buffer));
                while (buffer[0] != 'X')
                {
                    bzero(buffer, sizeof(buffer));
                    while ((sl.link = read(sl.links[i],
                                           buffer,
                                           sizeof(buffer))) < 1);
                    sl.link = 0;
                }

                Ctxt input(*sd.publicKey);
#ifdef DEBUG
    cout << "User " << i << " being processed." << endl;
#endif // DEBUG

                fs.open(&filename4[0], fstream::in);
                fs >> input;
                fs.close();

#ifdef DEBUG
    cout << "New Co-ords recieved." << endl;
#endif // DEBUG

                them = handle_user(them, &primary,
                                   input, filename5, filename6);
#ifdef DEBUG
    cout << "New Output Generated." << endl;
#endif // DEBUG
                //Inform the client that the location has been processed.
                bzero(buffer, sizeof(buffer));
                buffer[0] = 'K';
                while ((sl.link = write(sl.links[i], buffer,
                                        sizeof(buffer))) < 1);
                sl.link = 0;
            }
        }

        if ((sl.links[sd.users] = accept(sl.sockFD,
                                        (struct sockaddr *)&sl.clientAddr,
                                        &sl.len)) > 0)
        {
#ifdef DEBUG
    cout << "New User Accepted." << endl;
#endif // DEBUG
            //Wait to hear from the client that they have sent their location.
            bzero(buffer, sizeof(buffer));
            while (buffer[0] != 'X')
            {
                bzero(buffer, sizeof(buffer));
                while ((sl.link = read(sl.links[sd.users],
                                       buffer, sizeof(buffer))) < 1);
                sl.link = 0;
            }

            Ctxt input(*sd.publicKey);

            fs.open(&filename4[0], fstream::in);
            fs >> input;
            fs.close();
#ifdef DEBUG
    cout << "New Co-ords recieved." << endl;
#endif // DEBUG

            them = handle_new_user(them, &primary,
                                   input, filename5, filename6);

            //Inform the client that the location has been processed.
            bzero(buffer, sizeof(buffer));
            buffer[0] = 'K';
            while ((sl.link = write(sl.links[sd.users - 1],
                                    buffer, sizeof(buffer))) < 1);
            sl.link = 0;

            sl.links.push_back(0);
        }
    }

    while (true)
    {
        /**
        Handle new connections if there is space.
        **/
        if (sd->users < maxthreads)
        {
            sl.len = sizeof(sl.clientAddr);
            ClientLink client;
            client.server = &sd;
            if ((client.link.sockFD = accept(sl.sockFD,
                                        (struct sockaddr *)&sl.clientAddr,
                                        &sl.len)) > 0)
            {
#ifdef DEBUG
                cout << "New Client Accepted" << endl;
#endif // DEBUG
                pthread_create(&client.threadID,
                               NULL,
                               handle_client,
                               (void*)&client);

            }
            else
            {
                cout << "Error Accepting Client" << endl;
            }
        }
        else
        {
            //Do something here...
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

    fstream keyfile;
    string filename1 = "Prox.Base";

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

    keyfile.open(&filename1[0], fstream::out | fstream::trunc);
    writeContextBase(keyfile, *sd->context);
    keyfile.close();
    cout << "ContextBase written to " << filename1 << endl;

    cout << "Init Complete." << endl;

    return 1;
}

/**************************************
 *Generates files for the clients to use
 *so they can have access to the FHE scheme
 **************************************/
int generate_upkg(ServerData * sd)
{
    fstream keyFile;
    string filename2 = "Prox.Ctxt";
    string filename3 = "Prox.PubKey";

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

/**********************************
 *Stream the Context Base, Context and
 *Public Key to a connected client.
 **********************************/
void generate_upkg_android(ServerData * sd, ServerLink * sl)
{
    fstream keyFile;
    stringstream stream;
    char * buffer = new char[4096];
    int blk = sizeof(buffer);
    string filename = "Prox.Base";

    keyFile.open(&filename[0], fstream::in);
    stream << keyFile.rdbuf();
    keyFile.close();

    stream.read(buffer, 4096);

    while (stream)
    {
        write_to_socket(&buffer, blk, sl);
        stream.read(buffer, 4096);
    }

    if (!recv_ack(sl))
    {
#ifdef DEBUG
        cout << "Socket buffer error, no ACK." << endl;
        exit(0);
#endif
    }

    bzero(buffer, sizeof(buffer));
    stream << *sd->context;

    stream.read(buffer, 4096);

    while (stream)
    {
        write_to_socket(&buffer, blk, sl);
        stream.read(buffer, 4096);
    }

    if (!recv_ack(sl))
    {
#ifdef DEBUG
        cout << "Socket buffer error, no ACK." << endl;
        exit(0);
#endif
    }

    bzero(buffer, sizeof(buffer));
    stream << *sd->publicKey;

    stream.read(buffer, 4096);

    while (stream)
    {
        write_to_socket(&buffer, blk, sl);
        stream.read(buffer, 4096);
    }

    if (!recv_ack(sl))
    {
#ifdef DEBUG
        cout << "Socket buffer error, no ACK." << endl;
        exit(0);
#endif
    }

    delete [] buffer;
}

/****************************
 *When a new position comes in,
 *generate an output ciphertext that
 *should correspond to the square of
 *the distance to each other user
 *****************************/
Ctxt generate_output(Ctxt input, ServerData * sd, const FHEPubKey &pk)
{
    int i;

    /**
    Initialise an empty vector and ciphertext to
    contain the output encrypted with the client's
    public key.
    **/
    Ctxt vx(pk);
    vector<long> px;

    /**
    Blank vector and ciphertext to facilitate operations
    on server key encrypted input. pvec() is initialised with
    as an empty vector encrypted with the server's public key.
    **/
    Ctxt cx(*sd->publicKey);
    vector<long> pvec;

    for (int i = 0; i < sd->publicKey->getContext().ea->size(); i++)
    {
		pvec.push_back(0);
	}

	sd->ea->getContext().ea->encrypt(cx, *sd->publicKey, pvec);

	/**
	Iterate over the current collection of encrypted coordinate
	sets and create a single ciphertext corresponding to the
	Euclidean distances between the input and all of the existing
	co-ordinates.
	**/
    for (i=0; i < sd->positions.size(); i++)
    {
        Ctxt zx(sd->positions[i]); /**Copy coordinate set i to a temporary Ctxt**/
        zx = compute(zx, input, *sd->publicKey); /**Calculate distance**/
        zx.getContext().ea->shift(zx, i); /**Offset Ctxt based on index i**/
        cx+=zx; /**Add offset ciphertext to empty output**/
    }

    /**********************
     *And here we have a security bottleneck.
     *Decrypt the ciphertext containing the distances
     *using the server's secret key.
     *Encrypt it again using the client's public key.
     **********************/
    sd->ea->decrypt(cx, *sd->secretKey, px);
    pk.getContext().ea->encrypt(vx, pk, px);

    return vx;
}

/**************************
 *Creates a single element ciphertext
 *that contains the x^2 + y^2 value of
 *two Cartesian co-ordinate pairs.
 **************************/
Ctxt compute(Ctxt c1, Ctxt c2, const FHEPubKey &pk)
{
    /**
    Create a "purge" vector to remove extraneous values
    from the ciphertext after computation.
    purge = Enc[1 0 0 0 ...]
    **/
    vector<long> pvec;
    Ctxt purge(pk);
    pvec.push_back(1);

    for (int i = 1; i < pk.getContext().ea->size(); i++)
    {
		pvec.push_back(0);
	}

    pk.getContext().ea->encrypt(purge, pk, pvec);

	c1.addCtxt(c2, true); /**c1 = Enc[(x1-x2) (y1-y2) 0 0 ...] **/
	c1.square(); /**c1 = Enc[(x1-x2)^2 (y1-y2)^2 0 0 ...] **/

	Ctxt inv(c1); /**inv = Enc[(y1-y2)^2 0 .. 0 (x1-x2)^2] **/
	inv.getContext().ea->rotate(inv, -1);

	c1+=inv; /**c1 = Enc[(x1-x2)^2+(y1-y2)^2 (y1-y2)^2 0 .. 0 (x1-x2)^2] **/
	c1*=purge; /**c1 = Enc[(x1-x2)^2+(y1-y2)^2 0 0 ...] **/
	return c1;
}

/**********************************
 *Every time a user sends in co-ords,
 *return an output to them, then rebuild
 *the co-ordinate vector.
 *********************************/
vector<Ctxt> handle_user(vector<Ctxt>  locs, ServerData * sd, Ctxt newusr, string outname, string keyfile)
{

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
 *Socket code to handle connection between client and
 *server, takes a client's encrypted location and the vector
 *of encrypted co-ordinate sets as input.
 *Returns the updated vector of co-ords as an output
 *and sends the distances to the client via socket.
 *********************/
void handle_user_socket(ServerData * sd, Ctxt newusr, ServerLink * sl)
{
    stringstream stream;
    char * buffer = new char[4096];
    int blk = sizeof(buffer);

    /**
    Create temporary copy of client's public key,
    then import key data from socket.
    **/
    FHEPubKey pk(*sd->context);
    while ((stream_from_socket(&buffer, blk, sl)) > 0)
    {
        stream << buffer;
        stream >> pk;
    }

    /**
    Send ACK when key has been acquired.
    **/
    if (!send_ack(sl))
    {
#ifdef DEBUG
        cout << "Socket buffer error." << endl;
        exit(0);
#endif
    }

    /**
    Generate output ciphertext
    **/
    Ctxt out = generate_output(newusr, sd, pk);

    /**
    Push to socket.
    **/
    stream << out;
    stream.read(buffer, 4096);

    while(stream)
    {
        write_to_socket(&buffer, blk, sl);
        stream.read(buffer, 4096);
    }

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

    /**
    Update co-ordinate vector, removing the old
    co-ords and appending the new ones.
    **/
    sd->positions[sl->thisclient] = newusr;

    delete [] buffer;
}

/**********************
 *Same as the standard handle_user()
 *except does not change the current
 *vector of ciphertexts
 *********************/
vector<Ctxt> handle_new_user(ServerData * sd,
                             Ctxt newusr, string outname, string keyfile)
{
    vector<Ctxt> updated;

    if (sd->users > 0)
    {
        fstream fs;
        //Import the client's public key
        fs.open(&keyfile[0], fstream::in);
        FHEPubKey pk(*sd->context);
        fs >> pk;
        fs.close();

        fs.open(&outname[0], fstream::out | fstream::trunc);
        Ctxt out = generate_output(newusr, sd, pk);
        fs << out << endl;
        fs.close();
    }

    updated = sd->positions;
    updated.push_back(newusr);
    sd->users++;

    return updated;
}

/*******************************
 *Handle a new user connecting to the server for the first time.
 *Either process their location and send it, then append
 *their position to the current vector,
 *Or simply send add them to the vector if they are the first
 *client to join.
 *******************************/
void handle_new_user_socket(ServerData * sd, Ctxt newusr, ServerLink * sl)
{
    /**
    If this client is not the first,
    process their location.
    **/
    if (sd->users > 0)
    {
        stringstream stream;
        char * buffer = new char[4096];
        int blk = sizeof(buffer);

        /**
        Create temporary copy of client's public key,
        then import key data from socket.
        **/
        FHEPubKey pk(*sd->context);
        while ((stream_from_socket(&buffer, blk, sl)) > 0)
        {
            stream << buffer;
            stream >> pk;
        }

        /**
        Send ACK when key has been acquired.
        **/
        if (!send_ack(sl))
        {
#ifdef DEBUG
            cout << "Socket buffer error." << endl;
            exit(0);
#endif
        }

        /**
        Generate output ciphertext
        **/
        Ctxt out = generate_output(newusr, sd, pk);

        /**
        Push to socket.
        **/
        stream << out;
        stream.read(buffer, 4096);

        while(stream)
        {
            write_to_socket(&buffer, blk, sl);
            stream.read(buffer, 4096);
        }

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

        delete [] buffer;
    }

    sd->positions.push_back(newusr);
    sl->thisclient = sd->users;
    sd->users++;
}

/*****************************
 *Listen on a port provided in the
 *arguments.
 *****************************/
int prepare_server_socket(ServerLink * sl, char * argv[])
{
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

	if (bind(sl->sockFD, (struct sockaddr *)&sl->servAddr,
            sizeof(sl->servAddr)) < 0)
    {
		cerr << "Failed to bind server to socket." << endl;
		return -1;
	}

	listen(sl->sockFD, sl->blocklen);

	return 1;
}

/*****
 *Writes a specific length of data from a socket to a buffer.
 *This could possibly be made more efficient by making it
 *into a stream of some sort.
 *BOTH THIS FUNCTION AND IT'S COMPLEMENT WIPE THE BUFFER AFTER
 *OPERATIONS.
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
 *Complementary writing function to stream_from_socket().
 *Pushes the contents of a buffer to the port.
 *This probably requires some sort of guarantee that data has not
 *been lost.
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
bool recv_ack(ServerLink * sl)
{
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

/*****************************
 *Multithreading handler for the client
 *connection.
 *****************************/
void *handle_client(void *param)
{
    ClientLink * me = (ClientLink*) param;
}
