#/**********************************************************\ 
# CMakeLists.txt for the Reitek PluginSIP project
#\**********************************************************/

# Written to work with cmake 2.6
cmake_minimum_required (VERSION 2.6)
set (CMAKE_BACKWARDS_COMPATIBILITY 2.6)

include(PluginConfig.cmake)

# REITEK: Add our ENV lib directory in order to find the local libraries installed there
if (UNIX)
	link_directories($ENV{ENVDIR}/lib)
endif()

Project(${PLUGIN_NAME})

set (FB_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/firebreath CACHE PATH "Path to FireBreath Root")
if (NOT EXISTS ${FB_ROOT}/CMakeLists.txt)
    message(FATAL_ERROR "You need to configure FB_ROOT to the root directory of your FireBreath installation!")
endif()

include(${FB_ROOT}/PluginIncludes.cmake)
add_subdirectory(${FB_PLUGINAUTO_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/PluginAuto)

add_subdirectory(${FB_ROOT} ${CMAKE_CURRENT_BINARY_DIR}/firebreath)

#Visual Studio Express editions don't want Solution Folders enabled.
if(NOT ATL_LIBRARY)
    set_property(GLOBAL PROPERTY USE_FOLDERS On)
endif()

file (GLOB GENERAL RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    [^.]*.h
	[^.]*.c	
	[^.]*.cpp
    [^.]*.cmake
    )

if (WIN32)
	include_directories(
		${FB_PLUGIN_INCLUDE_DIRS}
		${Boost_INCLUDE_DIRS}
		#${FB_ROOT}\\src\\libs\\log4cplus\\include
		\\..\\..\\..\\..\\env\\Win32\\include\\pjproject
		\\..\\..\\..\\..\\env\\Win32\\include\\ziplib
		\\..\\..\\..\\..\\env\\Win32\\include\\curl
	)

	set(FB_PLUGIN_INTERNAL_DEPS 
		${FB_PLUGIN_INTERNAL_DEPS}
		Iphlpapi.lib
		netapi32.lib
		mswsock.lib
		ws2_32.lib
	)

	add_definitions(-DPJ_WIN32=1)
endif()

if (UNIX)
	find_package(PkgConfig)

	# REITEK: Additional directory to locate PJSIP using pkgconfig
	set(ENV{PKG_CONFIG_PATH} "$ENV{ENVDIR}/lib/pkgconfig")

	pkg_check_modules(PJSIP REQUIRED libpjproject)
	pkg_check_modules(CURL REQUIRED libcurl)

	include_directories(
		${FB_PLUGIN_INCLUDE_DIRS}
		${PJSIP_INCLUDEDIR}
		$ENV{ENVDIR}/include/ziplib
		${CURL_INCLUDEDIR}
	)

	set(FB_PLUGIN_INTERNAL_DEPS 
		${FB_PLUGIN_INTERNAL_DEPS}
	)

	#Add GCC debug flags when in debug build
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fPIC -O0 -g3" )
endif()

if (APPLE)
	find_library(AUDIOTOOLBOX_LIBRARY AudioToolbox)
	find_library(AUDIOUNIT_LIBRARY AudioUnit)
	find_library(COREAUDIO_LIBRARY CoreAudio)

	mark_as_advanced(AUDIOTOOLBOX_LIBRARY
				AUDIOUNIT_LIBRARY
				COREAUDIO_LIBRARY)
	set(EXTRA_LIBS ${AUDIOTOOLBOX_LIBRARY} ${AUDIOUNIT_LIBRARY} ${COREAUDIO_LIBRARY})

	include_directories(
		${FB_PLUGIN_INCLUDE_DIRS}
		/usr/local/include
	)

	set(FB_PLUGIN_INTERNAL_DEPS 
		${FB_PLUGIN_INTERNAL_DEPS}
		${EXTRA_LIBS}
	)
endif()
	
# Generated files are stored in ${GENERATED} by the project configuration
SET_SOURCE_FILES_PROPERTIES(
    ${FB_PLUGIN_GENERATED_SOURCES}
    PROPERTIES
        GENERATED 1
    )

SOURCE_GROUP(Generated FILES
    ${FB_PLUGIN_GENERATED_SOURCES}
    )

SET( SOURCES
    ${GENERAL}
    ${FB_PLUGIN_GENERATED_SOURCES}
    )

#add_firebreath_library(log4cplus)

# REITEK: Added useful macros (Begin)

############################################################################################################
# Append str to a string property of a target.
# target:      string: target name.
# property:            name of target’s property. e.g: COMPILE_FLAGS, or LINK_FLAGS
# str:         string: string to be appended to the property
macro(my_append_target_property target property str)
  get_target_property(current_property ${target} ${property})
  if(NOT current_property) # property non-existent or empty
      set_target_properties(${target} PROPERTIES ${property} ${str})
  else()
      set_target_properties(${target} PROPERTIES ${property} "${current_property} ${str}")
  endif()
endmacro(my_append_target_property)

############################################################################################################
# Add/append compile flags to a target.
# target: string: target name.
# flags : string: compile flags to be appended
macro(my_add_compile_flags target flags)
  my_append_target_property(${target} COMPILE_FLAGS ${flags})
endmacro(my_add_compile_flags)

############################################################################################################
# Add/append link flags to a target.
# target: string: target name.
# flags : string: link flags to be appended
macro(my_add_link_flags target flags)
  my_append_target_property(${target} LINK_FLAGS ${flags})
endmacro(my_add_link_flags)

# REITEK: Added useful macros (End)

if (WIN32)
    set (PLATFORM_NAME "Win")
    include(Win/projectDef.cmake)
elseif(APPLE)
    set (PLATFORM_NAME "Mac")
    include(Mac/projectDef.cmake)
elseif(UNIX)
    set (PLATFORM_NAME "X11")
    include(X11/projectDef.cmake)
endif()

configure_chrome_wyrmhole()
add_native_message_host(${PROJECT_NAME})
