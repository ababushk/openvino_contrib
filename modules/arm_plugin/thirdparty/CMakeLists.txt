# Copyright (C) 2020-2022 Intel Corporation
# SPDX-License-Identifier: Apache-2.0
#

if(ARM_COMPUTE_INCLUDE_DIR OR ARM_COMPUTE_LIB_DIR)
    if (NOT ARM_COMPUTE_INCLUDE_DIR)
        message(FATAL_ERROR "Undefined ARM_COMPUTE_INCLUDE_DIR input variable should be set manually")
    else()
        message(STATUS "Using ${ARM_COMPUTE_INCLUDE_DIR} to include arm compute library headers")
    endif()

    if (NOT ARM_COMPUTE_LIB_DIR)
        message(FATAL_ERROR "Undefined ARM_COMPUTE_LIB_DIR input variable should be set manually")
    else()
        find_library(
            ARM_COMPUTE_LIB
            arm_compute-static
            PATHS ${ARM_COMPUTE_LIB_DIR}
        )
        message(STATUS "Found arm_compute-static: ${ARM_COMPUTE_LIB}")
        add_library(arm_compute STATIC IMPORTED GLOBAL)
        set_target_properties(arm_compute PROPERTIES
            IMPORTED_LOCATION ${ARM_COMPUTE_LIB}
            INTERFACE_INCLUDE_DIRECTORIES ${ARM_COMPUTE_INCLUDE_DIR})
        find_library(
            ARM_COMPUTE_CORE_LIB
            arm_compute_core-static
            PATHS ${ARM_COMPUTE_LIB_DIR}
        )
        message(STATUS "Found arm_compute_core-static: ${ARM_COMPUTE_CORE_LIB}")
        add_library(arm_compute_core STATIC IMPORTED GLOBAL)
        set_target_properties(arm_compute_core PROPERTIES
            IMPORTED_LOCATION ${ARM_COMPUTE_CORE_LIB}
            INTERFACE_INCLUDE_DIRECTORIES ${ARM_COMPUTE_INCLUDE_DIR})
    endif()

    add_library(half INTERFACE IMPORTED GLOBAL)
    set_target_properties(half PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES  ${ARM_COMPUTE_INCLUDE_DIR})
