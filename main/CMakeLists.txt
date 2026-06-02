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
set_target_properties(VisionPlayback PROPERTIES
    # In-tree runs resolve HCNetSDK from the sibling 3rdparty checkout.
    BUILD_RPATH   "${TD_3RDPARTY_PATH}/HCNetSDK/lib"
    # The install tree is relocatable: both Qt and HCNetSDK libs are bundled
    # under <prefix>/lib, found relative to the binary in <prefix>/bin.
    INSTALL_RPATH "$ORIGIN/../lib"
)
