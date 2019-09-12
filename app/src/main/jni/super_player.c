#include "Java_com_example_ndkapplication.h"
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
#include <libyuv.h>
#include <android/log.h>
#include <android/native_window.h>
#include <libswresample/swresample.h>
#include <android/native_window_jni.h>
#include <malloc.h>
#include <unistd.h>
#include <libavutil/time.h>
#include "queue.h"

#define LOGI(FORMAT, ...) __android_log_print(ANDROID_LOG_INFO,"super",FORMAT,__VA_ARGS__);
#define LOGE(FORMAT2, ...) __android_log_print(ANDROID_LOG_ERROR,"super",FORMAT2,__VA_ARGS__);


#define MAX_STREAM  2

#define MAX_AUDIO_FRME_SIZE 48000*4

#define PACKET_QUEUE_SIZE 50

#define MIN_SLEEP_TIME_US  1000ll

#define AUDIO_TIME_ADJUST_US  -200000ll

typedef struct Player {
    JavaVM *javaVM;

    AVFormatContext *input_format_ctx;

    int video_stream_index;
    int audio_stream_index;

    int captrue_streams_no;

    //解码器上下文数组
    AVCodecContext *input_codec_ctx[MAX_STREAM];

    pthread_t decode_threads[MAX_STREAM];

    ANativeWindow *nativeWindow;
    SwrContext *swr_ctx;

    enum AVSampleFormat in_sample_fmt;
    enum AVSampleFormat out_sample_fmt;
    //输入采样率
    int in_sample_rate;
    //输出采样率
    int out_sample_rate;
    //输出的声道个数
    int out_channel_nb;

    //JNI
    jobject audio_track;
    jmethodID audio_track_write_mid;

    pthread_t thread_read_from_stream;
    //音频，视频队列数组
    Queue *packets[MAX_STREAM];

    //互斥锁
    pthread_mutex_t mutex;
    // 条件变量
    pthread_cond_t cond;

    int64_t start_time;

    int64_t audio_clock;

    int framecount;

} Player;

typedef struct _DecoderData {
    Player *player;
    int stream_index;
} DecoderData;

DecoderData decoderData;

/**
 * 初始化封装格式上下文
 */
void init_input_format_ctx(Player *player, const char *input_cstr) {
    // 1.注册
    av_register_all();
    // 封装上下文
    AVFormatContext *pFormatCtx = avformat_alloc_context();
    LOGI("%s", "pFormatCtx");
    // 2.打开视频文件
    if (avformat_open_input(&pFormatCtx, input_cstr, NULL, NULL) != 0) {
        LOGE("%s", "打开视频失败");
        return;
    }

    // 3.获取视频信息
    if (avformat_find_stream_info(pFormatCtx, NULL) < 0) {
        LOGE("%s", "获取视频信息失败");
        return;
    }
    player->captrue_streams_no = pFormatCtx->nb_streams;
    LOGI("%s", "step 0.1");
    int i = 0;
    // 获取视频流的索引位置
    for (; i < player->captrue_streams_no; i++) {
        if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
            player->video_stream_index = i;
        } else if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
            player->audio_stream_index = i;
        }
    }
    LOGI("%s", "step 0.2");
    player->input_format_ctx = pFormatCtx;

}

/**
 * 初始化编码器上下文
 */
void init_codec_context(Player *player, int stream_idx) {
    LOGI("%s", "step 2.1");
    //4.获取视频解码器
    AVCodecContext *pCodecCtx = player->input_format_ctx->streams[stream_idx]->codec;
    AVCodec *pCodec = avcodec_find_decoder(pCodecCtx->codec_id);

    if (pCodec == NULL) {
        LOGE("%s", "无法解码")
        return;
    }
    LOGI("%s", "step 2.2");
    // 5.打开解码器
    if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0) {
        LOGI("%s", "step 2.1 解码器无法打开");
        LOGE("%s", "解码器无法打开");
        return;
    }
    player->input_codec_ctx[stream_idx] = pCodecCtx;
    LOGI("%s", "step 2.3");

}


int64_t player_get_current_video_time(Player *player) {
    int64_t current_time = av_gettime();
    return current_time - player->start_time;
}

