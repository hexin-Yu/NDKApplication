package com.example.ndkapplication;

import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioTrack;
import android.view.Surface;

public class VideoUtils {
    public native static void decode(String input, String output);

    public native static void render(String input, Surface surface);

    public native static void palySound(String input, String output);

    public static AudioTrack createAudioTrack(int sampleRateInHz, int nb_channels) {
        int audioFormat = AudioFormat.ENCODING_PCM_16BIT;

        int channelConfig;
        if (nb_channels == 1) {
            channelConfig = AudioFormat.CHANNEL_OUT_MONO;
        } else if (nb_channels == 2) {
            channelConfig = AudioFormat.CHANNEL_OUT_STEREO;
        } else {
            channelConfig = AudioFormat.CHANNEL_OUT_STEREO;
        }

        int minBufferSize = AudioTrack.getMinBufferSize(sampleRateInHz, channelConfig, audioFormat);

        AudioTrack audioTrack = new AudioTrack(AudioManager.STREAM_MUSIC
                , sampleRateInHz
                , channelConfig
                , audioFormat
                , minBufferSize
                , AudioTrack.MODE_STATIC
        );
//        audioTrack.play();
        return audioTrack;

    }


    static {
        System.loadLibrary("avutil-54");
        System.loadLibrary("swresample-1");
        System.loadLibrary("avcodec-56");
        System.loadLibrary("avformat-56");
        System.loadLibrary("swscale-3");
        System.loadLibrary("postproc-53");
        System.loadLibrary("avfilter-5");
        System.loadLibrary("avdevice-56");
        System.loadLibrary("myffmpeg");
    }


}
