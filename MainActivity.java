package sharktank.pinger;

import android.support.v7.app.AppCompatActivity;
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
import java.io.IOException;
import java.io.InputStream;

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

    DataOutputStream toFHBB;
    DataOutputStream toServer;

    InputStream fromServer;
    InputStream fromFHBB;

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
                FHEengine fhe = new FHEengine(editTextAddress.getText().toString(),
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
            Process process = Runtime.getRuntime().exec("/system/bin/chmod 777 /data/user/0/sharktank.pinger/files/FHBB_x");
            process.waitFor();
        } catch (Exception e) {
            Log.d(TAG, "Unable to change FHBB_x permissions", e);
        }
        newExe.setExecutable(true);
        return finalPath;
    }

    public class FHEengine extends Thread {

        int blocksz;
        byte[] buffer;
        int portnum;
        String hostname;

        FHEengine(String host, int blocklen, int port) {
            blocksz = blocklen;
            buffer = new byte[blocksz+1];
            hostname = host;
            portnum = port;
        }

        @Override
        public void run() {
            Process fhbb = null;
            try {
                fhbb = Runtime.getRuntime().exec(exepath);
            } catch (IOException e) {
                Log.d(TAG, "Could not execute FHBB");
                e.printStackTrace();
            }
            try {
                fromFHBB = fhbb.getInputStream(); //this is cout for FHBB_x
                toFHBB = new DataOutputStream(fhbb.getOutputStream()); //this is cin for FHBB_x
                fromFHBB.read(buffer);
                String test = new String(purgeFilth(buffer), "utf-8");
                Log.d(TAG, test);
            } catch (Exception e) {
                Log.d(TAG, "Crash During piping to FHBB", e);
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
                toServer = new DataOutputStream(socket.getOutputStream());
                fromServer = socket.getInputStream();
            } catch (Exception e) {
                Log.d(TAG, "Crash During Open Socket", e);
            }
        }

        private void install_upkg(byte[] buf, int blocklen) {
            try {
                socketToFHBB(buf, blocklen);
                Log.d(TAG, "Base got");
                socketToFHBB(buf, blocklen);
                Log.d(TAG, "Context got");
                FHBBtoSocket(buf, blocklen);
                Log.d(TAG, "PK sent");
            } catch (Exception e) {
                Log.d(TAG, "Crash During Install FHE", e);
            }
        }

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

        private void getDist(byte[] buffer, int blocklen) {
            try {
                socketToFHBB(buffer, blocklen);
            } catch (Exception e) {
                Log.d(TAG, "Crash During Distance Download", e);
            }
        }

        private void printDist(byte[] buffer, int blocklen) throws Exception {
            StringBuffer stream = new StringBuffer("");
            fHBBtoLocal(buffer, blocklen, stream);
            textResponse.setText(stream.toString());
        }

        private int readFromFHBB(byte[] buffer, int blocklen) throws Exception {
            //DataInputStream reader = new DataInputStream(fromFHBB);
            int i = fromFHBB.available();
            if (i==0) return 0;
            int rd;
            if (i < blocklen) {
                fromFHBB.read(buffer, 0, i);
                rd = i;
            } else {
                rd = fromFHBB.read(buffer, 0, blocklen);
            }
            byte [] buf2 = purgeFilth(buffer);
            buffer = buf2;
            String str = new String(buf2, "utf-8");
            Log.d(TAG, "From FHBB: " + str);
            return rd;
        }

        private void pushToFHBB(byte[] buffer, int blocklen) throws Exception {
            toFHBB.write(buffer, 0, blocklen);
//            String output = new String(buffer, "utf-8");
//            output += "\n\0";
//            Log.d(TAG, "To FHBB: " + output);
//            toFHBB.writeBytes(output);
            toFHBB.flush();
        }

        private int readFromSocket(byte[] buffer, int blocklen) throws Exception {
            DataInputStream reader = new DataInputStream(fromServer);
            int i = fromServer.available();
            int rd;
            if (i==0) return 0;
            if (i < blocklen) {
                reader.read(buffer, 0, i);
                rd = i;
            } else {
                rd = reader.read(buffer, 0, blocklen);
            }
            byte [] buf2 = purgeFilth(buffer);
            buffer = buf2;
            return rd;
        }

        private void pushToSocket(byte[] buffer) throws Exception {
            String output = new String(buffer, "utf-8");
            output += "\0";
            toServer.writeBytes(output);
            toServer.flush();
        }

        private void localToFHBB(byte[] buffer, int blocklen, StringBuffer stream) throws Exception {
            int k, j;
            k = 0;
            j = 0;
            do {
                j = k + blocklen;
                if ((k + blocklen) > stream.length()) {
                    j = stream.length();
                }
                toFHBB.writeBytes(stream.substring(k, j));
                toFHBB.flush();
//                pushToFHBB(buffer, j - k);
                //terminateFHBBblock();
                k = j;
                getFHBBACK();
            } while ((j - k) == blocklen);
        }

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

        private void socketToFHBB(byte[] buffer, int blocklen) throws Exception {
            int k;
            do {
                k = 0;
                k = readFromSocket(buffer, blocklen);
                pushToFHBB(buffer, k);
                //terminateFHBBblock();
                getFHBBACK();
                sendServerACK();
            } while (k == blocklen);
        }

        private void FHBBtoSocket(byte[] buffer, int blocklen) throws Exception {
            int k;
            do {
                k = 0;
                k = readFromFHBB(buffer, blocklen);
                pushToSocket(buffer);
                getServerACK();
                sendFHBBACK();
            } while (k == blocklen);
        }

        private boolean getServerACK() throws Exception {
            byte[] ack = new byte[blocksz+1];
            readFromSocket(ack, blocksz);
            String ref = new String(purgeFilth(ack), "utf-8");
            if (ref.equals("ACK")) {
                return true;
            } else {
                return false;
            }
        }

        private boolean getFHBBACK() throws Exception {
            byte[] ack = new byte[4];
            readFromFHBB(ack, 4);
            String ref = new String(purgeFilth(ack), "utf-8");
            Log.d(TAG, "ACK == " + ref);
            if (ref.equals("ACK")) {
                return true;
            } else {
                return false;
            }
        }

        private void sendServerACK() throws Exception {
            byte[] ack = new byte[4];
            ack[0] = 'A';
            ack[1] = 'C';
            ack[2] = 'K';
            ack[3] = '\0';
            pushToSocket(ack);
        }

        private void sendFHBBACK() throws Exception {
            byte[] ack = new byte[5];
            ack[0] = 'A';
            ack[1] = 'C';
            ack[2] = 'K';
            ack[3] = '\n';
            ack[3] = '\0';
            pushToFHBB(ack, 4);
        }

        private void sendFHBBNAK() throws Exception {
            byte[] ack = new byte[4];
            ack[0] = 'N';
            ack[1] = 'A';
            ack[2] = 'K';
            ack[3] = '\n';
            ack[3] = '\0';
            pushToFHBB(ack, 5);
        }

        private void terminateFHBBblock() throws Exception {
            byte[] term = new byte[2];
            term[0] = '\n';
            term[1] = '\0';
            pushToFHBB(term, 2);
        }

        private byte [] purgeFilth(byte[] victim) throws Exception {
            int j = 0;
            while (((victim[j] < 32) || (victim[j] > 126)) && (j < (victim.length-1))) j++;
            if (victim.length-1 == j) {
                byte [] out = new byte[1];
                out[0] = '\0';
                return out;
            }
            int i = j;
            while ((victim[i] > 31) && (victim[i] < 127) && (i < (victim.length-1))) i++;
            if ((j==0) && (i==victim.length-1)) return victim;
            ByteArrayInputStream good = new ByteArrayInputStream(victim, j, i);
            byte[] out = new byte[i];
            good.read(out, 0, good.available());
            return out;
        }
    }
}