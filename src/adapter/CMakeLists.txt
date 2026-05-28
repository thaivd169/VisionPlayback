#----------------------------------------------------------------------------------
# vp_adapter — primary-adapter layer. HTTP API + console event logger.
# Drives the use cases; does NOT touch HCNetSDK / ffmpeg / the filesystem
# (those are infra concerns).
#
# Direct dep is vp_usecase only — domain types reach this code transitively
# through vp_usecase's PUBLIC link. Qt::Network/HttpServer is PRIVATE since
# only the route classes need them. Audit invariant:
#     grep -rE 'NET_DVR_|HCNetSDK' src/adapter/
# must return empty.
#----------------------------------------------------------------------------------
qt_add_library(vp_adapter STATIC
    http/ControlApi.h               http/ControlApi.cpp
    http/PollingApi.h               http/PollingApi.cpp
    http/DashFileServer.h           http/DashFileServer.cpp
    http/ApiKeyGuard.h              http/ApiKeyGuard.cpp
    http/JsonCodec.h                http/JsonCodec.cpp
    console/ConsoleEventLogger.h    console/ConsoleEventLogger.cpp
)
target_include_directories(vp_adapter PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/http
    ${CMAKE_CURRENT_SOURCE_DIR}/console
)
target_link_libraries(vp_adapter
    PUBLIC  vp_usecase
            # Qt headers leak into adapter's public headers (QObject in
            # ControlApi/PollingApi/DashFileServer, QHttpServer in the route
            # registrars, QByteArray in ApiKeyGuard), so these must be PUBLIC.
            Qt${QT_VERSION_MAJOR}::Core
            Qt${QT_VERSION_MAJOR}::Network
            Qt${QT_VERSION_MAJOR}::HttpServer
)
