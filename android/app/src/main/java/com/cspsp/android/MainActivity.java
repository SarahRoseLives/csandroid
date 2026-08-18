package com.cspsp.android;

import android.app.NativeActivity;
import android.content.res.AssetManager;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.view.WindowManager;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

public class MainActivity extends NativeActivity {

    private static final String TAG = "CSPSP.MainActivity";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        // Extract the game assets to the app's files dir before the native
        // activity starts, so the C++ code can chdir() there and use relative
        // fopen()/opendir() paths unchanged.
        try {
            int count = 0;
            for (String dir : GAME_ASSET_DIRS) {
                count += extractAssets(getAssets(), dir, getFilesDir());
            }
            Log.i(TAG, "extracted " + count + " asset files to " + getFilesDir());
        } catch (Exception e) {
            Log.e(TAG, "asset extraction failed", e);
        }

        super.onCreate(savedInstanceState);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

        hideSystemUi();
    }

    private static final String[] GAME_ASSET_DIRS = {"data", "gfx", "maps", "sfx"};

    private static int extractAssets(AssetManager am, String path, File dest) throws Exception {
        int count = 0;
        String[] entries = am.list(path);
        if (entries == null) return 0;

        for (String e : entries) {
            String full = path.isEmpty() ? e : path + "/" + e;

            // am.open() throws IOException for directories, succeeds for files.
            InputStream in = null;
            try {
                in = am.open(full);
            } catch (IOException ex) {
                in = null;
            }

            if (in != null) {
                File f = new File(dest, full);
                File parent = f.getParentFile();
                if (parent != null) parent.mkdirs();

                OutputStream out = new FileOutputStream(f);
                byte[] buf = new byte[65536];
                int n;
                while ((n = in.read(buf)) > 0) out.write(buf, 0, n);
                out.close();
                in.close();
                count++;
            } else {
                count += extractAssets(am, full, dest);
            }
        }
        return count;
    }

    private void hideSystemUi() {
        final View decor = getWindow().getDecorView();
        decor.setSystemUiVisibility(
            View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
            | View.SYSTEM_UI_FLAG_FULLSCREEN
            | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
            | View.SYSTEM_UI_FLAG_LAYOUT_STABLE
            | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
            | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION);
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) {
            hideSystemUi();
        }
    }
}
