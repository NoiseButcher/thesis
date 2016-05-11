package com.example.sharky.a42588900_thesis;

import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.content.Context;
import android.location.Location;
import android.location.LocationListener;
import android.location.LocationManager;
import android.provider.Settings;
import android.content.Intent;
import android.content.res.AssetManager;

import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.File;
import java.net.Socket;

public class MainActivity extends AppCompatActivity implements LocationListener {

    private LocationManager locationManager;
    private Location location;
    private String assetpath = "FHBB_x";
    private Socket socket = null;
    private InputStreamReader fromServer;
    private InputStreamReader fromFHBB;
    private OutputStreamWriter toServer;
    private OutputStreamWriter toFHBB;
    private String host = "localhost";
    private int port = 55555;
    private int blocksize = 4096;
    private char [] buffer = new char[blocksize+1];

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        locationManager = (LocationManager) this.getSystemService(Context.LOCATION_SERVICE);
        locationManager.requestLocationUpdates(LocationManager.GPS_PROVIDER, 2000, 1, this);
        String exe = copyAssets(assetpath);
        execFHBB(exe, location);
    }

    private String copyAssets(String path) {
        AssetManager assetManager = getAssets();
        InputStream in;
        OutputStream out;
        String appFileDir = getFilesDir().getPath();
        String finalPath = appFileDir + "/FHBB_x";
        byte [] buf;
        try {
            in = assetManager.open(path);
            buf = new byte[in.available()];
            in.read(buf);
            in.close();

            File outfile = new File(appFileDir, path);
            out = new FileOutputStream(outfile);
            out.write(buf);
            out.close();

        } catch (Exception e) {
            e.printStackTrace();
        }

        File newExe = new File(finalPath);
        newExe.setExecutable(true);
        return finalPath;
    }

    private void initSocket(int portnum, String hostname) {
        try {
            socket = new Socket(hostname, portnum);
            toServer = new OutputStreamWriter(socket.getOutputStream());
            fromServer = new InputStreamReader(socket.getInputStream());
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    private void execFHBB(String command, Location location) {
        try {
            Process fhbb = Runtime.getRuntime().exec(command);
            //Diversion of FHBB cout
            fromFHBB = new InputStreamReader(fhbb.getInputStream());
            //Diversion of FHBB cin
            toFHBB =  new OutputStreamWriter(fhbb.getOutputStream());
            //Open the socket
            initSocket(port, host);

            try {
                fHBBInterface(location, blocksize);
            } catch (Exception e) {
                e.printStackTrace();
            }
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
    }

    private void fHBBInterface(Location location, int blocklen) throws Exception {

        try {
            install_upkg(buffer, blocklen);
            getCoords(location, buffer, blocklen);
            sendLoc(buffer, blocklen);
            getDist(buffer, blocklen);
            printDist(buffer, blocklen);

            while(true) {
                getCoords(location, buffer, blocklen);
                getFHBBACK();
                sendServerACK();
                sendLoc(buffer, blocklen);
                getDist(buffer, blocklen);
                printDist(buffer, blocklen);
            }

        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    private void install_upkg (char[] buf, int blocklen) {
        try {
            //Transfer Base
            socketToFHBB(buf, blocklen);
            //Transfer Context
            socketToFHBB(buf, blocklen);
            //Transfer Public Key to server
            FHBBtoSocket(buf, blocklen);
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    private void getCoords(Location location, char [] buf, int blocklen){
        String lat;
        String lng;
        StringBuffer stream = null;
        //Display "Enter Location" or something?
        try {
            fHBBtoLocal(buf, blocklen, stream);
            //Should be X:
            lng = Location.convert(location.getLatitude(), Location.FORMAT_DEGREES);
            stream.append(lng);
            localToFHBB(buf, blocklen, stream);
            stream = null;

            fHBBtoLocal(buf, blocklen, stream);
            //Should be Y:
            stream = null;
            lat = Location.convert(location.getLongitude(), Location.FORMAT_DEGREES);
            stream.append(lat);
            localToFHBB(buf, blocklen, stream);
            stream = null;

        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    private void sendLoc(char[] buffer, int blocklen) throws Exception {
        try {
            while (getServerACK())
            {
                sendFHBBACK();
                getFHBBACK();
                sendServerACK();
                socketToFHBB(buffer, blocklen);
                FHBBtoSocket(buffer, blocklen);
            }
            sendFHBBNAK();
            getServerACK();
            sendFHBBACK();

        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    private void getDist(char [] buffer, int blocklen) {
        try {
            socketToFHBB(buffer, blocklen);
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    private void printDist(char[] buffer, int blocklen) throws Exception {
        StringBuffer stream = null;
        fHBBtoLocal(buffer, blocklen, stream);
        //Print this somehow
    }

    private int readFromFHBB(char[] buffer, int blocklen) throws Exception {
        int rd = 0;
        rd = fromFHBB.read(buffer, 0, blocklen);
        return rd;
    }

    private int readFromSocket(char[] buffer, int blocklen) {
        int rd = 0;
        try {
            rd = fromServer.read(buffer, 0, blocklen);
        } catch (Exception e) {
            e.printStackTrace();
        }
        return rd;
    }

    private void pushToFHBB(char[] buffer, int blocklen) throws Exception {
        toFHBB.write(buffer, 0, blocklen);
    }

    private void pushToSocket(char[] buffer, int blocklen) throws Exception {
        toServer.write(buffer, 0, blocklen);
    }

    private void localToFHBB (char [] buffer, int blocklen, StringBuffer stream) throws Exception {
        int k, j;
        k = 0;
        j = 0;
        do {
            j = k+blocklen;
            if ((k+blocklen) > stream.length()) {
                j = stream.length();
            }
            else {

            }
            stream.getChars(k,j, buffer, 0);
            pushToFHBB(buffer, j-k);
            terminateFHBBblock();
            k = j;
            getFHBBACK();
            buffer = null;
        } while ((j-k) == blocklen);
    }

    private void fHBBtoLocal (char [] buffer, int blocklen, StringBuffer stream) throws Exception {
        int k;
        do {
            k = 0;
            k = readFromFHBB(buffer, blocklen);
            stream.append(buffer);
            buffer = null;
            sendFHBBACK();
        } while (k == blocklen);
    }

    private void localToSocket (char [] buffer, int blocklen, StringBuffer stream) throws Exception {
        int k, j;
        k = 0;
        j = 0;
        do {
            j = k+blocklen;
            if ((k+blocklen) > stream.length()) {
                j = stream.length();
            }
            else {

            }
            stream.getChars(k,j, buffer, 0);
            pushToFHBB(buffer, j-k);
            k = j;
            getFHBBACK();
            buffer = null;
        } while ((j-k) == blocklen);
    }

    private void socketToLocal (char [] buffer, int blocklen, StringBuffer stream) throws Exception {
        int k;
        do {
            k = 0;
            k = readFromSocket(buffer, blocklen);
            stream.append(buffer);
            buffer = null;
            sendFHBBACK();
        } while (k == blocklen);
    }

    private void socketToFHBB(char [] buffer, int blocklen) throws Exception {
        int k;
        do {
            k = 0;
            k = readFromSocket(buffer, blocklen);
            pushToFHBB(buffer, k);
            terminateFHBBblock();
            getFHBBACK();
            sendServerACK();
        } while (k == blocklen);
    }

    private void FHBBtoSocket(char [] buffer, int blocklen) throws Exception {
        int k;
        do {
            k = 0;
            k = readFromFHBB(buffer, blocklen);
            pushToSocket(buffer, k);
            getServerACK();
            sendFHBBACK();
        } while (k == blocklen);
    }

    private boolean getServerACK() throws Exception {
        char [] ack = new char[4];
        readFromSocket(ack, 3);
        if (ack.toString() == "ACK") {
            return true;
        }
        else {
            return false;
        }
    }

    private boolean getFHBBACK() throws Exception {
        char [] ack = new char[4];
        readFromFHBB(ack, 3);
        if (ack.toString() == "ACK") {
            return true;
        }
        else {
            return false;
        }
    }

    private void sendServerACK() throws Exception {
        char [] ack = new char[4];
        ack[0] = 'A';
        ack[1] = 'C';
        ack[2] = 'K';
        ack[3] = '\n';
        pushToSocket(ack, 4);
    }

    private void sendFHBBACK() throws Exception {
        char [] ack = new char[5];
        ack[0] = 'A';
        ack[1] = 'C';
        ack[2] = 'K';
        ack[3] = '\n';
        ack[4] = '\0';
        pushToFHBB(ack, 5);
    }

    private void sendFHBBNAK() throws Exception {
        char [] ack = new char[5];
        ack[0] = 'N';
        ack[1] = 'A';
        ack[2] = 'K';
        ack[3] = '\n';
        ack[4] = '\0';
        pushToFHBB(ack, 5);
    }

    private void terminateFHBBblock() throws Exception {
        char [] term = new char[2];
        term[0] = '\n';
        term[1] = '\0';
        pushToFHBB(term, 2);
    }

    @Override
    public void onStart() {

    }

    @Override
    public void onStop() {

    }

    @Override
    public void onProviderDisabled(String provider) {
        Intent intent = new Intent(Settings.ACTION_LOCATION_SOURCE_SETTINGS);
        startActivity(intent);
    }

    @Override
    public void onLocationChanged(Location location) {

    }

    @Override
    public void onProviderEnabled(String provider) {

    }

    @Override
    public void onStatusChanged(String provider, int status, Bundle extras) {
        // TODO Auto-generated method stub

    }
}
