#include "FHEsrv.h"

/**********************************************
 *Server program for operating on encrypted data.
 *Generates the basis for the security scheme
 *and distributes it amongst connected clients.
 *********************************************/
int main(int argc, char * argv[])
{
    ServerData sd;
    ServerLink sl;
    sd.mutex = PTHREAD_MUTEX_INITIALIZER;
    if (argc != 6)
    {
        cerr << "./FHEops_x portnum maxclients p r L" << endl;
        exit(0);
    }
    sd.maxthreads = atoi(argv[2]);
    pthread_t freads[sd.maxthreads];
    generate_scheme(&sd, argv);
    cout << "FHE Scheme generated." << endl;
    prepare_server_socket(&sl, argv);
    cout << "Connection open for clients." << endl;
    sd.users = 0;
    while (true)
    {
        if (sd.users < sd.maxthreads)
        {
            sl.len = sizeof(sl.clientAddr);
            ClientLink client;
            client.id = freads[sd.users];
            client.server = &sd;
            sd.threadID.push_back(client.id);
            if ((client.sockFD = accept(sl.sockFD,
                                        (struct sockaddr *)&sl.clientAddr,
                                        &sl.len)) > 0)
            {
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
                cerr << "Socket Error: accept()" << endl;
                exit(2);
            }
        }
        else
        {
            cout << "Unable to accept new users, server at capacity,";
            cout << endl;
        }
    }
    cout << "Terminating threads" << endl;
    for (int i = 0; i < sd.users; i++)
    {
        pthread_join(sd.threadID[i], NULL);
    }
    pthread_barrier_destroy(&sd.popcap);
    pthread_mutex_destroy(&sd.mutex);
    return 1;
}

/**LOGISTICS FUNCTIONS**/
/**********************************
 *Stream the Context Base, Context and
 *Public Key to a connected client.
 *Socket mode version.
 **********************************/
