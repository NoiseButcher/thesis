#include "BBHandler.h"

/***********************************
Testing rig for the black box binary. Interfaces with sockets to
the server and a terminal with the user to simulate the communication
layer above all of the FHE stuff. Forks itself and executes the
FHBB binary, using the pipes to and from it
 ************************************/
int main(int argc, char * argv[])
{
    ServerLink op;
    pid_t blackbox;
    int bb_in[2];
    int bb_out[2];
    char * buffer = new char[BUFFSIZE+1];
    stringstream stream;
    bzero(buffer, sizeof(buffer));
    stream.str("");
    stream.clear();
    if (argc != 4)
    {
        cerr << "./BBHandler_x portnum localhost(hostname) FHBB_path" << endl;
        delete [] buffer;
        return 0;
    }
    pipe(bb_in);
    pipe(bb_out);
    blackbox = fork();
    if (blackbox == 0)
    {
        close(0);
        if ((dup2(bb_in[0], 0) != 0) ||
            (close(bb_in[0]) != 0) ||
            (close(bb_in[1]) != 0))
        {
            cerr << "Handler: FHBB standard input pipe failure." << endl;
            exit(0);
        }
        close(1);
        if ((dup2(bb_out[1], 1) != 1) ||
            (close(bb_out[0]) != 0) ||
            (close(bb_out[1]) != 0))
        {
            cerr << "Handler: FHBB standard output pipe failure." << endl;
            exit(0);
        }
        // --->/home/sharky/Thesis/Guy_Code/FHBB_x <--- e.g my path
        execl(argv[3], "FHBB_x", NULL);
        cerr << "Handler: FHBB execution failure." << endl;
        delete [] buffer;
        return 1;
    }
    else
    {
        close(bb_out[1]);
        close(bb_in[0]);
        if (prepare_socket(&op, argv) == 2)
        {
            delete [] buffer;
            kill(blackbox, SIGKILL);
            close(bb_out[0]);
            close(bb_in[1]);
            exit(2);
        }
        cout << "Handler Started." << endl;
        install_upkg_handler(bb_in[1], bb_out[0], &buffer,
                             &op, BUFFSIZE);
        get_gps_handler(bb_in[1], bb_out[0], &buffer, BUFFSIZE,
                        stream);
        send_location_handler(bb_in[1], bb_out[0], &buffer, &op,
                              BUFFSIZE);
        get_distance_handler(bb_in[1], bb_out[0], &buffer, &op,
                             BUFFSIZE);
        display_positions_handler(bb_in[1], bb_out[0], &buffer,
                                  BUFFSIZE, stream);
        while(true)
        {
            get_gps_handler(bb_in[1], bb_out[0], &buffer, BUFFSIZE,
                            stream);
            if (!recv_ack_pipe(bb_out[0]))
            {
                cerr << "Handler: Main loop, ACK not received from FHBB";
                cerr << endl;
                delete [] buffer;
                kill(blackbox, SIGKILL);
                close(bb_out[0]);
                close(bb_in[1]);
                exit(0);
            }
            send_ack_socket(&op);
            send_location_handler(bb_in[1], bb_out[0], &buffer, &op,
                              BUFFSIZE);
            get_distance_handler(bb_in[1], bb_out[0], &buffer,
                                 &op, BUFFSIZE);
            display_positions_handler(bb_in[1], bb_out[0], &buffer,
                                      BUFFSIZE, stream);
        }
        delete [] buffer;
        kill(blackbox, SIGKILL);
        close(bb_out[0]);
        close(bb_in[1]);
        return 0;
    }
    kill(blackbox, SIGKILL);
    close(bb_out[0]);
    close(bb_in[1]);
    close(bb_out[1]);
    close(bb_in[0]);
    delete [] buffer;
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
                       int blocksize, stringstream &stream)
{
    size_t quit;
    string input;
    cout << "Enter a position:" << endl;
    cout << "X: ";
    cin >> input;       //Get integer longitude
    stream << input;
    handler_to_pipe(stream, infd, outfd, buffer, blocksize);
    stream.str("");
    stream.clear();
    quit = input.find_first_of("Qq");
	if (quit != input.npos)
    {
        delete [] buffer;
        close(infd);
        close(outfd);
        exit(5);
	}
    cout << "Y: ";
    cin >> input;       //Get integer latitude
    stream << input;
    handler_to_pipe(stream, infd, outfd, buffer, blocksize);
    stream.str("");
    stream.clear();
    quit = input.find_first_of("Qq");
	if (quit != input.npos)
    {
        delete [] buffer;
        close(infd);
        close(outfd);
        exit(5);
	}
}

