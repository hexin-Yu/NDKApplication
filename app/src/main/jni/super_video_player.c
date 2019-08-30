#include "Java_com_example_ndkapplication.h"
#include <android/log.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
#include <libyuv.h>
#include <unistd.h>
#include "android/native_window.h"
#include "android/native_window_jni.h"

#define LOGI(FORMAT, ...) __android_log_print(ANDROID_LOG_INFO,"super",FORMAT,##__VA_ARGS__);
#define LOGE(FORMAT, ...) __android_log_print(ANDROID_LOG_ERROR,"jason",FORMAT,##__VA_ARGS__);

JNIEXPORT void JNICALL Java_com_example_ndkapplication_VideoUtils_render
        (JNIEnv *env, jclass jobj, jstring input_jstr, jobject surface) {
    LOGI("%s", "JNICALL Java_com_example_ndkapplication_VideoUtils_render");
    LOGE("%", "ERROR LOG TEST");
    // 需要转码的视频文件(输入的视频文件)
    const char *input_cstr = (*env)->GetStringUTFChars(env, input_jstr, NULL);
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

    int video_stream_idx = -1;
    int i = 0;
    // 获取视频流的索引位置
    for (; i < pFormatCtx->nb_streams; i++) {
        if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_idx = i;
            break;
        }
    }
    LOGI("%s", "step 4");
    //4.获取视频解码器
    AVCodecContext *pCodecCtx = pFormatCtx->streams[video_stream_idx]->codec;
    AVCodec *pCodec = avcodec_find_decoder(pCodecCtx->codec_id);

    if (pCodec == NULL) {
        LOGE("%s", "无法解码")
        return;
    }
    LOGI("%s", "step 5");
    // 5.打开解码器
    if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0) {
        LOGI("%s", "step 5.1 解码器无法打开");
        LOGE("%", "解码器无法打开");
        return;
    }
    LOGI("%s", "step 5.1");
    AVPacket *packet = (AVPacket *) av_malloc(sizeof(AVPacket));
    AVFrame *yuv_frame = av_frame_alloc();
    AVFrame *rgb_frame = av_frame_alloc();
    // native绘制
    ANativeWindow *nativeWindow = ANativeWindow_fromSurface(env, surface);
    LOGI("%s", "step 5.2");
    // 缓冲区
    ANativeWindow_Buffer outBuffer;
    int len, get_frame, framecount = 0;

    LOGE("%", "1-5步完成");

    // 按帧读取
    while (av_read_frame(pFormatCtx, packet) >= 0) {
        len = avcodec_decode_video2(pCodecCtx, yuv_frame, &get_frame, packet);

        if (get_frame) {// 读取标志
            LOGI("第%d帧", framecount++)
            // 缓冲区属性
            ANativeWindow_setBuffersGeometry(
                    nativeWindow, pCodecCtx->width, pCodecCtx->height, WINDOW_FORMAT_RGBA_8888
            );
            //  lock
            ANativeWindow_lock(nativeWindow, &outBuffer, NULL);
            // 填充
            avpicture_fill(
                    (AVPicture *) rgb_frame, outBuffer.bits, AV_PIX_FMT_ARGB, pCodecCtx->width,
                    pCodecCtx->height
            );
            // YUV ->RGBA_8888
            I420ToARGB(yuv_frame->data[0], yuv_frame->linesize[0],
                       yuv_frame->data[2], yuv_frame->linesize[2],
                       yuv_frame->data[1], yuv_frame->linesize[1],
                       rgb_frame->data[0], rgb_frame->linesize[0],
                       pCodecCtx->width, pCodecCtx->height);
            // unlcok
            ANativeWindow_unlockAndPost(nativeWindow);
            usleep(1000 * 16);
        }

        av_free_packet(packet);
    }

    ANativeWindow_release(nativeWindow);
    av_frame_free(&yuv_frame);
    avcodec_close(pCodecCtx);
    avformat_free_context(pFormatCtx);

    (*env)->ReleaseStringUTFChars(env, input_jstr, input_cstr);
}