void generate_upkg(ServerData * sd, ClientLink * sl, char ** buffer)
{
    stringstream stream;
    bzero(*buffer, sizeof(*buffer));
    writeContextBase(stream, *sd->context);
    stream_to_socket(stream, buffer, sl, BUFFSIZE);


    cout << "Base File streaming complete: " <<endl;
    stream.str("");
    stream.clear();

    try
    {
        stream << *sd->context;
    }
    catch (...)
    {
        cerr << "HElib Exception; terminating this thread." << endl;
        delete [] buffer;
        close_client_thread(sl, sl->server);
    }

    stream_to_socket(stream, buffer, sl, BUFFSIZE);

    cout << "Context Stream Complete " << endl;
    stream.str("");
    stream.clear();

    socket_to_stream(stream, buffer, sl, BUFFSIZE);
    Cluster cx(*sd->context);
    try
    {
        stream >> *cx.thisKey;
    }
    catch (...)
    {
        cerr << "HElib Exception; terminating this thread." << endl;
        delete [] buffer;
        close_client_thread(sl, sl->server);
    }
    if (sl->thisClient < (sd->users-1))
    {
        sd->cluster.insert(sd->cluster.begin()+sl->thisClient, cx);
    } else {
        sd->cluster.push_back(cx);
    }


    stream.str("");
    stream.clear();
    cout << "Client install complete." << endl;
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
    char * buffer = new char[BUFFSIZE+1];
    bzero(buffer, sizeof(buffer));
    struct timeval timeout;
    fd_set incoming;
    fd_set master;
    pthread_mutex_lock(&me.server->mutex);
    me.thisClient=free_slot(me.server->clients);
    if (me.thisClient == me.server->users) {
        me.server->clients.push_back(1);
    } else {
        me.server->clients[me.thisClient] = 1;
    }
    me.server->users++;
    cout << "New client " << me.thisClient << " accepted on ";
    cout << me.sockFD << "." << endl;
    generate_upkg(me.server, &me, &buffer);
    pthread_mutex_unlock(&me.server->mutex);
    pthread_barrier_wait(&me.server->popcap);
    if (me.thisClient == 0)
    {
        pthread_barrier_wait(&me.server->popcap);
    }
    pthread_mutex_lock(&me.server->mutex);
    get_client_position(me.server, &me, me.thisClient, &buffer);
    if (me.thisClient == 1)
    {
        pthread_mutex_unlock(&me.server->mutex);
        pthread_barrier_wait(&me.server->popcap);
        pthread_barrier_wait(&me.server->popcap);
        pthread_mutex_lock(&me.server->mutex);
    }
    calculate_distances(me.server, &me, me.thisClient, &buffer);
    pthread_mutex_unlock(&me.server->mutex);
    pthread_barrier_wait(&me.server->popcap);
    if (me.thisClient == 0)
    {
        pthread_barrier_wait(&me.server->popcap);
    }
    cout << "Client " << me.thisClient;
    cout << " active." << endl;
    while(true)
    {
        FD_ZERO(&incoming);
        FD_ZERO(&master);
        timeout.tv_sec = 10;
        FD_SET(me.sockFD, &master);
        incoming = master;
        select(me.sockFD + 1, &incoming, NULL, NULL, &timeout);

        if (me.server->cluster[me.thisClient].thisLoc.size() !=
            me.server->users)
        {
            if (recv_ack(&me))
            {
#ifdef THREADSYNC
                cout << "Client " << me.thisClient;
                cout << " requesting mutex: update." << endl;
#endif

                pthread_mutex_lock(&me.server->mutex);

#ifdef THREADSYNC
                cout << "Client " << me.thisClient;
                cout << " has the mutex for updating their location." << endl;
#endif

                get_client_position(me.server, &me, me.thisClient, &buffer);
                pthread_mutex_unlock(&me.server->mutex);

#ifdef THREADSYNC
                cout << "Client " << me.thisClient;
                cout << " surrendering mutex:update." << endl;
#endif

                pthread_barrier_wait(&me.server->popcap);
                pthread_barrier_wait(&me.server->popcap);

#ifdef THREADSYNC
                cout << "Client " << me.thisClient;
                cout << " requesting mutex:update" << endl;
#endif

                pthread_mutex_lock(&me.server->mutex);

#ifdef THREADSYNC
                cout << "Client " << me.thisClient;
                cout << " has the mutex for updating distances." << endl;
#endif

                calculate_distances(me.server, &me, me.thisClient, &buffer);
                pthread_mutex_unlock(&me.server->mutex);

#ifdef THREADSYNC
                cout << "Client " << me.thisClient;
                cout << " surrendering mutex:update" << endl;
#endif
            }
            else
            {
                delete [] buffer;
                close_client_thread(&me, me.server);
            }
        }

        if (FD_ISSET(me.sockFD, &incoming))
        {
            if (recv_ack(&me))
            {
#ifdef THREADSYNC
                cout << "Client " << me.thisClient;
                cout << " requesting mutex." << endl;
#endif

                pthread_mutex_lock(&me.server->mutex);

#ifdef THREADSYNC
                cout << "Client " << me.thisClient;
                cout << " has the mutex for uploading their new position." << endl;
#endif

                get_client_position(me.server, &me, me.thisClient, &buffer);
                calculate_distances(me.server, &me, me.thisClient, &buffer);
                pthread_mutex_unlock(&me.server->mutex);

#ifdef THREADSYNC
                cout << "Client " << me.thisClient;
                cout << " surrendering mutex." << endl;
#endif
            }
            else
            {
                delete [] buffer;
                close_client_thread(&me, me.server);
            }
        }
        else
        {
            cout << "Client " << me.thisClient;
            cout << " waiting for user input.." << endl;
        }
    }
}

/*******************************
 *Obtains the location of the client encrypted
 *with all of the public keys on the server at
 *any one time.
 *******************************/
