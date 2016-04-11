#include "FHEops.h"
#include <sys/resource.h>

#define DEBUG
//#define FILEMODE
#define SOCKETMODE
/***********************************
 *Server program for operating on encrypted data.
 *Generates the basis for the security scheme
 *and distributes it amongst connected clients.
 ************************************/
ServerData sd;

int main(int argc, char * argv[])
{
#ifdef FILEMODE
    /**
    File-mode variables
    **/
    fstream fs;
    string filename4 = "Prx.Loc";
    string filename5 = "Prx.Dis";
    string filename6 = "Cli.PubKey";
#endif
    /**
    Data structures and buffers
    **/
    ServerLink sl;
    QueueHandle qh;
    int maxthreads;
    sd.mutex = PTHREAD_MUTEX_INITIALIZER;
    sd.myturn = PTHREAD_COND_INITIALIZER;
    sem_init(&sd.kickittome, 0, 1);
    vector<ClientLink> cl;

    /**
    Argument check... just in case...
    **/
    if (argc != 3)
    {
        cout << "./FHEops_x portnum maxclients" << endl;
        return 0;
    }

    maxthreads = atoi(argv[2]);

    generate_scheme(&sd);

#ifdef DEBUG
    cout << "FHE Scheme generated." << endl;
#endif // DEBUG

    prepare_server_socket(&sl, argv);

#ifdef DEBUG
    cout << "Socket Prepped." << endl;
#endif // DEBUG

    sd.currentuser = 1;
    sd.users = 0;

    qh.server = &sd;
    pthread_t queuehandler;

/*
    pthread_create(&queuehandler, NULL,
                   handle_queue, (void*)&qh);
*/
    while (true)
    {
        /**
        Handle new connections if there is space.
        **/
        if (sd.users < maxthreads)
        {
            /**
            Generate all of the necessary data
            structures to handle a new user.
            **/
            sl.len = sizeof(sl.clientAddr);
            ClientLink client;
            client.server = &sd;
            cl.push_back(client);
            sd.threadID.push_back(cl[sd.users].id);

            if ((cl[sd.users].link.sockFD = accept(sl.sockFD,
                                        (struct sockaddr *)&sl.clientAddr,
                                        &sl.len)) > 0)
            {
                /*

                if(sd.users == 0)
                {
                pthread_barrier_init(&sd.barrier, NULL,
                                     2);
                }
                else
                {
                    pthread_barrier_destroy(&sd.barrier);
                    pthread_barrier_init(&sd.barrier, NULL,
                                     sd.users+1);
                }
                */

                pthread_create(&cl[sd.users].id,
                               NULL,
                               handle_client,
                               (void*)&cl[sd.users]);
            }
            else
            {
                cout << "Error Accepting Client" << endl;
            }
        }
        /**
        Refuse connection if maximum users have been reached.
        **/
        else
        {
            cout << "FUCK OFF WE'RE FULL" << endl;
        }
    }
    /**
    Wipe all of the thread data.
    **/
    for (int i = 0; i < sd.users; i++)
    {
        pthread_join(sd.threadID[i], NULL);
    }

    /**
    Destroy thread mutex.
    **/
    pthread_mutex_destroy(&sd.mutex);
    pthread_cond_destroy(&sd.myturn);
    pthread_barrier_destroy(&sd.barrier);

    return 0;
}

/**********GENERIC FUNCTIONS*********************/
/*******************************
 *Generate the FHE scheme with the specified
 *parameters. Writes the scheme to the ServerData
 *structure.
 *******************************/
