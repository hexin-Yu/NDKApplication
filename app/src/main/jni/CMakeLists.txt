
cmake_minimum_required(VERSION 3.4.1)

## 每次生成os时不会删除之前的，所以需要主动删除处理
##set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/../jniLibs/${ANDROID_ABI})

add_library( # Specifies the name of the library.
    # 这里是你so的名字。刚才在 MainActivity里面要引用的
    jni_lib

    # Sets the library as a shared library.
    SHARED

    # Provides a relative path to your source file(s).
    #这里是刚才 创建的c++ 代码的名字
        Java_com_example_ndkapplication.h
    c_06_jni.c)

find_library( # Sets the name of the path variable.
    log-lib

    # Specifies the name of the NDK library that
    # you want CMake to locate.
    log )

target_link_libraries( # Specifies the target library.
    # 这里是你so的名字。刚才在 MainActivity里面要引用的 一样。
    jni_lib

# Links the target library to the log library
    # included in the NDK.
    ${log-lib} )