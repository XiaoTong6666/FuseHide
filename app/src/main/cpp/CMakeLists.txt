cmake_minimum_required(VERSION 3.22.1)

project(fusefixer)

add_library(fusefixer SHARED fusefixer.cpp)

target_compile_features(fusefixer PRIVATE cxx_std_20)

target_compile_options(
    fusefixer
    PRIVATE
    -Wall
    -Wextra
    -Werror
    -fvisibility=hidden
)

find_library(log-lib log)
find_library(dl-lib dl)

target_link_libraries(fusefixer ${log-lib} ${dl-lib})