void player_wait_for_frame(Player *player, int64_t stream_time, int stream_no) {
    pthread_mutex_lock(&player->mutex);
    for (;;) {
        int64_t current_video_time = player_get_current_video_time(player);
        int64_t sleep_time = stream_time - current_video_time;
        if (sleep_time < -300000ll) {
            // 300mslate
            int64_t new_value = player->start_time - sleep_time;
//            LOGI("player_wait_for_frame[%d] correcting %f to %f because late",
//                 stream_no, (av_gettime() - player->start_time) / 1000000.0,
//                 (av_gettime() - new_value) / 1000000.0);
            player->start_time = new_value;
            pthread_cond_broadcast(&player->cond);
        }
        if (sleep_time <= MIN_SLEEP_TIME_US) {
            break;
        }
        if (sleep_time > 500000ll) {
            // if sleep time is bigger then 500ms just sleep this 500ms
            // and check everything again
            sleep_time = 500000ll;
        }
        //等待指定时长
        int timeout_ret = pthread_cond_timeout_np(&player->cond, &player->mutex,
                                                  sleep_time / 1000ll);
    }
    pthread_mutex_unlock(&player->mutex);

}


void decode_video_prepare(JNIEnv *env, Player *player, jobject surface) {
    player->nativeWindow = ANativeWindow_fromSurface(env, surface);
}

/**
 * 解码视频
 */
void decode_video(Player *player, AVPacket *packet) {
    LOGI("decode_video : packet  %#x", packet);
    AVFormatContext *input_format_ctx = player->input_format_ctx;
    AVStream *stream = input_format_ctx->streams[player->video_stream_index];

    AVFrame *yuv_frame = av_frame_alloc();
    AVFrame *rgb_frame = av_frame_alloc();
    // native绘制
    ANativeWindow *nativeWindow = player->nativeWindow;
    LOGI("%s", "step 5.2");
    // 缓冲区
    ANativeWindow_Buffer outBuffer;
    int len, get_frame;

    AVCodecContext *codecCtx = player->input_codec_ctx[player->video_stream_index];

    avcodec_decode_video2(codecCtx, yuv_frame, &get_frame, packet);

    if (get_frame) {// 读取标志
        // 缓冲区属性
        ANativeWindow_setBuffersGeometry(
                nativeWindow, codecCtx->width, codecCtx->height, WINDOW_FORMAT_RGBA_8888
        );
        //  lock
        ANativeWindow_lock(nativeWindow, &outBuffer, NULL);
        LOGI("%s", "ANativeWindow_lock");
        // 填充
        LOGI("渲染第%d帧", player->framecount++)
        avpicture_fill(
                (AVPicture *) rgb_frame, outBuffer.bits, AV_PIX_FMT_ARGB, codecCtx->width,
                codecCtx->height
        );
        LOGI("%s", "avpicture_fill");
        // YUV ->RGBA_8888
        I420ToARGB(yuv_frame->data[0], yuv_frame->linesize[0],
                   yuv_frame->data[2], yuv_frame->linesize[2],
                   yuv_frame->data[1], yuv_frame->linesize[1],
                   rgb_frame->data[0], rgb_frame->linesize[0],
                   codecCtx->width, codecCtx->height);
        LOGI("%s", "I420ToARGB");
        int64_t pts = av_frame_get_best_effort_timestamp(yuv_frame);

        int64_t time = av_rescale_q(pts, stream->time_base, AV_TIME_BASE_Q);
        LOGI("%s", "pts");
//        player_wait_for_frame(player, time, player->video_stream_index);

        // unlcok
        ANativeWindow_unlockAndPost(nativeWindow);
        LOGI("%s", "ANativeWindow_unlockAndPost");
    }
    av_frame_free(&yuv_frame);
    av_frame_free(&rgb_frame);

}

/**
 * 解码音频准备
 */
void decode_audio_prepare(Player *player) {
    LOGI("%s", "step 3.1");
    AVCodecContext *codecContext = player->input_codec_ctx[player->audio_stream_index];

    enum AVSampleFormat in_sample_fmt = codecContext->sample_fmt;
    // 16bit PCM
    enum AVSampleFormat out_sample_fmt = AV_SAMPLE_FMT_S16;

    int in_sample_rate = codecContext->sample_rate;

    int out_sample_rate = in_sample_rate;

    uint64_t in_ch_layout = codecContext->channel_layout;

    uint64_t out_ch_layout = AV_CH_LAYOUT_STEREO;

    SwrContext *swrContext = swr_alloc();

    swr_alloc_set_opts(swrContext, out_ch_layout, out_sample_fmt, out_sample_rate, in_ch_layout,
                       in_sample_fmt, in_sample_rate, 0, NULL
    );
    swr_init(swrContext);


    int out_channel_nb = av_get_channel_layout_nb_channels(out_ch_layout);

    player->out_sample_rate = out_sample_rate;
    player->out_sample_fmt = out_sample_fmt;
    player->in_sample_rate = in_sample_rate;
    player->in_sample_fmt = in_sample_fmt;
    player->out_channel_nb = out_channel_nb;
    player->swr_ctx = swrContext;

}

