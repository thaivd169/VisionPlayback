#----------------------------------------------------------------------------------
# vp_usecase — Qt-FREE pipeline interactors + login cache.
# Pure C++; depends only on vp_domain. Audit invariant:
#     grep -rE 'Q[A-Z]|NET_DVR_|HCNetSDK' src/usecase/
# must return empty.
#----------------------------------------------------------------------------------
add_library(vp_usecase STATIC
    StreamPlaybackUseCase.h   StreamPlaybackUseCase.cpp
    LoginUseCase.h            LoginUseCase.cpp
)
target_include_directories(vp_usecase PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(vp_usecase PUBLIC vp_domain)
target_compile_features(vp_usecase PUBLIC cxx_std_20)
