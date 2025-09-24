if(${CMAKE_CURRENT_SOURCE_DIR} STREQUAL ${CMAKE_CURRENT_BINARY_DIR})
  message(FATAL_ERROR "Prevented in-tree build. Please create a build directory outside of the SDL source code and run \"cmake -S ${CMAKE_SOURCE_DIR} -B .\" from there")
endif()

cmake_minimum_required(VERSION 3.0.0)
project(SDL2 C CXX)

# ---------------------------------------------------------------------------
# Optimization flags for Cortex-A7
# ---------------------------------------------------------------------------
set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)

add_compile_options(
    -march=armv7-a
    -mfpu=neon-vfpv4
    -mfloat-abi=hard
    -O3
    -ffast-math
    -fdata-sections
    -ffunction-sections
    -fomit-frame-pointer
)

# Enable IPO/LTO policy
cmake_policy(SET CMP0069 NEW)

# Enable Link-Time Optimization (LTO) if supported
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)

set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--gc-sections,--as-needed")

set(SDL_SHARED ON CACHE BOOL "Build SDL2 as shared" FORCE)
set(SDL_STATIC OFF CACHE BOOL "Disable static SDL2" FORCE)
# ---------------------------------------------------------------------------

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
  set(SDL2_SUBPROJECT OFF)
else()
  set(SDL2_SUBPROJECT ON)
endif()

set(EXTRA_LIBS)
set(EXTRA_LDFLAGS)

# This is a virtual "library" that just exists to collect up compiler and
#  linker options that used to be global to this CMake project. When you
#  specify it as part of a real library's target_link_libraries(), that
#  library will also gain all those build options too. This is meant to
#  modularize old calls to the global add_definitions and include_directories,
#  etc. See https://github.com/libsdl-org/SDL/issues/4150
add_library(sdl-build-options INTERFACE)

# CMake 3.0 expands the "if(${A})" in "set(OFF 1);set(A OFF);if(${A})" to "if(1)"
# CMake 3.24+ emits a warning when not set.
unset(OFF)
unset(ON)
if(POLICY CMP0054)
  cmake_policy(SET CMP0054 NEW)
endif()

# !!! FIXME: this should probably do "MACOSX_RPATH ON" as a target property
# !!! FIXME:  for the SDL2 shared library (so you get an
# !!! FIXME:  install_name ("soname") of "@rpath/libSDL-whatever.dylib"
# !!! FIXME:  instead of "/usr/local/lib/libSDL-whatever.dylib"), but I'm
# !!! FIXME:  punting for now and leaving the existing behavior. Until this
# !!! FIXME:  properly resolved, this line silences a warning in CMake 3.0+.
# !!! FIXME:  remove it and this comment entirely once the problem is
# !!! FIXME:  properly resolved.
#cmake_policy(SET CMP0042 OLD)

include(CheckLibraryExists)
include(CheckIncludeFiles)
include(CheckIncludeFile)
include(CheckLanguage)
include(CheckSymbolExists)
include(CheckCSourceCompiles)
include(CheckCSourceRuns)
include(CheckCCompilerFlag)
include(CheckCXXCompilerFlag)
include(CheckStructHasMember)
include(CMakeDependentOption)
include(CMakePushCheckState)
include(GNUInstallDirs)

find_package(PkgConfig)

list(APPEND CMAKE_MODULE_PATH "${SDL2_SOURCE_DIR}/cmake")
include(${SDL2_SOURCE_DIR}/cmake/macros.cmake)
include(${SDL2_SOURCE_DIR}/cmake/sdlchecks.cmake)
include(${SDL2_SOURCE_DIR}/cmake/CheckCPUArchitecture.cmake)

# Enable large file support on 32-bit glibc, so that we can access files
# with large inode numbers
check_symbol_exists("__GLIBC__" "stdlib.h" LIBC_IS_GLIBC)
if (LIBC_IS_GLIBC)
    add_definitions(-D_FILE_OFFSET_BITS=64)
endif()

# See docs/release_checklist.md
set(SDL_MAJOR_VERSION 2)
set(SDL_MINOR_VERSION 26)
set(SDL_MICRO_VERSION 5)
set(SDL_VERSION "${SDL_MAJOR_VERSION}.${SDL_MINOR_VERSION}.${SDL_MICRO_VERSION}")

# Set defaults preventing destination file conflicts
set(SDL_CMAKE_DEBUG_POSTFIX "d" CACHE STRING "Name suffix for debug builds")

mark_as_advanced(CMAKE_IMPORT_LIBRARY_SUFFIX SDL_CMAKE_DEBUG_POSTFIX)

# Calculate a libtool-like version number
math(EXPR SDL_BINARY_AGE "${SDL_MINOR_VERSION} * 100 + ${SDL_MICRO_VERSION}")
# Stable branch, 2.24.1 -> libSDL2-2.0.so.0.2400.1
set(SDL_INTERFACE_AGE ${SDL_MICRO_VERSION})

# Increment this if there is an incompatible change - but if that happens,
# we should rename the library from SDL2 to SDL3, at which point this would
# reset to 0 anyway.
set(LT_MAJOR "0")

math(EXPR LT_AGE "${SDL_BINARY_AGE} - ${SDL_INTERFACE_AGE}")
math(EXPR LT_CURRENT "${LT_MAJOR} + ${LT_AGE}")
set(LT_REVISION "${SDL_INTERFACE_AGE}")
# For historical reasons, the library name redundantly includes the major
# version twice: libSDL2-2.0.so.0.
# TODO: in SDL 3, set the OUTPUT_NAME to plain SDL3, which will simplify
# it to libSDL3.so.0
set(LT_RELEASE "2.0")
set(LT_VERSION "${LT_MAJOR}.${LT_AGE}.${LT_REVISION}")

# The following should match the versions in the Xcode project file.
# Each version is 1 higher than you might expect, for compatibility
# with libtool: macOS ABI versioning is 1-based, unlike other platforms
# which are normally 0-based.
math(EXPR DYLIB_CURRENT_VERSION_MAJOR "${LT_MAJOR} + ${LT_AGE} + 1")
math(EXPR DYLIB_CURRENT_VERSION_MINOR "${LT_REVISION}")
set(DYLIB_CURRENT_VERSION "${DYLIB_CURRENT_VERSION_MAJOR}.${DYLIB_CURRENT_VERSION_MINOR}.0")
set(DYLIB_COMPATIBILITY_VERSION "${DYLIB_CURRENT_VERSION_MAJOR}.0.0")