void get_client_position(ServerData * sd, ClientLink * sl, int id,
                         char ** buffer)
{
    stringstream stream;
    bzero(*buffer, sizeof(*buffer));
    int k, j;
    j = 0;
    for (k = 0; k < id; k++) {
        if (sd->clients[k] == 0) j++;
    }
    id -= j;
    if (sd->cluster[id].thisLoc.size() > 0)
    {
        sd->cluster[id].thisLoc.erase(sd->cluster[id].thisLoc.begin(),
                                        sd->cluster[id].thisLoc.end());
    }
#ifdef MEMTEST
    fstream fs;
    long int klusta;
    klusta = 0;
    fs.open("cluster.mem", fstream::out | fstream::app);
    fs << "Number of Clients = " << sd->users << endl;
#endif // MEMTEST

    for (k = 0; k < sd->users; k++)
    {
        sock_handshake(sl);
        Ctxt newusr(*sd->cluster[k].thisKey);
        try
        {
            stream << *sd->cluster[k].thisKey;
        }
        catch (...)
        {
            cerr << "HElib Exception; terminating this thread." << endl;
            delete [] buffer;
            close_client_thread(sl, sl->server);
        }
#ifdef MEMTEST
        stream.seekg(0, ios::end);
        klusta += stream.tellg();
        cout << "Public Key at server: " << stream.tellg() << endl;
        stream.seekg(0, ios::beg);
#endif
        stream_to_socket(stream, buffer, sl, BUFFSIZE);
        stream.str("");
        stream.clear();

        socket_to_stream(stream, buffer, sl, BUFFSIZE);
        try
        {
            stream >> newusr;
        }
        catch (...)
        {
            cerr << "HElib Exception; terminating this thread." << endl;
            delete [] buffer;
            close_client_thread(sl, sl->server);
        }

#ifdef MEMTEST
        stream.seekg(0, ios::end);
        klusta += stream.tellg();
        cout << "Location at server: " << stream.tellg() << endl;
        stream.seekg(0, ios::beg);
#endif

        stream.str("");
        stream.clear();
        sd->cluster[id].thisLoc.push_back(newusr);
    }

#ifdef MEMTEST
    fs << "Size of Cluster = " << klusta << "B" << endl;
    fs.close();
#endif

    send_nak(sl);
    send_ack(sl);
}

/**********************
 *Calculate the distances between this client and all of the
 *others on the server. Upon completion of the distance processing,
 *sends the distances as an encrypted vector back to this client
 *via socket.
 *********************/
void calculate_distances(ServerData * sd, ClientLink * sl, int id,
                         char ** buffer)
{
    stringstream stream;
    bzero(*buffer, sizeof(*buffer));
    int k, j;
    j = 0;
    for (k = 0; k < id; k++) {
        if (sd->clients[k] == 0) j++;
    }
    id -= j;
    if (sd->cluster[id].theirLocs.size() > 0)
    {
        sd->cluster[id].theirLocs.erase(sd->cluster[id].theirLocs.begin(),
                                    sd->cluster[id].theirLocs.end());
    }
    for (k = 0; k < sd->cluster.size(); k++)
    {
        if (k != id)
        {
            sd->cluster[id].theirLocs.push_back(sd->cluster[k].thisLoc[id]);
        }
    }

#ifdef TIMING
    fstream fs;
    fs.open("server.time", fstream::out | fstream::app);
    struct timeval clk;
    double msecs_x, msecs_y;
    gettimeofday(&clk, NULL);
    msecs_x = clk.tv_usec / 1000;
#endif

    Ctxt out = generate_output(sd->cluster[id].thisLoc[id],
                          sd->cluster[id].theirLocs,
                          *sd->cluster[id].thisKey);
#ifdef TIMING
    gettimeofday(&clk, NULL);
    msecs_y = clk.tv_usec / 1000;
    fs << "generate_output() " << (msecs_y - msecs_x) << "ms" << endl;
    fs << sd->users << " clients online." << endl;
    fs.close();
#endif

    try
    {
        stream << out;
    }
    catch (...)
    {
        cerr << "HElib Exception; terminating this thread." << endl;
        delete [] buffer;
        close_client_thread(sl, sl->server);
    }
    stream_to_socket(stream, buffer, sl, BUFFSIZE);
    stream.str("");
    stream.clear();
}

/**FHE FUNCTIONS**/
/*******************************
 *Generate the FHE scheme with the specified
 *parameters. Writes the scheme to the ServerData
 *structure.
 *******************************/
