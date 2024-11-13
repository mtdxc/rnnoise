cmake_minimum_required(VERSION 3.6)
project(RnNoise LANGUAGES C CXX)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
if(MSVC)
# for M_PI define
add_definitions(-D_USE_MATH_DEFINES)
endif()

include_directories(include/)
include_directories(src/)
set(RN_NOISE_SRC
        src/celt_lpc.c
        src/denoise.c
        src/kiss_fft.c
        src/nnet.c
        src/nnet_default.c
        src/parse_lpcnet_weights.c
        src/pitch.c
        src/rnn.c
        src/rnnoise_tables.c)

if(BUILD_RTCD)
        add_compile_definitions(CPU_INFO_BY_ASM)
        add_compile_definitions(RNN_ENABLE_X86_RTCD)
        if(MSVC)
                set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /arch:AVX /arch:AVX2 /arch:FMA")
                set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /arch:SSE4.1")
        else()
                set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mavx -mfma -mavx2")
                set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -msse4.1")
        endif()
        list(APPEND RN_NOISE_SRC
                src/x86/x86cpu.c
                src/x86/x86_dnn_map.c
                src/x86/nnet_avx2.c
                src/x86/nnet_sse4_1.c
        )
endif()

add_library(RnNoise STATIC ${RN_NOISE_SRC} src/rnnoise_data.c)

add_executable(write_weights src/write_weights.c)
target_compile_definitions(write_weights PUBLIC DUMP_BINARY_WEIGHTS)

add_executable(dump_features src/dump_features.c ${RN_NOISE_SRC})
target_compile_definitions(dump_features PUBLIC TRAINING)

add_executable(dump_rnnoise_tables src/dump_rnnoise_tables.c src/kiss_fft.c)

add_executable(loopback examples/loopback.cc)
target_link_libraries(loopback PRIVATE RnNoise)

add_executable(rnnoise_demo examples/rnnoise_demo.c)
target_link_libraries(rnnoise_demo PRIVATE RnNoise)

add_library(RnNoiseFile STATIC ${RN_NOISE_SRC} src/rnnoise_data_mini.c)
target_compile_definitions(RnNoiseFile PUBLIC USE_WEIGHTS_FILE)

add_executable(rnnoise_demo_file examples/rnnoise_demo.c)
target_compile_definitions(rnnoise_demo_file PUBLIC USE_WEIGHTS_FILE)
target_link_libraries(rnnoise_demo_file PRIVATE RnNoiseFile)

# Disable all warnings, since it's an external library.
target_compile_options(RnNoise PRIVATE
        $<$<OR:$<C_COMPILER_ID:Clang>,$<C_COMPILER_ID:AppleClang>,$<C_COMPILER_ID:GNU>>:
        -w>
        $<$<CXX_COMPILER_ID:MSVC>:
        /w>)