# This list holds all generated headers.
# To avoid generating them twice, these are added to a dummy target on which all sdl targets depend.
set(SDL_GENERATED_HEADERS)

#message(STATUS "${LT_VERSION} :: ${LT_AGE} :: ${LT_REVISION} :: ${LT_CURRENT} :: ${LT_RELEASE}")

# General settings & flags
set(LIBRARY_OUTPUT_DIRECTORY "build")
# Check for 64 or 32 bit
set(ARCH_64 FALSE)
set(PROCESSOR_ARCH "ARM")
set(LIBNAME SDL2)
set(LIBTYPE SHARED)

# Get the platform
set(LINUX TRUE)
set(UNIX_SYS ON)

set(SDL_PTHREADS ON)
set(SDL_CLOCK_GETTIME ON)

# Compiler info
set(USE_GCC TRUE)
set(OPT_DEF_ASM TRUE)
set(OPT_DEF_GCC_ATOMICS ON)

# Default option knobs
set(OPT_DEF_LIBC ON)

if(NOT ("$ENV{CFLAGS}" STREQUAL ""))
  if(CMAKE_VERSION VERSION_LESS 3.11.0)
     message(WARNING "SDL's CMakeLists.txt no longer checks the CFLAGS environment.")
     message(WARNING "Please use CMake's CMAKE_C_FLAGS and CMAKE_BUILD_TYPE variables directly.")
     message(WARNING "Or upgrade to CMake >= 3.11.0, which respects the CFLAGS environment var.")
  endif()
endif()

# Those are used for pkg-config and friends, so that the sdl2.pc, sdl2-config,
# etc. are created correctly.
set(SDL_LIBS "-lSDL2")
set(SDL_CFLAGS )

set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} -D_GNU_SOURCE=1")

# Emscripten toolchain has a nonempty default value for this, and the checks
# in this file need to change that, so remember the original value, and
# restore back to that afterwards. For check_function_exists() to work in
# Emscripten, this value must be at its default value.
set(ORIG_CMAKE_REQUIRED_FLAGS ${CMAKE_REQUIRED_FLAGS})

# General includes
target_compile_definitions(sdl-build-options INTERFACE "-DUSING_GENERATED_CONFIG_H")
target_include_directories(sdl-build-options BEFORE INTERFACE "${SDL2_BINARY_DIR}/include" "${SDL2_BINARY_DIR}/include-config-$<LOWER_CASE:$<CONFIG>>")
# Note: The clang toolset for Visual Studio does not support the '-idirafter' option.
# !!! FIXME: do we _need_ to mess with CMAKE_C_FLAGS here?
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")

set(SDL_ATOMIC ON)
set(SDL_AUDIO ON)
set(SDL_VIDEO ON)
set(SDL_RENDER ON)
set(SDL_EVENTS ON)
set(SDL_POWER ON)
set(SDL_THREADS ON)
set(SDL_TIMERS ON)
set(SDL_FILE ON)
set(SDL_LOADSO ON)
set(SDL_CPUINFO ON)
set(SDL_FILESYSTEM ON)
set(SDL_LOCALE ON)
set(SDL_MISC ON)
set(SDL_JOYSTICK OFF)
set(SDL_HAPTIC OFF)
set(SDL_HIDAPI OFF)
set(SDL_SENSOR OFF)

set(SDL2_DISABLE_SDL2MAIN OFF)
set(SDL2_DISABLE_INSTALL ON)
set(SDL2_DISABLE_UNINSTALL ON)

set(SDL_LIBC ON)
set(SDL_GCC_ATOMICS ON)
set(SDL_ASSEMBLY ON)
set(SDL_SSEMATH OFF)
set(SDL_MMX OFF)
set(SDL_3DNOW OFF)
set(SDL_SSE OFF)
set(SDL_SSE2 OFF)
set(SDL_SSE3 OFF)
set(SDL_ALTIVEC OFF)
set(SDL_ARMSIMD ON)
set(SDL_ARMNEON ON)
set(SDL_DBUS OFF)
set(SDL_DISKAUDIO OFF)
set(SDL_DUMMYAUDIO OFF)
set(SDL_DIRECTFB OFF)
set(SDL_DIRECTFB_SHARED OFF)
set(SDL_FBCON ON)
set(SDL_DUMMYVIDEO OFF)
set(SDL_IBUS OFF)
set(SDL_SYSTEM_ICONV ON)
set(SDL_OPENGL OFF)
set(SDL_OPENGLES OFF)
set(SDL_PTHREADS ON)
set(SDL_PTHREADS_SEM ON)
set(SDL_OSS ON)
set(SDL_ALSA OFF)
set(SDL_ALSA_SHARED OFF)
set(SDL_JACK OFF)
set(SDL_JACK_SHARED OFF)
set(SDL_ESD OFF)
set(SDL_ESD_SHARED OFF)
set(SDL_PIPEWIRE OFF)
set(SDL_PIPEWIRE_SHARED OFF)
set(SDL_PULSEAUDIO OFF)
set(SDL_PULSEAUDIO_SHARED OFF)
set(SDL_ARTS OFF)
set(SDL_ARTS_SHARED OFF)
set(SDL_NAS OFF)
set(SDL_NAS_SHARED OFF)
set(SDL_SNDIO OFF)
set(SDL_SNDIO_SHARED OFF)
set(SDL_FUSIONSOUND OFF)
set(SDL_FUSIONSOUND_SHARED OFF)
set(SDL_LIBSAMPLERATE OFF)
set(SDL_LIBSAMPLERATE_SHARED OFF)
set(SDL_RPATH OFF)
set(SDL_CLOCK_GETTIME ON)
set(SDL_X11 OFF)
set(SDL_X11_SHARED OFF)
set(SDL_X11_Xcursor OFF)
set(SDL_X11_Xdbe OFF)
set(SDL_X11_XInput OFF)
set(SDL_X11_Xfixes OFF)
set(SDL_X11_Xrandr OFF)
set(SDL_X11_Xscrnsaver OFF)
set(SDL_X11_XShape OFF)
set(SDL_WAYLAND OFF)
set(SDL_WAYLAND_SHARED OFF)
set(SDL_WAYLAND_LIBDECOR OFF)
set(SDL_WAYLAND_LIBDECOR_SHARED OFF)
set(SDL_WAYLAND_QT_TOUCH OFF)
set(SDL_RPI OFF)
set(SDL_COCOA OFF)
set(SDL_DIRECTX OFF)
set(SDL_XINPUT OFF)
set(SDL_WASAPI OFF)
set(SDL_RENDER_D3D OFF)
set(SDL_RENDER_METAL OFF)
set(SDL_VIVANTE OFF)
set(SDL_VULKAN OFF)
set(SDL_METAL OFF)
set(SDL_KMSDRM OFF)
set(SDL_KMSDRM_SHARED OFF)
set(SDL_OFFSCREEN OFF)
set(SDL_BACKGROUNDING_SIGNAL OFF)
set(SDL_FOREGROUNDING_SIGNAL OFF)
set(SDL_HIDAPI OFF)
set(SDL_HIDAPI_LIBUSB OFF)
set(SDL_HIDAPI_JOYSTICK OFF)
set(SDL_VIRTUAL_JOYSTICK OFF)
set(SDL_ASAN OFF)
set(SDL_VENDOR_INFO pico)
set(SDL_CCACHE OFF)

