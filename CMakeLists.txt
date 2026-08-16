cmake_minimum_required(VERSION 3.21)

project(eFNote VERSION 4.0.0 LANGUAGES CXX)

include(GNUInstallDirs)
include(CTest)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets PrintSupport Svg)

qt_add_executable(efnote
    src/main.cpp
    src/MainWindow.cpp
    src/MainWindow.hpp
    src/DocumentEditor.cpp
    src/DocumentEditor.hpp
    src/SyntaxHighlighter.cpp
    src/SyntaxHighlighter.hpp
    src/SearchReplaceDialog.cpp
    src/SearchReplaceDialog.hpp
    resources/efnote.qrc
)

target_link_libraries(efnote PRIVATE
    Qt6::Widgets
    Qt6::PrintSupport
    Qt6::Svg
)

target_compile_definitions(efnote PRIVATE
    EFNOTE_VERSION="${PROJECT_VERSION}"
)

target_compile_options(efnote PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
)

install(TARGETS efnote RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(FILES packaging/io.github.drericflores.efnote.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES packaging/io.github.drericflores.efnote.metainfo.xml
    DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
install(FILES icons/efnote.svg
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)

if(BUILD_TESTING)
    find_package(Qt6 6.4 REQUIRED COMPONENTS Test)
    qt_add_executable(efnote_tests
        tests/EditorTests.cpp
        src/DocumentEditor.cpp
        src/DocumentEditor.hpp
        src/SyntaxHighlighter.cpp
        src/SyntaxHighlighter.hpp
    )
    target_include_directories(efnote_tests PRIVATE src)
    target_link_libraries(efnote_tests PRIVATE Qt6::Widgets Qt6::Test)
    target_compile_options(efnote_tests PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
    )
    add_test(NAME efnote_editor_tests COMMAND efnote_tests)
    set_tests_properties(efnote_editor_tests PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

set(CPACK_PACKAGE_NAME "efnote")
set(CPACK_PACKAGE_VENDOR "Dr. Eric O. Flores")
set(CPACK_PACKAGE_CONTACT "drericflores@users.noreply.github.com")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "Native C++20/Qt6 text, source-code, and Markdown editor")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/drericflores/eFNote")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_GENERATOR "TGZ;DEB")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_SECTION "editors")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "pandoc")
include(CPack)
