#include "Java_com_example_ndkapplication.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <android/log.h>


#define LOGI(FORMAT,...) __android_log_print(ANDROID_LOG_INFO, "jni", FORMAT, __VA_ARGS__)
#define LOGE(FORMAT,...) __android_log_print(ANDROID_LOG_ERROR, "jni", FORMAT, __VA_ARGS__)
#define LOGD(FORMAT,...) __android_log_print(ANDROID_LOG_DEBUG, "jni", FORMAT, __VA_ARGS__)

char *chars = "string from c";

// 这个生成的是静态的方法
// 访问静态变量
JNIEXPORT jstring JNICALL
Java_com_example_ndkapplication_MainActivity_getStringFromC(JNIEnv *env, jclass jclazz) {
    return (*env)->NewStringUTF(env, chars);
};


JNIEXPORT void JNICALL
Java_com_example_ndkapplication_MainActivity_path(JNIEnv *env, jclass jclazz, jstring path_jstr,
                                                  jint count) {


};


JNIEXPORT void JNICALL Java_com_example_ndkapplication_MainActivity_diff
        (JNIEnv *env, jclass jclazz, jstring path_jstr, jstring path_pattern_jstr, jint file_num) {

    const char *path = (*env)->GetStringUTFChars(env, path_jstr, NULL);
    LOGI("path : %s", path);
    const char *path_pattern = (*env)->GetStringUTFChars(env, path_pattern_jstr, NULL);
    LOGI("path_pattern : %s", path_pattern);
    char **paths = malloc(sizeof(char *) * file_num);
    int i = 0;
    for (; i < file_num; i++) {
        paths[i] = malloc(sizeof(char) * 100);
        //(char *string, char *farmat [,argument,...]);
        sprintf(paths[i], path_pattern, (i + 1));
//        paths[i] = strcat(path_pattern_jstr,file_num);
//        __android_log_print(ANDROID_LOG_INFO, "jni", "path : ", paths[i]);
        LOGI("split path : %s", paths[i]);
    }
    // 释放
    i = 0;
    for (; i < file_num; i++) {
        free(paths[i]);
    }

    (*env)->ReleaseStringUTFChars(env, path_jstr, path);
    (*env)->ReleaseStringUTFChars(env, path_jstr, path_pattern);

};



