int generate_scheme(ServerData * sd) {
    long long int p = 2;
    long r = 8;
    long L = 5;
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
     *This requires further investigation.
     *Security Loophole.
     *******************/
    sd->secretKey = new FHESecKey(*sd->context);
    sd->publicKey = sd->secretKey;
    G = sd->context->alMod.getFactorsOverZZ()[0];
    sd->secretKey->GenSecKey(w);
    addSome1DMatrices(*sd->secretKey);
    /*******************
     *This requires further investigation.
     *Security Loophole.
     *******************/

    sd->ea = new EncryptedArray(*sd->context, G);
    sd->nslots = sd->ea->size();

    keyfile.open(&filename1[0], fstream::out | fstream::trunc);
    writeContextBase(keyfile, *sd->context);
    keyfile.close();

    cout << "ContextBase written to " << filename1 << endl;

    cout << "Scheme Generation Complete." << endl;

    return 1;
}

/**************************************
 *Generates files for the clients to use
 *so they can have access to the FHE scheme.
 *FILE mode version.
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
 *Socket mode version.
 **********************************/
void generate_upkg_android(ServerData * sd, ServerLink * sl)
{
    fstream keyFile;
    stringstream stream;
    char * buffer = new char[1025];
    string filename = "Prox.Base";

    cout << "Streaming base file..." << endl;

    /**
    Open the ContextBase file, and
    stream it's contents over the socket.
    **/
    keyFile.open(&filename[0], fstream::in);
    stream_to_socket(keyFile, &buffer, sl, 1024);
    keyFile.close();

    cout << "Base File streaming complete." << endl;

    if (!recv_ack(sl))
    {
#ifdef DEBUG
        cout << "Socket buffer error, no ACK." << endl;
        exit(0);
#endif
    }

    cout << "Streaming Context..." << endl;;

    stream << *sd->context;
    stream_to_socket(stream, &buffer, sl, 1024);

    cout << "Context Stream Complete." << endl;

    stream.str("");
    stream.clear();

    if (!recv_ack(sl))
    {
#ifdef DEBUG
        cout << "Socket buffer error, no ACK." << endl;
        exit(0);
#endif
    }

    cout << "Streaming public Key..." << endl;

    /**
    Push the contents of the public key into
    the stream,
    Stream 1024 characters (bytes) at a time.
    Exit streaming when less than 1024 bytes have
    been read.
    **/
    keyFile.open("myKey.srv", fstream::out | fstream::trunc);
    keyFile << *sd->publicKey;
    keyFile.close();

    keyFile.open("myKey.srv", fstream::in);
    stream_to_socket(keyFile, &buffer, sl ,1024);
    keyFile.close();
/*
    stream << *sd->publicKey;
    stream.clear();
    stream_to_socket(stream, &buffer, sl, 1024);
*/
    cout << "Public Key streaming complete." << endl;

    if (!recv_ack(sl))
    {
#ifdef DEBUG
        cout << "Socket buffer error, no ACK." << endl;
        exit(0);
#endif
    }

    cout << "Ready to do stuff" << endl;

    delete [] buffer;
}

/*****************************
 *Multithreading handler for the client
 *connection. Uses the mutex in the shared
 *ServerData struct to make sure no
 *data corruption occurs.
 *****************************/
