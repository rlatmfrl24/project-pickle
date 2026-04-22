cmake_minimum_required(VERSION 3.16)

project(pickle VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Quick Multimedia Sql Concurrent QuickDialogs2 Test Gui)

include(GNUInstallDirs)

qt_standard_project_setup(REQUIRES 6.5)

if(QT_KNOWN_POLICY_QTP0004)
    qt_policy(SET QTP0004 NEW)
endif()

set_source_files_properties(qml/Main.qml PROPERTIES QT_RESOURCE_ALIAS Main.qml)
set_source_files_properties(qml/components/InfoPanel.qml PROPERTIES QT_RESOURCE_ALIAS components/InfoPanel.qml)
set_source_files_properties(qml/components/LibraryPanel.qml PROPERTIES QT_RESOURCE_ALIAS components/LibraryPanel.qml)
set_source_files_properties(qml/components/PlaybackBar.qml PROPERTIES QT_RESOURCE_ALIAS components/PlaybackBar.qml)
set_source_files_properties(qml/components/ReviewShortcutLayer.qml PROPERTIES QT_RESOURCE_ALIAS components/ReviewShortcutLayer.qml)
set_source_files_properties(qml/pages/PlayerPage.qml PROPERTIES QT_RESOURCE_ALIAS pages/PlayerPage.qml)

qt_add_library(pickle_domain STATIC
    src/domain/AppSettingsPolicy.cpp
    src/domain/AppSettingsPolicy.h
    src/domain/CancellationToken.cpp
    src/domain/CancellationToken.h
    src/domain/LibraryQuery.h
    src/domain/MediaEntities.h
    src/domain/OperationResult.h
)

target_link_libraries(pickle_domain
    PUBLIC
        Qt6::Core
)

target_include_directories(pickle_domain
    PUBLIC src
)

qt_add_library(pickle_core STATIC
    src/core/AppLogger.cpp
    src/core/AppLogger.h
    src/core/ExternalToolService.cpp
    src/core/ExternalToolService.h
    src/core/PathSecurity.cpp
    src/core/PathSecurity.h
    src/core/ProcessRunner.cpp
    src/core/ProcessRunner.h
)

target_link_libraries(pickle_core
    PUBLIC
        pickle_domain
        Qt6::Core
)

target_include_directories(pickle_core
    PUBLIC src
)

add_library(pickle_ports INTERFACE)

target_sources(pickle_ports
    INTERFACE
        src/ports/IExternalToolResolver.h
        src/ports/IFileRenamer.h
        src/ports/IFileScanner.h
        src/ports/IMediaRepository.h
        src/ports/IMetadataProbe.h
        src/ports/IPathPolicy.h
        src/ports/IProcessRunner.h
        src/ports/ISettingsRepository.h
        src/ports/ISnapshotGenerator.h
        src/ports/IThumbnailGenerator.h
)

target_link_libraries(pickle_ports
    INTERFACE
        pickle_domain
)

target_include_directories(pickle_ports
    INTERFACE src
)

qt_add_library(pickle_application STATIC
    src/application/BatchUpdateMediaUseCase.cpp
    src/application/BatchUpdateMediaUseCase.h
    src/application/BatchRenameMediaUseCase.cpp
    src/application/BatchRenameMediaUseCase.h
    src/application/CaptureSnapshotUseCase.cpp
    src/application/CaptureSnapshotUseCase.h
    src/application/ExtractMetadataUseCase.cpp
    src/application/ExtractMetadataUseCase.h
    src/application/LibraryLoadResult.h
    src/application/LoadLibraryUseCase.cpp
    src/application/LoadLibraryUseCase.h
    src/application/RebuildThumbnailsUseCase.cpp
    src/application/RebuildThumbnailsUseCase.h
    src/application/RenameMediaUseCase.cpp
    src/application/RenameMediaUseCase.h
    src/application/SavePlaybackPositionUseCase.cpp
    src/application/SavePlaybackPositionUseCase.h
    src/application/ScanCommitResult.h
    src/application/ScanLibraryUseCase.cpp
    src/application/ScanLibraryUseCase.h
    src/application/SettingsUseCase.cpp
    src/application/SettingsUseCase.h
    src/application/UpdateMediaDetailsUseCase.cpp
    src/application/UpdateMediaDetailsUseCase.h
    src/application/UpdateMediaFlagsUseCase.cpp
    src/application/UpdateMediaFlagsUseCase.h
)

target_link_libraries(pickle_application
    PUBLIC
        pickle_domain
        pickle_ports
        Qt6::Core
)

target_include_directories(pickle_application
    PUBLIC src
)

