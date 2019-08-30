package com.example.ndkapplication;

import androidx.appcompat.app.AppCompatActivity;

import android.media.AudioTrack;
import android.os.AsyncTask;
import android.os.Bundle;
import android.os.Environment;
import android.os.Handler;
import android.os.HandlerThread;
import android.text.TextUtils;
import android.util.Log;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.View;
import android.widget.Button;
import android.widget.VideoView;

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

    private HandlerThread handlerThread;
    private Handler handler;
    private Button render_video;
    private VideoView video_view;

    String inputPath = "";
    String outpuPaht = "";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        render_video = findViewById(R.id.render_video);
        video_view = findViewById(R.id.video_view);
        inputPath = new File(Environment.getExternalStorageDirectory(), "input.mp4").getAbsolutePath();
        outpuPaht = new File(Environment.getExternalStorageDirectory(), "output.mp4").getAbsolutePath();
        Log.i("MainActivity", "inputPath : " + inputPath);
        Log.i("MainActivity", "outpuPaht : " + outpuPaht);

        handlerThread = new HandlerThread("handlerThread");
        handlerThread.start();
        handler = new Handler(handlerThread.getLooper());
        render_video.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                SurfaceHolder holder = video_view.getHolder();
                holder.getSurface();
                renderVideo(inputPath, holder.getSurface());
            }
        });


    }

    private void renderVideo(String videoPath, Surface surface) {
        VideoUtils.render(videoPath, surface);
    }

    private void decodeVideo() {
        handler.post(new Runnable() {
            @Override
            public void run() {
                VideoUtils.decode(inputPath, outpuPaht);
            }
        });
    }


    @Override
    protected void onDestroy() {
        handlerThread.quit();
        super.onDestroy();
    }
}
