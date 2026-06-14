cmake_minimum_required(VERSION 3.22.1)

project(fusehide)

set(FUSEHIDE_COMMON_COMPILE_OPTIONS
    -Wall
    -Wextra
    -Werror
)

set(FUSEHIDE_HIDDEN_COMPILE_OPTIONS
    -fvisibility=hidden
)

add_library(
    fusehide
    SHARED
    fusehide.cpp
    fusehide/core/anchor_state.cpp
    fusehide/core/runtime_state.cpp
    fusehide/elf/elf_utils.cpp
    fusehide/filters/dirent_filter.cpp
    fusehide/filters/reply_buf_filter.cpp
    fusehide/hooks/payload_builtin.cpp
    fusehide/hooks/fuse_wrappers.cpp
    fusehide/hooks/hook_install.cpp
    fusehide/jni/jni_entry.cpp
    fusehide/policy/path_policy.cpp
    fusehide/policy/unicode_policy.cpp
    third_party/xz-embedded/xz_dec_stream.c
    third_party/xz-embedded/xz_dec_lzma2.c
    third_party/xz-embedded/xz_crc32.c
)

target_compile_features(fusehide PRIVATE cxx_std_20)

target_compile_options(
    fusehide
    PRIVATE
    ${FUSEHIDE_COMMON_COMPILE_OPTIONS}
    -fvisibility=default
)

target_include_directories(
    fusehide
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    third_party/xz-embedded
)

find_library(log-lib log)
find_library(dl-lib dl)
find_library(icu-lib icu)

if(NOT icu-lib)
    message(FATAL_ERROR "libicu.so not found")
endif()

target_link_libraries(fusehide ${log-lib} ${dl-lib} ${icu-lib})

add_library(
    fusehide_payload
    SHARED
    fusehide/hooks/payload_builtin.cpp
)

target_compile_features(fusehide_payload PRIVATE cxx_std_20)

target_compile_options(
    fusehide_payload
    PRIVATE
    ${FUSEHIDE_COMMON_COMPILE_OPTIONS}
    ${FUSEHIDE_HIDDEN_COMPILE_OPTIONS}
)

target_include_directories(
    fusehide_payload
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

set_target_properties(fusehide_payload PROPERTIES OUTPUT_NAME "fusehide_payload")

target_link_libraries(fusehide_payload fusehide)