qt_add_library(pickle_db STATIC
    src/db/AppSettingsRepository.cpp
    src/db/AppSettingsRepository.h
    src/db/DatabaseService.cpp
    src/db/DatabaseService.h
    src/db/MediaRepository.cpp
    src/db/MediaRepository.h
    src/db/MigrationService.cpp
    src/db/MigrationService.h
    src/db/ScopedDatabaseConnection.cpp
    src/db/ScopedDatabaseConnection.h
    src/media/MediaTypes.h
)

target_link_libraries(pickle_db
    PUBLIC
        pickle_core
        pickle_domain
        pickle_ports
        Qt6::Sql
)

target_include_directories(pickle_db
    PUBLIC src
)

qt_add_library(pickle_media STATIC
    src/media/MediaLibraryModel.cpp
    src/media/MediaLibraryModel.h
    src/media/MediaTypes.h
    src/media/MetadataService.cpp
    src/media/MetadataService.h
    src/media/PlaybackController.cpp
    src/media/PlaybackController.h
    src/media/RenameService.cpp
    src/media/RenameService.h
    src/media/ScanService.cpp
    src/media/ScanService.h
    src/media/SnapshotService.cpp
    src/media/SnapshotService.h
    src/media/ThumbnailService.cpp
    src/media/ThumbnailService.h
)

target_link_libraries(pickle_media
    PUBLIC
        pickle_core
        pickle_domain
        pickle_ports
        Qt6::Gui
)

target_include_directories(pickle_media
    PUBLIC src
)

qt_add_library(pickle_app STATIC
    src/app/AppControllerBatchRename.cpp
    src/app/AppController.cpp
    src/app/AppController.h
    src/app/AppControllerSelection.cpp
    src/app/DiagnosticsController.cpp
    src/app/DiagnosticsController.h
    src/app/LibraryController.cpp
    src/app/LibraryController.h
    src/app/LibraryReloadService.cpp
    src/app/LibraryReloadService.h
    src/app/MediaActionsController.cpp
    src/app/MediaActionsController.h
    src/app/MediaItemPresenter.cpp
    src/app/MediaItemPresenter.h
    src/app/MetadataController.cpp
    src/app/MetadataController.h
    src/app/ScanController.cpp
    src/app/ScanController.h
    src/app/SettingsController.cpp
    src/app/SettingsController.h
    src/app/SnapshotController.cpp
    src/app/SnapshotController.h
    src/app/ThumbnailController.cpp
    src/app/ThumbnailController.h
    src/app/WorkEventLog.cpp
    src/app/WorkEventLog.h
)

target_link_libraries(pickle_app
    PUBLIC
        pickle_application
        pickle_core
        pickle_db
        pickle_media
        Qt6::Concurrent
        Qt6::Gui
)

target_include_directories(pickle_app
    PUBLIC src
)

qt_add_library(pickle_infrastructure STATIC
    src/infrastructure/PathSecurityPolicy.cpp
    src/infrastructure/PathSecurityPolicy.h
)

target_link_libraries(pickle_infrastructure
    PUBLIC
        pickle_core
        pickle_db
        pickle_media
        pickle_ports
)

target_include_directories(pickle_infrastructure
    PUBLIC src
)

add_library(pickle_presentation INTERFACE)
target_link_libraries(pickle_presentation
    INTERFACE
        pickle_app
)

qt_add_library(pickle_test_support STATIC
    tests/TestSupport.cpp
    tests/TestSupport.h
)

target_link_libraries(pickle_test_support
    PUBLIC
        pickle_db
        pickle_media
)

target_include_directories(pickle_test_support
    PUBLIC
        src
        tests
)

qt_add_executable(apppickle
    src/main.cpp
)

qt_add_qml_module(apppickle
    URI pickle
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/components/InfoPanel.qml
        qml/components/LibraryPanel.qml
        qml/components/PlaybackBar.qml
        qml/components/ReviewShortcutLayer.qml
        qml/pages/PlayerPage.qml
)

