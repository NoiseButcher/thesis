#include "BBHandler.h"
#include <sys/resource.h>

#define DEBUG
/***********************************
Testing rig for the black box binary. Interfaces with sockets to
the server and a terminal with the user to simulate the communication
layer above all of the FHE stuff. Forks itself and executes the
FHBB binary, using the pipes too and from it
 ************************************/
int main(int argc, char * argv[])
{
    /*********************
     *Client instance data
     *structures and buffers
     *for per-instance operation.
     *********************/
    ServerLink op;
    pid_t blackbox;
    int bb_in[2];
    int bb_out[2];
    char * buffer = new char[1025];
    stringstream stream;

    /**
    Check input arguments.
    This only requires server information
    for the PC version. The android version uses
    the i/o to talk to the Java software.
    **/
    if (argc != 3)
    {
        cout << "./BBHandler_x portnum localhost(hostname)" << endl;
        return 0;
    }

    pipe(bb_in);
    pipe(bb_out);
    blackbox = fork();

    /**CHILD I AM IN YOU**/
    if (blackbox == 0)
    {
        /**Make cin pipe directly to the parent**/
        close(0);
        if ((dup2(bb_in[0], 0) != 0) ||
            (close(bb_in[0]) != 0) ||
            (close(bb_in[1]) != 0))
        {
            cerr << "OH MY GOD THEY ARE COMING FROM CIN!!" << endl;
            exit(0);
        }


        /**Make cout pipe directly to the parent**/
        close(1);
        if ((dup2(bb_out[1], 1) != 1) ||
            (close(bb_out[0]) != 0) ||
            (close(bb_out[1]) != 0))
        {
            cerr << "OH MY GOD THEY ARE COMING FROM COUT!!" << endl;
            exit(0);
        }

        /**Run the black box **/
        execl("/home/sharky/Thesis/Guy_Code/FHBB_x", "FHBB_x", NULL);

        cout << "Execution failure." << endl;

        return 1;
    }
    /**I AM YOUR FUCKING DADDY**/
    else
    {
        close(bb_out[1]);
        close(bb_in[0]);
        bzero(buffer, sizeof(buffer));

        /**
        Connect to the server or throw an error if it can't.
        **/
        if (!(prepare_socket(&op, argv)))
        {
            cout << "Server Unavailable.";
            return 0;
        }

#ifdef DEBUG
        cout << "Connection Established on fd: " << op.sockFD << endl;
        cout << "FHBB input on fd: " << bb_in[1] << endl;
        cout << "FHBB output on fd: " << bb_out[0] << endl;
#else
        cout << "Let's measure some shit." << endl;
#endif // DEBUG

        stream.str("Testu");
        handler_to_pipe(stream, bb_in[1], bb_out[0], &buffer, 1024);
        stream.str("");
        stream.clear();
        pipe_to_handler(stream, bb_in[1], bb_out[0], &buffer, 1024);
        cerr << "Test message: " << stream.str() << endl;
        stream.str("");
        stream.clear();

        install_upkg_handler(bb_in[1], bb_out[0], &buffer,
                             &op, 1024);

        get_gps_handler(bb_in[1], bb_out[0], &buffer, 1024);

        send_location_handler(bb_in[1], bb_out[0], &buffer, &op,
                              1024);

        get_distance_handler(bb_in[1], bb_out[0], &buffer, &op,
                             1024);

        display_positions_handler(bb_in[1], bb_out[0], &buffer, 1024);

        while(true)
        {
            get_gps_handler(bb_in[1], bb_out[0], &buffer, 1024);

            /**Pipe ACK from black box to handler**/
            pipe_to_handler(stream, bb_in[1], bb_out[0], &buffer, 1024);

            /**Check contents of transfer**/
            cout << stream.str() << " for GPS." << endl;

            /**Pipe ACK from handler to server**/
            handler_to_socket(stream, &buffer, &op, 1024);

            /**Clear stream buffer**/
            stream.str("");
            stream.clear();

            send_location_handler(bb_in[1], bb_out[0], &buffer, &op,
                              1024);

            get_distance_handler(bb_in[1], bb_out[0], &buffer,
                                 &op, 1024);

            display_positions_handler(bb_in[1], bb_out[0], &buffer,
                                      1024);
        }

        close(bb_out[0]);
        close(bb_in[1]);
    }

    return 0;
}

/*********LOGISTICS FUNCTIONS***********/
/***************************
 *This function expects the
 *latitude and longitude of
 *the client in radians multiplied
 *by 1000.
 *As it is an integer there is no decimal precision.
 **************************/
void get_gps_handler(int infd, int outfd, char ** buffer,
                       int blocksize)
{
    stringstream stream;
    string input;
    cout << "Enter a position." << endl;
    pipe_to_handler(stream, infd, outfd, buffer, blocksize);
    cout << stream.str() << endl; //should be "X"
    stream.str("");
    stream.clear();
    cin >> input;
    stream << input;
    handler_to_pipe(stream, infd, outfd, buffer, blocksize);
    stream.str("");
    stream.clear();
    pipe_to_handler(stream, infd, outfd, buffer, blocksize);
    cout << stream.str() << endl; //should be "Y"
    stream.str("");
    stream.clear();
    cin >> input;
    stream << input;
    handler_to_pipe(stream, infd, outfd, buffer, blocksize);
    stream.str("");
    stream.clear();
}

