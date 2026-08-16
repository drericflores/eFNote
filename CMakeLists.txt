cmake_minimum_required(VERSION 3.21)

project(eFNote VERSION 4.0.0 LANGUAGES CXX)

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

install(TARGETS efnote RUNTIME DESTINATION bin)
