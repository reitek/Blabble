#/**********************************************************\ 
# 
# Auto-generated CMakeLists.txt for the BlabblePhone project
#
#\**********************************************************/

# Written to work with cmake 2.6
cmake_minimum_required (VERSION 2.6)
set (CMAKE_BACKWARDS_COMPATIBILITY 2.6)

Project(${PLUGIN_NAME})

file (GLOB GENERAL RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    [^.]*.h
	[^.]*.c	
	[^.]*.cpp
    [^.]*.cmake
    )

if (WIN32)
INCLUDE_DIRECTORIES(
	${PLUGIN_INCLUDE_DIRS}
	..\\..\\..\\..\\env\\Win32\\include\\pjproject
	..\\..\\..\\..\\env\\Win32\\include\\ziplib
	..\\..\\..\\..\\env\\Win32\\include\\curl
)

SET(PLUGIN_INTERNAL_DEPS 
	${PLUGIN_INTERNAL_DEPS}
	Iphlpapi.lib
	netapi32.lib
	mswsock.lib
	ws2_32.lib
)

ADD_DEFINITIONS(-DPJ_WIN32=1)

endif()

if (UNIX)

INCLUDE_DIRECTORIES(
	${PLUGIN_INCLUDE_DIRS}
)

find_package(PkgConfig)

pkg_check_modules(PJSIP REQUIRED libpjproject)

SET(PLUGIN_INTERNAL_DEPS 
	${PLUGIN_INTERNAL_DEPS}
	${PJSIP_LIBRARIES}
)

#Add GCC debug flags when in debug build
SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fPIC -O0 -g3" )

endif()

if (APPLE)

FIND_LIBRARY(AUDIOTOOLBOX_LIBRARY AudioToolbox)
FIND_LIBRARY(AUDIOUNIT_LIBRARY AudioUnit)
FIND_LIBRARY(COREAUDIO_LIBRARY CoreAudio)

MARK_AS_ADVANCED(AUDIOTOOLBOX_LIBRARY
				AUDIOUNIT_LIBRARY
				COREAUDIO_LIBRARY)
SET(EXTRA_LIBS ${AUDIOTOOLBOX_LIBRARY} ${AUDIOUNIT_LIBRARY} ${COREAUDIO_LIBRARY})

INCLUDE_DIRECTORIES(
	${PLUGIN_INCLUDE_DIRS}
	/usr/local/include
)

SET(PLUGIN_INTERNAL_DEPS 
	${PLUGIN_INTERNAL_DEPS}
	${EXTRA_LIBS}
)

endif()
	
# Generated files are stored in ${GENERATED} by the project configuration
SET_SOURCE_FILES_PROPERTIES(
    ${GENERATED}
    PROPERTIES
        GENERATED 1
    )

SOURCE_GROUP(Generated FILES
    ${GENERATED}
    )

SET( SOURCES
    ${GENERAL}
    ${GENERATED}
    )

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

# This will include Win/projectDef.cmake, X11/projectDef.cmake, Mac/projectDef 
# depending on the platform
include_platform()

