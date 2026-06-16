cmake_minimum_required(VERSION 3.16)

project(20260530 VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#==========================================================
# Qt6 依赖组件
#   Widgets   : GUI 控件(QMainWindow / QPushButton 等)
#   Network   : WebRTC / WebSocket 等网络通信封装(QTcpSocket 等)
#   Multimedia: 音视频采集/播放(QAudioSource / QAudioOutput /
#               QMediaDevices / QMediaCaptureSession 等)
#               本项目用其实现麦克风采集和扬声器播放
#==========================================================
find_package(Qt6 REQUIRED COMPONENTS Widgets Network Multimedia)

set(OpenCV_DIR "E:/Code/Dependency/opencv/install/x64/mingw/lib")
find_package(OpenCV REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})

set(OPENSSL_ROOT_DIR "E:/Code/Dependency/openssl")
set(OPENSSL_USE_STATIC_LIBS FALSE)
find_package(OpenSSL REQUIRED)
if(MINGW)
    add_compile_options(-Wno-error=format -Wno-format)
endif()

set(NO_EXAMPLES ON CACHE BOOL "" FORCE)
set(NO_TESTS ON CACHE BOOL "" FORCE)
add_subdirectory(thirdparty/libdatachannel)

#==========================================================
# 链接 opus 静态库(已编译到 E:/Code/Dependency/opus)
#   opus 是 WebRTC 强制要求的语音编解码器,体积小、延迟低、
#   适合实时音视频通话。本项目把麦克风采集到的 PCM 数据
#   用 opus 编码后通过 DataChannel 发送
#----------------------------------------------------------
# 1. 指定头文件/库文件路径
# 2. 把 .a 当作 STATIC IMPORTED 目标手动声明
# 3. 设置 IMPORTED_LOCATION 指向具体 .a 文件,这样
#    后面 target_link_libraries(... opus) 就能直接链接
#==========================================================
set(OPUS_ROOT_DIR "E:/Code/Dependency/opus")
set(OPUS_INCLUDE_DIR "${OPUS_ROOT_DIR}/include")
set(OPUS_LIB_DIR "${OPUS_ROOT_DIR}/lib")
include_directories(${OPUS_INCLUDE_DIR})
link_directories(${OPUS_LIB_DIR})
#opus 是纯 C 库,这里通过 add_library 手动声明即可
add_library(opus STATIC IMPORTED)
set_target_properties(opus PROPERTIES IMPORTED_LOCATION "${OPUS_LIB_DIR}/libopus.a")

set(PROJECT_SOURCES
    main.cpp
    mainwindow_worker.cpp
    mainwindow_frontend.cpp
    mainwindow_uifunc.cpp
    mainwindow.h
    mainwindow.ui
    videochatwindow.h
    videochatwindow.cpp
    audiochatwindow.h
    audiochatwindow.cpp
)

qt_add_executable(20260530 WIN32 MANUAL_FINALIZATION ${PROJECT_SOURCES}
    dcworker.h
    wintun.h
    tunloader.h
    tunmanager.h
    dcmanager.h
    coornetworker.h
    peernetworker.h
    tuninworker.h
    tunoutworker.h
    filesender.h
    filedownloader.h
    videoencoder.h
    videodecoder.h
    audiocapture.h
    audioencoder.h
    audiodecoder.h
    audioplayer.h
    peerjsonworker.h
    coorjsonworker.h
    basejsonworker.h
    jsonloader.h
    netconfig.h
    settingsdialog.h
    startupdialog.h
    request.h
    logger.h
)

target_link_libraries(20260530
    PRIVATE
        Qt6::Widgets
        Qt6::Network
        Qt6::Multimedia
        ${OpenCV_LIBS}
        datachannel
        opus
        ws2_32
        iphlpapi
        netapi32
        crypt32
        mswsock
        strmiids
        uuid
)
qt_finalize_executable(20260530)
