#----------------------------------------------------------------------------------
# vp_domain — Qt-FREE, SDK-FREE pure C++ static library.
# Holds value objects and port interfaces. Audit invariant:
#     grep -rE 'Q[A-Z]|NET_DVR_|HCNetSDK' src/domain/
# must return empty.
#----------------------------------------------------------------------------------
add_library(vp_domain STATIC
    IHasher.h
    IAuthenticator.h
    IPlaybackDownloader.h
    IDashPackager.h
    IStreamCacheRepository.h
    IDispatcher.h
)
target_include_directories(vp_domain PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(vp_domain PUBLIC cxx_std_20)
