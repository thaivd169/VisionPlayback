#----------------------------------------------------------------------------------
# vp_adapter — implements ports + speaks to the outside world (HTTP, HCNetSDK,
# ffmpeg, filesystem, console). Single static lib aggregating all adapter
# subfolders.
#
# HCNetSDK include path is PRIVATE; the library is linked via absolute path
# so consumers (main) don't need to inherit `-L` flags.
#----------------------------------------------------------------------------------
qt_add_library(vp_adapter STATIC
    http/ControlApi.h               http/ControlApi.cpp
    http/PollingApi.h               http/PollingApi.cpp
    http/DashFileServer.h           http/DashFileServer.cpp
    http/ApiKeyGuard.h              http/ApiKeyGuard.cpp
    http/JsonCodec.h                http/JsonCodec.cpp
    console/ConsoleEventLogger.h    console/ConsoleEventLogger.cpp
    dvr/HCNetSDKDownloader.h        dvr/HCNetSDKDownloader.cpp
    dvr/HCNetSDKAuthenticator.h     dvr/HCNetSDKAuthenticator.cpp
    dvr/HCNetSDKTimeMapper.h
    packaging/FfmpegDashPackager.h  packaging/FfmpegDashPackager.cpp
    persistence/FileSystemStreamCache.h  persistence/FileSystemStreamCache.cpp
)
target_include_directories(vp_adapter
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/http
            ${CMAKE_CURRENT_SOURCE_DIR}/console
            ${CMAKE_CURRENT_SOURCE_DIR}/dvr
            ${CMAKE_CURRENT_SOURCE_DIR}/packaging
            ${CMAKE_CURRENT_SOURCE_DIR}/persistence
    PRIVATE ${TD_3RDPARTY_PATH}/HCNetSDK/incEn
)
target_link_libraries(vp_adapter
    PUBLIC  vp_usecase vp_domain
            Qt${QT_VERSION_MAJOR}::Core
            Qt${QT_VERSION_MAJOR}::Network
            Qt${QT_VERSION_MAJOR}::HttpServer
    PRIVATE vp_infra
            ${TD_3RDPARTY_PATH}/HCNetSDK/lib/libhcnetsdk.so
)