/**
 * 解码音频
 */
void decode_audio(Player *player, AVPacket *packet) {
    LOGI("%s", "decode_audio");
    AVFormatContext *input_format_ctx = player->input_format_ctx;
    AVStream *stream = input_format_ctx->streams[player->video_stream_index];
    AVCodecContext *codec_ctx = player->input_codec_ctx[player->audio_stream_index];
    AVFrame *frame = av_frame_alloc();
    int got_frame;

    avcodec_decode_audio4(codec_ctx, frame, &got_frame, packet);
    int *out_buffer = (uint8_t *) av_malloc(MAX_AUDIO_FRME_SIZE);
    if (got_frame > 0) {
        swr_convert(player->swr_ctx,
                    &out_buffer, MAX_AUDIO_FRME_SIZE,
                    (const uint8_t **) frame->data,
                    frame->nb_samples);

        int out_buffer_size = av_samples_get_buffer_size(NULL, player->out_channel_nb,
                                                         frame->nb_samples, player->out_sample_fmt,
                                                         1);
        // Presentation timestamp in AVStream->time_base units
        int64_t pts = packet->pts;
        if (pts != AV_NOPTS_VALUE) {
            player->audio_clock = av_rescale_q(pts, stream->time_base,
                                               AV_TIME_BASE_Q);
            // 计算延迟
//            player_wait_for_frame(player, player->audio_clock + AUDIO_TIME_ADJUST_US,
//                                  player->audio_stream_index);
        }
        LOGI("%s", "decode_audio stp 1");
        // 关联处理的线程
        JavaVM *javaVM = player->javaVM;
        JNIEnv *env;
        (*javaVM)->AttachCurrentThread(javaVM, &env, NULL);
        LOGI("%s", "decode_audio stp 1.1");
        jbyteArray audio_sample_array = (*env)->NewByteArray(env, out_buffer_size);
        LOGI("%s", "decode_audio stp 1.2");
        jbyte *sample_byte = (*env)->GetByteArrayElements(env, audio_sample_array, NULL);
        LOGI("%s", "decode_audio stp 1.3");
        memcpy(sample_byte, out_buffer, out_buffer_size);
        LOGI("%s", "decode_audio stp 1.4");
        (*env)->ReleaseByteArrayElements(env, audio_sample_array, sample_byte, 0);
        LOGI("%s", "decode_audio stp 2");

        (*env)->CallIntMethod(env, player->audio_track, player->audio_track_write_mid,
                              audio_sample_array, 0,
                              out_buffer_size);
        LOGI("%s", "decode_audio stp 4");
        (*env)->DeleteLocalRef(env, audio_sample_array);

        (*javaVM)->DetachCurrentThread(javaVM);
        usleep(1000 * 16);
    }
    av_frame_free(&frame);
}

void jni_audio_prepare(JNIEnv *env, jobject jobj, Player *player) {
    LOGI("%s", "jni_audio_prepare stp 0");
    // 使用java的AudioTrack播放音频
    jclass jclazz = (*env)->GetObjectClass(env, jobj);
    // (II)Landroid/media/AudioTrack;
    jmethodID create_audio_track = (*env)->GetStaticMethodID(env, jclazz, "createAudioTrack",
                                                             "(II)Landroid/media/AudioTrack;");
    LOGI("%s", "jni_audio_prepare step 4");
    //    android.media.AudioTrack;
    if (create_audio_track == NULL) {
        LOGI("%s", "create_audio_track is null");
    }
    jobject audio_track = (*env)->CallStaticObjectMethod(env, jclazz, create_audio_track,
                                                         player->out_sample_rate,
                                                         player->out_channel_nb);
    if (audio_track == NULL) {
        LOGI("%s", "audio_track is null");
    }
    LOGI("%s", "jni_audio_prepare step 4.0");
    jclass audio_track_class = (*env)->GetObjectClass(env, audio_track);
    jmethodID play = (*env)->GetMethodID(env, audio_track_class, "play", "()V");
    if (play == NULL) {
        LOGI("%s", "play is null");
    }
    LOGI("%s", "jni_audio_prepare step 4.1");
    (*env)->CallVoidMethod(env, audio_track, play);

    jmethodID audio_write = (*env)->GetMethodID(env, audio_track_class, "write", "([BII)I");
    LOGI("%s", "jni_audio_prepare step 4.2");

    // FIXME 使用全局变量
    player->audio_track = (*env)->NewGlobalRef(env, audio_track);
    player->audio_track_write_mid = audio_write;

}

