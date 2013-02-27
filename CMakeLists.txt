# ===================================================== #
#                                                       #
#    #    ##   #  #   #                           #     #
#   # #  #  #  #  #  # #                          #     #
#  ##### #  #  #  # #####  ##  ###  #  #  ## ###  ###   #
#  #   # #  #  #  # #   # #  # #  # #  # #   #  # #  #  #
#  #   # #  #  #  # #   # #  # #  # #  #   # #  # #  #  #
#  #   #  ## #  ##  #   #  ### ###   ### ##  ###  #  #  #
#                            # #             #          #
#                          ##  #             #          #
#                                                       #
# ===================================================== #
#                                                       #
#   Authors:                                            #
#       - Cercos Pita, Jose Luis                        #
#       - Miguel Gonzalez, Leo                          #
#       - Souto Iglesias, Antonio                       #
#                                                       #
# ===================================================== #


# ===================================================== #
# General CMake options                                 #
# ===================================================== #
cmake_minimum_required(VERSION 2.8.0 FATAL_ERROR)
project(ocland)
set(AQUAGPUSPH_VERSION "0.1.02")

set(PACKAGE_NAME "ocland")
set(PACKAGE_VERSION_MAJOR "0")
set(PACKAGE_VERSION_MINOR "1")
set(PACKAGE_VERSION_PATCH "02")
set(PACKAGE_VERSION "${PACKAGE_VERSION_MAJOR}.${PACKAGE_VERSION_MINOR}.${PACKAGE_VERSION_PATCH}")
set(PACKAGE_STRING "${PACKAGE_NAME} ${PACKAGE_VERSION}")

# include local modules
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake")

if(COMMAND cmake_policy)
     cmake_policy(SET CMP0003 NEW)
endif(COMMAND cmake_policy)

# No Debug/Release output paths
set(DEBUG_MAIN_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin)
set(RELEASE_MAIN_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin)

if(WIN32)
    set(PLATFORM_CP xcopy /Y /S)
    set(PLATFORM_MK mkdir)
else(WIN32)
    set(PLATFORM_CP cp)
    set(PLATFORM_MK mkdir -p)
endif(WIN32)

# ===================================================== #
# Output directories for install target                 #
# ===================================================== #
if(WIN32)
    SET(CMAKE_INSTALL_PREFIX ${CMAKE_BINARY_DIR}/install CACHE PATH "Installation root directory")
