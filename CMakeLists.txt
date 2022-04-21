cmake_minimum_required(VERSION 3.18)
project(mpeg-ts-client)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_FLAGS_DEBUG "-ggdb -O0 -DDEBUG -Wall -Wuninitialized -Wsign-compare -Wno-missing-braces")
set(CMAKE_CXX_FLAGS_DEBUG "-ggdb -O3 -DNDEBUG -Wall -Wuninitialized -Wsign-compare -Wno-missing-braces")

find_package(PkgConfig)
pkg_search_module(GLIB REQUIRED glib-2.0)
pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0)
pkg_check_modules(GSTREAMER_WEBRTC REQUIRED gstreamer-webrtc-1.0)
pkg_check_modules(SOUP REQUIRED libsoup-2.4)

set(FILES
        main.cpp
        utils/ScopedGLibObject.h
        utils/ScopedGstObject.h
        Pipeline.cpp
        Pipeline.h
        http/WhipClient.cpp
        http/WhipClient.h
        Pipeline.h
        Logger.h
        utils/ScopedGLibMem.h
        Logger.cpp)

add_executable(mpeg-ts-client ${FILES})

target_include_directories(${PROJECT_NAME} PRIVATE
        ${PROJECT_SOURCE_DIR}
        ${GLIB_INCLUDE_DIRS}
        ${GSTREAMER_INCLUDE_DIRS}
        ${GSTREAMER_WEBRTC_INCLUDE_DIRS}
        ${SOUP_INCLUDE_DIRS})

target_link_libraries(${PROJECT_NAME}
        ${GLIB_LIBRARIES}
        ${GSTREAMER_LDFLAGS}
        ${GSTREAMER_WEBRTC_LDFLAGS}
        ${SOUP_LDFLAGS})