void generate_scheme(ServerData * sd, char * argv[]) {
    long long int p = atoi(argv[3]);
    long r = atoi(argv[4]);
    long L = atoi(argv[5]);
    long security = 128;
    long m = 0;
    long c = 3;
    long w = 64;
    long d = 0;
    ZZX G;

#ifdef MEMTEST
    fstream fs;
    fs.open("cluster.mem", fstream::out | fstream::app);
    fs << "Server Memory Information" << endl;
    fs << "P = " << p << ", R = " << r << ", L = " << L << endl;
    fs.close();
    fs.open("upkg.mem", fstream::out | fstream::app);
    fs << "Client Memory Information" << endl;
    fs << "P = " << p << ", R = " << r << ", L = " << L << endl;
    fs.close();
#endif // MEMTEST

#ifdef TIMING
#ifndef MEMTEST
    fstream fs;
#endif
    fs.open("server.time", fstream::out | fstream::app);
    fs << "Server Timing Information" << endl;
    struct timeval clk;
    double msecs_x, msecs_y;
    gettimeofday(&clk, NULL);
    msecs_x = clk.tv_usec / 1000;
#endif

    m = FindM(security, L, c, p, d, 3, 0);
    sd->context = new FHEcontext(m, p, r);
    buildModChain(*sd->context, L, c);

#ifdef TIMING
    gettimeofday(&clk, NULL);
    msecs_y = clk.tv_usec / 1000;
    fs << "generate_scheme() " << (msecs_y - msecs_x);
    fs << "ms" << endl;
    fs.close();
#endif
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
    uint32_t i;
    Ctxt cx(pk);
    vector<long> pvec;
    for (i = 0; i < pk.getContext().ea->size(); i++)
    {
		pvec.push_back(0);
	}
	pk.getContext().ea->encrypt(cx, pk, pvec);
    for (i = 0; i < locs.size(); i++)
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
    uint32_t i;
    vector<long> pvec;
    Ctxt purge(pk);
    pvec.push_back(1);

#ifdef TIMING
    fstream fs;
    fs.open("server.time", fstream::out | fstream::app);
    struct timeval clk;
    double msecs_x, msecs_y;
#endif

    for (i = 1; i < pk.getContext().ea->size(); i++)
    {
		pvec.push_back(0);
	}

#ifdef TIMING
    gettimeofday(&clk, NULL);
    msecs_x = clk.tv_usec / 1000;
#endif

    pk.getContext().ea->encrypt(purge, pk, pvec);

#ifdef TIMING
    gettimeofday(&clk, NULL);
    msecs_y = clk.tv_usec / 1000;
    fs << "encrypt(empty) " << (msecs_y - msecs_x);
    fs << "ms" << endl;

    gettimeofday(&clk, NULL);
    msecs_x = clk.tv_usec / 1000;
#endif

	c1.addCtxt(c2, true); /**c1 = Enc[(x1-x2) (y1-y2) 0 0 ...] **/

#ifdef TIMING
    gettimeofday(&clk, NULL);
    msecs_y = clk.tv_usec / 1000;
    fs << "addCtxt() " << (msecs_y - msecs_x);
    fs << "ms" << endl;

    gettimeofday(&clk, NULL);
    msecs_x = clk.tv_usec / 1000;
#endif

	c1.square(); /**c1 = Enc[(x1-x2)^2 (y1-y2)^2 0 0 ...] **/

#ifdef TIMING
    gettimeofday(&clk, NULL);
    msecs_y = clk.tv_usec / 1000;
    fs << "ctxt.square() " << (msecs_y - msecs_x);
    fs << "ms" << endl;

    gettimeofday(&clk, NULL);
    msecs_x = clk.tv_usec / 1000;
#endif

	Ctxt inv(c1); /**inv = Enc[(y1-y2)^2 0 .. 0 (x1-x2)^2] **/

#ifdef TIMING
    gettimeofday(&clk, NULL);
    msecs_y = clk.tv_usec / 1000;
    fs << "invert ctxt " << (msecs_y - msecs_x);
    fs << "ms" << endl;

    gettimeofday(&clk, NULL);
    msecs_x = clk.tv_usec / 1000;
#endif

	inv.getContext().ea->rotate(inv, -1);

#ifdef TIMING
    gettimeofday(&clk, NULL);
    msecs_y = clk.tv_usec / 1000;
    fs << "rotate ctxt " << (msecs_y - msecs_x);
    fs << "ms" << endl;

#endif

	c1+=inv; /**c1=Enc[(x1-x2)^2+(y1-y2)^2 (y1-y2)^2 0 .. (x1-x2)^2]**/

#ifdef TIMING
    gettimeofday(&clk, NULL);
    msecs_x = clk.tv_usec / 1000;
#endif

	c1*=purge; /**c1 = Enc[(x1-x2)^2+(y1-y2)^2 0 0 ...] **/

#ifdef TIMING
    gettimeofday(&clk, NULL);
    msecs_y = clk.tv_usec / 1000;
    fs << "multiply ctxt " << (msecs_y - msecs_x);
    fs << "ms" << endl;
    fs.close();
#endif

	return c1;
}