else(WIN32)
    SET(CMAKE_INSTALL_PREFIX "/usr" CACHE PATH "Installation root directory")
    SET(INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
endif(WIN32)

SET(CMAKE_INSTALL_DATADIR share/ocland CACHE PATH "Output directory for data and resource files")

MESSAGE(STATUS "prefix: ${CMAKE_INSTALL_PREFIX}")
MESSAGE(STATUS "datadir: ${CMAKE_INSTALL_DATADIR}")

# ===================================================== #
# All the options for the build process                 #
# ===================================================== #
IF(NOT CMAKE_BUILD_TYPE)
	SET(CMAKE_BUILD_TYPE "Release")
ENDIF(NOT CMAKE_BUILD_TYPE)
OPTION(OCLAND_SERVER "Build and install ocland server." ON)
OPTION(OCLAND_SERVER_DAEMON "Build and install ocland server daemon." ON)
OPTION(OCLAND_CLIENT "Build and install ocland client." ON)
OPTION(OCLAND_CLIENT_ICD "Update OpenCL drivers with the ocland one." ON)
OPTION(OCLAND_EXAMPLES "Build ocland examples." ON)
IF(NOT DEFINED OCLAND_MAX_N_PLATFORMS)
	SET(OCLAND_MAX_N_PLATFORMS 65536)
ENDIF(NOT DEFINED OCLAND_MAX_N_PLATFORMS)
IF(NOT DEFINED OCLAND_MAX_N_DEVICES)
	SET(OCLAND_MAX_N_DEVICES 65536)
ENDIF(NOT DEFINED OCLAND_MAX_N_DEVICES)
IF(NOT DEFINED OCLAND_MAX_N_CONTEXTS)
	SET(OCLAND_MAX_N_CONTEXTS 65536)
ENDIF(NOT DEFINED OCLAND_MAX_N_CONTEXTS)
IF(NOT DEFINED OCLAND_MAX_N_QUEUES)
	SET(OCLAND_MAX_N_QUEUES 65536)
ENDIF(NOT DEFINED OCLAND_MAX_N_QUEUES)
IF(NOT DEFINED OCLAND_MAX_N_MEMS)
	SET(OCLAND_MAX_N_MEMS 1048576)
ENDIF(NOT DEFINED OCLAND_MAX_N_MEMS)
IF(NOT DEFINED OCLAND_MAX_N_PROGRAMS)
	SET(OCLAND_MAX_N_PROGRAMS 1048576)
ENDIF(NOT DEFINED OCLAND_MAX_N_PROGRAMS)
IF(NOT DEFINED OCLAND_MAX_N_KERNELS)
	SET(OCLAND_MAX_N_KERNELS 1048576)
ENDIF(NOT DEFINED OCLAND_MAX_N_KERNELS)
IF(NOT DEFINED OCLAND_MAX_N_EVENTS)
	SET(OCLAND_MAX_N_EVENTS 1048576)
ENDIF(NOT DEFINED OCLAND_MAX_N_EVENTS)
IF(NOT DEFINED OCLAND_BUFFSIZE)
	SET(OCLAND_BUFFSIZE 1025)
ENDIF(NOT DEFINED OCLAND_BUFFSIZE)
IF(NOT DEFINED OCLAND_PORT)
	SET(OCLAND_PORT 51000)
ENDIF(NOT DEFINED OCLAND_PORT)
IF(NOT DEFINED OCLAND_MAX_CLIENTS)
	SET(OCLAND_MAX_CLIENTS 32)
ENDIF(NOT DEFINED OCLAND_MAX_CLIENTS)

MARK_AS_ADVANCED(FORCE ${OCLAND_MAX_N_PLATFORMS}
${OCLAND_MAX_N_DEVICES}
${OCLAND_MAX_N_CONTEXTS}
${OCLAND_MAX_N_QUEUES}
${OCLAND_MAX_N_MEMS}
${OCLAND_MAX_N_PROGRAMS}
${OCLAND_MAX_N_KERNELS}
${OCLAND_MAX_N_EVENTS}
${OCLAND_BUFFSIZE}
${OCLAND_PORT}
${OCLAND_MAX_CLIENTS})

# ===================================================== #
# Definitions                                           #
# ===================================================== #
ADD_DEFINITIONS(-DMAX_N_PLATFORMS=${OCLAND_MAX_N_PLATFORMS}
-DMAX_N_DEVICES=${OCLAND_MAX_N_PLATFORMS}
-DMAX_N_CONTEXTS=${OCLAND_MAX_N_CONTEXTS}
-DMAX_N_QUEUES=${OCLAND_MAX_N_QUEUES}
-DMAX_N_MEMS=${OCLAND_MAX_N_MEMS}
-DMAX_N_PROGRAMS=${OCLAND_MAX_N_PROGRAMS}
-DMAX_N_KERNELS=${OCLAND_MAX_N_KERNELS}
-DMAX_N_EVENTS=${OCLAND_MAX_N_EVENTS}
)

# ===================================================== #
# Search the packages                                   #
# ===================================================== #

# OpenCL
FIND_PACKAGE(OpenCL REQUIRED)

IF(NOT OPENCL_FOUND)
MESSAGE(FATAL_ERROR "OpenCL not found, but ${PACKAGE_NAME} requires it. Please install OpenCL!")
ENDIF(NOT OPENCL_FOUND)

# pthreas
FIND_PACKAGE(Threads REQUIRED)

IF(NOT Threads_FOUND)
MESSAGE(FATAL_ERROR "pthreads not found, but ${PACKAGE_NAME} requires it. Please install pthreads!")
ENDIF(NOT Threads_FOUND)

IF(OCLAND_SERVER_DAEMON AND NOT OCLAND_SERVER)
	MESSAGE(SEND_ERROR "Server daemon requested, but server will not be built. Disable the deamon OCLAND_SERVER_DAEMON of activate the server OCLAND_SERVER")
ENDIF(OCLAND_SERVER_DAEMON AND NOT OCLAND_SERVER)

IF(OCLAND_CLIENT_ICD AND NOT OCLAND_CLIENT)
	MESSAGE(SEND_ERROR "OpenCL driver vendor requested, but client will not be built. Disable the icd OCLAND_CLIENT_ICD of activate the client OCLAND_CLIENT")
ENDIF(OCLAND_CLIENT_ICD AND NOT OCLAND_CLIENT)

# ===================================================== #
# Global Compiler and Linker Settings                   #
# ===================================================== #
INCLUDE_DIRECTORIES(${CMAKE_BINARY_DIR}/include ${CMAKE_SOURCE_DIR}/include)

# check for 64-bit platform
IF(CMAKE_SIZEOF_VOID_P EQUAL 8)
	MESSAGE(STATUS "Platform is 64-bit")
ELSE(CMAKE_SIZEOF_VOID_P EQUAL 8)
	MESSAGE(STATUS "Platform is 32-bit")
ENDIF(CMAKE_SIZEOF_VOID_P EQUAL 8)

IF(MSVC)
	# set default compiler settings
	#SET (CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /GF /GY")
	SET (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DAQUA_DEBUG -DHAVE_GPUPROFILE")
	# set default libs
	SET (CMAKE_C_STANDARD_LIBRARIES "kernel32.lib user32.lib gdi32.lib winspool.lib SHFolder.lib shell32.lib ole32.lib oleaut32.lib uuid.lib comdlg32.lib advapi32.lib ")
	set (CMAKE_CXX_STANDARD_LIBRARIES "${CMAKE_C_STANDARD_LIBRARIES}")
	# set linker flag /nodefaultlib
	set (CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /NODEFAULTLIB")
	SET (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /NODEFAULTLIB")

	# Mark 32 bit executables large address aware so they can use > 2GB address space
	# NOTE: This setting only has an effect on machines with at least 3GB of RAM, although it sets the linker option it doesn't set the the linker switch 'Enable Large Addresses'
	IF(CMAKE_SIZEOF_VOID_P EQUAL 4)
		set (CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /LARGEADDRESSAWARE")
		SET (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /LARGEADDRESSAWARE")
	ENDIF(CMAKE_SIZEOF_VOID_P EQUAL 4)
ELSE(MSVC)
	SET (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DOCLAND_DEBUG")
	MESSAGE(STATUS "DEBUG: ${CMAKE_CXX_FLAGS_DEBUG}")
ENDIF(MSVC)
IF(MINGW)
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mthreads")
	SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mthreads")
	SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -mthreads -Wl,--export-all-symbols")
	SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -mthreads -Wl,--export-all-symbols")
	LINK_LIBRARIES(-lgdi32)
ENDIF(MINGW)
# 0000661: cmake build on Mac OS: dealing with dylib versus so
IF(APPLE)
	SET(CMAKE_SHARED_LIBRARY_SUFFIX ".so")
ENDIF(APPLE)
IF(UNIX)
	ADD_DEFINITIONS(-DUNIX)
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -w")
	SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -w")
ENDIF(UNIX)

# ===================================================== #
# Compilation parts                                     #
# ===================================================== #
ADD_SUBDIRECTORY(src)
ADD_SUBDIRECTORY(include/ocland)

# ===================================================== #
# Show a brief                                          #
# ===================================================== #
MESSAGE("=====================================================")
IF(OCLAND_SERVER)
	MESSAGE("ocland server:")
	IF(OCLAND_SERVER_DAEMON)
		MESSAGE("    - With daemon")
	ENDIF(OCLAND_SERVER_DAEMON)
	MESSAGE("    - Listening in port ${OCLAND_PORT}")
	MESSAGE("    - ${OCLAND_MAX_CLIENTS} clients will be accepted")
ENDIF(OCLAND_SERVER)
IF(OCLAND_CLIENT)
	MESSAGE("ocland client:")
	IF(OCLAND_CLIENT_ICD)
		MESSAGE("    - Updating OpenCL drivers list")
	ENDIF(OCLAND_CLIENT_ICD)
	MESSAGE("    - Connecting to port ${OCLAND_PORT}")
	MESSAGE("    - ${OCLAND_MAX_N_PLATFORMS} platforms will be allowed")
	MESSAGE("    - ${OCLAND_MAX_N_DEVICES} devices will be allowed")
	MESSAGE("    - ${OCLAND_MAX_N_CONTEXTS} contexts will be allowed")
	MESSAGE("    - ${OCLAND_MAX_N_QUEUES} queues will be allowed")
	MESSAGE("    - ${OCLAND_MAX_N_MEMS} memory objects will be allowed")
	MESSAGE("    - ${OCLAND_MAX_N_PROGRAMS} programs will be allowed")
	MESSAGE("    - ${OCLAND_MAX_N_KERNELS} kernels will be allowed")
	MESSAGE("    - ${OCLAND_MAX_N_EVENTS} events will be allowed")
ENDIF(OCLAND_CLIENT)
IF(OCLAND_EXAMPLES)
	MESSAGE("examples will be built")
ENDIF(OCLAND_EXAMPLES)
MESSAGE("Destination: ${CMAKE_INSTALL_PREFIX}")
MESSAGE("Data destination: ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATADIR}")
MESSAGE("=====================================================")