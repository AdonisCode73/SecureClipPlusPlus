cmake_minimum_required(VERSION 3.14)
project(SecureClipPlusPlus VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(ExternalProject)

# ============================
# Source files
# ============================
set(SRC_FILES
    src/Main.cpp
)

foreach(FILE_PATH IN LISTS SRC_FILES)
    if(NOT EXISTS "${CMAKE_SOURCE_DIR}/${FILE_PATH}")
        message(FATAL_ERROR "Source file listed but not found: ${FILE_PATH}")
    endif()
endforeach()

# ============================
# Setup paths
# ============================
set(SODIUM_INSTALL_DIR ${CMAKE_SOURCE_DIR}/lib)
set(SODIUM_SRC_DIR     ${CMAKE_BINARY_DIR}/libsodium_ep-prefix/src/libsodium_ep)
set(SODIUM_TP_INC      ${CMAKE_SOURCE_DIR}/include/thirdparty/sodium)
set(SODIUM_TP_DIR      ${CMAKE_SOURCE_DIR}/include/thirdparty)

# ============================
# Build libsodium
# ============================
if (MSVC OR ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" AND CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC"))
    # ----- MSVC / Clang-CL -----
    ExternalProject_Add(libsodium_ep
        GIT_REPOSITORY https://github.com/jedisct1/libsodium.git
        GIT_TAG stable
        UPDATE_DISCONNECTED 1
        CONFIGURE_COMMAND ""  # no configure step for MSVC
        BUILD_COMMAND ${CMAKE_COMMAND} -E chdir ${SODIUM_SRC_DIR}/builds/msvc/vs2022
            MSBuild.exe libsodium.sln
            /p:Configuration=StaticRelease
            /p:Platform=x64
        INSTALL_COMMAND ${CMAKE_COMMAND} -P ${CMAKE_BINARY_DIR}/copy_sodium_lib.cmake
    )

    set(SODIUM_LIB_PATH "${SODIUM_INSTALL_DIR}/libsodium.lib")

    # Create helper script for copying the .lib and headers into include/thirdparty/sodium
    file(WRITE ${CMAKE_BINARY_DIR}/copy_sodium_lib.cmake
"file(GLOB_RECURSE CANDIDATES
  \"${SODIUM_SRC_DIR}/*.lib\")
if(NOT CANDIDATES)
  message(FATAL_ERROR \"Could not find libsodium.lib under: ${SODIUM_SRC_DIR}\")
endif()
list(GET CANDIDATES 0 LIB_PATH)
message(STATUS \"Copying libsodium from: \${LIB_PATH}\")

# Copy library
file(MAKE_DIRECTORY \"${SODIUM_INSTALL_DIR}\")
file(COPY \"\${LIB_PATH}\" DESTINATION \"${SODIUM_INSTALL_DIR}\")

# Remove old sodium headers
file(REMOVE_RECURSE \"${SODIUM_TP_INC}\")

# Create thirdparty dir and copy fresh headers
file(MAKE_DIRECTORY \"${SODIUM_TP_DIR}\")
if(EXISTS \"${SODIUM_SRC_DIR}/src/libsodium/include/sodium.h\")
  file(COPY \"${SODIUM_SRC_DIR}/src/libsodium/include/sodium.h\" DESTINATION \"${SODIUM_TP_DIR}\")
endif()
if(EXISTS \"${SODIUM_SRC_DIR}/src/libsodium/include/sodium\")
  file(COPY \"${SODIUM_SRC_DIR}/src/libsodium/include/sodium\" DESTINATION \"${SODIUM_TP_DIR}\")
endif()
")

elseif (MINGW)
    # ----- MinGW -----
    ExternalProject_Add(libsodium_ep
        GIT_REPOSITORY https://github.com/jedisct1/libsodium.git
        GIT_TAG stable
        UPDATE_DISCONNECTED 1
        CONFIGURE_COMMAND bash -c "./configure --prefix=${SODIUM_INSTALL_DIR}"
        BUILD_COMMAND bash -c "make -j$(nproc)"
        INSTALL_COMMAND bash -c "make install"
    )

    set(SODIUM_LIB_PATH "${SODIUM_INSTALL_DIR}/libsodium.a")

else()
    message(FATAL_ERROR "Unsupported compiler on Windows. Use MSVC, Clang-CL, or MinGW.")
endif()

add_library(sodium STATIC IMPORTED)
set_target_properties(sodium PROPERTIES
  IMPORTED_LOCATION "${SODIUM_LIB_PATH}"
  INTERFACE_INCLUDE_DIRECTORIES "${SODIUM_TP_DIR}"
)
add_dependencies(sodium libsodium_ep)

# ============================
# Main executable
# ============================
add_executable(SecureClipPlusPlus ${SRC_FILES})

target_include_directories(SecureClipPlusPlus PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/thirdparty/json
    ${SODIUM_TP_DIR}  # now points to include/thirdparty
)

target_compile_definitions(SecureClipPlusPlus PRIVATE UNICODE _UNICODE SODIUM_STATIC)

target_link_libraries(SecureClipPlusPlus PRIVATE
  sodium
  iphlpapi
  kernel32
  user32
  gdi32
  advapi32
)