#include "BBHandler.h"
#include <sys/resource.h>

//#define DEBUG
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
    pair <int, int> me;
    stringstream cmd;
    char * buffer = new char[1025];

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
        //close(0);
        dup2(bb_in[0], 0);
        close(bb_in[0]);
        close(bb_in[1]);

        /**Make cout pipe directly to the parent**/
        //close(1);
        dup2(bb_out[1], 1);
        close(bb_out[0]);
        close(bb_out[1]);

        /**Run the black box **/
        execl("~/Thesis/Guy_Code/FHBB_x", "FHBB_x", (char*) 0);

        return 1;
    }
    /**I AM YOUR FUCKING DADDY**/
    else
    {
        /**
        Connect to the server or throw an error if it can't.
        **/
        if (!(prepare_socket(&op, argv)))
        {
            cout << "Server Unavailable.";
            return 0;
        }

        cout << "Connection Established." << endl;

        close(bb_out[1]);
        close(bb_in[0]);
        bzero(buffer, sizeof(buffer));
        cmd.str("");
        cmd.clear();

        cout << "Ready to test FHBB." << endl;

        pipe_to_handler(cmd, bb_out[0], &buffer, 1024);

        //Get Base
        socket_to_pipe(bb_in[1], &buffer, &op, 1024);
        //Send ack
        pipe_to_socket(bb_out[0], &buffer, &op, 1024);
        //Get context
        socket_to_pipe(bb_in[1], &buffer, &op, 1024);
        //Send ack
        pipe_to_socket(bb_out[0], &buffer, &op, 1024);
        //Send my public key
        pipe_to_socket(bb_out[0], &buffer, &op, 1024);
        //Send ack


        me = get_gps();
        /*SEND LOCATION*/
        /*RECEIVE DISTANCES*/
        /*PRINT THE FUCKERS*/

        while(true)
        {
            me = get_gps();
            /*WRITE ACK*/
            /*SEND LOCATION*/
            /*RECEIVE DISTANCES*/
            /*PRINT THE FUCKERS*/
        }

    }

    return 0;
}

/*********LOGISTICS AND FHE FUNCTIONS***********/
/***************************
 *This function expects the
 *latitude and longitude of
 *the client in radians multiplied
 *by 1000.
 *As it is an integer there is no decimal precision.
 **************************/
pair<int, int> get_gps()
{
	int lat, lng;
	string input;

	cout << "X";
	cin >> lat;
	cout << "Y";
	cin >> lng;

	return make_pair(lat, lng);
}

/*********************
 *Display the proximity
 *of other users. Prints
 *to primary I/O, so only
 *one mode.
 **********************/
void display_positions(vector<long> d)
{
    int i;

    do
    {
        cout << "User " << i << " is ";
        cout << sqrt(d[i]) << "m ";
        cout << "from your position." << endl;
        i++;
    }
    while (d[i] > 0);
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
void socket_to_handler(ostream &stream, char ** buffer,
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
        cout << totalloops << " : " << rx;
        cout << " : " << stream.tellp() << endl;
#endif // DEBUG
}

/*****************************
 *Read data from stream buffer and send it down the pipe
 *one block at a time.
 *Use this to communicate between handler and FHBB.
 ****************************/
void handler_to_pipe(istream &stream, int fd, char** buffer,
                     int blocksize)
{
    bzero(*buffer, sizeof(*buffer));
    int x;

    do
    {
        x = 0;
        stream.read(*buffer, blocksize);
        x = stream.gcount();
        write(fd, *buffer, blocksize);
        bzero(*buffer, sizeof(*buffer));
    }
    while (x == blocksize);
}

/*******************
 *Read from a pipe and place data into a stream buffer
 *Use this to communicate between handler and FHBB.
 ******************/
void pipe_to_handler(ostream &stream, int fd, char ** buffer,
                     int blocksize)
{
    bzero(*buffer, sizeof(*buffer));
    int x;

    do
    {
        x = 0;
        x = read(fd, *buffer, blocksize);
        stream.write(*buffer, blocksize);
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
void pipe_to_socket(int fd, char ** buffer, ServerLink * sl,
                    int blocksize)
{
    bzero(*buffer, sizeof(*buffer));
    int x;

    do
    {
        /**Read pipe data**/
        x = 0;
        x = read(fd, *buffer, blocksize);
        /**Send it to socket**/
        write_to_socket(buffer, x, sl);
        bzero(*buffer, sizeof(*buffer));
    }
    while (x == blocksize);
}

/************************
 *Direct line between server socket and FHBB.
 ***********************/
void socket_to_pipe((int fd, char ** buffer, ServerLink * sl,
                    int blocksize)
{
    bzero(*buffer, sizeof(*buffer));
    int x;

    do
    {
        /**Get socket data into buffer**/
        x = 0;
        x = stream_from_socket(buffer, blocksize, sl);
        /**Write buffer to pipe**/
        write(fd, *buffer, x);
        bzero(*buffer, sizeof(*buffer));
    }
    while (x == blocksize);
}
