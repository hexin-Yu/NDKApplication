package com.example.ndkapplication;

import androidx.appcompat.app.AppCompatActivity;

import android.graphics.PixelFormat;
import android.os.Bundle;
import android.os.Environment;
import android.os.Handler;
import android.os.HandlerThread;
import android.util.Log;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.View;
import android.widget.Button;
import android.widget.VideoView;

import java.io.File;

public class MainActivity extends AppCompatActivity {
    String TAG = "MainActivity";

    public static native String getStringFromC();

    public static native void diff(String filePath, String pattern, int num);

    public static native void path(String filePath, int count);

    private HandlerThread handlerThread;
    private Handler handler;
    private Button render_video;
    private Button play_sound;
    private Button play;
    private VideoView video_view;

    String inputPath = "";
    String outpuPaht = "";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        render_video = findViewById(R.id.render_video);
        play_sound = findViewById(R.id.play_sound);
        play = findViewById(R.id.play);
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
                holder.setFormat(PixelFormat.RGBA_8888);
                renderVideo(inputPath, holder.getSurface());
            }
        });
        play_sound.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                playSound();
            }
        });

        play.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                SurfaceHolder holder = video_view.getHolder();
                holder.setFormat(PixelFormat.RGBA_8888);
                SuperPlayer superPlayer = new SuperPlayer();
                superPlayer.play(inputPath, holder.getSurface());
            }
        });


    }

    private void renderVideo(String videoPath, Surface surface) {
        SuperPlayer.render(videoPath, surface);
    }

    private void decodeVideo() {
        handler.post(new Runnable() {
            @Override
            public void run() {
                SuperPlayer.decode(inputPath, outpuPaht);
            }
        });
    }

    private void playSound() {
        SuperPlayer.playSound(inputPath, outpuPaht);
    }


    @Override
    protected void onDestroy() {
        handlerThread.quit();
        super.onDestroy();
    }
}
