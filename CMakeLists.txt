cmake_minimum_required(VERSION 3.16 FATAL_ERROR)

project(DMC LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()
find_package(Boost REQUIRED)

set(CMAKE_BUILD_TYPE Release)

set(penalty_us 0 CACHE STRING "set PENALTY_US")
set(hash_bucket_assoc 8 CACHE STRING "set HASH_BUCKET_ASSOC_NUM")
set(hash_bucket_num 40960 CACHE STRING "set HASH_NUM_BUCKETS")
set(max_group_size 512 CACHE STRING "set MAX_GROUP_SIZE")
set(group_bytes 16384 CACHE STRING "set GROUP_BYTES")
set(eva_real_size OFF CACHE BOOL "set EVA_REAL_SIZE")
set(num_thread_per_smm 128 CACHE STRING "set NUM_THREAD_PER_SMM")
set(alloc_retry 0 CACHE STRING "set ALLOC_RETRY")
set(max_sample_num 32 CACHE STRING "set MAX_SAMPLE_NUM")
set(num_cn 1 CACHE STRING "set NUM_CN")
set(num_mn 1 CACHE STRING "set NUM_MN")

include_directories(src)
add_subdirectory(src)

message("build FORGE")
message("max_group_size=${max_group_size}")
message("group_bytes=${group_bytes}")
message("num_thread_per_smm=${num_thread_per_smm}")
message("alloc_retry=${alloc_retry}")
message("max_sample_num=${max_sample_num}")
message("num_cn=${num_cn}")
message("num_mn=${num_mn}")

if(eva_real_size)
    message("set EVA_REAL_SIZE")
else()
    message("unset EVA_REAL_SIZE")
endif()


message("penalty_us=${penalty_us}")
message("hash_bucket_assoc=${hash_bucket_assoc}")
message("hash_bucket_num=${hash_bucket_num}")

add_subdirectory(experiments)
# enable_testing()
# add_subdirectory(tests)
