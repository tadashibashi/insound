/// Platform, CPU, and Architecture defines
/// Check with #if, not #ifdef, since all variables are defined with 0 or 1
#pragma once

/// ===== Target platform definitions =========================================
/// Update these as more platforms are supported
#if defined(__APPLE__)                 ///< Apple (MacOS, iOS)
#   include <TargetConditionals.h>
#   define INSOUND_TARGET_APPLE 1
#   if TARGET_OS_OSX
#       define INSOUND_TARGET_MACOS 1
#   elif TARGET_OS_IOS
#       define INSOUND_TARGET_IOS 1
#   elif TARGET_OS_TV
#       define INSOUND_TARGET_TVOS 1
#   elif TARGET_OS_WATCH
#       define INSOUND_TARGET_WATCHOS 1
#   elif TARGET_OS_VISION
#       define INSOUND_TARGET_VISIONOS 1
#   elif TARGET_OS_SIMULATOR
#       define INSOUND_TARGET_APPLE_SIMULATOR 1
#   endif
#elif defined(_WIN32)                  ///< Windows
#   define INSOUND_TARGET_WINDOWS 1
#elif defined(__ANDROID__)             ///< Android
#   define INSOUND_TARGET_ANDROID 1
#elif defined(__linux__)               ///< Linux
#   define INSOUND_TARGET_LINUX 1
#elif defined(__EMSCRIPTEN__)          ///< Emscripten (WebAssembly)
#   define INSOUND_TARGET_EMSCRIPTEN 1
#endif

// Catch missing defines
#ifndef INSOUND_TARGET_APPLE
#   define INSOUND_TARGET_APPLE 0
#endif
#ifndef INSOUND_TARGET_MACOS
#   define INSOUND_TARGET_MACOS 0
#endif
#ifndef INSOUND_TARGET_IOS
#   define INSOUND_TARGET_IOS 0
#endif
#ifndef INSOUND_TARGET_TVOS
#   define INSOUND_TARGET_TVOS 0
#endif
#ifndef INSOUND_TARGET_WATCHOS
#   define INSOUND_TARGET_WATCHOS 0
#endif
#ifndef INSOUND_TARGET_VISIONOS
#   define INSOUND_TARGET_VISIONOS 0
#endif
#ifndef INSOUND_TARGET_APPLE_SIMULATOR
#   define INSOUND_TARGET_APPLE_SIMULATOR 0
#endif
#ifndef INSOUND_TARGET_WINDOWS
#   define INSOUND_TARGET_WINDOWS 0
#endif
#ifndef INSOUND_TARGET_ANDROID
#   define INSOUND_TARGET_ANDROID 0
#endif
#ifndef INSOUND_TARGET_LINUX
#   define INSOUND_TARGET_LINUX 0
#endif
#ifndef INSOUND_TARGET_EMSCRIPTEN
#   define INSOUND_TARGET_EMSCRIPTEN 0
#endif

/// ===== Compiler definitions ================================================
#if defined(__INTEL_COMPILER)           ///< Intel Compiler Classic (ICC)
#   define INSOUND_COMPILER_INTEL 1
#   define INSOUND_COMPILER_INTEL_ICC 1
#elif defined(__INTEL_LLVM_COMPILER)    ///< Intel Compiler oneAPI
#   define INSOUND_COMPILER_INTEL 1
#   define INSOUND_COMPILER_INTEL_ONEAPI 1
#   if defined(SYCL_LANGUAGE_VERSION)
#       define INSOUND_COMPILER_INTEL_ONEAPI_DPCPP 1
#   else
#       define INSOUND_COMPILER_INTEL_ONEAPI_CPP 1
#   endif
#elif defined(_MSC_VER)                 ///< Microsoft MSVC
#   define INSOUND_COMPILER_MSVC 1
#elif defined(__clang__)                ///< Clang
#   define INSOUND_COMPILER_CLANG 1
#   if defined(__apple_build_version__)
        // Distinguishes between regular clang
#       define INSOUND_COMPILER_APPLE_CLANG 1
#   endif
#elif defined(__GNUC__)
#   define INSOUND_COMPILER_GCC 1
#endif

// Catch non-defined macros
#ifndef INSOUND_COMPILER_INTEL
#   define INSOUND_COMPILER_INTEL 0
#endif

