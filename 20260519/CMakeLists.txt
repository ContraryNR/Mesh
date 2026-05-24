cmake_minimum_required(VERSION 3.16)

project(20260519 VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)

set(CMAKE_AUTOMOC ON)

set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network)

set(OPENSSL_ROOT_DIR "C:/msys64/mingw64")
set(OPENSSL_USE_STATIC_LIBS TRUE)
set(CMAKE_PREFIX_PATH "C:/msys64/mingw64" ${CMAKE_PREFIX_PATH})
if(MINGW)
    add_compile_options(-Wno-error=format -Wno-format)
endif()

set(NO_EXAMPLES ON CACHE BOOL "" FORCE)
set(NO_TESTS ON CACHE BOOL "" FORCE)
add_subdirectory(thirdparty/libdatachannel)

set(PROJECT_SOURCES
    main.cpp
    mainwindow_worker.cpp
    mainwindow_frontend.cpp
    mainwindow_uifunc.cpp
    mainwindow.h
    mainwindow.ui
)

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
    filesender.h
    filereceiver.h
)

target_link_libraries(20260519
    PRIVATE
        Qt6::Widgets
        Qt6::Network
        datachannel
        ws2_32
        iphlpapi
        netapi32
        crypt32
        mswsock
        strmiids
        uuid

)
qt_finalize_executable(20260519)
