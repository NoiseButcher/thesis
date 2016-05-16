#include "FHBB.h"

#define BUFFSIZE    4096
/***********************************
 *Client side black box program designed for mobile
 *systems. Operates as an I/O function box that pipes
 *GPS data in and outputs an encrypted position. Needs
 *to be executed as a child process to something interfacing
 *with the FHE server. All I/O is done via the cin/cout streams
 ************************************/
int main(int argc, char * argv[])
{
    UserPackage me;
    vector<long> them;
    char * buffer = new char[BUFFSIZE+2];
    bzero(buffer, sizeof(buffer));

    if (argc != 1)
    {
        cerr << "./FHBB_x" << endl;
        delete [] buffer;
        return 0;
    }

    install_upkg_android(&me, &buffer);
    pair <int, int> loc = get_gps(&buffer);
    send_location_android(&me, loc.first, loc.second, &buffer);
    them = get_distances_android(&me, &buffer);
    display_positions(them);
    while (true)
    {
        loc = get_gps(&buffer);
        send_ack_android();
        send_location_android(&me, loc.first, loc.second, &buffer);
        them = get_distances_android(&me, &buffer);
        display_positions(them);
    }
    delete [] buffer;
    return 1;
}

/*********LOGISTICS AND FHE FUNCTIONS***********/
/***************************
 *This function expects the
 *latitude and longitude of
 *the client in radians multiplied
 *by 1000.
 *As it is an integer there is no decimal precision.
 **************************/
pair<int, int> get_gps(char ** buffer)
{
	int lat, lng;
    stringstream input;
	bzero(*buffer, sizeof(*buffer));
	input.str("");
	input.clear();
    pipe_in(input, buffer, BUFFSIZE);
    sleep(0.1);
	input >> lat;
    input.str("");
	input.clear();
    pipe_in(input, buffer, BUFFSIZE);
    sleep(0.1);
	input >> lng;
    input.str("");
	input.clear();
	return make_pair(lat, lng);
}

/*************
 *Encrypt the client's current GPS location
 *with the someone's public key.
***************/
Ctxt encrypt_location(int x, int y, FHEPubKey &pk)
{
	vector<long> loc;
	loc.push_back((long)x);
	loc.push_back((long)y);
	for (int i = 2; i < pk.getContext().ea->size(); i++)
    {
		loc.push_back(0);
	}
	Ctxt cloc(pk);
	pk.getContext().ea->encrypt(cloc, pk, loc);
	return cloc;
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
    cout.flush();
    recv_ack_android();
}

/*************ANDROID FUNCTIONS***************/
/*******************
 *Android mode install.
 *Reads from standard i/o.
 *******************/
void install_upkg_android(UserPackage * upk, char ** buffer)
{
    stringstream stream;
    stream.str("");
    stream.clear();
    bzero(*buffer, sizeof(*buffer));
    pipe_in(stream, buffer, BUFFSIZE);
    try
    {
        readContextBase(stream, upk->m, upk->p,
                        upk->r, upk->gens, upk->ords);
    }
    catch (...)
    {
        cerr << "HElib Exception" << endl;
        delete [] buffer;
        exit(3);
    }

    upk->context = new FHEcontext(upk->m, upk->p, upk->r,
                                  upk->gens, upk->ords);
    stream.str("");
    stream.clear();

    pipe_in(stream, buffer, BUFFSIZE);
    try
    {
        stream >> *upk->context;
    }
    catch (...)
    {
        cerr << "HElib Exception" << endl;
        delete [] buffer;
        exit(3);
    }
    stream.str("");
    stream.clear();
    upk->secretKey = new FHESecKey(*upk->context);
    upk->publicKey = upk->secretKey;
    upk->G = upk->context->alMod.getFactorsOverZZ()[0];
    upk->secretKey->GenSecKey(64);
    addSome1DMatrices(*upk->secretKey);
    upk->ea = new EncryptedArray(*upk->context, upk->G);
    upk->nslots = upk->ea->size();

    try
    {
        stream << *upk->publicKey;
    }
    catch (...)
    {
        cerr << "HElib Exception" << endl;
        delete [] buffer;
        exit(3);
    }
    pipe_out(stream, buffer, BUFFSIZE);
    stream.str("");
    stream.clear();
}

/*****************
 *Sends my location, encrypted with all of the public keys,
 *to the FHBB cout file descriptor.
 *****************/
