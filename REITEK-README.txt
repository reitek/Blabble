From a Visual Studio 2013 (Professional version only) command prompt execute the following (each time verify the working directory is this one):
- prep-vs2013.cmd
- build-vs2013.cmd
- install-vs2013.cmd

WARNING: FireWyrmNativeMessageHost and its dependencies:
- boost_filesystem
- boost_system
- boost_thread
- jsoncpp

MUST be compiled using the static Multithreaded runtime library, else the log download feature causes a spurious premature termination of the process.

A prebuilt FireWyrmNativeMessageHost executable is provided into env\Win32 so rebuilding it is necessary only if there are changes into FireWyrmNativeMessageHost itself.
The instructions to copy the FireWyrmNativeMessageHost files to env\Win32 are commented out by default in order to avoid accidentally overwriting it.