else()
    message(STATUS "Configure to build ${CMAKE_CURRENT_SOURCE_DIR}/ComputeLibrary")

    find_host_program(SCONS scons)

    if(NOT SCONS)
        message(FATAL_ERROR "Scons tool is not found!")
    endif()

    set(ARM_COMPURE_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ComputeLibrary)
    file(GLOB_RECURSE SOURCES
        ${ARM_COMPURE_SOURCE_DIR}/*.cpp
        ${ARM_COMPURE_SOURCE_DIR}/*.hpp
        ${ARM_COMPURE_SOURCE_DIR}/*.h
    )

    set(extra_cxx_flags "-fPIC ${CMAKE_CXX_FLAGS} -Wno-undef")

    set(ARM_COMPUTE_OPTIONS
        neon=1
        opencl=0
        openmp=0
        cppthreads=1
        examples=0
        Werror=0
        gemm_tuner=0
        reference_openmp=0
        validation_tests=0
        benchmark_tests=0
        # TODO: check this for Apple Silicon
        # multi_isa=1
        # TODO: use CC for ARM compute library to minimize binary size
        # build_config=<file>
        # TODO: use data_type_support to disable useless kernels
        # TODO: switch to NHWC layout later
        data_layout_support=nchw
        build_dir=${CMAKE_CURRENT_BINARY_DIR}
        install_dir=${CMAKE_CURRENT_BINARY_DIR}/install
        arch=${ARM_COMPUTE_TARGET_ARCH}
    )

    if(ARM_COMPUTE_SCONS_JOBS)
        list(APPEND ARM_COMPUTE_OPTIONS --jobs=${ARM_COMPUTE_SCONS_JOBS})
    endif()

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        list(APPEND ARM_COMPUTE_OPTIONS
             debug=1
             asserts=1
             logging=1)
    endif()

    if(EMSCRIPTEN)
        list(APPEND ARM_COMPUTE_OPTIONS os=linux)
    elseif(ANDROID)
        list(APPEND ARM_COMPUTE_OPTIONS os=android)
    elseif(APPLE)
        list(APPEND ARM_COMPUTE_OPTIONS os=macos)
    endif()

    if(CMAKE_CROSSCOMPILING)
        list(APPEND ARM_COMPUTE_OPTIONS build=cross_compile)
    else()
        list(APPEND ARM_COMPUTE_OPTIONS build=native)
    endif()

    if (CMAKE_CXX_COMPILER_LAUNCHER)
        list(APPEND ARM_COMPUTE_OPTIONS compiler_cache=${CMAKE_CXX_COMPILER_LAUNCHER})
    endif()

    # used to build for yocto
    if(ARM_COMPUTE_TOOLCHAIN_PREFIX)
        list(APPEND ARM_COMPUTE_OPTIONS toolchain_prefix=${ARM_COMPUTE_TOOLCHAIN_PREFIX})
    endif()

    if(ANDROID)
        if(ANDROID_PLATFORM_LEVEL LESS 18)
            message(FATAL_ERROR "ARM compute library requires Android API 18 level and higher"
                                "Please, speficy -DANDROID_PLATFORM=android-18 at least")
        endif()

        if(ANDROID_NDK_REVISION LESS "23.0")
            list(APPEND ARM_COMPUTE_OPTIONS toolchain_prefix="${ANDROID_TOOLCHAIN_PREFIX}")
        else()
            string(REGEX REPLACE "/bin/[^/]+-" "/bin/llvm-" ANDROID_TOOLCHAIN_PREFIX_FIXED "${ANDROID_TOOLCHAIN_PREFIX}")
            message(STATUS "SCONS: using ANDROID_TOOLCHAIN_PREFIX=${ANDROID_TOOLCHAIN_PREFIX_FIXED}")
            list(APPEND ARM_COMPUTE_OPTIONS toolchain_prefix="${ANDROID_TOOLCHAIN_PREFIX_FIXED}")
        endif()

        list(APPEND ARM_COMPUTE_OPTIONS
            compiler_prefix="${ANDROID_TOOLCHAIN_ROOT}/bin/")

        set(extra_flags "${extra_flags} --target=${ANDROID_LLVM_TRIPLE}")
        set(extra_flags "${extra_flags} --gcc-toolchain=${ANDROID_TOOLCHAIN_ROOT}")
        set(extra_flags "${extra_flags} --sysroot=${CMAKE_SYSROOT}")

        set(extra_link_flags "${extra_link_flags} ${extra_flags}")
        set(extra_cxx_flags "${extra_cxx_flags} ${extra_flags}")
    elseif(EMSCRIPTEN)
        set(cmake_build_env
            CC=emcc
            CXX=em++
            RANLIB=emranlib
            AR=emar)
        # EMSDK: Passing any of -msse, -msse2, -msse3, -mssse3, -msse4.1, -msse4.2,
        # -msse4, -mavx, -mfpu=neon flags also requires passing -msimd128 (or -mrelaxed-simd)!
        set(extra_cxx_flags "${extra_cxx_flags} -msimd128")
        # clang-16: error: argument unused during compilation: '-mthumb' [-Werror,-Wunused-command-line-argument]
        # clang-16: error: argument unused during compilation: '-mfloat-abi=hard' [-Werror,-Wunused-command-line-argument]
        set(extra_cxx_flags "${extra_cxx_flags} \
            -Wno-unused-command-line-argument \
            -Wno-unknown-warning-option \
            -Wno-unused-function \
            -Wno-unused-but-set-variable")

        get_filename_component(toolchain_prefix "${CMAKE_CXX_COMPILER}" DIRECTORY)
        list(APPEND ARM_COMPUTE_OPTIONS toolchain_prefix="${toolchain_prefix}/")
    elseif(APPLE)
        if(CMAKE_OSX_DEPLOYMENT_TARGET)
            set(extra_cxx_flags "${extra_cxx_flags} -mmacosx-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET}")
            set(minos_added ON)
        endif()

        if(HOST_X86_64)
            if(NOT minos_added)
                message(FATAL_ERROR "Please, specify either env MACOSX_DEPLOYMENT_TARGET or cmake CMAKE_OSX_DEPLOYMENT_TARGET variables")
            endif()
            set(extra_cxx_flags "${extra_cxx_flags} --sysroot ${CMAKE_OSX_SYSROOT}")
        endif()

        set(extra_cxx_flags "${extra_cxx_flags} -Wno-error=return-stack-address")
        get_filename_component(compiler_prefix "${CMAKE_CXX_COMPILER}" DIRECTORY)
        list(APPEND ARM_COMPUTE_OPTIONS compiler_prefix="${compiler_prefix}/")

        if(CMAKE_OSX_ARCHITECTURES)
            foreach(arch IN LISTS CMAKE_OSX_ARCHITECTURES)
                set(extra_cxx_flags "${extra_cxx_flags} -arch ${arch}")
            endforeach()
        endif()
    endif()

    if(ENABLE_LTO)
        if((CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR
            CMAKE_CXX_COMPILER_ID MATCHES "^(Apple)?Clang$")
           AND (NOT CMAKE_CROSSCOMPILING))
            set(extra_cxx_flags "${extra_cxx_flags} -flto=thin")
            set(extra_link_flags "${extra_link_flags} -flto=thin")
        endif()
    endif()

    if(extra_link_flags)
        list(APPEND ARM_COMPUTE_OPTIONS extra_link_flags=${extra_link_flags})
    endif()
    if(extra_cxx_flags)
        list(APPEND ARM_COMPUTE_OPTIONS extra_cxx_flags=${extra_cxx_flags})
    endif()

    if(NOT CMAKE_VERBOSE_MAKEFILE)
        list(APPEND ARM_COMPUTE_OPTIONS --silent)
    endif()

    set(arm_compute ${CMAKE_CURRENT_BINARY_DIR}/libarm_compute-static.a)
    set(arm_compute_core ${CMAKE_CURRENT_BINARY_DIR}/libarm_compute_core-static.a)

    add_custom_command(
        OUTPUT
            ${arm_compute}
            ${arm_compute_core}
        COMMAND ${CMAKE_COMMAND} -E env ${cmake_build_env}
            ${SCONS} ${ARM_COMPUTE_OPTIONS}
                ${arm_compute}
                ${arm_compute_core}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/ComputeLibrary
        COMMENT "Build Arm Compute Library"
        DEPENDS ${SOURCES}
                ${CMAKE_CURRENT_LIST_FILE}
                ${ARM_COMPURE_SOURCE_DIR}/SConscript
                ${ARM_COMPURE_SOURCE_DIR}/SConstruct
                )

    # Import targets

    add_custom_target(arm_compute_static_libs
        DEPENDS
            ${arm_compute}
            ${arm_compute_core}
    )

    add_library(arm_compute::arm_compute STATIC IMPORTED GLOBAL)
    set_target_properties(arm_compute::arm_compute PROPERTIES
        IMPORTED_LOCATION ${arm_compute}
        INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}/ComputeLibrary)
    add_dependencies(arm_compute::arm_compute arm_compute_static_libs)

    add_library(arm_compute::arm_compute_core STATIC IMPORTED GLOBAL)
    set_target_properties(arm_compute::arm_compute_core PROPERTIES
        IMPORTED_LOCATION ${arm_compute_core}
        INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}/ComputeLibrary)
    add_dependencies(arm_compute::arm_compute_core arm_compute_static_libs)

    add_library(arm_compute::half INTERFACE IMPORTED GLOBAL)
    set_target_properties(arm_compute::half PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}/ComputeLibrary/include)
endif()
