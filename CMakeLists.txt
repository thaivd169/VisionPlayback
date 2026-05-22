#----------------------------------------------------------------------------------
# THE ROOT CMAKE FILE FOR CORE_IPS APPLICATION
#----------------------------------------------------------------------------------
if(" ${CMAKE_SOURCE_DIR}" STREQUAL " ${CMAKE_BINARY_DIR}")
  message(FATAL_ERROR "FATAL: In-source builds are not allowed. You should create a separate directory for build files.")
endif()

cmake_minimum_required(VERSION 3.16)

project(visionPlayback
        VERSION 1.0.0
        LANGUAGES CXX
        DESCRIPTION "A backend service applying computer vision \
                    for auditing purposes to warehouse management.")

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-strict-aliasing -g")
set(TD_3RDPARTY_PATH "${CMAKE_SOURCE_DIR}/../3rdparty")

# Setup Qt framework
list(APPEND CMAKE_PREFIX_PATH "$ENV{HOME}/Qt/6.11.0/gcc_64/lib/cmake/Qt6")
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS
    Core Widgets Network Concurrent Multimedia MultimediaWidgets)
message ("[${CMAKE_PROJECT_NAME}] [Qt] version   :   ${QT_VERSION_MAJOR}.${QT_VERSION_MINOR}.${QT_VERSION_PATCH}")
qt_standard_project_setup()

# Setup HCNetSDK library
include_directories(${TD_3RDPARTY_PATH}/HCNetSDK/incEn)
link_directories(${TD_3RDPARTY_PATH}/HCNetSDK/lib)

qt_add_executable(VisionPlayback
    main.cpp
    LoginInfo.h
    LoginDialog.h       LoginDialog.cpp
    PlaybackRequest.h
    PlaybackDialog.h    PlaybackDialog.cpp
    DownloadWorker.h    DownloadWorker.cpp
    VideoPlayerWidget.h VideoPlayerWidget.cpp
    MainWindow.h        MainWindow.cpp
    PlaybackManagerDialog.h  PlaybackManagerDialog.cpp
)


target_link_libraries(VisionPlayback PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::Concurrent
    Qt${QT_VERSION_MAJOR}::Multimedia
    Qt${QT_VERSION_MAJOR}::MultimediaWidgets
    hcnetsdk
)

set_target_properties(VisionPlayback PROPERTIES
    BUILD_RPATH  "${TD_3RDPARTY_PATH}/HCNetSDK/lib"
    INSTALL_RPATH "${TD_3RDPARTY_PATH}/HCNetSDK/lib"
)
