#----------------------------------------------------------------------------------
# vp_infra — technical foundations, no business meaning.
# Owns RAII wrappers around third-party C APIs (HCNetSDK init/cleanup) and
# small standalone utilities (SHA-256). The HCNetSDK link is via an absolute
# path so consumers don't need to learn about -L flags.
#----------------------------------------------------------------------------------
add_library(vp_infra STATIC
    HCNetSDKBootstrap.h  HCNetSDKBootstrap.cpp
    Sha256.h             Sha256.cpp
)
target_include_directories(vp_infra
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE ${TD_3RDPARTY_PATH}/HCNetSDK/incEn
)
target_link_libraries(vp_infra
    PRIVATE ${TD_3RDPARTY_PATH}/HCNetSDK/lib/libhcnetsdk.so
)
target_compile_features(vp_infra PUBLIC cxx_std_20)
