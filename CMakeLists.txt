#----------------------------------------------------------------------------------
# THE ROOT CMAKE FILE FOR VISIONPLAYBACK
#----------------------------------------------------------------------------------
if(" ${CMAKE_SOURCE_DIR}" STREQUAL " ${CMAKE_BINARY_DIR}")
  message(FATAL_ERROR "FATAL: In-source builds are not allowed. You should create a separate directory for build files.")
endif()


# Setup CMake properties
execute_process(
  COMMAND cat ./VERSION
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE VISION_PLAYBACK_VERSION
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_QUIET
)
execute_process(
  COMMAND git rev-parse --short HEAD
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE GIT_COMMIT_HASH
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_QUIET
)

cmake_minimum_required(VERSION 3.16)

project(visionPlayback
        VERSION ${VISION_PLAYBACK_VERSION}
        LANGUAGES CXX
        DESCRIPTION "A headless HTTP daemon serving DASH playback streams \
                    pulled from Hikvision HCNetSDK cameras.")

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-strict-aliasing -g")
add_compile_definitions(VISION_PLAYBACK_VERSION_STRING="${VISION_PLAYBACK_VERSION}-${GIT_COMMIT_HASH}")
set(TD_3RDPARTY_PATH "${CMAKE_SOURCE_DIR}/../3rdparty")

# Setup Qt framework
list(APPEND CMAKE_PREFIX_PATH "$ENV{HOME}/Qt/6.11.0/gcc_64/lib/cmake/Qt6")
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS
    Core Network HttpServer)
message("[${CMAKE_PROJECT_NAME}] [Qt] version   :   ${QT_VERSION_MAJOR}.${QT_VERSION_MINOR}.${QT_VERSION_PATCH}")
qt_standard_project_setup()


add_subdirectory(src/domain)
add_subdirectory(src/infra)
add_subdirectory(src/usecase)
add_subdirectory(src/adapter)
add_subdirectory(main)

#----------------------------------------------------------------------------------
# Install / deploy — produce a self-contained, relocatable tree under
# CMAKE_INSTALL_PREFIX (<prefix>/{bin,lib,plugins}). The binary's INSTALL_RPATH
# is $ORIGIN/../lib (set in main/CMakeLists.txt), so it finds both the Qt
# runtime and HCNetSDK without LD_LIBRARY_PATH. ffmpeg stays an external
# dependency (invoked via QProcess on PATH) — not bundled.
#----------------------------------------------------------------------------------
install(TARGETS VisionPlayback RUNTIME DESTINATION bin)

# Qt runtime: libraries → lib, plugins → plugins, plus a generated bin/qt.conf.
# POST_EXCLUDE_REGEXES keeps Qt's runtime-dependency scan from copying a partial
# HCNetSDK set (it would only see the linked libhcnetsdk.so, not the dlopened
# core/plugin libs) — we install the full SDK ourselves below.
qt_generate_deploy_app_script(
    TARGET VisionPlayback
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
    POST_EXCLUDE_REGEXES "3rdparty/HCNetSDK"
)
install(SCRIPT ${deploy_script})

# HCNetSDK runtime: copy the whole shipped lib dir (~26 MB). The SDK dlopens
# most components lazily by name, so the dependency graph is undocumented —
# copying everything is the safe, zero-maintenance default. install(DIRECTORY)
# recurses, so the HCNetSDKCom/ plugin subdir is preserved next to the core libs.
install(DIRECTORY "${TD_3RDPARTY_PATH}/HCNetSDK/lib/"
        DESTINATION lib
        FILES_MATCHING
        PATTERN "*.so*"
        PATTERN "HCNetSDK_Log_Switch.xml")
