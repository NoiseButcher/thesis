package sharktank.pinger;

import android.support.v7.app.AppCompatActivity;
import android.os.AsyncTask;
import android.os.Bundle;
import android.content.res.AssetManager;
import android.util.Log;

import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.TextView;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;

import java.net.Socket;
import java.lang.Process;

public class MainActivity extends AppCompatActivity {

    private static final String TAG = MainActivity.class.getSimpleName();

    private EditText editTextAddress, editTextPort;
    private EditText xParam, yParam;
    private Button buttonUpload;
    private Button buttonConnect, buttonDC;
    private TextView textResponse;

    private String assetpath = "FHBB_x";
    private String exepath = "";
    private int blocksize = 4096;
    String lat = "0";
    String lng = "0";
    Socket socket = null;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        exepath = copyAssets(assetpath);

        xParam = (EditText) findViewById(R.id.lng);
        yParam = (EditText) findViewById(R.id.lat);
        buttonUpload = (Button) findViewById(R.id.upload_coords);

        editTextAddress = (EditText) findViewById(R.id.address);
        editTextPort = (EditText) findViewById(R.id.port);
        buttonConnect = (Button) findViewById(R.id.connect);
        buttonDC = (Button) findViewById(R.id.discon);
        textResponse = (TextView) findViewById(R.id.response);

