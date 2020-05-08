Blabble
=======

Blabble is an open source SIP plugin for modern web browsers. It allows for web-based SIP calls with a simple Javascript API. Blabble uses [PJSIP](http://www.pjsip.org/), an open source SIP library along with [Firebreath](http://www.firebreath.org/), an open source library to develop cross-platform browser plugins. Blabble is released under the [GPL version 3](http://www.gnu.org/licenses/gpl-3.0.html).

Read more at [http://blabblephone.com](http://blabblephone.com)


===============================================================


Enghouse Italy notes:


This version of PluginSIP is  intended for execution on browsers that use the Native Messaging technology (that is, Chrome/Firefox/Chromium-based Edge)

Building (Windows 32 bits):

Required dependencies and tested versions for building the project, as of 2020/05/08, are the following:
- Visual Studio 2017 (only paid versions are known to work)
- CMake 3.12 (either standalone or installed as a component of Visual Studio 2017)
- FireBreath 2.0 (www.firebreath.org)
- PJSIP 2.9
- bcg729-1.0.4 (indirect dependency included by PJSIP for G.729 codec support)
- curl 7.55.1 in order to upload log files
- ziplib bitbucket.org/wbenny/ziplib/wiki/Home (latest tested version is git commit 176e4b6 on 2018/07/16) in order to upload log files

The code and build scripts expect the code base to be into a PluginSIP directory within FireBreath 1.0 projects folder and the dependencies
indicated above to be installed into a specific "environment" directory (with subdirectories) that in our own case is R:\env\Win32


From a VS2017 command prompt within the base dir do:

- prep-vs2017.cmd to prepare build files (it must be executed each time CMake files are modified)
- build-vs2017.cmd to build the files using command line tools, or open the Visual Studio solution within the buildPluginSIP
  directory located into the base FireBreath 2.0 directory
- install-vs2017.cmd to install the built binaries within the "environment" directory

WARNING: FireWyrmNativeMessageHost and its dependencies:
- boost_filesystem
- boost_system
- boost_thread
- jsoncpp

MUST be compiled using the static Multithreaded runtime library, else the log download feature causes a spurious premature termination of the process.

A prebuilt FireWyrmNativeMessageHost executable is provided into env\Win32 so rebuilding it is necessary only if there are changes into FireWyrmNativeMessageHost itself.
The instructions to copy the FireWyrmNativeMessageHost files to env\Win32 are commented out by default in order to avoid accidentally overwriting it.


Building (Windows 64 bits):

Building Windows 64 bits binaries have not been tested yet (it may not be supported)


Building (Ubuntu Linux 14.04/18.04 64 bits):

TBD
