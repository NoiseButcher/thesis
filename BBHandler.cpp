#include "BBHandler.h"
#include <sys/resource.h>

//#define INTEGCHK
/***********************************
Testing rig for the black box binary. Interfaces with sockets to
the server and a terminal with the user to simulate the communication
layer above all of the FHE stuff. Forks itself and executes the
FHBB binary, using the pipes to and from it.
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
    bzero(buffer, sizeof(buffer));
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

    /**Child process to execute FHBB and configure the I/O
    necessary to use it**/
    if (blackbox == 0)
    {
        /**Make cin pipe directly to the parent**/
        close(0);
        if ((dup2(bb_in[0], 0) != 0) ||
            (close(bb_in[0]) != 0) ||
            (close(bb_in[1]) != 0))
        {
            cerr << "FHBB standard input pipe failure." << endl;
            exit(0);
        }


        /**Make cout pipe directly to the parent**/
        close(1);
        if ((dup2(bb_out[1], 1) != 1) ||
            (close(bb_out[0]) != 0) ||
            (close(bb_out[1]) != 0))
        {
            cerr << "FHBB standard output pipe failure." << endl;
            exit(0);
        }

        /**Run the black box **/
        execl("/home/sharky/Thesis/Guy_Code/FHBB_x", "FHBB_x", NULL);

        cerr << "FHBB execution failure." << endl;

        return 1;
    }
    /**Parent process, this is the handler for the FHBB binary**/
    else
    {
        close(bb_out[1]);
        close(bb_in[0]);

        /**
        Connect to the server or throw an error if it can't.
        **/
        if (!(prepare_socket(&op, argv)))
        {
            cout << "Server Unavailable.";
            return 0;
        }

#ifdef INTEGCHK
        cerr << "Connection Established on fd: " << op.sockFD << endl;
        cerr << "FHBB input on fd: " << bb_in[1] << endl;
        cerr << "FHBB output on fd: " << bb_out[0] << endl;
        stream.str("Spawn");
        handler_to_pipe(stream, bb_in[1], bb_out[0], &buffer, 1024);
        stream.str("");
        stream.clear();
        pipe_to_handler(stream, bb_in[1], bb_out[0], &buffer, 1024);
        stream.clear();
        stream << " more Overlords!";
        handler_to_pipe(stream, bb_in[1], bb_out[0], &buffer, 1024);
        stream.str("");
        stream.clear();
        pipe_to_handler(stream, bb_in[1], bb_out[0], &buffer, 1024);
        stream.str("");
        stream.clear();
#else
        cout << "Handler Started." << endl;
#endif // INTEGCHK

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

            if (!recv_ack_pipe(bb_out[0]))
            {
                cerr << "Main loop ACK not received from FHBB";
                cerr << endl;
                exit(0);
            }

            send_ack_socket(&op);

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

    cout << "Enter a position:" << endl;

    /**Get longitude from CIN and pipe to FHBB**/
    pipe_to_handler(stream, infd, outfd, buffer, blocksize);
    cout << stream.str(); //Get "X:"
    stream.str("");
    stream.clear();
    cin >> input;       //Get integer longitude
    stream << input;
    handler_to_pipe(stream, infd, outfd, buffer, blocksize);
    stream.str("");
    stream.clear();

    /**Get latitude from CIN and pipe to FHBB**/
    pipe_to_handler(stream, infd, outfd, buffer, blocksize);
    cout << stream.str(); //Get "Y:"
    stream.str("");
    stream.clear();
    cin >> input;       //Get integer latitude
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
    cout << stream.str();
    send_ack_pipe(infd);

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
    cerr << "Installing FHE Scheme.";
    socket_to_pipe(infd, outfd, buffer, sl, blocksize); //Base
    cerr << ".";
    recv_ack_pipe(outfd);
    cerr << ".";
    send_ack_socket(sl);
    cerr << ".";
    socket_to_pipe(infd, outfd, buffer, sl, blocksize); //Context
    cerr << ".";
    recv_ack_pipe(outfd);
    cerr << ".";
    send_ack_socket(sl);
    cerr << ".";
    pipe_to_socket(infd, outfd, buffer, sl, blocksize); //Public Key
    cerr << "." << endl;
    recv_ack_socket(sl);
    cerr << "Installation Complete." << endl;
    send_ack_pipe(infd);
}

/*********************************
 *Middle-man location streaming function.
 *The ACK is received by the handler, whereas all of the other
 *data is piped directly from the socket to the black box.
 **********************************/
