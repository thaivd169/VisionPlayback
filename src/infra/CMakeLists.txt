#----------------------------------------------------------------------------------
# vp_infra — secondary-adapter layer. Implements the domain port interfaces
# using HCNetSDK (DVR auth + downloader), ffmpeg via QProcess (DASH packager),
# the filesystem (stream cache), and Qt-based hashing.
#
# Also hosts technical foundations: HCNetSDKBootstrap (RAII init/cleanup) and
# Config (default constants).
#
# Depends on vp_domain (PUBLIC — port headers + value types) and Qt::Core
# (PRIVATE — QProcess, QFile). HCNetSDK is linked via absolute path so
# consumers don't inherit -L flags.
#----------------------------------------------------------------------------------
qt_add_library(vp_infra STATIC
    HCNetSDKBootstrap.h    HCNetSDKBootstrap.cpp
    Config.h
    dvr/HCNetSDKAuthenticator.h         dvr/HCNetSDKAuthenticator.cpp
    dvr/HCNetSDKDownloader.h            dvr/HCNetSDKDownloader.cpp
    dvr/HCNetSDKTimeMapper.h
    packaging/FfmpegDashPackager.h      packaging/FfmpegDashPackager.cpp
    packaging/FfmpegClipExporter.h      packaging/FfmpegClipExporter.cpp
    persistence/FileSystemStreamCache.h persistence/FileSystemStreamCache.cpp
    hashing/QtHasher.h                  hashing/QtHasher.cpp
)
target_include_directories(vp_infra
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/dvr
            ${CMAKE_CURRENT_SOURCE_DIR}/packaging
            ${CMAKE_CURRENT_SOURCE_DIR}/persistence
            ${CMAKE_CURRENT_SOURCE_DIR}/hashing
    PRIVATE ${TD_3RDPARTY_PATH}/HCNetSDK/incEn
)
target_link_libraries(vp_infra
    PUBLIC  vp_domain
            Qt${QT_VERSION_MAJOR}::Core
    PRIVATE ${TD_3RDPARTY_PATH}/HCNetSDK/lib/libhcnetsdk.so
)
target_compile_features(vp_infra PUBLIC cxx_std_20)