void send_location_android(UserPackage * upk, int x, int y,
                          char ** buffer)
{
    stringstream stream;
    stream.str("");
    stream.clear();
    int i = 0;
    bzero(*buffer, sizeof(*buffer));

    while (recv_ack_android())
    {
        send_ack_android();

        FHEPubKey * pk = new FHEPubKey(*upk->context);
        pipe_in(stream, buffer, BUFFSIZE);
        /*
        stream.seekg(0, ios::end);
        cerr << "Public Key at client: " << stream.tellg() << endl;
        stream.seekg(0, ios::beg);
        */
        try
        {
            stream >> *pk;
        }
        catch (...)
        {
            cerr << "HElib Exception" << endl;
            delete pk;
            delete [] buffer;
            exit(3);
        }

        stream.str("");
        stream.clear();
        Ctxt output(*pk);
        output = encrypt_location(x, y, *pk);
        try
        {
            stream << output;
        }
        catch (...)
        {
            cerr << "HElib Exception" << endl;
            delete pk;
            delete [] buffer;
            exit(3);
        }
        pipe_out(stream, buffer, BUFFSIZE);

        stream.str("");
        stream.clear();
        delete pk;
        i++;
    }
    /*Catch if the first client has crashed, exit*/
    if (i == 0)
    {
        cerr << "Server crash, probably HElib error" << endl;
        delete [] buffer;
        exit(3);
    }
    /*Receive an ACK from the server confirming all data received*/
    if (!recv_ack_android())
    {
        cerr << "Server Abort" << endl;
        delete [] buffer;
        exit(1);
    }
}

/***********************
 *Android mode distance recovery.
 **********************/
vector<long> get_distances_android(UserPackage * upk, char ** buffer)
{
    stringstream stream;
    vector<long> d;
    stream.str("");
    stream.clear();
    bzero(*buffer, sizeof(*buffer));
    Ctxt encrypted_distances(*upk->publicKey);
    pipe_in(stream, buffer, BUFFSIZE);
    try
    {
        stream >> encrypted_distances;
    }
    catch (...)
    {
        cerr << "HElib Exception" << endl;
        delete [] buffer;
        exit(3);
    }
    stream.str("");
    stream.clear();
    upk->ea->decrypt(encrypted_distances, *upk->secretKey, d);
    return d;
}

/****************
 *Android ACK check
 *for standard in.
 ****************/
bool recv_ack_android()
{
    purge_nulls();
    char * ack = new char [4];
    char * chk = new char[4];
    bzero(ack, sizeof(ack));
    bzero(chk, sizeof(chk));
    chk[0]='A';
    chk[1]='C';
    chk[2]='K';
    chk[3]='\0';
    cin.get(ack, 4);
    if (cin.fail()) cin.clear();
    cin.sync();
    if (strcmp(ack, chk) == 0) {
        delete [] chk;
        delete [] ack;
        return true;
    }
    delete [] chk;
    delete [] ack;
    return false;
}

/****************
 *This is a seperate function for continuity
 *purposes only.
 ****************/
void send_ack_android()
{
    cin.sync();
    cout << "ACK";
    cout.flush();
}

/*****************
 *Function to check the
 *stdin buffer for null characters
 *and remove them.
 *****************/
void purge_nulls()
{
    int pk;
    pk = cin.peek();
    if (cin.fail()) cin.clear();
    while (pk < 32)
    {
        cin.ignore(1);
        pk = cin.peek();
        if (cin.fail()) cin.clear();
    }
}

/**************************
 *Write data to standard out in blocks of
 *[blocksize]. flush stdout once the operations
 *are complete.
 ***************************/
void pipe_out(istream &stream, char** buffer, int blocksize)
{
    bzero(*buffer, sizeof(*buffer));
    int x;
    do
    {
        x = 0;
        stream.read(*buffer, blocksize);
        x = stream.gcount();
        cout.write(*buffer, x);
        cout.flush();
        if (!recv_ack_android())
        {
            cerr << "ACK error: pipe out()" << endl;
            exit(4);
        }
        bzero(*buffer, sizeof(*buffer));
    }
    while (x == blocksize);
}

/**************************
 *Read blocks of data from stdin, ideally in [blocksize]
 *chunks. Because read() operations on cin do not register
 *the same termination flags as sockets and standard streams,
 *this function uses a nested loop with getline to extract
 *the correct amount of data.
 **************************/
void pipe_in(ostream &stream, char ** buffer, int blocksize)
{
    bzero(*buffer, sizeof(*buffer));
    int x, y, pk, blocklim;
    blocklim = blocksize + 1;
    do
    {
        x = 0;
        do
        {
            y = 0;
            pk = 0;
            purge_nulls();
            /*Get [BUFFSIZE - streamsize(x) + 1] bytes
              from cin*/
            cin.getline(*buffer, blocklim - x);
            if (cin.fail()) cin.clear();
            /*Transfer all extracted bytes excluding the
              newline into stream*/
            y = cin.gcount() - 1;
            stream.write(*buffer, y);
            x += y;
            pk = cin.peek();
            if (cin.fail()) cin.clear();
            /*Conditional action to handle the final
              character of the read being a newline
              (which would be stripped) which would mean
              the next two characters would be '\n'
              and '\0'.*/
            if ((pk == 10) && (x == (blocksize-1)))
            {
                cin.ignore(2);
                stream << endl;
                x++;
                pk = cin.peek();
                if (cin.fail()) cin.clear();
            }
            /*Conditional Loop to compensate for getline()
              stripping the newline from a non-maximum
              buffer*/
            if ((x < blocksize) && (pk > 31))
            {
                stream << endl;
                x++;
            }
            bzero(*buffer, sizeof(*buffer));
        }
        while ((x < blocksize) && (pk > 31) && (pk < 127));
        send_ack_android();
    }
    while (x == blocksize);
    stream.clear();
}
