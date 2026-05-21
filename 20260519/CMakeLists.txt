cmake_minimum_required(VERSION 3.16)

project(20260519 VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)

set(CMAKE_AUTOMOC ON)

set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# 1. 【最先】导入 Qt 模块

find_package(Qt6 REQUIRED COMPONENTS Widgets Network)

# 2. 【然后】配置 OpenSSL 和 编译器选项
set(OPENSSL_ROOT_DIR "C:/msys64/mingw64")
set(OPENSSL_USE_STATIC_LIBS TRUE)
set(CMAKE_PREFIX_PATH "C:/msys64/mingw64" ${CMAKE_PREFIX_PATH})
if(MINGW)
    add_compile_options(-Wno-error=format -Wno-format)
endif()

# 3. 【接着】添加子目录 (libdatachannel)
# 禁用它自带的测试和例子，防止它乱串门
set(NO_EXAMPLES ON CACHE BOOL "" FORCE)
set(NO_TESTS ON CACHE BOOL "" FORCE)
add_subdirectory(thirdparty/libdatachannel)

# 4. 【最后】定义你自己的可执行文件
set(PROJECT_SOURCES
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
)

#作用域隔离:把 qt_add_executable 放在 add_subdirectory 之后，确保你的项目是站在"前人的肩膀"上,而不是在打地基的时候就被子项目给拽倒了
qt_add_executable(20260519 WIN32 MANUAL_FINALIZATION ${PROJECT_SOURCES}
    dcworker.h


    wintun.h
    tunloader.h
    tunmanager.h

    dcmanager.h
    basenetworker.h
    servernetworker.h
    clientnetworker.h
    tuninworker.h
    tunoutworker.h

)

# 5. 【末尾】进行链接
target_link_libraries(20260519
    PRIVATE
        Qt6::Widgets
        Qt6::Network
        datachannel
        # --- 必须手动添加以下 Windows 系统库 ---
        ws2_32 # 解决 socket 相关的 undefined reference
        iphlpapi # 解决网络接口相关的 undefined reference
        netapi32 #确保 IP 注入功能在 MinGW 下能正常编译
        crypt32 # 解决加密相关的 undefined reference
        mswsock # 解决扩展套接字相关的 undefined reference
        strmiids # 解决部分多媒体接口引用
        uuid # 解决系统唯一标识符引用

)
qt_finalize_executable(20260519)
