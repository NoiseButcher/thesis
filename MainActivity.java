package com.example.sharky.a42588900_thesis;

import android.app.Activity;
import android.os.AsyncTask;
import android.os.Bundle;
import android.content.res.AssetManager;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.EditText;
import android.widget.Button;
import android.widget.TextView;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.File;
import java.net.Socket;

public class MainActivity extends Activity {

    private EditText editTextAddress, editTextPort;
    private EditText xParam, yParam;
    private Button buttonUpload;
    private Button buttonConnect, buttonClear;
    private TextView textResponse;

    private String assetpath = "FHBB_x";
    private int blocksize = 4096;
    String lat = "0";
    String lng = "0";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        xParam = (EditText) findViewById(R.id.lng);
        yParam = (EditText) findViewById(R.id.lat);
        buttonUpload = (Button) findViewById(R.id.upload_coords);

        editTextAddress = (EditText) findViewById(R.id.address);
        editTextPort = (EditText) findViewById(R.id.port);
        buttonConnect = (Button) findViewById(R.id.connect);
        buttonClear = (Button) findViewById(R.id.clear);
        textResponse = (TextView) findViewById(R.id.response);

        buttonConnect.setOnClickListener(buttonConnectOnClickListener);

        buttonClear.setOnClickListener(new OnClickListener() {
                                           @Override
                                           public void onClick(View v) {
                                               textResponse.setText("");
                                           }
                                       }
                                        );
        buttonUpload.setOnClickListener(buttonUploadOnClickListener);
    }

    private String copyAssets(String path) {
        AssetManager assetManager = getAssets();
        InputStream in;
        OutputStream out;
        String appFileDir = getFilesDir().getPath();
        String finalPath = appFileDir + "/FHBB_x";
        byte[] buf;
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

    OnClickListener buttonConnectOnClickListener = new OnClickListener() {

        @Override
        public void onClick(View arg0) {
            try {
                String exe = copyAssets(assetpath);
                String portstr = editTextPort.getText().toString();
                FHEengine fhe = new FHEengine(exe, editTextAddress.getText().toString(),
                                                blocksize, Integer.parseInt(portstr));
                fhe.execute();
            } catch (Exception e) {
                e.printStackTrace();
                textResponse.setText("FHBB_x execution failure");
            }
        }
    };

    OnClickListener buttonUploadOnClickListener = new OnClickListener() {

        @Override
        public void onClick(View arg0) {
            try {
                lng = xParam.getText().toString();
                lat = yParam.getText().toString();
            } catch (Exception e) {
                e.printStackTrace();
            }
        }
    };

    public class FHEengine extends AsyncTask<Void, Void, Void> {

        int blocksz;
        char[] buffer;
        int portnum;
        String hostname;
        String cmd;
        Socket socket = null;

        InputStreamReader fromServer;
        InputStreamReader fromFHBB;
        OutputStreamWriter toServer;
        OutputStreamWriter toFHBB;

        FHEengine(String path, String host, int blocklen, int port) {
            blocksz = blocklen;
            buffer = new char[blocksz + 1];
            hostname = host;
            cmd = path;
            portnum = port;
        }

        @Override
        protected Void doInBackground(Void... arg0) {
            try {
                Process fhbb = Runtime.getRuntime().exec(cmd);
                //Diversion of FHBB cout
                fromFHBB = new InputStreamReader(fhbb.getInputStream());
                //Diversion of FHBB cin
                toFHBB = new OutputStreamWriter(fhbb.getOutputStream());
                initSocket(portnum, hostname);
                install_upkg(buffer, blocksz);
                getCoords(buffer, blocksz);
                sendLoc(buffer, blocksz);
                getDist(buffer, blocksz);
                printDist(buffer, blocksz);

                while (true) {
                    getCoords(buffer, blocksz);
                    getFHBBACK();
                    sendServerACK();
                    sendLoc(buffer, blocksz);
                    getDist(buffer, blocksz);
                    printDist(buffer, blocksz);
                }

            } catch (Exception e) {
                e.printStackTrace();
            }
            return null;
        }

        @Override
        protected void onPostExecute(Void result) {
            textResponse.setText("FHBB_x terminated");
            super.onPostExecute(result);
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

        private void install_upkg(char[] buf, int blocklen) {
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

        private void getCoords(char[] buf, int blocklen) {
            StringBuffer stream = null;
            try {
                fHBBtoLocal(buf, blocklen, stream);
                //Should be X:
                stream.append(lng);
                localToFHBB(buf, blocklen, stream);
                stream = null;

                fHBBtoLocal(buf, blocklen, stream);
                //Should be Y:
                stream = null;
                stream.append(lat);
                localToFHBB(buf, blocklen, stream);
                stream = null;

            } catch (Exception e) {
                e.printStackTrace();
            }
        }

        private void sendLoc(char[] buffer, int blocklen) throws Exception {
            try {
                while (getServerACK()) {
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

        private void getDist(char[] buffer, int blocklen) {
            try {
                socketToFHBB(buffer, blocklen);
            } catch (Exception e) {
                e.printStackTrace();
            }
        }

        private void printDist(char[] buffer, int blocklen) throws Exception {
            StringBuffer stream = null;
            fHBBtoLocal(buffer, blocklen, stream);
            textResponse.setText(stream.toString());
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

        private void localToFHBB(char[] buffer, int blocklen, StringBuffer stream) throws Exception {
            int k, j;
            k = 0;
            j = 0;
            do {
                j = k + blocklen;
                if ((k + blocklen) > stream.length()) {
                    j = stream.length();
                } else {

                }
                stream.getChars(k, j, buffer, 0);
                pushToFHBB(buffer, j - k);
                terminateFHBBblock();
                k = j;
                getFHBBACK();
                buffer = null;
            } while ((j - k) == blocklen);
        }

        private void fHBBtoLocal(char[] buffer, int blocklen, StringBuffer stream) throws Exception {
            int k;
            do {
                k = 0;
                k = readFromFHBB(buffer, blocklen);
                stream.append(buffer);
                buffer = null;
                sendFHBBACK();
            } while (k == blocklen);
        }

        private void localToSocket(char[] buffer, int blocklen, StringBuffer stream) throws Exception {
            int k, j;
            k = 0;
            j = 0;
            do {
                j = k + blocklen;
                if ((k + blocklen) > stream.length()) {
                    j = stream.length();
                } else {

                }
                stream.getChars(k, j, buffer, 0);
                pushToFHBB(buffer, j - k);
                k = j;
                getFHBBACK();
                buffer = null;
            } while ((j - k) == blocklen);
        }

        private void socketToLocal(char[] buffer, int blocklen, StringBuffer stream) throws Exception {
            int k;
            do {
                k = 0;
                k = readFromSocket(buffer, blocklen);
                stream.append(buffer);
                buffer = null;
                sendFHBBACK();
            } while (k == blocklen);
        }

        private void socketToFHBB(char[] buffer, int blocklen) throws Exception {
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

        private void FHBBtoSocket(char[] buffer, int blocklen) throws Exception {
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
            char[] ack = new char[4];
            readFromSocket(ack, 3);
            if (ack.toString() == "ACK") {
                return true;
            } else {
                return false;
            }
        }

        private boolean getFHBBACK() throws Exception {
            char[] ack = new char[4];
            readFromFHBB(ack, 3);
            if (ack.toString() == "ACK") {
                return true;
            } else {
                return false;
            }
        }

        private void sendServerACK() throws Exception {
            char[] ack = new char[4];
            ack[0] = 'A';
            ack[1] = 'C';
            ack[2] = 'K';
            ack[3] = '\n';
            pushToSocket(ack, 4);
        }

        private void sendFHBBACK() throws Exception {
            char[] ack = new char[5];
            ack[0] = 'A';
            ack[1] = 'C';
            ack[2] = 'K';
            ack[3] = '\n';
            ack[4] = '\0';
            pushToFHBB(ack, 5);
        }

        private void sendFHBBNAK() throws Exception {
            char[] ack = new char[5];
            ack[0] = 'N';
            ack[1] = 'A';
            ack[2] = 'K';
            ack[3] = '\n';
            ack[4] = '\0';
            pushToFHBB(ack, 5);
        }

        private void terminateFHBBblock() throws Exception {
            char[] term = new char[2];
            term[0] = '\n';
            term[1] = '\0';
            pushToFHBB(term, 2);
        }
    }
}