        buttonConnect.setOnClickListener(buttonConnectOnClickListener);
        buttonUpload.setOnClickListener(buttonUploadOnClickListener);
        buttonDC.setOnClickListener(new View.OnClickListener() {
                                           @Override
                                           public void onClick(View v) {
                                               try {
                                                   socket.close();
                                               } catch (Exception e) {
                                                   Log.d(TAG, "Cannot Close Socket, you are part of Legion now.", e);
                                                   textResponse.setText("Cannot Close Socket, you are part of Legion now.");
                                               }}});
    }

    private View.OnClickListener buttonConnectOnClickListener = new View.OnClickListener() {

        @Override
        public void onClick(View arg0) {
            try {
                String portstr = editTextPort.getText().toString();
                FHEengine fhe = new FHEengine(exepath, editTextAddress.getText().toString(),
                        blocksize, Integer.parseInt(portstr));
                fhe.execute();
            } catch (Exception e) {
                Log.d(TAG, "Unable to start FHE Engine", e);
                textResponse.setText("Unable to start FHE Engine");
            }
        }
    };

    View.OnClickListener buttonUploadOnClickListener = new View.OnClickListener() {

        @Override
        public void onClick(View arg0) {
            try {
                lng = xParam.getText().toString();
                lat = yParam.getText().toString();
            } catch (Exception e) {
                Log.d(TAG, "Unable to Upload GPS", e);
                textResponse.setText("Unable to Upload GPS");
            }
        }
    };

    private String copyAssets(String path) {
        AssetManager assetManager = getAssets();
        InputStream in;
        FileOutputStream out;
        String appFileDir = getFilesDir().getPath();
        /*** data/user/0/sharktank.pinger/files/ ***/
        String finalPath = appFileDir + "/FHBB_x";
        Log.d(TAG, finalPath);
        byte[] buf;
        try {
            in = assetManager.open(path);
            int bytesread;
            buf = new byte[4096];
            File outfile = new File(appFileDir, path);
            out = new FileOutputStream(outfile);
            while ((bytesread = in.read(buf)) >= 0) {
                out.write(buf, 0, bytesread);
            }

            in.close();
            out.close();

        } catch (Exception e) {
            Log.d(TAG, "Unable to make FHBB_x executable", e);
            textResponse.setText("Unable to make FHBB_x executable");
        }

        File newExe = new File(finalPath);
        try {
            Process process = Runtime.getRuntime().exec("/system/bin/chmod 744 /data/user/0/sharktank.pinger/files/FHBB_x");
        } catch (Exception e) {
            Log.d(TAG, "Unable to change FHBB_x permissions", e);
        }
        newExe.setExecutable(true);
        return finalPath;
    }

    public class FHEengine extends AsyncTask<Void, Void, Void> {

        int blocksz;
        char[] buffer;
        int portnum;
        String hostname;
        String cmd;

        InputStreamReader fromServer;
        OutputStreamWriter toServer;
        /*
        InputStreamReader fromFHBB;
        OutputStreamWriter toFHBB;
        */
        DataInputStream fromFHBB;
        DataOutputStream toFHBB;

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
                /*
                fromFHBB = new InputStreamReader(fhbb.getInputStream()); //this is cout for FHBB_x
                toFHBB = new OutputStreamWriter(fhbb.getOutputStream()); //this is cin for FHBB_x
                */
                fromFHBB = new DataInputStream(fhbb.getInputStream()); //this is cout for FHBB_x
                toFHBB = new DataOutputStream(fhbb.getOutputStream()); //this is cin for FHBB_x

            } catch (Exception e) {
                Log.d(TAG, "Crash During Execution/Piping to FHBB", e);
                e.printStackTrace();
            }
            try {
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
                Log.d(TAG, "Crash During Normal Operation", e);
                e.printStackTrace();
            }
            return null;
        }

        @Override
        protected void onPostExecute(Void result) {
            super.onPostExecute(result);
        }

        private void initSocket(int portnum, String hostname) {
            try {
                socket = new Socket(hostname, portnum);
                toServer = new OutputStreamWriter(socket.getOutputStream());
                fromServer = new InputStreamReader(socket.getInputStream());
            } catch (Exception e) {
                Log.d(TAG, "Crash During Open Socket", e);
                //textResponse.setText("Crash During Open Socket");
            }
        }

        private void install_upkg(char[] buf, int blocklen) {
            try {
                //Transfer Base
                socketToFHBB(buf, blocklen);
                Log.d(TAG, "Base got");
                //Transfer Context
                socketToFHBB(buf, blocklen);
                Log.d(TAG, "Context got");
                //Transfer Public Key to server
                FHBBtoSocket(buf, blocklen);
                Log.d(TAG, "PK sent");
            } catch (Exception e) {
                Log.d(TAG, "Crash During Install FHE", e);
                //textResponse.setText("Crash During Install FHE");
            }
        }

        private void getCoords(char[] buf, int blocklen) {
            StringBuffer stream = new StringBuffer("");
            try {
                fHBBtoLocal(buf, blocklen, stream);
                //Should be X:
                stream.append(lng);
                localToFHBB(buf, blocklen, stream);
                stream.delete(0, stream.length());

                fHBBtoLocal(buf, blocklen, stream);
                //Should be Y:
                stream.delete(0, stream.length());
                stream.append(lat);
                localToFHBB(buf, blocklen, stream);
                stream.delete(0, stream.length());

            } catch (Exception e) {
                Log.d(TAG, "Crash During GPS Extraction", e);
                //textResponse.setText("Crash During GPS Extraction");
            }
        }

        private void sendLoc(char[] buffer, int blocklen) {
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

            } catch (Exception e) {
                Log.d(TAG, "Crash During Location Upload", e);
                //textResponse.setText("Crash During Location Upload");
            }
        }

        private void getDist(char[] buffer, int blocklen) {
            try {
                socketToFHBB(buffer, blocklen);
            } catch (Exception e) {
                Log.d(TAG, "Crash During Distance Download", e);
            }
        }

        private void printDist(char[] buffer, int blocklen) throws Exception {
            StringBuffer stream = new StringBuffer("");
            fHBBtoLocal(buffer, blocklen, stream);
            textResponse.setText(stream.toString());
        }

        private int readFromFHBB(char[] buffer, int blocklen) throws Exception {
            int rd = 0;
            byte [] tmp = new byte[blocklen+1];
            //rd = fromFHBB.read(buffer, 0, blocklen);
            rd = fromFHBB.read(tmp, 0,  blocklen);
            buffer = tmp.toString().toCharArray();
            return rd;
        }

        private int readFromSocket(char[] buffer, int blocklen) throws Exception {
            int rd = 0;
            rd = fromServer.read(buffer, 0, blocklen);
            return rd;
        }

        private void pushToFHBB(char[] buffer, int blocklen) throws Exception {
            toFHBB.writeBytes(buffer.toString());
            //toFHBB.write(buffer, 0, blocklen);
            toFHBB.flush();
        }

        private void pushToSocket(char[] buffer, int blocklen) throws Exception {
            toServer.write(buffer, 0, blocklen);
            toServer.flush();
        }

        private void localToFHBB(char[] buffer, int blocklen, StringBuffer stream) throws Exception {
            int k, j;
            k = 0;
            j = 0;
            do {
                j = k + blocklen;
                if ((k + blocklen) > stream.length()) {
                    j = stream.length();
                }
                stream.getChars(k, j, buffer, 0);
                pushToFHBB(buffer, j - k);
                terminateFHBBblock();
                k = j;
                getFHBBACK();
            } while ((j - k) == blocklen);
        }

        private void fHBBtoLocal(char[] buffer, int blocklen, StringBuffer stream) throws Exception {
            int k;
            do {
                k = 0;
                k = readFromFHBB(buffer, blocklen);
                stream.append(buffer);
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