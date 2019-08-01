package com.example.ndkapplication;

import androidx.appcompat.app.AppCompatActivity;

import android.os.AsyncTask;
import android.os.Bundle;
import android.os.Environment;
import android.text.TextUtils;
import android.util.Log;

import java.io.File;
import java.util.concurrent.Executor;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.ThreadFactory;
import java.util.concurrent.ThreadPoolExecutor;
import java.util.concurrent.TimeUnit;

public class MainActivity extends AppCompatActivity {
    String TAG = "MainActivity";

    public static native String getStringFromC();

    public static native void diff(String filePath, String pattern, int num);

    public static native void path(String filePath, int count);

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        final String inputPath = new File(Environment.getExternalStorageDirectory(), "input.mp4").getAbsolutePath();
        final String outpuPaht = new File(Environment.getExternalStorageDirectory(), "output.mp4").getAbsolutePath();
        Log.i("MainActivity", "inputPath : " + inputPath);
        Log.i("MainActivity", "outpuPaht : " + outpuPaht);

        new Thread(new Runnable(){
            @Override
            public void run() {
                VideoUtils.decode(inputPath, outpuPaht);
            }
        }).start();

    }
}