option(SDL_WERROR "Enable -Werror" OFF)

set(SDL_SHARED ON)

set(SDL_STATIC OFF)
set(SDL_TEST OFF)
set(SDL_STATIC_PIC OFF)
set(SDL_TESTS OFF)
set(SDL_INSTALL_TESTS OFF)

set(HAVE_STATIC_PIC "${SDL_STATIC_PIC}")

# General source files
file(GLOB SOURCE_FILES
  ${SDL2_SOURCE_DIR}/src/*.c
  ${SDL2_SOURCE_DIR}/src/atomic/*.c
  ${SDL2_SOURCE_DIR}/src/audio/*.c
  ${SDL2_SOURCE_DIR}/src/cpuinfo/*.c
  ${SDL2_SOURCE_DIR}/src/dynapi/*.c
  ${SDL2_SOURCE_DIR}/src/events/*.c
  ${SDL2_SOURCE_DIR}/src/file/*.c
  ${SDL2_SOURCE_DIR}/src/libm/*.c
  ${SDL2_SOURCE_DIR}/src/locale/*.c
  ${SDL2_SOURCE_DIR}/src/misc/*.c
  ${SDL2_SOURCE_DIR}/src/power/*.c
  ${SDL2_SOURCE_DIR}/src/render/*.c
  ${SDL2_SOURCE_DIR}/src/render/*/*.c
  ${SDL2_SOURCE_DIR}/src/sensor/*.c
  ${SDL2_SOURCE_DIR}/src/stdlib/*.c
  ${SDL2_SOURCE_DIR}/src/thread/*.c
  ${SDL2_SOURCE_DIR}/src/timer/*.c
  ${SDL2_SOURCE_DIR}/src/video/*.c
  )

set(SDL_DEFAULT_ASSERT_LEVEL 0)
set(SDL_DEFAULT_ASSERT_LEVEL_CONFIGURED 0)
set(HAVE_ASSERTIONS ${SDL_ASSERTIONS})

if(NOT SDL_BACKGROUNDING_SIGNAL STREQUAL "OFF")
  target_compile_definitions(sdl-build-options INTERFACE "-DSDL_BACKGROUNDING_SIGNAL=${SDL_BACKGROUNDING_SIGNAL}")
endif()

if(NOT SDL_FOREGROUNDING_SIGNAL STREQUAL "OFF")
  target_compile_definitions(sdl-build-options INTERFACE "-DSDL_FOREGROUNDING_SIGNAL=${SDL_FOREGROUNDING_SIGNAL}")
endif()

# Compiler option evaluation
# Check for -Wall first, so later things can override pieces of it.
# Note: clang-cl treats -Wall as -Weverything (which is very loud),
#       /W3 as -Wall, and /W4 as -Wall -Wextra.  So: /W3 is enough.
check_c_compiler_flag(-Wall HAVE_GCC_WALL)
if(HAVE_GCC_WALL)
  list(APPEND EXTRA_CFLAGS "-Wall")
endif()

check_c_compiler_flag(-fno-strict-aliasing HAVE_GCC_NO_STRICT_ALIASING)
if(HAVE_GCC_NO_STRICT_ALIASING)
  list(APPEND EXTRA_CFLAGS "-fno-strict-aliasing")
endif()

check_c_compiler_flag(-Wdeclaration-after-statement HAVE_GCC_WDECLARATION_AFTER_STATEMENT)
if(HAVE_GCC_WDECLARATION_AFTER_STATEMENT)
  if(SDL_WERROR)
    check_c_compiler_flag(-Werror=declaration-after-statement HAVE_GCC_WERROR_DECLARATION_AFTER_STATEMENT)
    if(HAVE_GCC_WERROR_DECLARATION_AFTER_STATEMENT)
      list(APPEND EXTRA_CFLAGS "-Werror=declaration-after-statement")
    endif()
  endif()
  list(APPEND EXTRA_CFLAGS "-Wdeclaration-after-statement")
endif()

if(DEPENDENCY_TRACKING)
  check_c_source_compiles("
      #if !defined(__GNUC__) || __GNUC__ < 3
      #error Dependency tracking requires GCC 3.0 or newer
      #endif
      int main(int argc, char **argv) { return 0; }" HAVE_DEPENDENCY_TRACKING)
endif()

if(SDL_GCC_ATOMICS)
  check_c_source_compiles("int main(int argc, char **argv) {
      int a;
      void *x, *y, *z;
      __sync_lock_test_and_set(&a, 4);
      __sync_lock_test_and_set(&x, y);
      __sync_fetch_and_add(&a, 1);
      __sync_bool_compare_and_swap(&a, 5, 10);
      __sync_bool_compare_and_swap(&x, y, z);
      return 0; }" HAVE_GCC_ATOMICS)
  if(NOT HAVE_GCC_ATOMICS)
    check_c_source_compiles("int main(int argc, char **argv) {
        int a;
        __sync_lock_test_and_set(&a, 1);
        __sync_lock_release(&a);
        return 0; }" HAVE_GCC_SYNC_LOCK_TEST_AND_SET)
  endif()
endif()

set(CMAKE_REQUIRED_FLAGS "-mpreferred-stack-boundary=2")
check_c_source_compiles("int x = 0; int main(int argc, char **argv) { return 0; }"
  HAVE_GCC_PREFERRED_STACK_BOUNDARY)
set(CMAKE_REQUIRED_FLAGS ${ORIG_CMAKE_REQUIRED_FLAGS})

set(CMAKE_REQUIRED_FLAGS "-fvisibility=hidden -Werror")
check_c_source_compiles("
    #if !defined(__GNUC__) || __GNUC__ < 4
    #error SDL only uses visibility attributes in GCC 4 or newer
    #endif
    int main(int argc, char **argv) { return 0; }" HAVE_GCC_FVISIBILITY)
if(HAVE_GCC_FVISIBILITY)
  list(APPEND EXTRA_CFLAGS "-fvisibility=hidden")
endif()
set(CMAKE_REQUIRED_FLAGS ${ORIG_CMAKE_REQUIRED_FLAGS})

check_c_compiler_flag(-Wshadow HAVE_GCC_WSHADOW)
if(HAVE_GCC_WSHADOW)
  list(APPEND EXTRA_CFLAGS "-Wshadow")
endif()

set(CMAKE_REQUIRED_FLAGS "-Wl,--no-undefined")
check_c_compiler_flag("" HAVE_NO_UNDEFINED)
set(CMAKE_REQUIRED_FLAGS ${ORIG_CMAKE_REQUIRED_FLAGS})
if(HAVE_NO_UNDEFINED AND NOT (USE_CLANG AND WINDOWS))
  list(APPEND EXTRA_LDFLAGS_BUILD "-Wl,--no-undefined")
endif()

set(HAVE_ASSEMBLY TRUE)

set(HAVE_ARMNEON TRUE)
set(SDL_ARM_NEON_BLITTERS 1)
file(GLOB ARMNEON_SOURCES ${SDL2_SOURCE_DIR}/src/video/arm/pixman-arm-neon*.S)
list(APPEND SOURCE_FILES ${ARMNEON_SOURCES})
set(WARN_ABOUT_ARM_NEON_ASM_MIT TRUE)

# TODO: Can't deactivate on FreeBSD? w/o LIBC, SDL_stdinc.h can't define
# anything.
set(HAVE_LIBC TRUE)
set(headers_to_check
    ctype.h
    float.h
    iconv.h
    inttypes.h
    limits.h
    malloc.h
    math.h
    memory.h
    signal.h
    stdarg.h
    stddef.h
    stdint.h
    stdio.h
    stdlib.h
    string.h
    strings.h
    sys/types.h
    wchar.h
)
foreach(_HEADER ${headers_to_check})
  string(TOUPPER "HAVE_${_HEADER}" _UPPER)
  string(REGEX REPLACE "[./]" "_" _HAVE_H ${_UPPER})
  check_include_file("${_HEADER}" ${_HAVE_H})
endforeach()
check_include_file(linux/input.h HAVE_LINUX_INPUT_H)

set(STDC_HEADER_NAMES "stddef.h;stdarg.h;stdlib.h;string.h;stdio.h;wchar.h;float.h")
check_include_files("${STDC_HEADER_NAMES}" STDC_HEADERS)
check_symbol_exists(M_PI math.h HAVE_M_PI)
# TODO: refine the mprotect check
check_c_source_compiles("#include <sys/types.h>
                         #include <sys/mman.h>
                         int main(void) { return 0; }" HAVE_MPROTECT)
foreach(_FN
        strtod malloc calloc realloc free getenv setenv putenv unsetenv
        bsearch qsort abs bcopy memset memcpy memmove memcmp strlen strlcpy strlcat
        _strrev _strupr _strlwr index rindex strchr strrchr strstr strtok_r
        itoa _ltoa _uitoa _ultoa strtol strtoul _i64toa _ui64toa strtoll strtoull
        atoi atof strcmp strncmp _stricmp strcasecmp _strnicmp strncasecmp strcasestr
        wcscmp _wcsdup wcsdup wcslcat wcslcpy wcslen wcsncmp wcsstr
        wcscasecmp _wcsicmp wcsncasecmp _wcsnicmp
        sscanf vsscanf vsnprintf fopen64 fseeko fseeko64 _Exit
        )
  string(TOUPPER ${_FN} _UPPER)
  set(_HAVEVAR "HAVE_${_UPPER}")
  check_symbol_exists("${_FN}" "${STDC_HEADER_NAMES}" ${_HAVEVAR})
endforeach()

check_symbol_exists(sigaction "signal.h" HAVE_SIGACTION)
check_symbol_exists(setjmp "setjmp.h" HAVE_SETJMP)
check_symbol_exists(nanosleep "time.h" HAVE_NANOSLEEP)
check_symbol_exists(sysconf "unistd.h" HAVE_SYSCONF)
check_symbol_exists(sysctlbyname "sys/types.h;sys/sysctl.h" HAVE_SYSCTLBYNAME)
check_symbol_exists(getauxval "sys/auxv.h" HAVE_GETAUXVAL)
check_symbol_exists(elf_aux_info "sys/auxv.h" HAVE_ELF_AUX_INFO)
check_symbol_exists(poll "poll.h" HAVE_POLL)

check_library_exists(m pow "" HAVE_LIBM)
if(HAVE_LIBM)
  set(CMAKE_REQUIRED_LIBRARIES m)
endif()
foreach(_FN
          atan atan2 atanf atan2f ceil ceilf copysign copysignf cos cosf
          exp expf fabs fabsf floor floorf fmod fmodf log logf log10 log10f
          lround lroundf pow powf round roundf scalbn scalbnf sin sinf sqrt
          sqrtf tan tanf acos acosf asin asinf trunc truncf)
    string(TOUPPER ${_FN} _UPPER)
    set(_HAVEVAR "HAVE_${_UPPER}")
    check_symbol_exists("${_FN}" "math.h" ${_HAVEVAR})
endforeach()
if(HAVE_LIBM)
  set(CMAKE_REQUIRED_LIBRARIES)
  if(NOT VITA)
    list(APPEND EXTRA_LIBS m)
  endif()
endif()

if(SDL_SYSTEM_ICONV)
  check_library_exists(iconv iconv_open "" HAVE_LIBICONV)
  if(HAVE_LIBICONV)
    list(APPEND EXTRA_LIBS iconv)
    set(HAVE_ICONV 1)
    set(HAVE_SYSTEM_ICONV TRUE)
  else()
    check_library_exists(c iconv_open "" HAVE_BUILTIN_ICONV)
    if(HAVE_BUILTIN_ICONV)
      set(HAVE_ICONV 1)
      set(HAVE_SYSTEM_ICONV TRUE)
    endif()
  endif()
endif()

check_include_file(alloca.h HAVE_ALLOCA_H)
check_symbol_exists(alloca "alloca.h" HAVE_ALLOCA1)
check_symbol_exists(alloca "stdlib.h" HAVE_ALLOCA2)
check_symbol_exists(alloca "malloc.h" HAVE_ALLOCA3)
if(HAVE_ALLOCA1 OR HAVE_ALLOCA2 OR HAVE_ALLOCA3)
  set(HAVE_ALLOCA 1)
endif()


check_struct_has_member("struct sigaction" "sa_sigaction" "signal.h" HAVE_SA_SIGACTION)

# Disable various subsystems of the SDL library
set(SDL_JOYSTICK_DISABLED 1)
set(SDL_HAPTIC_DISABLED 1)
set(SDL_HIDAPI_DISABLED 1)
set(SDL_SENSOR_DISABLED 1)

set(DYNAPI_NEEDS_DLOPEN 1)
CheckDLOPEN()
if(SDL_LOADSO AND HAVE_DLOPEN)
  set(SDL_LOADSO_DLOPEN 1)
  file(GLOB DLOPEN_SOURCES ${SDL2_SOURCE_DIR}/src/loadso/dlopen/*.c)
  list(APPEND SOURCE_FILES ${DLOPEN_SOURCES})
  set(HAVE_SDL_LOADSO TRUE)
endif()

CheckO_CLOEXEC()

CheckOSS()
CheckALSA()
CheckFBCon()

file(GLOB CORE_UNIX_SOURCES ${SDL2_SOURCE_DIR}/src/core/unix/*.c)
list(APPEND SOURCE_FILES ${CORE_UNIX_SOURCES})

set(HAVE_INPUT_EVENTS TRUE)
set(SDL_INPUT_LINUXEV 1)

check_c_source_compiles("
    #include <linux/kd.h>
    #include <linux/keyboard.h>
    #include <sys/ioctl.h>
    int main(int argc, char **argv) {
        struct kbentry kbe;
        kbe.kb_table = KG_CTRL;
        ioctl(0, KDGKBENT, &kbe);
        return 0;
    }" HAVE_INPUT_KD)

if(HAVE_INPUT_KD)
  set(SDL_INPUT_LINUXKD 1)
endif()

if(HAVE_INPUT_KBIO)
  set(SDL_INPUT_FBSDKBIO 1)
endif()

set(HAVE_LIBUDEV_H FALSE)
check_include_file("sys/inotify.h" HAVE_SYS_INOTIFY_H)
check_symbol_exists(inotify_init "sys/inotify.h" HAVE_INOTIFY_INIT)
check_symbol_exists(inotify_init1 "sys/inotify.h" HAVE_INOTIFY_INIT1)

if(HAVE_SYS_INOTIFY_H AND HAVE_INOTIFY_INIT)
  set(HAVE_INOTIFY 1)
endif()

if(HAVE_DBUS_DBUS_H)
  list(APPEND SOURCE_FILES "${SDL2_SOURCE_DIR}/src/core/linux/SDL_dbus.c")
endif()

if(SDL_USE_IME)
  list(APPEND SOURCE_FILES "${SDL2_SOURCE_DIR}/src/core/linux/SDL_ime.c")
endif()

if(HAVE_IBUS_IBUS_H)
  list(APPEND SOURCE_FILES "${SDL2_SOURCE_DIR}/src/core/linux/SDL_ibus.c")
endif()

if(HAVE_FCITX)
  list(APPEND SOURCE_FILES "${SDL2_SOURCE_DIR}/src/core/linux/SDL_fcitx.c")
endif()

if(HAVE_INPUT_EVENTS)
  list(APPEND SOURCE_FILES "${SDL2_SOURCE_DIR}/src/core/linux/SDL_evdev.c")
  list(APPEND SOURCE_FILES "${SDL2_SOURCE_DIR}/src/core/linux/SDL_evdev_kbd.c")
endif()

if(HAVE_INPUT_KBIO)
  list(APPEND SOURCE_FILES "${SDL2_SOURCE_DIR}/src/core/freebsd/SDL_evdev_kbd_freebsd.c")
endif()

# Always compiled for Linux, unconditionally:
list(APPEND SOURCE_FILES "${SDL2_SOURCE_DIR}/src/core/linux/SDL_evdev_capabilities.c")
list(APPEND SOURCE_FILES "${SDL2_SOURCE_DIR}/src/core/linux/SDL_threadprio.c")
list(APPEND SOURCE_FILES "${SDL2_SOURCE_DIR}/src/core/linux/SDL_sandbox.c")
# src/core/unix/*.c is included in a generic if(UNIX) section, elsewhere.


CheckPTHREAD()

check_library_exists(rt clock_gettime "" FOUND_CLOCK_GETTIME_LIBRT)
if(FOUND_CLOCK_GETTIME_LIBRT)
  list(APPEND EXTRA_LIBS rt)
  set(HAVE_CLOCK_GETTIME 1)
else()
  check_library_exists(c clock_gettime "" FOUND_CLOCK_GETTIME_LIBC)
  if(FOUND_CLOCK_GETTIME_LIBC)
    set(HAVE_CLOCK_GETTIME 1)
  endif()
endif()

check_include_file(linux/version.h HAVE_LINUX_VERSION_H)
if(HAVE_LINUX_VERSION_H)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DHAVE_LINUX_VERSION_H")
endif()

if(SDL_MISC)
  file(GLOB MISC_SOURCES ${SDL2_SOURCE_DIR}/src/misc/unix/*.c)
  list(APPEND SOURCE_FILES ${MISC_SOURCES})
  set(HAVE_SDL_MISC TRUE)
endif()

if(SDL_POWER)
  set(SDL_POWER_LINUX 1)
  file(GLOB POWER_SOURCES ${SDL2_SOURCE_DIR}/src/power/linux/*.c)
  list(APPEND SOURCE_FILES ${POWER_SOURCES})
  set(HAVE_SDL_POWER TRUE)
endif()

if(SDL_LOCALE)
  file(GLOB LOCALE_SOURCES ${SDL2_SOURCE_DIR}/src/locale/unix/*.c)
  list(APPEND SOURCE_FILES ${LOCALE_SOURCES})
  set(HAVE_SDL_LOCALE TRUE)
endif()

if(SDL_FILESYSTEM)
  set(SDL_FILESYSTEM_UNIX 1)
  file(GLOB FILESYSTEM_SOURCES ${SDL2_SOURCE_DIR}/src/filesystem/unix/*.c)
  list(APPEND SOURCE_FILES ${FILESYSTEM_SOURCES})
  set(HAVE_SDL_FILESYSTEM TRUE)
endif()

if(SDL_TIMERS)
  set(SDL_TIMER_UNIX 1)
  file(GLOB TIMER_SOURCES ${SDL2_SOURCE_DIR}/src/timer/unix/*.c)
  list(APPEND SOURCE_FILES ${TIMER_SOURCES})
  set(HAVE_SDL_TIMERS TRUE)
endif()

set(SDL_RLD_FLAGS "")
if(SDL_RPATH AND SDL_SHARED)
  set(CMAKE_REQUIRED_FLAGS "-Wl,--enable-new-dtags")
  check_c_compiler_flag("" HAVE_ENABLE_NEW_DTAGS)
  set(CMAKE_REQUIRED_FLAGS ${ORIG_CMAKE_REQUIRED_FLAGS})
  if(HAVE_ENABLE_NEW_DTAGS)
    set(SDL_RLD_FLAGS "-Wl,-rpath,\${libdir} -Wl,--enable-new-dtags")
  else()
    set(SDL_RLD_FLAGS "-Wl,-rpath,\${libdir}")
  endif()
  set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
  set(HAVE_RPATH TRUE)
endif()

# Platform-independent options
CheckLibSampleRate()

if(NOT SDLMAIN_SOURCES)
  file(GLOB SDLMAIN_SOURCES ${SDL2_SOURCE_DIR}/src/main/dummy/*.c)
endif()

if(SDL_WERROR)
  cmake_push_check_state(RESET)
  check_c_compiler_flag(-Werror HAVE_WERROR)
  if(HAVE_WERROR)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")
    set(CMAKE_OBJC_FLAGS "${CMAKE_OBJC_FLAGS} -Werror")
  endif()
  cmake_pop_check_state()
endif()

# config variables may contain generator expression, so we need to generate SDL_config.h in 2 steps:
# 1. replace all `#cmakedefine`'s and `@abc@`
configure_file("${SDL2_SOURCE_DIR}/include/SDL_config.h.cmake"
  "${SDL2_BINARY_DIR}/SDL_config.h.intermediate")
# 2. Create the "include-config-${CMAKE_BUILD_TYPE}" folder (fails on older CMake versions when it does not exist)
string(TOLOWER "${CMAKE_BUILD_TYPE}" lower_build_type)
execute_process(COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/include-config-${lower_build_type}")
# 3. generate SDL_config in an build_type-dependent folder (which should be first in the include search path)
file(GENERATE
    OUTPUT "${SDL2_BINARY_DIR}/include-config-$<LOWER_CASE:$<CONFIG>>/SDL_config.h"
    INPUT "${SDL2_BINARY_DIR}/SDL_config.h.intermediate")

# Prepare the flags and remove duplicates
if(EXTRA_LDFLAGS)
  list(REMOVE_DUPLICATES EXTRA_LDFLAGS)
endif()
if(EXTRA_LIBS)
  list(REMOVE_DUPLICATES EXTRA_LIBS)
endif()
if(EXTRA_CFLAGS)
  list(REMOVE_DUPLICATES EXTRA_CFLAGS)
endif()
listtostr(EXTRA_CFLAGS _EXTRA_CFLAGS)
set(EXTRA_CFLAGS ${_EXTRA_CFLAGS})

# Compat helpers for the configuration files

if(EXISTS "${PROJECT_SOURCE_DIR}/VERSION.txt")
  file(READ "${PROJECT_SOURCE_DIR}/VERSION.txt" SDL_SOURCE_VERSION)
  string(STRIP "${SDL_SOURCE_VERSION}" SDL_SOURCE_VERSION)
endif()

find_package(Git)
if(Git_FOUND)
  execute_process(COMMAND
    "${GIT_EXECUTABLE}" describe --always --tags --long
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    RESULT_VARIABLE GIT_REVISION_STATUS
    OUTPUT_VARIABLE GIT_REVISION
    ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
else()
  set(GIT_REVISION_STATUS 1)
  set(GIT_REVISION "")
endif()

if(SDL_SOURCE_VERSION)
  set(SDL_REVISION "SDL-${SDL_SOURCE_VERSION}")
elseif(GIT_REVISION_STATUS EQUAL 0)
  if(GIT_REVISION MATCHES "^[0-9a-f]+$")
    # Just a truncated sha1, so prefix it with the version number
    set(SDL_REVISION "SDL-${SDL_VERSION}-g${GIT_REVISION}")
  else()
    # e.g. release-2.24.0-542-g96361fc47
    set(SDL_REVISION "SDL-${GIT_REVISION}")
  endif()
else()
  set(SDL_REVISION "SDL-${SDL_VERSION}-no-vcs")
endif()

configure_file("${SDL2_SOURCE_DIR}/include/SDL_revision.h.cmake"
  "${SDL2_BINARY_DIR}/include/SDL_revision.h")

# Copy all non-generated headers to "${SDL2_BINARY_DIR}/include"
# This is done to avoid the inclusion of a pre-generated SDL_config.h
file(GLOB SDL2_INCLUDE_FILES ${SDL2_SOURCE_DIR}/include/*.h)
set(SDL2_COPIED_INCLUDE_FILES)
foreach(_hdr IN LISTS SDL2_INCLUDE_FILES)
  if(_hdr MATCHES ".*(SDL_config|SDL_revision).*")
    list(REMOVE_ITEM SDL2_INCLUDE_FILES "${_hdr}")
  else()
    get_filename_component(_name "${_hdr}" NAME)
    set(_bin_hdr "${SDL2_BINARY_DIR}/include/${_name}")
    list(APPEND SDL2_COPIED_INCLUDE_FILES "${_bin_hdr}")
    add_custom_command(OUTPUT "${_bin_hdr}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_hdr}" "${_bin_hdr}"
        DEPENDS "${_hdr}")
  endif()
endforeach()
list(APPEND SDL_GENERATED_HEADERS ${SDL2_COPIED_INCLUDE_FILES})

if(CMAKE_STATIC_LIBRARY_PREFIX STREQUAL "" AND CMAKE_STATIC_LIBRARY_SUFFIX STREQUAL ".lib")
  # Avoid conflict between the dll import library and the static library
  set(sdl_static_libname "SDL2-static")
else()
  set(sdl_static_libname "SDL2")
endif()

set(prefix ${CMAKE_INSTALL_PREFIX})
file(RELATIVE_PATH bin_prefix_relpath "${CMAKE_INSTALL_FULL_BINDIR}" "${CMAKE_INSTALL_PREFIX}")

set(exec_prefix "\${prefix}")
set(libdir "\${exec_prefix}/${CMAKE_INSTALL_LIBDIR}")
set(bindir "\${exec_prefix}/${CMAKE_INSTALL_BINDIR}")
set(includedir "\${prefix}/${CMAKE_INSTALL_INCLUDEDIR}")
set(ENABLE_STATIC_TRUE "#")
set(ENABLE_STATIC_FALSE "")
set(PKGCONFIG_LIBS_PRIV "
Libs.private:")
set(ENABLE_SHARED_TRUE "")
set(ENABLE_SHARED_FALSE "#")

# Clean up the different lists
listtostr(EXTRA_LIBS _EXTRA_LIBS "-l")
set(SDL_STATIC_LIBS ${SDL_LIBS} ${EXTRA_LDFLAGS} ${_EXTRA_LIBS})
list(REMOVE_DUPLICATES SDL_STATIC_LIBS)
listtostr(SDL_STATIC_LIBS _SDL_STATIC_LIBS)
set(SDL_STATIC_LIBS ${_SDL_STATIC_LIBS})
listtostr(SDL_LIBS _SDL_LIBS)
set(SDL_LIBS ${_SDL_LIBS})
listtostr(SDL_CFLAGS _SDL_CFLAGS "")
set(SDL_CFLAGS ${_SDL_CFLAGS})
string(REGEX REPLACE "-lSDL2( |$)" "-l${sdl_static_libname} " SDL_STATIC_LIBS "${SDL_STATIC_LIBS}")
if(NOT SDL_SHARED)
  string(REGEX REPLACE "-lSDL2( |$)" "-l${sdl_static_libname} " SDL_LIBS "${SDL_LIBS}")
endif()

if(SDL_STATIC AND SDL_SHARED AND NOT sdl_static_libname STREQUAL "SDL2")
  message(STATUS "\"pkg-config --static --libs sdl2\" will return invalid information")
endif()

# MESSAGE(STATUS "SDL_LIBS: ${SDL_LIBS}")
# MESSAGE(STATUS "SDL_STATIC_LIBS: ${SDL_STATIC_LIBS}")

configure_file("${SDL2_SOURCE_DIR}/sdl2.pc.in"
  "${SDL2_BINARY_DIR}/sdl2.pc" @ONLY)
configure_file("${SDL2_SOURCE_DIR}/sdl2-config.in"
  "${SDL2_BINARY_DIR}/sdl2-config" @ONLY)
configure_file("${SDL2_SOURCE_DIR}/SDL2.spec.in"
  "${SDL2_BINARY_DIR}/SDL2.spec" @ONLY)

macro(check_add_debug_flag FLAG SUFFIX)
    check_c_compiler_flag(${FLAG} HAS_C_FLAG_${SUFFIX})
    if (HAS_C_FLAG_${SUFFIX})
        set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} ${FLAG}")
    endif()

    check_cxx_compiler_flag(${FLAG} HAS_CXX_${SUFFIX})
    if (HAS_CXX_${SUFFIX})
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${FLAG}")
    endif()
endmacro()

macro(asan_check_add_debug_flag ASAN_FLAG)
    check_add_debug_flag("-fsanitize=${ASAN_FLAG}" "${ASAN_FLAG}")
    if(HAS_C_${ASAN_FLAG} OR HAS_CXX_${ASAN_FLAG})
      set(HAVE_ASAN ON)
    endif()
endmacro()

macro(asan_check_add_debug_flag2 ASAN_FLAG)
    # for some sanitize flags we have to manipulate the CMAKE_REQUIRED_LIBRARIES:
    # http://cmake.3232098.n2.nabble.com/CHECK-CXX-COMPILER-FLAG-doesn-t-give-correct-result-for-fsanitize-address-tp7600216p7600217.html

    set(FLAG "-fsanitize=${ASAN_FLAG}")

    set (STORED_REQLIBS ${CMAKE_REQUIRED_LIBRARIES})
    set (CMAKE_REQUIRED_LIBRARIES "${FLAG};asan")
    check_c_compiler_flag (${FLAG} HAS_C_FLAG_${ASAN_FLAG})
    check_cxx_compiler_flag (${FLAG} HAS_CXX_FLAG_${ASAN_FLAG})
    set (CMAKE_REQUIRED_LIBRARIES ${STORED_REQLIBS})

    if (HAS_C_FLAG_${ASAN_FLAG})
        set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} ${FLAG}")
    endif()

    if (HAS_CXX_${ASAN_FLAG})
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${FLAG}")
    endif()

    if(HAS_C_${ASAN_FLAG} OR HAS_CXX_${ASAN_FLAG})
      set(HAVE_ASAN ON)
    endif()
endmacro()

# enable AddressSanitizer if supported
if (SDL_ASAN)
  asan_check_add_debug_flag2("address")
  asan_check_add_debug_flag("bool")
  asan_check_add_debug_flag("bounds")
  asan_check_add_debug_flag("enum")
  asan_check_add_debug_flag("float-cast-overflow")
  asan_check_add_debug_flag("float-divide-by-zero")
  asan_check_add_debug_flag("nonnull-attribute")
  asan_check_add_debug_flag("returns-nonnull-attribute")
  asan_check_add_debug_flag("signed-integer-overflow")
  asan_check_add_debug_flag("undefined")
  asan_check_add_debug_flag("vla-bound")
  asan_check_add_debug_flag("leak")
  # The object size sanitizer has no effect on unoptimized builds on Clang,
  # but causes warnings.
  asan_check_add_debug_flag("object-size")
endif()

# Create target that collects all all generated include files.
add_custom_target(sdl_headers_copy
    DEPENDS ${SDL_GENERATED_HEADERS})

##### Info output #####
message(STATUS "")
message(STATUS "SDL2 was configured with the following options:")
message(STATUS "")
message(STATUS "Platform: ${CMAKE_SYSTEM}")
message(STATUS "64-bit:   ${ARCH_64}")
message(STATUS "Compiler: ${CMAKE_C_COMPILER}")
message(STATUS "Revision: ${SDL_REVISION}")
message(STATUS "")
message(STATUS "Options:")
list(SORT ALLOPTIONS)
foreach(_OPT ${ALLOPTIONS})
  # Get the padding
  string(LENGTH ${_OPT} _OPTLEN)
  math(EXPR _PADLEN "(${LONGESTOPTIONNAME} + 1) - ${_OPTLEN}")
  string(RANDOM LENGTH ${_PADLEN} ALPHABET " " _PADDING)
  message_tested_option(${_OPT} ${_PADDING})
endforeach()
if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
  message(STATUS "")
  message(STATUS " CMAKE_C_FLAGS_DEBUG:   ${CMAKE_C_FLAGS_DEBUG}")
  message(STATUS " CMAKE_CXX_FLAGS_DEBUG: ${CMAKE_CXX_FLAGS_DEBUG}")
endif()
message(STATUS "")
message(STATUS " CFLAGS:        ${CMAKE_C_FLAGS}")
message(STATUS " EXTRA_CFLAGS:  ${EXTRA_CFLAGS}")
message(STATUS " EXTRA_LDFLAGS: ${EXTRA_LDFLAGS} ${EXTRA_LDFLAGS_BUILD}")
message(STATUS " EXTRA_LIBS:    ${EXTRA_LIBS}")
message(STATUS "")
message(STATUS " Build Shared Library: ${SDL_SHARED}")
message(STATUS "")

if(WARN_ABOUT_ARM_SIMD_ASM_MIT)
  message(STATUS "")
  message(STATUS "SDL is being built with ARM SIMD optimizations, which")
  message(STATUS "uses code licensed under the MIT license. If this is a")
  message(STATUS "problem, please disable that code by rerunning CMake with:")
  message(STATUS "")
  message(STATUS "    -DSDL_ARMSIMD=OFF")
endif()

if(WARN_ABOUT_ARM_NEON_ASM_MIT)
  message(STATUS "")
  message(STATUS "SDL is being built with ARM NEON optimizations, which")
  message(STATUS "uses code licensed under the MIT license. If this is a")
  message(STATUS "problem, please disable that code by rerunning CMake with:")
  message(STATUS "")
  message(STATUS "    -DSDL_ARMNEON=OFF")
endif()

# Ensure that the extra cflags are used at compile time
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${EXTRA_CFLAGS} ${EXTRA_CFLAGS_BUILD}")

if(NOT SDL2_DISABLE_SDL2MAIN)
  # Build SDLmain
  add_library(SDL2main STATIC ${SDLMAIN_SOURCES})
  add_dependencies(SDL2main sdl_headers_copy)
  # alias target for in-tree builds
  add_library(SDL2::SDL2main ALIAS SDL2main)
  target_include_directories(SDL2main BEFORE PRIVATE "${SDL2_BINARY_DIR}/include" PRIVATE "${SDL2_BINARY_DIR}/include-config-$<LOWER_CASE:$<CONFIG>>")
  target_include_directories(SDL2main PUBLIC "$<BUILD_INTERFACE:${SDL2_BINARY_DIR}/include>" $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}> $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/SDL2>)
  set_target_properties(SDL2main PROPERTIES DEBUG_POSTFIX "${SDL_CMAKE_DEBUG_POSTFIX}")
endif()

if(SDL_SHARED)
  add_library(SDL2 SHARED ${SOURCE_FILES} ${VERSION_SOURCES})
  add_dependencies(SDL2 sdl_headers_copy)
  # alias target for in-tree builds
  add_library(SDL2::SDL2 ALIAS SDL2)
  set_target_properties(SDL2 PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
  if(NOT SDL_LIBC)
    check_cpu_architecture(x86 HAS_X86)
    if(HAS_X86)
      # FIXME: should be added for all architectures (missing symbols for ARM)
      target_link_libraries(SDL2 PRIVATE "-nodefaultlib:MSVCRT")
    endif()
  endif()
  set_target_properties(SDL2 PROPERTIES
    VERSION ${LT_VERSION}
    SOVERSION ${LT_MAJOR}
    OUTPUT_NAME "SDL2-${LT_RELEASE}")
  
  # FIXME: if CMAKE_VERSION >= 3.13, use target_link_options for EXTRA_LDFLAGS
  target_link_libraries(SDL2 PRIVATE ${EXTRA_LIBS} ${EXTRA_LDFLAGS} ${EXTRA_LDFLAGS_BUILD})
  target_include_directories(SDL2 PUBLIC
      "$<BUILD_INTERFACE:${SDL2_BINARY_DIR}/include>"
      "$<BUILD_INTERFACE:${SDL2_BINARY_DIR}/include-config-$<LOWER_CASE:$<CONFIG>>>"
      "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
      "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/SDL2>")
  # This picks up all the compiler options and such we've accumulated up to here.
  target_link_libraries(SDL2 PRIVATE $<BUILD_INTERFACE:sdl-build-options>)
  set_target_properties(SDL2 PROPERTIES DEBUG_POSTFIX "${SDL_CMAKE_DEBUG_POSTFIX}")
  # Use `Compatible Interface Properties` to allow consumers to enforce a shared/static library
  set_property(TARGET SDL2 PROPERTY INTERFACE_SDL2_SHARED TRUE)
  set_property(TARGET SDL2 APPEND PROPERTY COMPATIBLE_INTERFACE_BOOL SDL2_SHARED)
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DSDL_BUILD_MAJOR_VERSION=${SDL_MAJOR_VERSION}")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DSDL_BUILD_MINOR_VERSION=${SDL_MINOR_VERSION}")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DSDL_BUILD_MICRO_VERSION=${SDL_MICRO_VERSION}")

##### Fix Objective C builds #####
set(CMAKE_OBJC_FLAGS "${CMAKE_OBJC_FLAGS} ${CMAKE_C_FLAGS}")

# Make sure SDL2::SDL2 always exists
if(TARGET SDL2::SDL2-static AND NOT TARGET SDL2::SDL2)
  add_library(SDL2::SDL2 ALIAS SDL2-static)
endif()