/**SOCKET FUNCTIONS**/
/*****************************
 *Listen on a port provided in the
 *arguments.
 *****************************/
void prepare_server_socket(ServerLink * sl, char * argv[])
{
    sl->port = atoi(argv[1]);
    if ((sl->sockFD = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
    {
        cerr << "Socket Error: socket()" << endl;
		exit(2);
    }
    memset((char*)&sl->servAddr, 0, sizeof(sl->servAddr));
    sl->servAddr.sin_family = AF_INET;
	sl->servAddr.sin_addr.s_addr = INADDR_ANY;
	sl->servAddr.sin_port = htons(sl->port);

	if (bind(sl->sockFD, (struct sockaddr *)&sl->servAddr,
            sizeof(sl->servAddr)) < 0)
    {
        cerr << "Socket Error: bind()" << endl;
		exit(2);
	}
	listen(sl->sockFD, sl->blocklen);
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
    bzero(*buffer, sizeof(*buffer));
    sl->xfer = read(sl->sockFD, *buffer, blocksize);
    return sl->xfer;
}

/*****
 *Complementary writing function to stream_from_socket().
 *Pushes the contents of a buffer to the port.
 *This probably requires some sort of guarantee that data has not
 *been lost.
*****/
int write_to_socket(char ** buffer, int blocksize, ClientLink * sl)
{
    sl->xfer = write(sl->sockFD, *buffer, blocksize);
    bzero(*buffer, sizeof(*buffer));
    return sl->xfer;
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
                           sizeof(buffer), sl) <= sizeof(buffer))
    {
        buffer[3] = '\0';
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
 *Sends ACK to the client and awaits an ACK in return.
 *Necessary to break up data transmission and
 *prevent the FHBB pipes from flooding.
 *******************/
bool sock_handshake(ClientLink * sl)
{
    if (!send_ack(sl)) return false;
    if (!recv_ack(sl)) return false;
    return true;
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

        if (!recv_ack(sl))
        {
            cerr << "ACK error: socket data block; terminating this thread." << endl;
            close_client_thread(sl, sl->server);
        }
    }
    while (k == blocksize);

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
                      ClientLink * sl, int blocksize)
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

        send_ack(sl);
    }
    while (k == blocksize);
    stream.clear();

#ifdef TRANSFER
        cout << totalloops << " : " << rx;
        cout << " : " << stream.tellp() << endl;
#endif // TRANSFER
}

void close_client_thread(ClientLink * sl, ServerData * server) {
    close(sl->sockFD);
    server->cluster[sl->thisClient].thisLoc.erase(server->cluster[sl->thisClient].thisLoc.begin(),
                                                      server->cluster[sl->thisClient].thisLoc.end());
    server->cluster[sl->thisClient].theirLocs.erase(server->cluster[sl->thisClient].theirLocs.begin(),
                                                      server->cluster[sl->thisClient].theirLocs.end());
    delete server->cluster[sl->thisClient].thisKey;
    server->cluster.erase(server->cluster.begin()+sl->thisClient);
    cout << "Client " << sl->thisClient << " disconnecting." << endl;
    server->clients[sl->thisClient]=0;
    server->users--;
    pthread_join(server->threadID[sl->thisClient], NULL);
}

int free_slot(vector<int> input) {
    if (input.size()==0) {
        return 0;
    }
    int i;
    for (i=0; i<input.size(); i++) {
        if (input[i]==0) return i;
    }
    return i;
}
