# ---< Z S T D >---

SET(LIBRARY_DIR lib)

set(ZSTDLIB_HEADERS
  ${LIBRARY_DIR}/common/bitstream.h
  ${LIBRARY_DIR}/common/error_private.h
  ${LIBRARY_DIR}/common/error_public.h
  ${LIBRARY_DIR}/common/fse.h
  ${LIBRARY_DIR}/common/huf.h
  ${LIBRARY_DIR}/common/mem.h
  ${LIBRARY_DIR}/common/zbuff.h
  ${LIBRARY_DIR}/common/zstd_internal.h
  ${LIBRARY_DIR}/zstd.h
  ${LIBRARY_DIR}/dictBuilder/zdict.h
)

set(ZSTDLIB_SOURCES
  ${LIBRARY_DIR}/common/entropy_common.c
  ${LIBRARY_DIR}/common/zstd_common.c
  ${LIBRARY_DIR}/common/xxhash.c
  ${LIBRARY_DIR}/common/fse_decompress.c
  ${LIBRARY_DIR}/compress/fse_compress.c
  ${LIBRARY_DIR}/compress/huf_compress.c
  ${LIBRARY_DIR}/compress/zbuff_compress.c
  ${LIBRARY_DIR}/compress/zstd_compress.c
  ${LIBRARY_DIR}/decompress/huf_decompress.c
  ${LIBRARY_DIR}/decompress/zbuff_decompress.c
  ${LIBRARY_DIR}/decompress/zstd_decompress.c
  ${LIBRARY_DIR}/dictBuilder/divsufsort.c
  ${LIBRARY_DIR}/dictBuilder/zdict.c
)

include_directories(${CMAKE_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${LIBRARY_DIR} ${LIBRARY_DIR}/common)

link_directories( ${PRECOMPILED_LIB_DIR} )

add_library(zstdlib STATIC ${ZSTDLIB_HEADERS} ${ZSTDLIB_SOURCES})
set_target_properties(zstdlib PROPERTIES FOLDER "3rdparty")

set_target_properties(zstdlib PROPERTIES COMPILE_FLAGS "${G_CPP_FLAGS}")
set_target_properties(zstdlib PROPERTIES LINK_FLAGS "${G_LINK_FLAGS}")
set_target_properties(zstdlib PROPERTIES DEBUG_POSTFIX "_d")

source_group("Headers" FILES ${ZSTDLIB_HEADERS})
source_group("Sources" FILES ${ZSTDLIB_SOURCES})

IF (MSVC)
    SET_TARGET_PROPERTIES(zstdlib PROPERTIES COMPILE_DEFINITIONS "ZSTD_HEAPMODE=0;_CRT_SECURE_NO_WARNINGS")
ENDIF (MSVC)

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

if( UNIX AND NOT APPLE )
  set_target_properties(zstdlib PROPERTIES COMPILE_FLAGS "-fPIC")
endif()
