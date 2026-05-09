list(APPEND source_dmc
    dmc_utils.cc
    dmc_table.cc
    ib.cc
    nm.cc
    server_mm.cc
    client_mm.cc
    server.cc
    client.cc
    client_master.cc
    local_cache.cc
    debug.cc
)

add_library(libdmc SHARED ${source_dmc})

target_compile_definitions(libdmc PUBLIC
    HASH_BUCKET_ASSOC_NUM=${hash_bucket_assoc}LL
    HASH_NUM_BUCKETS=${hash_bucket_num}LL
    MAX_GROUP_SIZE=${max_group_size}LL
    GROUP_BYTES=${group_bytes}LL
    # Meta: 16, 8
    # Twitter053: 16 8
    NUM_THREAD_PER_SMM=${num_thread_per_smm}LL
    ALLOC_RETRY=${alloc_retry}LL
    MAX_SAMPLE_NUM=${max_sample_num}LL
    PENALTY_US=${penalty_us}LL
    NUM_CN=${num_cn}LL
    NUM_MN=${num_mn}LL)

if(eva_real_size)
    target_compile_definitions(libdmc PUBLIC EVA_REAL_SIZE)
endif()

target_compile_options(
    libdmc

    # PRIVATE
    PUBLIC

    # the following three flags are for debugging
    # ${CMAKE_CXX_FLAGS_DEBUG}
    # "-D_DEBUG"
    # "-DNDEBUG"
    # "-Og" # Use -Og for debugging, replace with -O2 for optimized build
    # "-g"
    "-O2"
)

target_link_libraries(libdmc PUBLIC ${Boost_LIBRARIES} ibverbs)
