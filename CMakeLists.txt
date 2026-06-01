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

install(TARGETS VisionPlayback RUNTIME DESTINATION bin)
