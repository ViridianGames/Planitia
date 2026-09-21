/* Stand-in for the header CMake generates from miniupnpcstrings.h.cmake.
   The Visual Studio project does not run that configure step.
   OS_STRING and the version match the CMake build (Windows, miniupnpc 2.2.8). */
#ifndef MINIUPNPCSTRINGS_H_INCLUDED
#define MINIUPNPCSTRINGS_H_INCLUDED

#define OS_STRING "Windows"
#define MINIUPNPC_VERSION_STRING "2.2.8"

#if 0
/* according to "UPnP Device Architecture 1.0" */
#define UPNP_VERSION_MAJOR 1
#define UPNP_VERSION_MINOR 0
#define UPNP_VERSION_STRING "UPnP/1.0"
#else
/* according to "UPnP Device Architecture 1.1" */
#define UPNP_VERSION_MAJOR 1
#define UPNP_VERSION_MINOR 1
#define UPNP_VERSION_STRING "UPnP/1.1"
#endif

#endif