void send_location_handler(int infd, int outfd, char ** buffer,
                          ServerLink * sl, int blocksize)
{
    cerr << "Getting location data" << endl;
    while (recv_ack_socket(sl))
    {
        cerr << ".";
        send_ack_pipe(infd); //Send ACK to pipe
        send_ack_socket(sl); //Reply with ACK to server
        cerr << ".";
        socket_to_pipe(infd, outfd, buffer, sl, blocksize); //PK
        cerr << ".";
        pipe_to_socket(infd, outfd, buffer, sl, blocksize); //EncLoc
        cerr << ".";
    }
    cerr << endl;
    cerr << "Locations sent to server." << endl;
    send_nak_pipe(infd);
    if (recv_ack_socket(sl))
    {
        send_ack_pipe(infd);
    }
}

/************************************
 *Pipes the encrypted distances between the server and the
 *black box so that they can be decrypted.
 **********************************/
void get_distance_handler(int infd, int outfd, char ** buffer,
                          ServerLink * sl, int blocksize)
{
    cerr << "Downloading distances.";
    socket_to_pipe(infd, outfd, buffer, sl, blocksize);
    cerr << ".";
    recv_ack_pipe(outfd);
    cerr << "." << endl;
    send_ack_socket(sl);
    cerr << "Done." << endl;
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
        bzero(*buffer, sizeof(*buffer));
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
        x = 0;
        x = read(outfd, *buffer, blocksize);
        write_to_socket(buffer, x, sl);
    }
    while (x == blocksize);
}

/************************
 *Direct line between server socket and FHBB.
 ***********************/
void socket_to_pipe(int infd, int outfd, char ** buffer,
                    ServerLink * sl, int blocksize)
{
    int x;
    do
    {
        x = 0;
        x = stream_from_socket(buffer, blocksize, sl);
        write(infd, *buffer, x);
        terminate_pipe_msg(infd);
        if (!recv_ack_pipe(outfd))
        {
            cerr << "No ACK for socket data received." << endl;
#ifdef INTEGCHK
            cerr << "Last block: " << *buffer << endl;
#endif
            bzero(*buffer, sizeof(*buffer));
            exit(0);
        }
        bzero(*buffer, sizeof(*buffer));
    }
    while (x == blocksize);
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
        terminate_pipe_msg(infd);
        if (!recv_ack_pipe(outfd))
        {
            cerr << "No ACK for handler data received." << endl;
#ifdef INTEGCHK
            cerr << "Last block: " << *buffer << endl;
#endif
            bzero(*buffer, sizeof(*buffer));
            exit(0);
        }
        bzero(*buffer, sizeof(*buffer));
    }
    while (x == blocksize);
}

/*****************
 *Send ACK to the pipe.
 *****************/
bool send_ack_pipe(int infd)
{
    char * ack = new char[3];
    bzero(ack, sizeof(ack));
    ack[0] = 'A';
    ack[1] = 'C';
    ack[2] = 'K';
    if (write(infd, ack, sizeof(ack)) == sizeof(ack))
    {
        terminate_pipe_msg(infd);
        delete [] ack;
        return true;
    }
    else
    {
        delete [] ack;
        return false;
    }
}

/*****************
 *Send NAK to the pipe.
 *****************/
bool send_nak_pipe(int infd)
{
    char * ack = new char[3];
    bzero(ack, sizeof(ack));
    ack[0] = 'N';
    ack[1] = 'A';
    ack[2] = 'K';
    if (write(infd, ack, sizeof(ack)) == sizeof(ack))
    {
        terminate_pipe_msg(infd);
        delete [] ack;
        return true;
    }
    else
    {
        delete [] ack;
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
    bzero(ack, sizeof(ack));
    bzero(chk, sizeof(chk));
    ack[0] = 'A';
    ack[1] = 'C';
    ack[2] = 'K';
    ack[3] = '\0';

    read(outfd, chk, sizeof(chk));
    chk[3] = '\0';

    if (strcmp(ack, chk) == 0)
    {
        delete [] ack;
        delete [] chk;
        return true;
    }
    else
    {
        delete [] ack;
        delete [] chk;
        return false;
    }
}

/*******************************
 *Quick function to help with timing.
 *Sends an ACK command to the server.
 *******************************/
bool send_ack_socket(ServerLink * sl)
{
    char * buffer = new char[4];
    bzero(buffer, sizeof(buffer));
    buffer[0] = 'A';
    buffer[1] = 'C';
    buffer[2] = 'K';
    buffer[3] = '\0';
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
bool recv_ack_socket(ServerLink * sl)
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

/*************************
 *Send a newline and null terminator to
 *the black box, to terminate any messages
 *to the pipe to its stdin.
 *************************/
void terminate_pipe_msg(int infd)
{
    char * buf = new char[2];
    bzero(buf, sizeof(buf));
    buf[0] = '\n';
    buf[1] = '\0';
    write(infd, buf, sizeof(buf));
    delete [] buf;
}