set_target_properties(apppickle PROPERTIES
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(apppickle
    PRIVATE
        pickle_app
        Qt6::Quick
        Qt6::Multimedia
        Qt6::QuickDialogs2
)

target_compile_definitions(apppickle
    PRIVATE PICKLE_VERSION="${PROJECT_VERSION}"
)

qt_add_executable(pickle_perf_probe
    tools/perf_probe/main.cpp
)

target_link_libraries(pickle_perf_probe
    PRIVATE
        pickle_core
        pickle_db
        pickle_media
)

target_compile_definitions(pickle_perf_probe
    PRIVATE PICKLE_VERSION="${PROJECT_VERSION}"
)

enable_testing()

function(pickle_add_test target source)
    qt_add_executable(${target}
        ${source}
    )
    target_link_libraries(${target}
        PRIVATE
            ${ARGN}
            Qt6::Test
    )
    target_include_directories(${target}
        PRIVATE
            src
            tests
    )
    add_test(NAME ${target} COMMAND ${target})
endfunction()

pickle_add_test(m03_scan_repository_test
    tests/M03ScanRepositoryTest.cpp
    pickle_db
    pickle_media
)

pickle_add_test(m04_library_query_test
    tests/M04LibraryQueryTest.cpp
    pickle_app
)

pickle_add_test(m06_metadata_details_test
    tests/M06MetadataDetailsTest.cpp
    pickle_db
    pickle_media
)

pickle_add_test(m07_file_management_test
    tests/M07FileManagementTest.cpp
    pickle_app
)

pickle_add_test(m08_snapshot_thumbnail_test
    tests/M08SnapshotThumbnailTest.cpp
    pickle_db
    pickle_media
)

pickle_add_test(m09_quality_performance_test
    tests/M09QualityPerformanceTest.cpp
    pickle_db
    pickle_media
)

pickle_add_test(m10_settings_diagnostics_test
    tests/M10SettingsDiagnosticsTest.cpp
    pickle_app
)

pickle_add_test(m11_performance_security_regression_test
    tests/M11PerformanceSecurityRegressionTest.cpp
    pickle_app
    pickle_test_support
)

pickle_add_test(m12_utility_modularization_test
    tests/M12UtilityModularizationTest.cpp
    pickle_app
    pickle_test_support
)

pickle_add_test(m13_architecture_boundary_test
    tests/M13ArchitectureBoundaryTest.cpp
    pickle_application
)

target_compile_definitions(m13_architecture_boundary_test
    PRIVATE PICKLE_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

pickle_add_test(m14_use_case_library_scan_test
    tests/M14UseCaseLibraryScanTest.cpp
    pickle_application
)

pickle_add_test(m15_use_case_media_actions_test
    tests/M15UseCaseMediaActionsTest.cpp
    pickle_application
)

pickle_add_test(m16_infrastructure_adapter_test
    tests/M16InfrastructureAdapterTest.cpp
    pickle_infrastructure
    pickle_media
)

pickle_add_test(m17_presentation_facade_test
    tests/M17PresentationFacadeTest.cpp
    pickle_app
)

pickle_add_test(m18_settings_diagnostics_controller_test
    tests/M18SettingsDiagnosticsControllerTest.cpp
    pickle_app
)

pickle_add_test(m19_repository_presentation_mapper_test
    tests/M19RepositoryPresentationMapperTest.cpp
    pickle_app
    pickle_test_support
)

pickle_add_test(m20_architecture_boundary_closure_test
    tests/M20ArchitectureBoundaryClosureTest.cpp
    pickle_app
)

target_compile_definitions(m20_architecture_boundary_closure_test
    PRIVATE PICKLE_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

pickle_add_test(m21_app_controller_facade_closure_test
    tests/M21AppControllerFacadeClosureTest.cpp
    pickle_app
)

target_compile_definitions(m21_app_controller_facade_closure_test
    PRIVATE PICKLE_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

pickle_add_test(m22_smart_views_filter_test
    tests/M22SmartViewsFilterTest.cpp
    pickle_app
    pickle_test_support
)

pickle_add_test(m23_multi_selection_batch_test
    tests/M23MultiSelectionBatchTest.cpp
    pickle_app
    pickle_test_support
)

target_compile_definitions(m23_multi_selection_batch_test
    PRIVATE PICKLE_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

pickle_add_test(m24_batch_rename_test
    tests/M24BatchRenameTest.cpp
    pickle_app
    pickle_test_support
)

target_compile_definitions(m24_batch_rename_test
    PRIVATE PICKLE_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

pickle_add_test(m27_keyboard_review_mode_test
    tests/M27KeyboardReviewModeTest.cpp
    pickle_app
    pickle_test_support
)

target_compile_definitions(m27_keyboard_review_mode_test
    PRIVATE PICKLE_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

find_program(OPENCPPCOVERAGE_EXECUTABLE OpenCppCoverage)
if(OPENCPPCOVERAGE_EXECUTABLE)
    add_custom_target(coverage_opencppcoverage
        COMMAND "${OPENCPPCOVERAGE_EXECUTABLE}"
            --sources "${CMAKE_SOURCE_DIR}/src"
            --excluded_sources "${CMAKE_SOURCE_DIR}/tests"
            --excluded_sources "${CMAKE_BINARY_DIR}"
            --export_type "html:${CMAKE_BINARY_DIR}/coverage/opencppcoverage"
            -- "${CMAKE_CTEST_COMMAND}" --test-dir "${CMAKE_BINARY_DIR}" --output-on-failure
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Running CTest with OpenCppCoverage"
    )
else()
    message(STATUS "OpenCppCoverage not found; coverage_opencppcoverage target is unavailable.")
endif()

install(TARGETS apppickle
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
