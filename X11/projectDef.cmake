#/**********************************************************\ 
# Auto-generated X11 project definition file for the
# Blabble project
#\**********************************************************/

# X11 template platform definition CMake file
# Included from ../CMakeLists.txt

# remember that the current source dir is the project root; this file is in X11/
file (GLOB PLATFORM RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    X11/[^.]*.cpp
    X11/[^.]*.h
    X11/[^.]*.cmake
    )

SOURCE_GROUP(X11 FILES ${PLATFORM})

# use this to add preprocessor definitions
add_definitions(
)

set (SOURCES
    ${SOURCES}
    ${PLATFORM}
    )

add_x11_plugin(${PROJECT_NAME} SOURCES)

link_boost_library(${PROJECT_NAME} date_time)
link_boost_library(${PROJECT_NAME} filesystem)
link_boost_library(${PROJECT_NAME} regex)

# add library dependencies here; leave ${FB_PLUGIN_LIBRARIES} there unless you know what you're doing!
target_link_libraries(${PROJECT_NAME}
    ${FB_PLUGIN_LIBRARIES}
    ${PJSIP_STATIC_LIBRARIES}
    ${CURL_STATIC_LIBRARIES}
    $ENV{ENVDIR}/lib/libzip.a
    )
