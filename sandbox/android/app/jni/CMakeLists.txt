cmake_minimum_required(VERSION 3.6)

project(GAME)

# armeabi-v7a requires cpufeatures library
# include(AndroidNdkModules)
# android_ndk_import_module_cpufeatures()

# App sources are in a subfolder named "insound"
# The top-level CMake file should automatically create a symlink on configure.
set(INSOUND_BUILD_EXAMPLES ON)
add_subdirectory(insound)

# Your app and its CMakeLists.txt are in a subfolder named "src"
add_subdirectory(src)