void *decode_data(void *arg) {
    DecoderData *decoder_data = (DecoderData *) arg;
    Player *player = decoder_data->player;

    int stream_index = decoder_data->stream_index;
    Queue *queue = (Queue *) player->packets[stream_index];

    AVFormatContext *formatContext = player->input_format_ctx;
    int video_frame_count = 0, audio_frame_count = 0, frame_count = 0;

    for (;;) {
        LOGI("decode_data frame_count :%d", ++frame_count);
        pthread_mutex_lock(&player->mutex);
        AVPacket *packet = (AVPacket *) queue_pop(queue, &player->mutex, &player->cond);
        pthread_mutex_unlock(&player->mutex);
        if (stream_index == player->video_stream_index) {
            decode_video(player, packet);
            LOGI("video_frame_count:%d", ++video_frame_count);
        } else {
            decode_audio(player, packet);
            LOGI("audio_frame_count:%d", ++audio_frame_count);
        }
    }

}


void *player_fill_packet() {
    AVPacket *packet = malloc(sizeof(AVPacket));
    return packet;
}

void player_alloc_queues(Player *player) {
    int i;
    for (i = 0; i < player->captrue_streams_no; i++) {
        Queue *queue = queue_init(PACKET_QUEUE_SIZE, (queue_fill_func) player_fill_packet);
        player->packets[i] = queue;
        LOGI("stream index:%d,queue:%#x", i, queue);
    }
}

void *packet_free_func(AVPacket *packet) {
    av_free_packet(packet);
    return 0;
}


void *player_read_from_stream(Player *player) {
    int index = 0;
    int ret;
    AVPacket packet, *pkt = &packet;
    for (;;) {
        ret = av_read_frame(player->input_format_ctx, pkt);
        if (ret < 0) {
            break;
        }
//        LOGI("pkt->stream_index %s",pkt->stream_index);

        Queue *queue = player->packets[pkt->stream_index];

        pthread_mutex_lock(&player->mutex);
        AVPacket *packet_data = queue_push(queue, &player->mutex, &player->cond);
        *packet_data = packet;
        pthread_mutex_unlock(&player->mutex);
        LOGI("queue:%#x, packet:%#x", queue, packet);
    }

}

JNIEXPORT void JNICALL Java_com_example_ndkapplication_SuperPlayer_play
        (JNIEnv *env, jobject jobj, jstring input_jstr, jobject surface) {
    LOGI("%s", "JNICALL Java_com_example_ndkapplication_SuperPlayer_render");
    LOGE("%s", "ERROR LOG TEST");
    // 需要转码的视频文件(输入的视频文件)
    const char *input_cstr = (*env)->GetStringUTFChars(env, input_jstr, NULL);

    Player *player = malloc(sizeof(Player));
    (*env)->GetJavaVM(env, &(player->javaVM));
    player->framecount = 0;
    LOGI("%s", "step 0");
    init_input_format_ctx(player, input_cstr);
    int video_stream_index = player->video_stream_index;
    int audio_stream_index = player->audio_stream_index;
    LOGI("%s", "step 1");
    init_codec_context(player, video_stream_index);
    LOGI("%s", "step 2");
    init_codec_context(player, audio_stream_index);
    LOGI("%s", "step 3");
    decode_audio_prepare(player);
    LOGI("%s", "step 4");
    decode_video_prepare(env, player, surface);
    LOGI("%s", "step 5");
    jni_audio_prepare(env, jobj, player);
    LOGI("%s", "step 6");
    player_alloc_queues(player);

    pthread_mutex_init(&player->mutex, NULL);
    pthread_cond_init(&player->cond, NULL);

    pthread_create(&(player->thread_read_from_stream), NULL, player_read_from_stream,
                   (void *) player
    );

    player->start_time = 0;

    DecoderData data1 = {player, video_stream_index}, *decoder_data1 = &data1;
    pthread_create(&(player->decode_threads[video_stream_index]), NULL, decode_data,
                   (void *) decoder_data1);

    DecoderData data2 = {player, audio_stream_index}, *decoder_data2 = &data2;
    pthread_create(&(player->decode_threads[audio_stream_index]), NULL, decode_data,
                   (void *) decoder_data2);

    pthread_join(player->thread_read_from_stream, NULL);
    pthread_join(player->decode_threads[video_stream_index], NULL);
    pthread_join(player->decode_threads[audio_stream_index], NULL);

}