/*********************
 *Display the proximity
 *of other users. Prints
 *to primary I/O, so only
 *one mode.
 **********************/
void display_positions_handler(int infd, int outfd, char ** buffer,
                               int blocksize)
{
    stringstream stream;

    pipe_to_handler(stream, infd, outfd, buffer, blocksize);
    cout << stream.str() << endl;

}

/**********************************
 *Install the FHE scheme to the black
 *box binary. Serious plumbing happens here.
 *The ACK between the server and client is printed
 *from the interface so that the debugging
 *can be easily followed up.
 *********************************/
void install_upkg_handler(int infd, int outfd, char ** buffer,
                          ServerLink * sl, int blocksize)
{
    stringstream stream;

#ifdef DEBUG
    socket_to_pipe(infd, outfd, buffer, sl, blocksize); //Base

    pipe_to_handler(stream, infd, outfd, buffer, blocksize);

    cerr << stream.str() << " Base." << endl;

    handler_to_socket(stream, buffer, sl, blocksize);
    stream.str("");
    stream.clear();

    socket_to_pipe(infd, outfd, buffer, sl, blocksize); //Context

    pipe_to_handler(stream, infd, outfd, buffer, blocksize);

    cerr << stream.str() << " Context." << endl;

    handler_to_socket(stream, buffer, sl, blocksize);
    stream.str("");
    stream.clear();

    pipe_to_socket(infd, outfd, buffer, sl, blocksize); //Public Key

    socket_to_handler(stream, buffer, sl, blocksize);

    cout << stream.str() << " Public Key." << endl;

    handler_to_pipe(stream, infd, outfd, buffer, blocksize);
    stream.str("");
    stream.clear();

#else
    socket_to_pipe(infd, buffer, sl, blocksize); //Base
    pipe_to_socket(outfd, buffer, sl, blocksize); //send ACK
    socket_to_pipe(infd, buffer, sl, blocksize); //Context
    pipe_to_socket(outfd, buffer, sl, blocksize); //send ACK
    pipe_to_socket(outfd, buffer, sl, blocksize); //Public Key
    socket_to_pipe(infd, buffer, sl, blocksize); //get ACK
#endif
}

/*********************************
 *Middle-man location streaming function.
 *The ACK is received by the handler, whereas all of the other
 *data is piped directly from the socket to the black box.
 **********************************/
void send_location_handler(int infd, int outfd, char ** buffer,
                          ServerLink * sl, int blocksize)
{
    stringstream stream;

    /**Pipe ACK from server to handler**/
    socket_to_handler(stream, buffer, sl, blocksize);

    while (stream.str() == "ACK")
    {
        /**Pipe ACK from handler to black box**/
        handler_to_pipe(stream, infd, outfd, buffer, blocksize);
        /**Clear stream buffer**/
        stream.str("");
        stream.clear();
        /**Pipe a Public Key from server to black box**/
        socket_to_pipe(infd, outfd, buffer, sl, blocksize);
        /**Pipe the corresponding encrypted location to server**/
        pipe_to_socket(infd, outfd, buffer, sl, blocksize);
        /**Pipe ACK from server to handler**/
        socket_to_handler(stream, buffer, sl, blocksize);
    }

    /**Pipe NAK from handler to black box**/
    handler_to_pipe(stream, infd, outfd, buffer, blocksize);
    /**Clear stream buffer**/
    stream.str("");
    stream.clear();

    /**Pipe ACK from server to handler**/
    socket_to_handler(stream, buffer, sl, blocksize);
    /**Pipe ACK from handler to black box**/
    handler_to_pipe(stream, infd, outfd, buffer, blocksize);
    /**Clear stream buffer**/
    stream.str("");
    stream.clear();
}

/************************************
 *Pipes the encrypted distances between the server and the
 *black box so that they can be decrypted.
 **********************************/
void get_distance_handler(int infd, int outfd, char ** buffer,
                          ServerLink * sl, int blocksize)
{
    stringstream stream;

    /**Get those encrypted distances**/
    socket_to_pipe(infd, outfd, buffer, sl, blocksize);

    /**Pipe ACK from black box to handler**/
    pipe_to_handler(stream, infd, outfd, buffer, blocksize);

    /**Check contents of transfer**/
    cout << stream.str() << " Context." << endl;

    /**Pipe ACK from handler to server**/
    handler_to_socket(stream, buffer, sl, blocksize);

    /**Clear stream buffer**/
    stream.str("");
    stream.clear();

}

/*******************COMMUNICATION FUNCTIONS************************/
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

/****************************
 *Function to allow data transfer from
 *an input stream to a socket with
 *consistent block size.
 ****************************/
void handler_to_socket(istream &stream, char ** buffer,
                      ServerLink * sl, int blocksize)
{
    bzero(*buffer, sizeof(*buffer));
    int k;

    do
    {
        k = 0;
        stream.read(*buffer, blocksize);
        k = stream.gcount();
        write_to_socket(buffer, k, sl);
        bzero(*buffer, sizeof(*buffer));

    }
    while (k == blocksize);
}

