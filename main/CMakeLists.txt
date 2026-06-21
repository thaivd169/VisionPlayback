#----------------------------------------------------------------------------------
# VisionPlayback executable — composition root.
# main.cpp is intentionally tiny; Session owns the entire object graph and is
# the one place that knows both vp_adapter and vp_infra.
#----------------------------------------------------------------------------------
qt_add_executable(VisionPlayback
    main.cpp
    Session.h  Session.cpp
)
target_link_libraries(VisionPlayback PRIVATE
    vp_adapter
    vp_infra
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::HttpServer
)
if(WIN32)
  # RPATH is a no-op on Windows. Copy the HCNetSDK runtime DLLs (core +
  # HCNetSDKCom/ plugin subdir) next to the exe so in-tree dev runs resolve
  # them from the exe's own folder. (Qt + MinGW DLLs are deployed only in the
  # install/release tree — see the root CMakeLists install block.)
  add_custom_command(TARGET VisionPlayback POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${HCNETSDK_ROOT}/lib" "$<TARGET_FILE_DIR:VisionPlayback>"
    COMMENT "Copying HCNetSDK runtime DLLs next to VisionPlayback.exe")
  if(FFMPEG_DIR)
    add_custom_command(TARGET VisionPlayback POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
              "${FFMPEG_DIR}/ffmpeg.exe" "${FFMPEG_DIR}/ffprobe.exe"
              "$<TARGET_FILE_DIR:VisionPlayback>"
      COMMENT "Copying bundled ffmpeg/ffprobe next to VisionPlayback.exe")
  endif()
else()
  set_target_properties(VisionPlayback PROPERTIES
      # In-tree runs resolve HCNetSDK from the sibling 3rdparty checkout.
      BUILD_RPATH   "${HCNETSDK_ROOT}/lib"
      # The install tree is relocatable: both Qt and HCNetSDK libs are bundled
      # under <prefix>/lib, found relative to the binary in <prefix>/bin.
      INSTALL_RPATH "$ORIGIN/../lib"
  )
endif()
