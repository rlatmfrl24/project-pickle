cmake_minimum_required(VERSION 3.16)

project(pickle VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Quick Multimedia Sql Concurrent QuickDialogs2 Test)

include(GNUInstallDirs)

qt_standard_project_setup(REQUIRES 6.5)

if(QT_KNOWN_POLICY_QTP0004)
    qt_policy(SET QTP0004 NEW)
endif()

set_source_files_properties(qml/Main.qml PROPERTIES QT_RESOURCE_ALIAS Main.qml)
set_source_files_properties(qml/components/InfoPanel.qml PROPERTIES QT_RESOURCE_ALIAS components/InfoPanel.qml)
set_source_files_properties(qml/components/LibraryPanel.qml PROPERTIES QT_RESOURCE_ALIAS components/LibraryPanel.qml)
set_source_files_properties(qml/components/PlaybackBar.qml PROPERTIES QT_RESOURCE_ALIAS components/PlaybackBar.qml)
set_source_files_properties(qml/pages/PlayerPage.qml PROPERTIES QT_RESOURCE_ALIAS pages/PlayerPage.qml)

qt_add_executable(apppickle
    src/main.cpp
    src/app/AppController.cpp
    src/app/AppController.h
    src/db/DatabaseService.cpp
    src/db/DatabaseService.h
    src/db/MediaRepository.cpp
    src/db/MediaRepository.h
    src/db/MigrationService.cpp
    src/db/MigrationService.h
    src/media/MediaTypes.h
    src/media/MediaLibraryModel.cpp
    src/media/MediaLibraryModel.h
    src/media/PlaybackController.cpp
    src/media/PlaybackController.h
    src/media/ScanService.cpp
    src/media/ScanService.h
)

qt_add_qml_module(apppickle
    URI pickle
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/components/InfoPanel.qml
        qml/components/LibraryPanel.qml
        qml/components/PlaybackBar.qml
        qml/pages/PlayerPage.qml
)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
set_target_properties(apppickle PROPERTIES
#    MACOSX_BUNDLE_GUI_IDENTIFIER com.example.apppickle
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(apppickle
    PRIVATE
        Qt6::Quick
        Qt6::Multimedia
        Qt6::Sql
        Qt6::Concurrent
        Qt6::QuickDialogs2
)

target_include_directories(apppickle
    PRIVATE src
)

enable_testing()

qt_add_executable(m03_scan_repository_test
    tests/M03ScanRepositoryTest.cpp
    src/db/MediaRepository.cpp
    src/db/MediaRepository.h
    src/db/MigrationService.cpp
    src/db/MigrationService.h
    src/media/MediaTypes.h
    src/media/ScanService.cpp
    src/media/ScanService.h
)

target_link_libraries(m03_scan_repository_test
    PRIVATE
        Qt6::Core
        Qt6::Sql
        Qt6::Test
)

target_include_directories(m03_scan_repository_test
    PRIVATE src
)

add_test(NAME m03_scan_repository_test COMMAND m03_scan_repository_test)

qt_add_executable(m04_library_query_test
    tests/M04LibraryQueryTest.cpp
    src/app/AppController.cpp
    src/app/AppController.h
    src/db/MediaRepository.cpp
    src/db/MediaRepository.h
    src/db/MigrationService.cpp
    src/db/MigrationService.h
    src/media/MediaLibraryModel.cpp
    src/media/MediaLibraryModel.h
    src/media/MediaTypes.h
    src/media/PlaybackController.cpp
    src/media/PlaybackController.h
    src/media/ScanService.cpp
    src/media/ScanService.h
)

target_link_libraries(m04_library_query_test
    PRIVATE
        Qt6::Core
        Qt6::Sql
        Qt6::Concurrent
        Qt6::Test
)

target_include_directories(m04_library_query_test
    PRIVATE src
)

add_test(NAME m04_library_query_test COMMAND m04_library_query_test)

install(TARGETS apppickle
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
