#include "FHEsrv.h"
#include <sys/resource.h>

//#define DEBUG
/***********************************
 *Server program for operating on encrypted data.
 *Generates the basis for the security scheme
 *and distributes it amongst connected clients.
 ************************************/

int main(int argc, char * argv[])
{
    /**
    Data structures and buffers
    **/
    ServerData sd;
    ServerLink sl;
    sd.mutex = PTHREAD_MUTEX_INITIALIZER;

    /**
    Argument check... just in case...
    **/
    if (argc != 3)
    {
        cout << "./FHEops_x portnum maxclients" << endl;
        return 0;
    }

    sd.maxthreads = atoi(argv[2]);
    pthread_t freads[sd.maxthreads];

    generate_scheme(&sd);

    cout << "FHE Scheme generated." << endl;

    prepare_server_socket(&sl, argv);

    cout << "Connection open for clients." << endl;

    sd.users = 0;

    while (true)
    {
        /**
        Handle new connections if there is space.
        **/
        if (sd.users < sd.maxthreads)
        {
            /**
            Generate all of the necessary data
            structures to handle a new user.
            **/
            sl.len = sizeof(sl.clientAddr);
            ClientLink client;
            client.id = freads[sd.users];
            client.server = &sd;
            sd.threadID.push_back(client.id);

            if ((client.sockFD = accept(sl.sockFD,
                                        (struct sockaddr *)&sl.clientAddr,
                                        &sl.len)) > 0)
            {
                /**Barrier management code, rebuild the barrier
                 *whenever a new user connects.
                 **/
                if (sd.users > 1)
                {
                    pthread_barrier_destroy(&sd.popcap);
                    pthread_barrier_init(&sd.popcap,
                                        NULL,
                                        sd.users + 1);
                }
                else if (sd.users < 1)
                {
                    pthread_barrier_init(&sd.popcap,
                                        NULL, 2);
                }

                pthread_create(&freads[sd.users],
                               NULL,
                               handle_client,
                               (void*)&client);
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
    Destroy synchronisation things.
    **/
    pthread_barrier_destroy(&sd.popcap);
    pthread_mutex_destroy(&sd.mutex);

    return 0;
}

/**********LOGISTICS FUNCTIONS*********************/
/**********************************
 *Stream the Context Base, Context and
 *Public Key to a connected client.
 *Socket mode version.
 **********************************/
void generate_upkg(ServerData * sd, ClientLink * sl)
{
    stringstream stream;
    char * buffer = new char[1025];

#ifdef DEBUG
    cout << "Streaming base data..." << endl;
#endif // DEBUG

    writeContextBase(stream, *sd->context);
    stream_to_socket(stream, &buffer, sl, 1024);
    stream.str("");
    stream.clear();

    recv_ack(sl);

    cout << "Base File streaming complete." << endl;

#ifdef DEBUG
    cout << "Streaming Context..." << endl;;
#endif // DEBUG

    stream << *sd->context;
    stream_to_socket(stream, &buffer, sl, 1024);
    stream.str("");
    stream.clear();

    recv_ack(sl);

    cout << "Context Stream Complete." << endl;

#ifdef DEBUG
    cout << "Obtaining client public key." << endl;
#endif // DEBUG

    socket_to_stream(stream, &buffer, sl ,1024);
    Cluster cx(*sd->context);
    stream >> *cx.thisKey;

#ifdef DEBUG
    cout << "Appending to Cluster." << endl;
#endif // DEBUG

    sd->cluster.push_back(cx);
    stream.str("");
    stream.clear();

    send_ack(sl);

    cout << "Client install complete." << endl;

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
    ClientLink me = *(ClientLink*) param;
    struct timeval timeout;
    fd_set incoming;
    fd_set master;

    pthread_mutex_lock(&me.server->mutex);

    me.thisClient = me.server->users;

    me.server->users++;

    cout << "New client " << me.thisClient << " accepted on ";
    cout << me.sockFD << "." << endl;

    generate_upkg(me.server, &me);

    /**This barrier is to allow other clients to add
      *this client's public key to their encrypted
      *locations**/
    pthread_mutex_unlock(&me.server->mutex);
    pthread_barrier_wait(&me.server->popcap);

    /**Barrier tomfoolery to handle the first client**/
    if (me.thisClient == 0)
    {
        pthread_barrier_wait(&me.server->popcap);
    }

    pthread_mutex_lock(&me.server->mutex);

    get_client_position(me.server, &me, me.thisClient);

    if (me.thisClient == 1)
    {
        pthread_mutex_unlock(&me.server->mutex);
        pthread_barrier_wait(&me.server->popcap);
        pthread_barrier_wait(&me.server->popcap);
        pthread_mutex_lock(&me.server->mutex);
    }

    calculate_distances(me.server, &me, me.thisClient);

    pthread_mutex_unlock(&me.server->mutex);
    pthread_barrier_wait(&me.server->popcap);

    /**Final bit of tomfoolery**/
    if (me.thisClient == 0)
    {
        pthread_barrier_wait(&me.server->popcap);
    }

    cout << "Client " << me.thisClient;
    cout << " entering the mainloop." << endl;

    /**
    Primary loop to process client positions.
    **/
    while(true)
    {
        FD_ZERO(&incoming);
        FD_ZERO(&master);
        timeout.tv_sec = 10;
        FD_SET(me.sockFD, &master);
        incoming = master;
        select(me.sockFD + 1, &incoming, NULL, NULL, &timeout);

        /**
        Test the size of this client's location vector
        vs the amount of users on the server, and see if it
        needs updating.
        **/
        if (me.server->cluster[me.thisClient].thisLoc.size() !=
            me.server->users)
        {
            recv_ack(&me);

            cout << "Client " << me.thisClient;
            cout << " requesting mutex: update." << endl;

            pthread_mutex_lock(&me.server->mutex);

            cout << "Client " << me.thisClient;
            cout << " has the mutex for updating their location." << endl;

            get_client_position(me.server, &me, me.thisClient);

            pthread_mutex_unlock(&me.server->mutex);

            cout << "Client " << me.thisClient;
            cout << " surrendering mutex:update." << endl;

            pthread_barrier_wait(&me.server->popcap);

            pthread_barrier_wait(&me.server->popcap);

            cout << "Client " << me.thisClient;
            cout << " requesting mutex:update" << endl;

            pthread_mutex_lock(&me.server->mutex);

            cout << "Client " << me.thisClient;
            cout << " has the mutex for updating distances." << endl;

            calculate_distances(me.server, &me, me.thisClient);

            pthread_mutex_unlock(&me.server->mutex);
            cout << "Client " << me.thisClient;
            cout << " surrendering mutex:update" << endl;
        }

        if (FD_ISSET(me.sockFD, &incoming))
        {
            recv_ack(&me);
            cout << "Client " << me.thisClient;
            cout << " requesting mutex." << endl;

            pthread_mutex_lock(&me.server->mutex);

            cout << "Client " << me.thisClient;
            cout << " has the mutex for uploading their new position." << endl;

            get_client_position(me.server, &me, me.thisClient);
            calculate_distances(me.server, &me, me.thisClient);

            pthread_mutex_unlock(&me.server->mutex);
            cout << "Client " << me.thisClient;
            cout << " surrendering mutex." << endl;
        }
        else
        {
            cout << "Client " << me.thisClient;
            cout << " user input timeout." << endl;
        }
    }
}

/*******************************
 *Obtains the location of the client encrypted
 *with all of the public keys on the server at
 *any one time.
 *******************************/
void get_client_position(ServerData * sd, ClientLink * sl, int id)
{
    stringstream stream;
    char * buffer = new char[1025];
    int k;

#ifdef DEBUG
    cout << "Acquiring encrypted positions" << endl;
#endif // DEBUG

    /**Erase previous locations if they exist**/
    if (sd->cluster[id].thisLoc.size() > 0)
    {
        sd->cluster[id].thisLoc.erase(sd->cluster[id].thisLoc.begin(),
                                        sd->cluster[id].thisLoc.end());
    }

    /**
    Create temporary copy of client's public key,
    then import key data from socket.
    **/
    for (k = 0; k < sd->users; k++)
    {
        send_ack(sl);
        Ctxt newusr(*sd->cluster[k].thisKey);
        stream << *sd->cluster[k].thisKey;
        stream_to_socket(stream, &buffer, sl, 1024);
        stream.str("");
        stream.clear();

        socket_to_stream(stream, &buffer, sl, 1024);
        stream >> newusr;
        stream.str("");
        stream.clear();

        sd->cluster[id].thisLoc.push_back(newusr);
    }

    send_nak(sl);

#ifdef DEBUG
    cout << "Locations for client " << sl->thisClient;
    cout << " loaded." << endl;
#endif

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

    delete [] buffer;
}

/**********************
 *Calculate the distances between this client and all of the
 *others on the server. Upon completion of the distance processing,
 *sends the distances as an encrypted vector back to this client
 *via socket.
 *********************/
void calculate_distances(ServerData * sd, ClientLink * sl, int id)
{
    stringstream stream;
    char * buffer = new char[1025];
    int k;

    /**Erase previous distances if they exist**/
    if (sd->cluster[id].theirLocs.size() > 0)
    {
        sd->cluster[id].theirLocs.erase(sd->cluster[id].theirLocs.begin(),
                                    sd->cluster[id].theirLocs.end());
    }

    /**
    Get ciphertexts with the correct index from the cluster
    **/
    for (k = 0; k < sd->cluster.size(); k++)
    {
        if (k != id)
        {
            sd->cluster[id].theirLocs.push_back(sd->cluster[k].thisLoc[id]);
        }
    }

    /**
    Generate output ciphertexts for the client
    **/
    Ctxt out = generate_output(sd->cluster[id].thisLoc[id],
                          sd->cluster[id].theirLocs,
                          *sd->cluster[id].thisKey);

#ifdef DEBUG
    cout << "Output for client " << sl.thisClient;
    cout << " generated." << endl;
#endif // DEBUG

    /**
    Push to socket.
    **/
    stream << out;
    stream_to_socket(stream, &buffer, sl, 1024);
    stream.str("");
    stream.clear();

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

/*********FHE FUNCTIONS*******************/
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

    m = FindM(security, L, c, p, d, 3, 0);
    sd->context = new FHEcontext(m, p, r);
    buildModChain(*sd->context, L, c);

#ifdef DEBUG
    cout << "Scheme Generation Complete." << endl;
#endif

    return 1;
}

/****************************
 *When a new position comes in,
 *generate an output ciphertext that
 *should correspond to the square of
 *the distance to each other user
 *****************************/
Ctxt generate_output(Ctxt input, vector<Ctxt> locs,
                     const FHEPubKey &pk)
{
    int i;

    /**
    Blank vector and ciphertext to facilitate operations
    on server key encrypted input. pvec() is initialised with
    as an empty vector encrypted with the server's public key.
    **/
    Ctxt cx(pk);
    vector<long> pvec;

    for (int i = 0; i < pk.getContext().ea->size(); i++)
    {
		pvec.push_back(0);
	}

	pk.getContext().ea->encrypt(cx, pk, pvec);

	/**
	Iterate over the current collection of encrypted coordinate
	sets and create a single ciphertext corresponding to the
	Euclidean distances between the input and all of the existing
	co-ordinates.
	**/
    for (i=0; i < locs.size(); i++)
    {
        Ctxt zx(locs[i]); /**Copy coordinate set i to a temporary Ctxt**/
        zx = compute(zx, input, pk); /**Calculate distance**/
        zx.getContext().ea->shift(zx, i); /**Offset Ctxt based on index i**/
        cx+=zx; /**Add offset ciphertext to empty output**/
    }

    return cx;
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
int stream_from_socket(char ** buffer, int blocksize, ClientLink * sl)
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
int write_to_socket(char ** buffer, int blocksize, ClientLink * sl)
{
    int p;
    sl->xfer = write(sl->sockFD, *buffer, blocksize);
    p = sl->xfer;
    bzero(*buffer, sizeof(*buffer));
    return p;
}

/**************
 *Send update message to client,
 *so it can continue work.
 *************/
void send_server_update(ClientLink * sl)
{
    char * buffer = new char[1025];
    stringstream stream("UPDATED");

    stream_to_socket(stream, &buffer, sl, 1024);

    delete [] buffer;
}

/*******************************
 *Quick function to help with timing.
 *Sends an ACK command to the server.
 *******************************/
bool send_ack(ClientLink * sl)
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
bool send_nak(ClientLink * sl)
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
bool recv_ack(ClientLink * sl)
{
    char * buffer = new char[4];
    char * ack = new char[4];
    bzero(buffer, sizeof(buffer));
    bzero(ack, sizeof(ack));
    ack[0] = 'A';
    ack[1] = 'C';
    ack[2] = 'K';
    ack[3] = '\0';
    if (stream_from_socket(&buffer,
                           sizeof(buffer), sl) == sizeof(ack))
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
                      ClientLink * sl, int blocksize)
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
                      ClientLink * sl, int blocksize)
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
