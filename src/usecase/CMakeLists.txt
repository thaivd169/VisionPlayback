#----------------------------------------------------------------------------------
# vp_usecase — pipeline interactors + port interfaces + login cache.
# Links Qt::Core only (for QObject / signals / QTimer). Audit invariant:
#     grep -rE 'NET_DVR_|HCNetSDK|hcnetsdk' src/usecase/
# must return empty (comments aside).
#----------------------------------------------------------------------------------
qt_add_library(vp_usecase STATIC
    StreamPlaybackUseCase.h   StreamPlaybackUseCase.cpp
    LoginUseCase.h            LoginUseCase.cpp
    PlaybackKeyFactory.h      PlaybackKeyFactory.cpp
    ports/IPlaybackDownloader.h
    ports/IDashPackager.h
    ports/IStreamCacheRepository.h
    ports/IAuthenticator.h
)
target_include_directories(vp_usecase PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/ports
)
target_link_libraries(vp_usecase
    PUBLIC  vp_domain
            Qt${QT_VERSION_MAJOR}::Core
    PRIVATE vp_infra            # only PlaybackKeyFactory.cpp needs Sha256
)
