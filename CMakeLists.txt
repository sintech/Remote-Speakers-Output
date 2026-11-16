cmake_minimum_required(VERSION 3.24)
project(RemoteSpeakersOutput VERSION 5.6 LANGUAGES C CXX RC)

if(NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
  message(FATAL_ERROR "Use MSVC with a Win32 (x86) generator")
endif()
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(APX_SYNC_LATENCY_MS 1750 CACHE STRING "RAOP sync delay in milliseconds (experimental below 1750)")
if(NOT APX_SYNC_LATENCY_MS MATCHES "^[0-9]+$" OR
    APX_SYNC_LATENCY_MS LESS 100 OR APX_SYNC_LATENCY_MS GREATER 2000)
  message(FATAL_ERROR "APX_SYNC_LATENCY_MS must be an integer between 100 and 2000")
endif()
set_property(SOURCE rsoutput/src/core/impl/Options.cpp APPEND PROPERTY
  COMPILE_DEFINITIONS "APX_SYNC_LATENCY_MS=${APX_SYNC_LATENCY_MS}")
set(DEPS "${CMAKE_SOURCE_DIR}/.deps" CACHE PATH "Prepared dependency sources")
set(OPENSSL_ROOT_DIR "${DEPS}/install")
set(OPENSSL_USE_STATIC_LIBS TRUE)
set(OPENSSL_MSVC_STATIC_RT TRUE)
list(PREPEND CMAKE_PREFIX_PATH "${DEPS}/install")
find_package(OpenSSL 3.0 REQUIRED COMPONENTS Crypto)
find_package(Poco 1.9 REQUIRED COMPONENTS Foundation Net)

add_library(samplerate STATIC
  "${DEPS}/samplerate/src/samplerate.c"
  "${DEPS}/samplerate/src/src_linear.c"
  "${DEPS}/samplerate/src/src_sinc.c"
  "${DEPS}/samplerate/src/src_zoh.c")
target_include_directories(samplerate PUBLIC "${DEPS}/samplerate/src"
  PRIVATE "${DEPS}/samplerate/Win32")
target_compile_definitions(samplerate PRIVATE WIN32 _CRT_SECURE_NO_WARNINGS
  HAVE_LRINT=1 HAVE_LRINTF=1)

file(GLOB CORE_SOURCES CONFIGURE_DEPENDS
  rsoutput/src/core/impl/*.cpp rsoutput/src/core/impl/raop/*.cpp
  rsoutput/src/view/impl/*.cpp)
add_library(apxCore SHARED ${CORE_SOURCES} rsoutput/src/view/impl/Resources.rc)
target_include_directories(apxCore PUBLIC rsoutput/sdk
  PRIVATE rsoutput/src/core rsoutput/src/view "${DEPS}/alac/codec" "${DEPS}/bonjour")
target_compile_definitions(apxCore PRIVATE RSOUTPUT_EXPORTS POCO_STATIC POCO_NO_AUTOMATIC_LIBS)
target_link_libraries(apxCore PRIVATE samplerate Poco::Foundation Poco::Net
  OpenSSL::Crypto advapi32 bcrypt crypt32 gdi32 iphlpapi user32 ws2_32)

add_library(out_apx SHARED winamp/src/Main.cpp winamp/src/Output.cpp
  winamp/src/WinampHooks.cpp winamp/src/WinampPlayer.cpp winamp/src/Resources.rc)
target_include_directories(out_apx PRIVATE winamp/sdk rsoutput/sdk)
target_link_libraries(out_apx PRIVATE apxCore ole32 oleaut32 shell32 user32 windowscodecs)

foreach(target apxCore out_apx)
  target_compile_definitions(${target} PRIVATE UNICODE _UNICODE NOMINMAX
    WIN32_LEAN_AND_MEAN _WIN32_WINNT=0x0601 _CRT_SECURE_NO_WARNINGS
    _WINSOCK_DEPRECATED_NO_WARNINGS _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING)
  target_compile_options(${target} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/utf-8>
    $<$<COMPILE_LANGUAGE:CXX>:/W3> $<$<COMPILE_LANGUAGE:CXX>:/MP>)
  # Let rc.exe resolve icon paths against the repository root.
  target_include_directories(${target} PRIVATE "${CMAKE_SOURCE_DIR}")
endforeach()

enable_testing()
add_executable(plugin_smoke tests/plugin_smoke.cpp)
target_include_directories(plugin_smoke PRIVATE winamp/sdk)
add_test(NAME plugin_load COMMAND plugin_smoke "$<TARGET_FILE:out_apx>")
add_executable(metadata_startup tests/metadata_startup.cpp winamp/src/WinampPlayer.cpp)
target_include_directories(metadata_startup PRIVATE winamp/src winamp/sdk rsoutput/sdk)
target_compile_definitions(metadata_startup PRIVATE UNICODE _UNICODE NOMINMAX
  WIN32_LEAN_AND_MEAN _WIN32_WINNT=0x0601 _CRT_SECURE_NO_WARNINGS)
target_compile_options(metadata_startup PRIVATE /utf-8)
target_link_libraries(metadata_startup PRIVATE apxCore ole32 oleaut32 shell32 user32 windowscodecs)
add_test(NAME metadata_startup_safety COMMAND metadata_startup)
set_tests_properties(metadata_startup_safety PROPERTIES TIMEOUT 15)
add_executable(core_startup tests/core_startup.cpp
  rsoutput/src/core/impl/raop/RAOPEngine.cpp
  rsoutput/src/core/impl/raop/RAOPDevice.cpp
  rsoutput/src/core/impl/raop/RTSPClient.cpp
  rsoutput/src/core/impl/raop/NTPTimestamp.cpp
  rsoutput/src/core/impl/raop/PacketBuffer.cpp
  rsoutput/src/core/impl/OutputFormat.cpp
  rsoutput/src/core/impl/OutputMetadata.cpp
  rsoutput/src/core/impl/Debugger.cpp
  rsoutput/src/core/impl/Platform.cpp
  rsoutput/src/core/impl/Plugin.cpp)
target_include_directories(core_startup PRIVATE rsoutput/sdk rsoutput/src/core
  rsoutput/src/core/impl/raop "${DEPS}/alac/codec")
target_compile_definitions(core_startup PRIVATE RSOUTPUT_EXPORTS POCO_STATIC
  POCO_NO_AUTOMATIC_LIBS UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN
  _WIN32_WINNT=0x0601 _CRT_SECURE_NO_WARNINGS _WINSOCK_DEPRECATED_NO_WARNINGS)
target_compile_options(core_startup PRIVATE /utf-8 /MP)
target_link_libraries(core_startup PRIVATE Poco::Foundation Poco::Net
  OpenSSL::Crypto advapi32 bcrypt crypt32 gdi32 iphlpapi user32 ws2_32)
add_test(NAME core_session_startup COMMAND core_startup)
set_tests_properties(core_session_startup PROPERTIES TIMEOUT 30)
install(TARGETS apxCore RUNTIME DESTINATION .)
install(TARGETS out_apx RUNTIME DESTINATION Plugins)