/*********************
 *Display the proximity
 *of other users. Prints
 *to primary I/O, so only
 *one mode.
 **********************/
void display_positions_handler(int infd, int outfd, char ** buffer,
                               int blocksize, stringstream &stream)
{
    stream.str("");
    stream.clear();
    pipe_to_handler(stream, infd, outfd, buffer, blocksize);
    cout << stream.str();
    stream.str("");
    stream.clear();
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
    cerr << "Installing FHE Scheme." << endl;
    socket_to_pipe(infd, outfd, buffer, sl, blocksize); //Base
    socket_to_pipe(infd, outfd, buffer, sl, blocksize); //Context
    pipe_to_socket(infd, outfd, buffer, sl, blocksize); //Public Key
    cerr << "Installation Complete." << endl;
}

/*********************************
 *Middle-man location streaming function.
 *The ACK is received by the handler, whereas all of the other
 *data is piped directly from the socket to the black box.
 **********************************/
void send_location_handler(int infd, int outfd, char ** buffer,
                          ServerLink * sl, int blocksize)
{
    int i = 0;
    cerr << "Uploading location data from FHBB..." << endl;
    while (recv_ack_socket(sl))
    {
        send_ack_pipe(infd);
        if (!recv_ack_pipe(outfd))
        {
            cerr << "Handler: ACK error: handshake" << endl;
            exit(4);
        }
        send_ack_socket(sl);

        socket_to_pipe(infd, outfd, buffer, sl, blocksize); //PK
        pipe_to_socket(infd, outfd, buffer, sl, blocksize); //EncLoc

        i++;
    }

    send_nak_pipe(infd);

    if (i == 0)
    {
        cerr << "Handler: Server crash; HElib error probably" << endl;
        delete [] buffer;
        exit(3);
    }

    if (!recv_ack_socket(sl))
    {
        cerr << "Handler: ACK error: get_client_position()" << endl;
        exit(1);
    }
    send_ack_pipe(infd);
}

/************************************
 *Pipes the encrypted distances between the server and the
 *black box so that they can be decrypted.
 **********************************/
void get_distance_handler(int infd, int outfd, char ** buffer,
                          ServerLink * sl, int blocksize)
{
    cerr << "Downloading distances..." << endl;
    socket_to_pipe(infd, outfd, buffer, sl, blocksize);
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
        cerr << "Handler: Socket Error: socket()" << endl;
		return 2;
    }
    if((sl->serv=gethostbyname(argv[2]))==NULL)
    {
        cerr << "Handler: Socket error: gethostbyname()" << endl;
		return 2;
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
        cerr << "Handler: Socket Error: connect()" << endl;
		return 2;
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
    bzero(*buffer, sizeof(*buffer));
    return sl->xfer = read(sl->sockFD, *buffer, blocksize);
}

/*****
Complementary writing function to stream_from_socket().
Pushes the contents of a buffer to the port.
This probably requires some sort of guarantee that data has not
been lost.
*****/
int write_to_socket(char ** buffer, int blocksize, ServerLink * sl)
{
    sl->xfer = write(sl->sockFD, *buffer, blocksize);
    bzero(*buffer, sizeof(*buffer));
    return sl->xfer;
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
        if (!recv_ack_socket(sl))
        {
            cerr << "Handler: ACK error, handler to socket()" << endl;
            delete [] buffer;
            exit(4);
        }
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
        send_ack_socket(sl);
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
        send_ack_pipe(infd);
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
        if (!recv_ack_socket(sl))
        {
            cerr << "Handler: ACK error, pipe_to_socket()" << endl;
            close(infd);
            close(outfd);
            delete [] buffer;
            exit(4);
        }
        send_ack_pipe(infd);
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
            cerr << "Handler: ACK error, socket_to_pipe()" << endl;
            close(infd);
            close(outfd);
            delete [] buffer;
            exit(0);
        }
        send_ack_socket(sl);
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
            cerr << "Handler: ACK error, handler_to_pipe()" << endl;
            close(infd);
            close(outfd);
            delete [] buffer;
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
