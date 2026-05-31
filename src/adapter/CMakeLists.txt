#----------------------------------------------------------------------------------
# vp_adapter — primary-adapter layer. HTTP API and the PlaybackProcessor that
# bridges HTTP and the pipeline on a dedicated thread.
#
# Depends on vp_usecase (PUBLIC — domain types reach this code transitively) and
# vp_infra (PRIVATE). The infra link exists because PlaybackProcessor.cpp builds
# its own collaborators (HCNetSDKAuthenticator, HCNetSDKDownloaderFactory,
# FfmpegDashPackagerFactory); those concrete types are named only in that .cpp,
# never in a public adapter header. The old invariant
#     grep -rE 'NET_DVR_|HCNetSDK' src/adapter/   (must be empty)
# is intentionally relaxed — it now matches PlaybackProcessor.cpp only.
#----------------------------------------------------------------------------------
qt_add_library(vp_adapter STATIC
    http/HttpListener.h             http/HttpListener.cpp
    processor/PlaybackProcessor.h   processor/PlaybackProcessor.cpp
)
target_include_directories(vp_adapter PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/http
    ${CMAKE_CURRENT_SOURCE_DIR}/processor
)
target_link_libraries(vp_adapter
    PUBLIC  vp_usecase
            # Qt headers leak into adapter's public headers (QObject +
            # QHttpServer in HttpListener and PlaybackProcessor), so these
            # must be PUBLIC.
            Qt${QT_VERSION_MAJOR}::Core
            Qt${QT_VERSION_MAJOR}::Network
            Qt${QT_VERSION_MAJOR}::HttpServer
    # Infra concrete impls are instantiated inside PlaybackProcessor.cpp only.
    PRIVATE vp_infra
)
