cmake_minimum_required(VERSION 3.22.1)

project(fusehide)

add_library(
    fusehide
    SHARED
    fusehide.cpp
    fusehide/core/runtime_state.cpp
    fusehide/elf/elf_utils.cpp
    fusehide/filters/dirent_filter.cpp
    fusehide/filters/reply_buf_filter.cpp
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
    -Wall
    -Wextra
    -Werror
    -fvisibility=hidden
    $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
    $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
)

target_link_options(
    fusehide
    PRIVATE
    -Wl,--as-needed
)

if(NOT CMAKE_BUILD_TYPE MATCHES "Debug")
    target_compile_options(fusehide PRIVATE
        -Oz
        -flto=full
        -ffunction-sections
        -fdata-sections
        $<$<COMPILE_LANGUAGE:CXX>:-fno-unwind-tables>
        $<$<COMPILE_LANGUAGE:CXX>:-fno-asynchronous-unwind-tables>
        -fno-ident
        -fno-math-errno
        -fomit-frame-pointer
    )

    target_link_options(fusehide PRIVATE
        -flto=full
        -Wl,--gc-sections
        -Wl,--icf=all
        -Wl,--exclude-libs,ALL
        -Wl,-s
        -Wl,--hash-style=gnu
        -Wl,--build-id=none
        -Wl,-Bsymbolic
    )
endif()

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


# Vector/LSPosed retains native_init's onModuleLoaded function pointer for
# the process lifetime and provides no callback unregistration. Physical
# Unhook alone cannot prove that a racing loader thread has stopped holding
# this module's callback PC. Keep its text/data mapped even after dlclose.
target_link_options(fusehide PRIVATE -Wl,-z,max-page-size=16384 -Wl,-z,nodelete)

# The fork exports a single "dobby" target; embed its static variant in libzygisk.so.
set(DOBBY_GENERATE_SHARED OFF CACHE BOOL "Build Dobby statically for Zygisk" FORCE)
if(ANDROID_ABI STREQUAL "arm64-v8a")
    set(NearBranch ON CACHE BOOL "Require available ARM64 near trampoline backend" FORCE)
endif()
add_subdirectory(third_party/Dobby)

add_library(
    zygisk
    SHARED
    zygisk/module.cpp
    zygisk/dex_loader.cpp
    zygisk/dlopen_monitor.cpp
    zygisk/native_hook_adapter.cpp
    fusehide/elf/elf_utils.cpp
    third_party/xz-embedded/xz_dec_stream.c
    third_party/xz-embedded/xz_dec_lzma2.c
    third_party/xz-embedded/xz_crc32.c
)

target_compile_features(zygisk PRIVATE cxx_std_20)

target_compile_options(
    zygisk
    PRIVATE
    -Wall
    -Wextra
    -Werror
    -fvisibility=hidden
    $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
    $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
)

target_include_directories(
    zygisk
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    third_party/Dobby/include
    third_party/xz-embedded
    zygisk/include
)

target_link_libraries(zygisk dobby ${log-lib} ${dl-lib})
target_link_options(zygisk PRIVATE -Wl,-z,max-page-size=16384)

option(FUSEHIDE_NATIVE_HOOK_REVIEW_TEST "Build standalone native v2 ownership regression" OFF)
if(FUSEHIDE_NATIVE_HOOK_REVIEW_TEST)
    add_executable(fusehide_native_hook_review
        tests/native_hook_adapter_test.cpp
        zygisk/native_hook_adapter.cpp
    )
    target_compile_features(fusehide_native_hook_review PRIVATE cxx_std_20)
    target_include_directories(fusehide_native_hook_review PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        third_party/Dobby/include
    )
    target_link_libraries(fusehide_native_hook_review PRIVATE dobby dl)
    if(ANDROID)
        target_link_libraries(fusehide_native_hook_review PRIVATE log)
    endif()
    target_link_options(fusehide_native_hook_review PRIVATE -Wl,--wrap=DobbyCodePatch)
endif()

option(FUSEHIDE_LINKER_REVIEW_TEST "Build isolated Android do_dlopen smoke test" OFF)
if(FUSEHIDE_LINKER_REVIEW_TEST AND ANDROID)
    add_executable(fusehide_linker_review
        tests/linker_monitor_test.cpp
        zygisk/dlopen_monitor.cpp
        zygisk/native_hook_adapter.cpp
        fusehide/elf/elf_utils.cpp
        third_party/xz-embedded/xz_dec_stream.c
        third_party/xz-embedded/xz_dec_lzma2.c
        third_party/xz-embedded/xz_crc32.c
    )
    target_compile_features(fusehide_linker_review PRIVATE cxx_std_20)
    target_compile_definitions(fusehide_linker_review PRIVATE FUSEHIDE_LINKER_REVIEW_TEST=1)
    target_include_directories(fusehide_linker_review PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        third_party/Dobby/include
        third_party/xz-embedded
        zygisk/include
    )
    target_link_libraries(fusehide_linker_review PRIVATE dobby log dl)
    target_link_options(fusehide_linker_review PRIVATE -Wl,--wrap=DobbyCodePatch)
endif()

option(FUSEHIDE_CALLBACK_LIFETIME_TEST "Build native module callback lifetime review" OFF)
if(FUSEHIDE_CALLBACK_LIFETIME_TEST AND ANDROID)
    add_executable(fusehide_callback_lifetime_review
        tests/native_callback_lifetime_test.cpp
        zygisk/native_hook_adapter.cpp
    )
    target_compile_features(fusehide_callback_lifetime_review PRIVATE cxx_std_20)
    target_include_directories(fusehide_callback_lifetime_review PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    target_link_libraries(fusehide_callback_lifetime_review PRIVATE dobby dl log)
    add_executable(fusehide_callback_gate_review
        tests/native_callback_gate_test.cpp
    )
    target_compile_features(fusehide_callback_gate_review PRIVATE cxx_std_20)
    target_include_directories(fusehide_callback_gate_review PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
endif()
