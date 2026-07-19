cmake_minimum_required(VERSION 3.16)
project(WIFI_FileTransfer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.11.1 REQUIRED COMPONENTS Core Widgets Network)

qt_standard_project_setup()

set(SOURCES
    src/main.cpp
    src/FileTransfer.cpp
    src/WFTheader.cpp
)
set(HEADERS
    src/FileTransfer.hpp
    src/WFTheader.hpp
)

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        ole32
        shell32
    )
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

if(WIN32)
    find_program(WINDEPLOYQT_EXECUTABLE
        NAMES windeployqt6 windeployqt
        HINTS ${Qt6_DIR}/../../bin
        DOC "Qt windeployqt tool"
    )
    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E echo "Running windeployqt to copy Qt dependencies..."
            COMMAND ${WINDEPLOYQT_EXECUTABLE}
                --no-compiler-runtime   
                --no-system-d3d-compiler
                --verbose 0
                $<TARGET_FILE:${PROJECT_NAME}>
            COMMAND ${CMAKE_COMMAND} -E echo "Qt dependencies deployed."
            COMMENT "Deploying Qt libraries to output folder"
            WORKING_DIRECTORY $<TARGET_FILE_DIR:${PROJECT_NAME}>
        )
    else()
        message(WARNING "windeployqt not found! Qt DLLs will not be copied automatically.")
    endif()
    if(MINGW)
        get_filename_component(MINGW_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
        
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E echo "Copying MinGW runtime DLLs..."
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${MINGW_BIN_DIR}/libgcc_s_seh-1.dll"
                "${MINGW_BIN_DIR}/libstdc++-6.dll"
                "${MINGW_BIN_DIR}/libwinpthread-1.dll"
                $<TARGET_FILE_DIR:${PROJECT_NAME}>
            COMMAND ${CMAKE_COMMAND} -E echo "MinGW runtime copied."
            COMMENT "Deploying MinGW runtime"
            WORKING_DIRECTORY $<TARGET_FILE_DIR:${PROJECT_NAME}>
        )
    endif()
endif()

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)