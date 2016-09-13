# ---< L Z 4 >---

set(LZ4LIB_HEADERS
  lib/lz4.h 
  lib/lz4hc.h 
  lib/lz4frame.h 
  lib/xxhash.h
)

set(LZ4LIB_SOURCES
  lib/lz4.c
  lib/lz4hc.c 
  lib/lz4frame.c 
  lib/xxhash.c
)

include_directories(${CMAKE_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(lib/)

link_directories( ${PRECOMPILED_LIB_DIR} )

add_library(lz4lib STATIC ${LZ4LIB_HEADERS} ${LZ4LIB_SOURCES})
set_target_properties(lz4lib PROPERTIES FOLDER "3rdparty")

set_target_properties(lz4lib PROPERTIES COMPILE_FLAGS "${G_CPP_FLAGS}")
set_target_properties(lz4lib PROPERTIES LINK_FLAGS "${G_LINK_FLAGS}")
set_target_properties(lz4lib PROPERTIES DEBUG_POSTFIX "_d")

source_group("Headers" FILES ${LZ4LIB_HEADERS})
source_group("Sources" FILES ${LZ4LIB_SOURCES})

if(MSVC)
ADD_DEFINITIONS("-W4")
endif()
if(GNU_COMPATIBLE_COMPILER)
ADD_DEFINITIONS("-Wall")
endif()
if(CMAKE_COMPILER_IS_GNUCXX)
ADD_DEFINITIONS("-Wextra")
ADD_DEFINITIONS("-Wundef")
ADD_DEFINITIONS("-Wshadow")
ADD_DEFINITIONS("-Wcast-align")
ADD_DEFINITIONS("-Wstrict-prototypes")
endif(CMAKE_COMPILER_IS_GNUCXX)
if(GNU_COMPATIBLE_COMPILER AND
   (NOT CMAKE_SYSTEM_NAME MATCHES "SunOS"))
ADD_DEFINITIONS("-std=c99")
endif()

set(CPACK_PACKAGE_VERSION_PATCH r128)
ADD_DEFINITIONS("-DLZ4_VERSION=\"${CPACK_PACKAGE_VERSION_PATCH}\"")

if( UNIX AND NOT APPLE )
  set_target_properties(lz4lib PROPERTIES COMPILE_FLAGS "-fPIC")
endif()
