cmake_minimum_required(VERSION 3.21)
project(RecordRoom VERSION 0.1.0 LANGUAGES CXX)

find_package(Qt6 6.9 REQUIRED COMPONENTS Quick QuickControls2 Network Multimedia Qml DBus)
find_package(Qt6 6.9 OPTIONAL_COMPONENTS ShaderTools)
find_package(OpenSSL REQUIRED)
qt_standard_project_setup(REQUIRES 6.9)


qt_add_executable(record-room
    src/main.cpp
    src/Library.cpp
    src/Library.h
    src/AudioCache.cpp
    src/AudioCache.h
    src/AudioStream.cpp
    src/AudioStream.h
    src/ResponseStore.cpp
    src/ResponseStore.h
    src/TrackModel.cpp
    src/TrackModel.h
    src/NeteaseApi.cpp
    src/NeteaseApi.h
    src/NeteaseCrypto.cpp
    src/NeteaseCrypto.h
    src/QrCode.cpp
    src/QrCode.h
    src/AntiCheat.cpp
    src/AntiCheat.h
    src/Mpris.cpp
    src/Mpris.h
)
target_include_directories(record-room PRIVATE src)
target_compile_features(record-room PRIVATE cxx_std_17)
target_link_libraries(record-room PRIVATE
    Qt6::Quick Qt6::QuickControls2 Qt6::Network Qt6::Multimedia Qt6::DBus OpenSSL::Crypto
)

qt_add_resources(record-room dun
    PREFIX "/dun"
    FILES assets/dun/browser-shim.js
)
qt_add_resources(record-room branding
    PREFIX "/branding"
    FILES assets/netease-music.png assets/netease-logo.png
        assets/icons/app-16.png assets/icons/app-32.png assets/icons/app-48.png
        assets/icons/app-64.png assets/icons/app-128.png
)
set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
qt_add_qml_module(record-room
    URI RecordRoom
    QML_FILES
        qml/Main.qml
        qml/WindowResize.qml
        qml/Theme.qml
        qml/Symbol.qml
        qml/IconButton.qml
        qml/Cover.qml
        qml/StatusView.qml
        qml/DiscoverPage.qml
        qml/CollectionGrid.qml
        qml/RankingStrip.qml
        qml/HomeShelves.qml
        qml/TrackList.qml
        qml/PlayerBar.qml
        qml/DesktopLyrics.qml
        qml/NowPlayingPage.qml
        qml/Sidebar.qml
        qml/BrowsePage.qml
        qml/LoginDialog.qml
        qml/PillButton.qml
        qml/TabStrip.qml
        qml/SongTag.qml
        qml/SongShelf.qml
        qml/SectionTitle.qml
        qml/ChartTiles.qml
        qml/PlaylistPage.qml
        qml/SearchPage.qml
)