#ifndef INSOUND_COMPILER_INTEL_ICC
#   define INSOUND_COMPILER_INTEL_ICC 0
#endif

#ifndef INSOUND_COMPILER_INTEL_ONEAPI
#   define INSOUND_COMPILER_INTEL_ONEAPI 0
#endif

#ifndef INSOUND_COMPILER_INTEL_ONEAPI_DPCPP
#   define INSOUND_COMPILER_INTEL_ONEAPI_DPCPP 0
#endif

#ifndef INSOUND_COMPILER_INTEL_ONEAPI_CPP
#   define INSOUND_COMPILER_INTEL_ONEAPI_CPP 0
#endif

#ifndef INSOUND_COMPILER_MSVC
#   define INSOUND_COMPILER_MSVC 0
#endif

#ifndef INSOUND_COMPILER_CLANG
#   define INSOUND_COMPILER_CLANG 0
#endif

#ifndef INSOUND_COMPILER_APPLE_CLANG
#   define INSOUND_COMPILER_APPLE_CLANG 0
#endif

#ifndef INSOUND_COMPILER_GCC
#   define INSOUND_COMPILER_GCC 0
#endif

/// ===== Architecture definitions ===============================================
#if defined(__ARM_ARCH_2__)
#   define INSOUND_ARCH_ARM 1
#   define INSOUND_ARCH_ARM2 1

#elif defined(__ARM_ARCH_3__) || defined(__ARM_ARCH_3M__)
#   define INSOUND_ARCH_ARM 1
#   define INSOUND_ARCH_ARM3 1

#elif defined(__ARM_ARCH_4__)
#   define INSOUND_ARCH_ARM 1
#   define INSOUND_ARCH_ARM4 1

#elif defined(__ARM_ARCH_4T__) || defined(__TARGET_ARM_4T)
#   define INSOUND_ARCH_ARM 1
#   define INSOUND_ARCH_ARM4T 1

#elif defined(__ARM_ARCH_5__) || defined(__ARM_ARCH_5E__)
#   define INSOUND_ARCH_ARM 1
#   define INSOUND_ARCH_ARM5 1

#elif defined(__ARM_ARCH_5T__) || defined(__ARM_ARCH_5TE__) || defined(__ARM_ARCH_5TEJ__)
#   define INSOUND_ARCH_ARM 1
#   define INSOUND_ARCH_ARM5T 1

#elif defined(__ARM_ARCH_6__) || defined(__ARM_ARCH_6J__) || defined(__ARM_ARCH_6K__) || \
  defined(__ARM_ARCH_6Z__) || defined(__ARM_ARCH_6ZK__)
#   define INSOUND_ARCH_ARM 1
#   define INSOUND_ARCH_ARM6 1

#elif defined(__ARM_ARCH_6T2__) || defined(__ARM_ARCH_6T2__)
#   define INSOUND_ARCH_ARM 1
#   define INSOUND_ARCH_ARM6T2 1

#elif defined(__ARM_ARCH_7__) || defined(__ARM_ARCH_7A__) || defined(__ARM_ARCH_7R__) || \
  defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7S__)
#   define INSOUND_ARCH_ARM 1
#   define INSOUND_ARCH_ARM7 1

#elif defined(__ARM_ARCH_7A__) || defined(__ARM_ARCH_7R__) || defined(__ARM_ARCH_7M__) || \
  defined(__ARM_ARCH_7S__)
#   define INSOUND_ARCH_ARM 1
#   define INSOUND_ARCH_ARM7A 1

#elif defined(__ARM_ARCH_7R__) || defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7S__)
#   define INSOUND_ARCH_ARM 1
#   define INSOUND_ARCH_ARM7R 1

#elif defined(__ARM_ARCH_7M__)
#   define INSOUND_ARCH_ARM 1
#   define INSOUND_ARCH_ARM7M 1

#elif defined(__ARM_ARCH_7S__)
#   define INSOUND_ARCH_ARM 1
#   define INSOUND_ARCH_ARM7S 1

#elif defined(__aarch64__) || defined(_M_ARM64) || defined(__AARCH64EL__) || defined(__arm64)
#   define INSOUND_ARCH_ARM 1
#   define INSOUND_ARCH_ARM64 1