void *handle_client(void *param)
{
    ClientLink * me = (ClientLink*) param;

    //pthread_mutex_lock(&me->server->mutex);
    sem_wait(&me->server->kickittome);

    me->server->users++;
    me->thisClient = me->server->users;
    /**
    This is blocking apparently.
    Send the FHE details to the client
    via socket.
    **/
    if (me->server->currentuser != me->thisClient)
    {
        cout << "Client " << me->thisClient << endl;
        cout << " waiting for mutex."  << endl;
        /*
        pthread_cond_wait(&me->server->myturn,
                          &me->server->mutex);
        */
        //pthread_barrier_wait(&me->server->barrier);
    }

    cout << "New Client " << me->thisClient << " Accepted" << endl;

    generate_upkg_android(me->server, &me->link);

    sem_post(&me->server->kickittome);

    /**
    Primary loop to process client positions.
    **/
    while(true)
    {
        if (me->server->currentuser == me->thisClient)
        {
            sem_wait(&me->server->kickittome);
            if (me->server->users == me->thisClient)
            {
                handle_new_user_socket(me->server, &me->link);

                /**
                If I am the first user, block until another user
                joins. Otherwise reset counter to the beginning.
                **/
                if (me->server->users > 1)
                {
                    me->server->currentuser = 1;
                    //pthread_cond_broadcast(&me->server->myturn);
                }
                else
                {
                    me->server->currentuser++;
                    //pthread_cond_broadcast(&me->server->myturn);
                }
            }
            else
            {
                handle_user_socket(me->server, &me->link,
                                   me->thisClient);

                /**
                If I am the most recently join user,block until
                another user joins. Otherwise reset counter to
                the beginning and start from first user.
                **/
                if (me->server->currentuser == me->server->users)
                {
                    me->server->currentuser = 1;
                    //pthread_cond_broadcast(&me->server->myturn);
                }
                else
                {
                    me->server->currentuser++;
                    //pthread_cond_broadcast(&me->server->myturn);
                }
            }

            cout << "Next Client: " << me->server->currentuser;
            cout << ", Me: " << me->thisClient;
            cout << ", " << me->id << endl;

            sem_post(&me->server->kickittome);
        }

        else
        {
            /*
            pthread_cond_wait(&me->server->myturn,
                              &me->server->mutex);
            */
            cout << me->thisClient;
            cout << " : " << me->server->threadID[me->thisClient] << endl;
            sleep(10);
        }

        //pthread_barrier_wait(&me->server->barrier);
    }
    //pthread_mutex_unlock(&me->server->mutex);
}

/*********************
 *Thread to handle the information
 *queue between threads.
 *********************/
void *handle_queue(void *param)
{
    QueueHandle * threads = (QueueHandle*) param;

    //pthread_mutex_lock(&threads->server->mutex);

    cout << "QueueHandler Started." << endl;

    while(true) {
        /*
        if ((threads->server->currentuser >=
           threads->server->users) &&
           (threads->server->users > 1))
        {
            pthread_cond_broadcast(&threads->server->myturn);
        }

        pthread_cond_wait(&threads->server->myturn
                          &threads->server->mutex);
        */

    }
    //pthread_mutex_unlock(&threads->server->mutex);
}

/*********FHE FUNCTIONS*******************/
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

/**********************
 *Socket code to handle connection between client and
 *server, takes a client's encrypted location and the vector
 *of encrypted co-ordinate sets as input.
 *Returns the updated vector of co-ords as an output
 *and sends the distances to the client via socket.
 *********************/
void handle_user_socket(ServerData * sd, ServerLink * sl, int id)
{
    fstream fs;
    stringstream stream;
    char * buffer = new char[1025];

    /**
    Create temporary copy of client's public key,
    then import key data from socket.
    **/
    cout << "Getting the client's public key." << endl;

    FHEPubKey pk(*sd->context);

    fs.open("PubKey.srv", fstream::out | fstream::trunc);
    socket_to_stream(fs, &buffer, sl, 1024);
    fs.close();

    fs.open("PubKey.srv", fstream::in);
    fs >> pk;
    fs.close();

/*
    socket_to_stream(stream, &buffer, sl, 1024);
    stream >> pk;
    stream.str("");
    stream.clear();
*/
    cout << "Public Key obtained." << endl;

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

    cout << "Acquiring Encrypted Position" << endl;

    Ctxt newusr(*sd->publicKey);
    socket_to_stream(stream, &buffer, sl, 1024);
    stream >> newusr;

    cout << "Encrypted Location obtained" << endl;

    stream.str("");
    stream.clear();

    /**
    Send ACK when the ciphertext is acquired.
    **/
    if (!send_ack(sl))
    {
#ifdef DEBUG
        cout << "Socket buffer error." << endl;
        exit(0);
#endif
    }

    send_ack(sl);

    /**
    Generate output ciphertext
    **/
    Ctxt out = generate_output(newusr, sd, pk);

    /**
    Push to socket.
    **/
    stream << out;
    stream.clear();
    stream_to_socket(stream, &buffer, sl, 1024);;

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
    sd->positions[id] = newusr;

    delete [] buffer;
}

