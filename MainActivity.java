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

import java.io.ByteArrayInputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.io.CharArrayReader;

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
    boolean haveCoOrds = false;
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
                fhe.start();
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
                haveCoOrds = true;
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
            buf = new byte[in.available()];
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

    public class FHEengine extends Thread {

        int blocksz;
//        char[] buffer;
        byte[] buffer;
        int portnum;
        String hostname;
        String cmd;

//        InputStreamReader fromServer;
//        InputStreamReader fromFHBB;

        OutputStreamWriter toFHBB;
        OutputStreamWriter toServer;

//        DataInputStream fromServer
//        DataInputStream fromFHBB;

//        DataOutputStream toFHBB;
//        DataOutputStream toServer;

        InputStream fromServer;
        InputStream fromFHBB;

        FHEengine(String path, String host, int blocklen, int port) {
            blocksz = blocklen;
//            buffer = new char[blocksz+1];
            buffer = new byte[blocksz+1];
            hostname = host;
            cmd = path;
            portnum = port;
        }

        @Override
        public void run() {
            try {
                Process fhbb = Runtime.getRuntime().exec(cmd);
                //fromFHBB = new InputStreamReader(fhbb.getInputStream()); //this is cout for FHBB_x
                toFHBB = new OutputStreamWriter(fhbb.getOutputStream(), "utf-8"); //this is cin for FHBB_x
                fromFHBB = fhbb.getInputStream(); //this is cout for FHBB_x
                //fromFHBB = new DataInputStream(fhbb.getInputStream()); //this is cout for FHBB_x
//                toFHBB = new DataOutputStream(fhbb.getOutputStream()); //this is cin for FHBB_x

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
        }

        private void initSocket(int portnum, String hostname) {
            try {
                socket = new Socket(hostname, portnum);
                toServer = new OutputStreamWriter(socket.getOutputStream(), "utf-8");
//                toServer = new DataOutputStream(socket.getOutputStream());
//                fromServer = new InputStreamReader(socket.getInputStream());
                fromServer = socket.getInputStream();
            } catch (Exception e) {
                Log.d(TAG, "Crash During Open Socket", e);
            }
        }

//        private void install_upkg(char[] buf, int blocklen) {
        private void install_upkg(byte[] buf, int blocklen) {
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

//        private void getCoords(char[] buf, int blocklen) {
        private void getCoords(byte[] buf, int blocklen) {
            StringBuffer stream = new StringBuffer("");
            try {
                while (haveCoOrds == false) {}
                stream.append(lng);
                localToFHBB(buf, blocklen, stream);
                stream.delete(0, stream.length());

                stream.append(lat);
                localToFHBB(buf, blocklen, stream);
                stream.delete(0, stream.length());
                haveCoOrds = false;

            } catch (Exception e) {
                Log.d(TAG, "Crash During GPS Extraction", e);
            }
        }

//        private void sendLoc(char[] buffer, int blocklen) {
        private void sendLoc(byte[] buffer, int blocklen) {
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
            }
        }

//        private void getDist(char[] buffer, int blocklen) {
        private void getDist(byte[] buffer, int blocklen) {
            try {
                socketToFHBB(buffer, blocklen);
            } catch (Exception e) {
                Log.d(TAG, "Crash During Distance Download", e);
            }
        }

//        private void printDist(char[] buffer, int blocklen) throws Exception {
        private void printDist(byte[] buffer, int blocklen) throws Exception {
            StringBuffer stream = new StringBuffer("");
            fHBBtoLocal(buffer, blocklen, stream);
            textResponse.setText(stream.toString());
        }

//        private int readFromFHBB(char[] buffer, int blocklen) throws Exception {
        private int readFromFHBB(byte[] buffer, int blocklen) throws Exception {
            DataInputStream reader = new DataInputStream(fromFHBB);
//            InputStreamReader reader = new InputStreamReader(fromFHBB, "utf-8");
            int i = fromFHBB.available();
            int rd;
            if (i < blocklen) {
                reader.read(buffer, 0, blocklen);
                rd = i;
            } else {
                rd = reader.read(buffer, 0, blocklen);
            }
            byte[] buf2 = purgeFilth(buffer);
            String str = new String(buf2, "utf-8");
            Log.d(TAG, "From FHBB: " + str);
            return rd;
        }

//        private int readFromSocket(char[] buffer, int blocklen) throws Exception {
        private int readFromSocket(byte[] buffer, int blocklen) throws Exception {
            DataInputStream reader = new DataInputStream(fromServer);
//            InputStreamReader reader = new InputStreamReader(fromServer, "utf-8");
            int rd;
            int i = fromServer.available();
            if (i < blocklen) {
                reader.read(buffer, 0, blocklen);
                rd = i;
            } else {
                rd = reader.read(buffer, 0, blocklen);
            }
            byte[] buf2 = purgeFilth(buffer);
            String str = new String(buf2, "utf-8");
            Log.d(TAG, "From Server: " + str);
            return rd;
        }

//        private void pushToFHBB(char[] buffer, int blocklen) throws Exception {
        private void pushToFHBB(byte[] buffer, int blocklen) throws Exception {
            String output = new String(buffer, "utf-8");
            Log.d(TAG, "To FHBB: " + output);
            toFHBB.write(output);
            toFHBB.flush();
        }

//        private void pushToSocket(char[] buffer, int blocklen) throws Exception {
        private void pushToSocket(byte[] buffer, int blocklen) throws Exception {
            String output = new String(buffer, "utf-8");
            Log.d(TAG, "To Server: " + output);
            toServer.write(output);
            toServer.flush();
        }

//        private void localToFHBB(char[] buffer, int blocklen, StringBuffer stream) throws Exception {
        private void localToFHBB(byte[] buffer, int blocklen, StringBuffer stream) throws Exception {
            int k, j;
            k = 0;
            j = 0;
            do {
                j = k + blocklen;
                if ((k + blocklen) > stream.length()) {
                    j = stream.length();
                }
//                TODO FIX ME IM FUCKED
//                stream.getChars(k, j, buffer, 0);
                pushToFHBB(buffer, j - k);
                //terminateFHBBblock();
                k = j;
                getFHBBACK();
            } while ((j - k) == blocklen);
        }

//        private void fHBBtoLocal(char[] buffer, int blocklen, StringBuffer stream) throws Exception {
        private void fHBBtoLocal(byte[] buffer, int blocklen, StringBuffer stream) throws Exception {
            int k;

            do {
                k = 0;
                k = readFromFHBB(buffer, blocklen);
                String str = new String(purgeFilth(buffer), "utf-8");
                stream.append(str);
                sendFHBBACK();
            } while (k == blocklen);
        }

//        private void socketToFHBB(char[] buffer, int blocklen) throws Exception {
        private void socketToFHBB(byte[] buffer, int blocklen) throws Exception {
            int k;
            do {
                k = 0;
                k = readFromSocket(buffer, blocklen);
                pushToFHBB(purgeFilth(buffer), k);
                //terminateFHBBblock();
                if (!getFHBBACK()) {
                    Log.d(TAG, "NO ACK");
            }
                sendServerACK();
            } while (k == blocklen);
        }

//        private void FHBBtoSocket(char[] buffer, int blocklen) throws Exception {
        private void FHBBtoSocket(byte[] buffer, int blocklen) throws Exception {
            int k;
            do {
                k = 0;
                k = readFromFHBB(buffer, blocklen);
                pushToSocket(purgeFilth(buffer), k);
                getServerACK();
                sendFHBBACK();
            } while (k == blocklen);
        }

        private boolean getServerACK() throws Exception {
            byte[] ack = new byte[4];
//            char[] ack = new char[4];
            readFromSocket(ack, 4);
            String ref = new String(purgeFilth(ack), "utf-8");
            if (ref == "ACK") {
                return true;
            } else {
                return false;
            }
        }

        private boolean getFHBBACK() throws Exception {
            byte[] ack = new byte[4];
//            char[] ack = new char[4];
            readFromFHBB(ack, 4);
            String ref = new String(purgeFilth(ack), "utf-8");
            if (ref == "ACK") {
                return true;
            } else {
                return false;
            }
        }

        private void sendServerACK() throws Exception {
            byte[] ack = new byte[4];
//            char[] ack = new char[4];
            ack[0] = 'A';
            ack[1] = 'C';
            ack[2] = 'K';
            ack[3] = '\0';
            pushToSocket(ack, 4);
        }

        private void sendFHBBACK() throws Exception {
            byte[] ack = new byte[5];
//            char[] ack = new char[5];
            ack[0] = 'A';
            ack[1] = 'C';
            ack[2] = 'K';
            ack[3] = '\n';
            ack[4] = '\0';
            pushToFHBB(ack, 5);
        }

        private void sendFHBBNAK() throws Exception {
            byte[] ack = new byte[5];
//            char[] ack = new char[5];
            ack[0] = 'N';
            ack[1] = 'A';
            ack[2] = 'K';
            ack[3] = '\n';
            ack[4] = '\0';
            pushToFHBB(ack, 5);
        }

        private void terminateFHBBblock() throws Exception {
//            char[] term = new char[2];
            byte[] term = new byte[2];
            term[0] = '\n';
            term[1] = '\0';
            pushToFHBB(term, 2);
        }

        private byte [] purgeFilth(byte[] victim) throws Exception {
            int j = 0;
            while (((victim[j] < 32) || (victim[j] > 126)) && (j < (victim.length-1))) j++;
            if (victim.length == j) {
                byte [] out = new byte[1];
                out[0] = '\0';
                return out;
            }

            int i = j;
            while ((victim[i] > 31) && (victim[i] < 127) && (i < (victim.length-1))) i++;
            if ((j==0) && (i==victim.length)) return victim;

            ByteArrayInputStream good = new ByteArrayInputStream(victim, j, i-1);
            byte[] out = new byte[i-1];
            good.read(out, 0, good.available());
            return out;
        }
    }
}