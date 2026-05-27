#----------------------------------------------------------------------------------
# VisionPlayback executable — composition root.
# main.cpp is intentionally tiny; Session owns the entire object graph.
#----------------------------------------------------------------------------------
qt_add_executable(VisionPlayback
    main.cpp
    Session.h  Session.cpp
)
target_link_libraries(VisionPlayback PRIVATE vp_adapter vp_infra)
set_target_properties(VisionPlayback PROPERTIES
    BUILD_RPATH   "${TD_3RDPARTY_PATH}/HCNetSDK/lib"
    INSTALL_RPATH "${TD_3RDPARTY_PATH}/HCNetSDK/lib"
)
