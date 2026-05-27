#----------------------------------------------------------------------------------
# vp_domain — Qt-FREE, SDK-FREE header-only INTERFACE library.
# Pure value objects + enums. Audit invariant:
#     grep -rE 'Q[A-Z]|NET_DVR_|HCNetSDK' src/domain/
# must return empty.
#----------------------------------------------------------------------------------
add_library(vp_domain INTERFACE)
target_include_directories(vp_domain INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(vp_domain INTERFACE cxx_std_20)
