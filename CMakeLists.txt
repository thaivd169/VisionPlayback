#----------------------------------------------------------------------------------
# THE ROOT CMAKE FILE FOR VISIONPLAYBACK
#----------------------------------------------------------------------------------
if(" ${CMAKE_SOURCE_DIR}" STREQUAL " ${CMAKE_BINARY_DIR}")
  message(FATAL_ERROR "FATAL: In-source builds are not allowed. You should create a separate directory for build files.")
endif()


# Setup CMake properties
# Read VERSION with CMake's own file API (cross-platform; no `cat` dependency,
# which is absent on a stock Windows shell).
file(READ "${CMAKE_SOURCE_DIR}/VERSION" VISION_PLAYBACK_VERSION)
string(STRIP "${VISION_PLAYBACK_VERSION}" VISION_PLAYBACK_VERSION)
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

# HCNetSDK root. On Windows the SDK is checked out next to the repo at
# ../HCNetSDK; on Linux it lives under ../3rdparty/HCNetSDK. Overridable.
if(WIN32)
  set(HCNETSDK_ROOT "${CMAKE_SOURCE_DIR}/../HCNetSDK" CACHE PATH "HCNetSDK root")
else()
  set(HCNETSDK_ROOT "${TD_3RDPARTY_PATH}/HCNetSDK" CACHE PATH "HCNetSDK root")
endif()

# Windows release packaging: folder holding ffmpeg.exe + ffprobe.exe to bundle
# into the install tree. Empty = don't bundle (ffmpeg must then be on PATH).
set(FFMPEG_DIR "" CACHE PATH "Folder with ffmpeg.exe + ffprobe.exe to bundle (Windows release)")

# Setup Qt framework
if(WIN32)
  list(APPEND CMAKE_PREFIX_PATH "C:/Qt/6.11.0/mingw_64/lib/cmake/Qt6")
else()
  list(APPEND CMAKE_PREFIX_PATH "$ENV{HOME}/Qt/6.11.0/gcc_64/lib/cmake/Qt6")
endif()
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
if(WIN32)
  # Windows uses a FLAT layout: the exe and every DLL live in one directory
  # (the loader searches the exe's own folder first). No bin/lib split.
  install(TARGETS VisionPlayback RUNTIME DESTINATION .)

  # HCNetSDK runtime: every *.dll from the shipped lib dir. install(DIRECTORY)
  # recurses, so the dlopened HCNetSDKCom/ plugin subdir is preserved beside the
  # core DLLs (the SDK loads those components lazily by name). Installed before
  # windeployqt so the SDK DLLs are already present when it scans the exe.
  install(DIRECTORY "${HCNETSDK_ROOT}/lib/"
          DESTINATION .
          FILES_MATCHING
          PATTERN "*.dll")

  # Qt6 DLLs + plugin subdirs (platforms/, tls/, …) + the MinGW runtime
  # (libstdc++/libgcc/libwinpthread), deployed FLAT next to the exe by calling
  # windeployqt directly — its native layout matches our flat tree (unlike Qt's
  # CMake deploy script, which forces a nested bin/+plugins/ split). PATH is
  # seeded with the Qt and MinGW bin dirs (derived from the Qt qmake target and
  # the compiler path) so windeployqt loads and can locate the compiler runtime.
  get_target_property(_qmake_exe Qt${QT_VERSION_MAJOR}::qmake IMPORTED_LOCATION)
  get_filename_component(_qt_bin_dir "${_qmake_exe}" DIRECTORY)
  find_program(WINDEPLOYQT_EXE windeployqt HINTS "${_qt_bin_dir}" REQUIRED)
  get_filename_component(_mingw_bin_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
  install(CODE "
    set(ENV{PATH} \"${_qt_bin_dir};${_mingw_bin_dir};\$ENV{PATH}\")
    execute_process(
      COMMAND \"${WINDEPLOYQT_EXE}\" --no-translations --compiler-runtime
              \"\${CMAKE_INSTALL_PREFIX}/VisionPlayback.exe\"
      RESULT_VARIABLE _wdq_result)
    if(NOT _wdq_result EQUAL 0)
      message(FATAL_ERROR \"windeployqt failed: \${_wdq_result}\")
    endif()
  ")

  # Bundle ffmpeg/ffprobe so target machines need nothing external.
  if(FFMPEG_DIR)
    install(PROGRAMS "${FFMPEG_DIR}/ffmpeg.exe" "${FFMPEG_DIR}/ffprobe.exe"
            DESTINATION .)
  endif()

  # Launcher: prepends the install folder to PATH so the bundled ffmpeg/ffprobe
  # (spawned via QProcess) and all DLLs resolve regardless of the working dir.
  install(PROGRAMS "${CMAKE_SOURCE_DIR}/scripts/win/VisionPlayback.bat"
          DESTINATION .)
else()
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
endif()