/*******************************
 *Handle a new user connecting to the server for the first time.
 *Either process their location and send it, then append
 *their position to the current vector,
 *Or simply send add them to the vector if they are the first
 *client to join.
 *******************************/
void handle_new_user_socket(ServerData * sd, ServerLink * sl)
{
    fstream fs;
    stringstream stream;
    char * buffer = new char[1025];

    /**
    Create temporary copy of client's public key,
    then import key data from socket.
    **/

    cout << "Getting the NEW client's public key." << endl;

    FHEPubKey pk(*sd->context);

    fs.open("PubKey.srv", fstream::out | fstream::trunc);
    socket_to_stream(fs, &buffer, sl, 1024);
    fs.close();

    fs.open("PubKey.srv", fstream::in);
    fs >> pk;
    fs.close();
/*
    socket_to_stream(stream, &buffer, sl, 1024);
    stream >> pk;
    stream.str("");
    stream.clear();
*/

    cout << "Public Key obtained." << endl;

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

    cout << "Acquiring Encrypted Position" << endl;

    Ctxt newusr(*sd->publicKey);
    socket_to_stream(stream, &buffer, sl, 1024);
    stream >> newusr;

    cout << "Encrypted Location obtained" << endl;

    stream.str("");
    stream.clear();

    /**
    Send ACK when the ciphertext is acquired.
    **/
    if (!send_ack(sl))
    {
#ifdef DEBUG
        cout << "Socket buffer error." << endl;
        exit(0);
#endif
    }

    /**
    If this client is not the first,
    process their location.
    **/
    if (sd->users > 1)
    {
        send_ack(sl);
        cout << "Generating output vector" << endl;
        /**
        Generate output ciphertext, push to socket.
        **/

        Ctxt out = generate_output(newusr, sd, pk);

        stream << out;
        stream_to_socket(stream, &buffer, sl, 1024);

        cout << "Output sent" << endl;

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
    }
    else
    {
        send_nak(sl);
    }

    delete [] buffer;
    sd->positions.push_back(newusr);
}

/**********************************
 *Every time a user sends in co-ords,
 *return an output to them, then rebuild
 *the co-ordinate vector.
 *********************************/
vector<Ctxt> handle_user(vector<Ctxt>  locs, ServerData * sd,
                         Ctxt newusr, string outname, string keyfile)
{

    fstream fs;

    //Import the client's public key
    fs.open(&keyfile[0], fstream::in);
    FHEPubKey pk(*sd->context);
    fs >> pk;
    fs.close();

    //Generate an output using for the connected client.
    fs.open(&outname[0], fstream::out | fstream::trunc);
    Ctxt out = generate_output(newusr, sd, pk);
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

/***************SOCKET FUNCTIONS**********************/
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
    sl->xfer = read(sl->sockFD, *buffer, blocksize);
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
    if (write_to_socket(&buffer, sizeof(buffer), sl) == sizeof(buffer)) {
        delete [] buffer;
        return true;
    }
    delete [] buffer;
    return false;
}

/**********
 *Thing to handle first client
 *********/
bool send_nak(ServerLink * sl)
{
    char * buffer = new char[4];
    bzero(buffer, sizeof(buffer));
    buffer[0] = 'N';
    buffer[1] = 'A';
    buffer[2] = 'K';
    buffer[3] = '\0';
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
/*
    if (k == 0) {
        bzero(*buffer, sizeof(*buffer));
        *buffer[0] = '\n';
        write_to_socket(buffer, 1, sl);
    }
*/
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
        cout << totalloops << " : " << rx << " : " << stream.tellp() << endl;
#endif // DEBUG
}
