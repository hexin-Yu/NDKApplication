#include <libavformat/avformat.h>
#include <android/log.h>
#include <libswresample/swresample.h>
#include <unistd.h>
#include "Java_com_example_ndkapplication.h"


#define LOGI(FORMAT, ...) __android_log_print(ANDROID_LOG_INFO,"super",FORMAT,__VA_ARGS__);
#define LOGE(FORMAT, ...) __android_log_print(ANDROID_LOG_ERROR,"super",FORMAT,__VA_ARGS__);


#define MAX_AUDIO_FRME_SIZE 4800 * 4

JNIEXPORT void JNICALL Java_com_example_ndkapplication_VideoUtils_paly_sound
        (JNIEnv *env, jclass jthiz, jstring input_jstr, jstring output_jstr) {
    const char *input_cstr = (*env)->GetStringUTFChars(env, input_jstr, NULL);
    const char *output_cstr = (*env)->GetStringUTFChars(env, output_jstr, NULL);
    // 1.注册
    av_register_all();
    AVFormatContext *formatContext = avformat_alloc_context();

    if (avformat_open_input(&formatContext, input_cstr, NULL, NULL) != 0) {
        LOGI("%s", "无法打开音频");
        return;
    }

    if (avformat_find_stream_info(formatContext, NULL) < 0) {
        LOGI("%s", "无法获取音频信息");
        return;
    }

    int i = 0, audio_stream_idx = -1;
    for (; i < formatContext->nb_streams; i++) {
        if (formatContext->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
            audio_stream_idx = i;
            break;
        }
    }

    AVCodecContext *codecContext = formatContext->streams[audio_stream_idx]->codec;
    AVCodec *codec = avcodec_find_decoder(codecContext->codec_id);

    if (codec == NULL) {
        LOGI("%s", "无法获取编码器")
        return;
    }

    if (avcodec_open2(codecContext, codec, NULL) == NULL) {
        LOGI("%s", "无法打开编码器")
        return;
    }

    AVPacket *packet = (AVPacket *) (sizeof(AVPacket));

    AVFrame *frame = av_frame_alloc();

    SwrContext *swrContext = swr_alloc();

    enum AVSampleFormat in_sample_fmt = codecContext->sample_fmt;
    // 16bit PCM
    enum AVSampleFormat out_sample_fmt = AV_SAMPLE_FMT_S16;

    int in_sample_rate = codecContext->sample_rate;

    int out_sample_rate = in_sample_rate;

    uint64_t in_ch_layout = codecContext->channel_layout;

    uint64_t out_ch_layout = AV_CH_LAYOUT_STEREO;

    swr_alloc_set_opts(swrContext, out_ch_layout, out_sample_fmt, out_sample_rate, in_ch_layout,
                       in_sample_fmt, in_sample_rate, 0, NULL
    );

    swr_init(swrContext);

    int out_channel_nb = av_get_channel_layout_nb_channels(out_ch_layout);

    // 使用java的AudioTrack播放音频
    jclass player_class = (*env)->GetObjectClass(env, jthiz);
    // (II)Landroid/media/AudioTrack;
    jmethodID create_audio_track = (*env)->GetMethodID(env, player_class, "createAudioTrack",
                                                       "(II)Landroid/media/AudioTrack;");

    jobject audio_track = (*env)->CallObjectMethod(env, jthiz, create_audio_track, out_sample_rate,
                                                   out_channel_nb);

    jclass audio_track_class = (*env)->GetObjectClass(env, audio_track);
    jmethodID play = (*env)->GetMethodID(env, audio_track_class, "play", "()V");

    jmethodID audio_write = (*env)->GetMethodID(env, audio_track_class, "write", "([BII)I");

    (*env)->CallVoidMethod(env, audio_track, play);

    FILE *fp_pcm = fopen(output_cstr, "wb");

    uint8_t *out_buffer = av_malloc(MAX_AUDIO_FRME_SIZE);

    int got_fram = 0, index = 0, ret;

    while (av_read_frame(formatContext, packet) >= 0) {
        if (packet->stream_index == audio_stream_idx) {
            ret = avcodec_decode_audio4(codecContext, frame, &got_fram, packet);

            if (ret < 0) {
                LOGI("%s", "解码完成")
            }

            if (got_fram > 0) {
                LOGI("%d", index++);
                swr_convert(swrContext,
                            &out_buffer, MAX_AUDIO_FRME_SIZE,
                            (const uint8_t **) frame->data,
                            frame->nb_samples
                );

                int out_buffer_size = av_samples_get_buffer_size(NULL, out_channel_nb,
                                                                 frame->nb_samples, out_sample_fmt,
                                                                 1);
                fwrite(out_buffer, 1, out_buffer_size, fp_pcm);

                jbyteArray audio_sample_array = (*env)->NewByteArray(env, out_buffer_size);

                jbyte *sample_byte = (*env)->GetByteArrayElements(env, audio_sample_array, NULL);

                memcpy(sample_byte, out_buffer, out_buffer_size);

                (*env)->ReleaseByteArrayElements(env, audio_sample_array, sample_byte, 0);

                (*env)->CallIntMethod(env, audio_track, audio_write, audio_sample_array, 0,
                                      out_buffer_size);
                (*env)->DeleteLocalRef(env, audio_sample_array);

                usleep(1000 * 16);
            }
        }

        av_frame_free(&frame);
        av_free(out_buffer);

        swr_free(out_buffer);
        avcodec_close(codecContext);
        avformat_close_input(&formatContext);

        (*env)->ReleaseStringUTFChars(env, input_jstr, input_cstr);
        (*env)->ReleaseStringUTFChars(env, output_jstr, output_cstr);
    }






























//    avcodec_decode_audio4()
//    AVCodecContext




}
