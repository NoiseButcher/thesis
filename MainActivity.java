package com.example.sharky.fheclient;

import android.content.Context;
import android.net.Uri;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.location.Location;
import android.location.LocationListener;
import android.location.LocationManager;
import android.provider.Settings;
import android.content.Intent;
import android.content.res.AssetManager;

import com.google.android.gms.appindexing.Action;
import com.google.android.gms.appindexing.AppIndex;
import com.google.android.gms.common.api.GoogleApiClient;

import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.File;


public class MainActivity extends AppCompatActivity implements LocationListener {

    /**
     * ATTENTION: This was auto-generated to implement the App Indexing API.
     * See https://g.co/AppIndexing/AndroidStudio for more information.
     */
    private GoogleApiClient client;
    private LocationManager locationManager;
    private Location location;
    private String assetpath = "prox_x";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        // ATTENTION: This was auto-generated to implement the App Indexing API.
        // See https://g.co/AppIndexing/AndroidStudio for more information.
        client = new GoogleApiClient.Builder(this).addApi(AppIndex.API).build();

        locationManager = (LocationManager) this.getSystemService(Context.LOCATION_SERVICE);
        locationManager.requestLocationUpdates(LocationManager.GPS_PROVIDER, 2000, 1, this);
        String exe = copyAssets(assetpath);
        execprox(exe, location);

    }

    private String copyAssets(String path) {
        AssetManager assetManager = getAssets();
        InputStream in;
        OutputStream out;
        String appFileDir = getFilesDir().getPath();
        String finalPath = appFileDir + "/prox_x";
        byte [] buffer;
        try {
            in = assetManager.open(path);
            buffer = new byte[in.available()];
            in.read(buffer);
            in.close();

            File outfile = new File(appFileDir, path);
            out = new FileOutputStream(outfile);
            out.write(buffer);
            out.close();

        } catch (Exception e) {
            e.printStackTrace();
        }

        File newExe = new File(finalPath);
        newExe.setExecutable(true);
        return finalPath;
    }


    private void execprox(String command, Location location) {

        try {
            Process prox = Runtime.getRuntime().exec(command);
            InputStreamReader reader = new InputStreamReader(prox.getInputStream());
            OutputStreamWriter writer = new OutputStreamWriter(prox.getOutputStream());
            try {
                proxInterface(location, reader, writer);
            } catch (Exception e) {
                e.printStackTrace();
            }
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
    }

    private void proxInterface(Location location, InputStreamReader in, OutputStreamWriter out) throws Exception {

        try {
            char[] buffer = new char[256];
            String dis;

            while(true) {
                //Send GPS coords.
                pushToProx(out, location);
                //Read the distances.
                dis = getProxOutput(buffer, in);
            }

        } catch (Exception e) {
            e.printStackTrace();
        }

    }

    private String getProxOutput(char[] buffer, InputStreamReader rx) {
        int rd;
        //Read the distances from the client process.
        try {
            while ((rd = rx.read(buffer, 0, 255)) > 0) ;
        } catch (Exception e) {
            e.printStackTrace();
        }
        //Return them as a string
        return buffer.toString();
    }

    private void pushToProx(OutputStreamWriter wx, Location location) {
        int rd;
        String lat, lng;
        //Get the current co-ordinates from the GPS thing.
        lng = Location.convert(location.getLatitude(), Location.FORMAT_DEGREES);
        lat = Location.convert(location.getLongitude(), Location.FORMAT_DEGREES);
        //Write the longitude to the client process
        try {
            wx.write(lng, 0, lng.length());
            //Write the latitude to the client process
            wx.write(lat, 0, lat.length());
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    @Override
    public void onStart() {
        super.onStart();

        // ATTENTION: This was auto-generated to implement the App Indexing API.
        // See https://g.co/AppIndexing/AndroidStudio for more information.
        client.connect();
        Action viewAction = Action.newAction(
                Action.TYPE_VIEW, // TODO: choose an action type.
                "Main Page", // TODO: Define a title for the content shown.
                // TODO: If you have web page content that matches this app activity's content,
                // make sure this auto-generated web page URL is correct.
                // Otherwise, set the URL to null.
                Uri.parse("http://host/path"),
                // TODO: Make sure this auto-generated app deep link URI is correct.
                Uri.parse("android-app://com.example.sharky.fheclient/http/host/path")
        );
        AppIndex.AppIndexApi.start(client, viewAction);
    }

    @Override
    public void onStop() {
        super.onStop();

        // ATTENTION: This was auto-generated to implement the App Indexing API.
        // See https://g.co/AppIndexing/AndroidStudio for more information.
        Action viewAction = Action.newAction(
                Action.TYPE_VIEW, // TODO: choose an action type.
                "Main Page", // TODO: Define a title for the content shown.
                // TODO: If you have web page content that matches this app activity's content,
                // make sure this auto-generated web page URL is correct.
                // Otherwise, set the URL to null.
                Uri.parse("http://host/path"),
                // TODO: Make sure this auto-generated app deep link URI is correct.
                Uri.parse("android-app://com.example.sharky.fheclient/http/host/path")
        );
        AppIndex.AppIndexApi.end(client, viewAction);
        client.disconnect();
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