#elif defined(__arm__) || defined(__thumb__) || defined(__TARGET_ARCH_ARM) ||               \
  defined(__TARGET_ARCH_THUMB) || defined(__ARM) || defined(_M_ARM) || defined(_M_ARM_T) || \
  defined(__ARM_ARCH)
#   define INSOUND_ARCH_ARM 1
#endif

#if defined(__x86_64__) || defined(__x86_64) || defined(__amd64__) || defined(__amd64) || \
  defined(_M_X64) || defined(_M_AMD64)
#   define INSOUND_ARCH_X86_64 1
#endif

#if defined(i386) || defined(__i386) || defined(__i386__) || defined(__i486__) ||    \
  defined(__i586__) || defined(__i686__) || defined(__IA32__)|| defined(_M_I86) ||   \
  defined(_M_IX86)|| defined(__X86__) || defined(_X86_) || defined(__THW_INTEL__) || \
  defined(__I86__) || defined(__INTEL__) || defined(__386)
#   define INSOUND_ARCH_X86_32 1
#endif

#if defined(__x86_64__) || defined(_M_X64) || defined(__aarch64__) || defined(_M_ARM64)
  #define INSOUND_ARCH_64BIT 1
#else
  #define INSOUND_ARCH_64BIT 0
#endif

#if defined(__i386__) || defined(_M_IX86) || defined(__arm__) || defined(_M_ARM)
  #define INSOUND_ARCH_32BIT 1
#else
  #define INSOUND_ARCH_32BIT 0
#endif

// Catch missing defines
#ifndef INSOUND_ARCH_ARM2
#   define INSOUND_ARCH_ARM2 0
#endif
#ifndef INSOUND_ARCH_ARM3
#   define INSOUND_ARCH_ARM3 0
#endif
#ifndef INSOUND_ARCH_ARM4
#   define INSOUND_ARCH_ARM4 0
#endif
#ifndef INSOUND_ARCH_ARM4T
#   define INSOUND_ARCH_ARM4T 0
#endif
#ifndef INSOUND_ARCH_ARM5
#   define INSOUND_ARCH_ARM5 0
#endif
#ifndef INSOUND_ARCH_ARM5T
#   define INSOUND_ARCH_ARM5T 0
#endif
#ifndef INSOUND_ARCH_ARM6
#   define INSOUND_ARCH_ARM6 0
#endif
#ifndef INSOUND_ARCH_ARM6T2
#   define INSOUND_ARCH_ARM6T2 0
#endif
#ifndef INSOUND_ARCH_ARM7
#   define INSOUND_ARCH_ARM7 0
#endif
#ifndef INSOUND_ARCH_ARM7A
#   define INSOUND_ARCH_ARM7A 0
#endif
#ifndef INSOUND_ARCH_ARM7R
#   define INSOUND_ARCH_ARM7R 0
#endif
#ifndef INSOUND_ARCH_ARM7M
#   define INSOUND_ARCH_ARM7M 0
#endif
#ifndef INSOUND_ARCH_ARM7S
#   define INSOUND_ARCH_ARM7S 0
#endif
#ifndef INSOUND_ARCH_ARM64
#   define INSOUND_ARCH_ARM64 0
#endif
#ifndef INSOUND_ARCH_ARM
#   define INSOUND_ARCH_ARM 0
#endif
#ifndef INSOUND_ARCH_X86_64
#   define INSOUND_ARCH_X86_64 0
#endif
#ifndef INSOUND_ARCH_X86_32
#   define INSOUND_ARCH_X86_32 0
#endif

#if INSOUND_TARGET_MACOS || INSOUND_TARGET_WINDOWS || INSOUND_TARGET_LINUX
#   define INSOUND_TARGET_DESKTOP 1
#else
#   define INSOUND_TARGET_DESKTOP 0
#endif

#if INSOUND_TARGET_ANDROID || INSOUND_TARGET_IOS
#   define INSOUND_TARGET_MOBILE 1
#else
#   define INSOUND_TARGET_MOBILE 0
#endif

// No-op
#if INSOUND_COMPILER_MSVC
#define INSOUND_NOOP __noop
#else
#define INSOUND_NOOP do {} while(0)
#endif
