#/**********************************************************\ 
# Auto-generated Windows project definition file for the
# Reitek PluginSIP project
#\**********************************************************/

# Windows template platform definition CMake file
# Included from ../CMakeLists.txt

# remember that the current source dir is the project root; this file is in Win/
file (GLOB PLATFORM RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    Win/[^.]*.cpp
    Win/[^.]*.h
    Win/[^.]*.cmake
    )

# use this to add preprocessor definitions
add_definitions(
    /D "_ATL_STATIC_REGISTRY"
)

SOURCE_GROUP(Win FILES ${PLATFORM})

set (SOURCES
    ${SOURCES}
    ${PLATFORM}
    )

add_windows_plugin(${PROJECT_NAME} SOURCES)

# This is an example of how to add a build step to sign the plugin DLL before
# the WiX installer builds.  The first filename (certificate.pfx) should be
# the path to your pfx file.  If it requires a passphrase, the passphrase
# should be located inside the second file. If you don't need a passphrase
# then set the second filename to "".  If you don't want signtool to timestamp
# your DLL then make the last parameter "".
#
# Note that this will not attempt to sign if the certificate isn't there --
# that's so that you can have development machines without the cert and it'll
# still work. Your cert should only be on the build machine and shouldn't be in
# source control!
# -- uncomment lines below this to enable signing --
#firebreath_sign_plugin(${PROJECT_NAME}
#    "${CMAKE_CURRENT_SOURCE_DIR}/sign/certificate.pfx"
#    "${CMAKE_CURRENT_SOURCE_DIR}/sign/passphrase.txt"
#    "http://timestamp.verisign.com/scripts/timestamp.dll")

link_boost_library(${PROJECT_NAME} date_time)
link_boost_library(${PROJECT_NAME} filesystem)
link_boost_library(${PROJECT_NAME} regex)

# add library dependencies here; leave ${FB_PLUGIN_LIBRARIES} there unless you know what you're doing!
target_link_libraries(${PROJECT_NAME}
    ${FB_PLUGIN_LIBRARIES}
#	log4cplus
	optimized $(MSBuildProjectDirectory)\\..\\..\\..\\..\\..\\env\\Win32\\lib\\libpjproject-i386-Win32-vc14-Release.lib
	debug $(MSBuildProjectDirectory)\\..\\..\\..\\..\\..\\env\\Win32\\lib\\libpjproject-i386-Win32-vc14-Debug.lib
	optimized $(MSBuildProjectDirectory)\\..\\..\\..\\..\\..\\env\\Win32\\lib\\libcurls.lib
	debug $(MSBuildProjectDirectory)\\..\\..\\..\\..\\..\\env\\Win32\\lib\\libcurlsd.lib
	optimized $(MSBuildProjectDirectory)\\..\\..\\..\\..\\..\\env\\Win32\\lib\\ziplib.lib
	debug $(MSBuildProjectDirectory)\\..\\..\\..\\..\\..\\env\\Win32\\lib\\ziplibd.lib
    )

my_add_link_flags(${PROJECT_NAME} "/LIBPATH:\"$(MSBuildProjectDirectory)/../../../../../env/Win32/lib\" /SAFESEH:NO")

set(WIX_HEAT_FLAGS
    -gg                 # Generate GUIDs
    -srd                # Suppress Root Dir
    -cg PluginDLLGroup  # Set the Component group name
    -dr INSTALLDIR      # Set the directory ID to put the files in
    )

get_plugin_path(PLUGIN_FILEPATH ${PROJECT_NAME})
get_filename_component(PLUGIN_PATH ${PLUGIN_FILEPATH} DIRECTORY)

add_wix_installer( ${PLUGIN_NAME}
    ${CMAKE_CURRENT_SOURCE_DIR}/Win/WiX/BlabbleInstaller.wxs
    PluginDLLGroup
    ${PLUGIN_PATH}
    $<TARGET_FILE:${PROJECT_NAME}>
    ${PROJECT_NAME}
    )

# This is an example of how to add a build step to sign the WiX installer
# -- uncomment lines below this to enable signing --
#firebreath_sign_file("${PLUGIN_NAME}_WiXInstall"
#    "${FB_BIN_DIR}/${PLUGIN_NAME}/${CMAKE_CFG_INTDIR}/${PLUGIN_NAME}.msi"
#    "${CMAKE_CURRENT_SOURCE_DIR}/sign/certificate.pfx"
#    "${CMAKE_CURRENT_SOURCE_DIR}/sign/passphrase.txt"
#    "http://timestamp.verisign.com/scripts/timestamp.dll")