/******************************
 *Reads blocksize socket data from
 *a TCP socket and writes it to the
 *stream specified as the first argument.
 ******************************/
void socket_to_handler(ostream &stream, char ** buffer,
                      ServerLink * sl, int blocksize)
{
    bzero(*buffer, sizeof(*buffer));
    int k;

    do
    {
        k = 0;
        k = stream_from_socket(buffer, blocksize, sl);
        stream.write(*buffer, k);
        bzero(*buffer, sizeof(*buffer));
    }
    while (k == blocksize);
    stream.clear();
}

/*****************************
 *Read data from stream buffer and send it down the pipe
 *one block at a time.
 *Use this to communicate between handler and FHBB.
 ****************************/
void handler_to_pipe(istream &stream, int infd, int outfd,
                     char** buffer, int blocksize)
{

    bzero(*buffer, sizeof(*buffer));
    int x;

    do
    {
        x = 0;
        stream.read(*buffer, blocksize);
        x = stream.gcount();
        write(infd, *buffer, x);

        cerr << *buffer << " : to FHBB from handler." << endl;

        bzero(*buffer, sizeof(*buffer));
        *buffer[0] = '\n';
        *buffer[1] = '\0';
        write(infd, *buffer, 2);
        bzero(*buffer, sizeof(*buffer));

        recv_ack_pipe(outfd);
        cerr << "for handler." << endl;
    }
    while (x == blocksize);
}

/*******************
 *Read from a pipe and place data into a stream buffer
 *Use this to communicate between handler and FHBB.
 ******************/
void pipe_to_handler(ostream &stream, int infd, int outfd,
                     char ** buffer, int blocksize)
{
    bzero(*buffer, sizeof(*buffer));
    int x;

    do
    {
        x = 0;
        x = read(outfd, *buffer, blocksize);
        stream.write(*buffer, x);

        cerr << *buffer << " : from FHBB to handler." << endl;

        bzero(*buffer, sizeof(*buffer));
        //send_ack_pipe(infd);
    }
    while (x == blocksize);

    stream.clear();
}

/********************
 *Communicate between FHBB and server directly, this function does
 *not preserve any data for the handler to see. But it is a
 *bajillion times more space efficient,
 *******************/
void pipe_to_socket(int infd, int outfd, char ** buffer,
                    ServerLink * sl, int blocksize)
{
    bzero(*buffer, sizeof(*buffer));
    int x;

    do
    {
        /**Read pipe data**/
        x = 0;
        x = read(outfd, *buffer, blocksize);
        write_to_socket(buffer, x, sl);

        cerr << *buffer << " : from FHBB to socket." << endl;

        bzero(*buffer, sizeof(*buffer));
        //send_ack_pipe(infd);
    }
    while (x == blocksize);
}

/************************
 *Direct line between server socket and FHBB.
 ***********************/
void socket_to_pipe(int infd, int outfd, char ** buffer,
                    ServerLink * sl, int blocksize)
{
    bzero(*buffer, sizeof(*buffer));
    int x;
    int z;

    do
    {
        /**Get socket data into buffer**/
        x = 0;
        x = stream_from_socket(buffer, blocksize, sl);
        write(infd, *buffer, x);

        cerr << *buffer << " : to FHBB from socket." << endl;

        bzero(*buffer, sizeof(*buffer));
        *buffer[0] = '\n';
        *buffer[1] = '\0';
        write(infd, *buffer, 2);
        bzero(*buffer, sizeof(*buffer));

        recv_ack_pipe(outfd);
        cerr << "for socket." << endl;
    }
    while (x == blocksize);
}

/*****************
 *Send ACK to the pipe.
 *****************/
bool send_ack_pipe(int infd)
{
    char * ack = new char[4];
    ack[0] = 'A';
    ack[1] = 'C';
    ack[2] = 'K';
    ack[3] = '\n';
    //ack[4] = '\0';
    if (write(infd, ack, sizeof(ack)) == sizeof(ack))
    {
        cerr << ack << " sent." << endl;
        delete [] ack;
        sleep(0.1);
        return true;
    }
    else
    {
        cerr << "OH MY GOD THEY ARE IN THE PIPES!" << endl;
        return false;
    }
}

/*************
 *Get ACK from the pipe
 ************/
bool recv_ack_pipe(int outfd)
{
    char * ack = new char[4];
    char * chk = new char[4];
    ack[0] = 'A';
    ack[1] = 'C';
    ack[2] = 'K';
    ack[3] = '\0';
    bzero(chk, sizeof(chk));
    read(outfd, chk, 4);
    chk[3] = '\0';
    if (strcmp(ack, chk) == 0)
    {
        cerr << chk << " got ";
        sleep(0.1);
        delete [] ack;
        delete [] chk;
        return true;
    }
    else
    {
        cerr << chk << " OH MY GOD THEY ARE IN THE PIPES!" << endl;
        delete [] ack;
        delete [] chk;
        return false;
    }
}
