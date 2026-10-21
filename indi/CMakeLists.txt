cmake_minimum_required(VERSION 3.28)
project(indi-batterymonitor LANGUAGES CXX VERSION 0.1.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(GNUInstallDirs)

# PowerInterface richiede INDI recente (introdotta a inizio 2025, 2.1.x)
find_package(INDI 2.1 REQUIRED)
find_package(Threads REQUIRED)

find_package(ixwebsocket QUIET)
if(ixwebsocket_FOUND)
    set(IXWS_TARGET ixwebsocket::ixwebsocket)
else()
    include(FetchContent)
    set(USE_TLS OFF CACHE BOOL "" FORCE)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    FetchContent_Declare(ixwebsocket
        GIT_REPOSITORY https://github.com/machinezone/IXWebSocket.git
        GIT_TAG v11.4.6
	EXCLUDE_FROM_ALL)
    FetchContent_MakeAvailable(ixwebsocket)
    set(IXWS_TARGET ixwebsocket)
endif()

# JSON: di default si usa indijson.hpp, installato con libindi (nessuna dipendenza aggiuntiva).
option(USE_SYSTEM_JSONLIB "Use system nlohmann_json instead of INDI's bundled indijson.hpp" OFF)
if(USE_SYSTEM_JSONLIB)
    find_package(nlohmann_json REQUIRED)
    add_compile_definitions(_USE_SYSTEM_JSONLIB)
endif()

include_directories(${INDI_INCLUDE_DIR})

add_executable(indi_batterymonitor
    battery_monitor.cpp
    batterymon_link.cpp)

target_link_libraries(indi_batterymonitor
    ${INDI_DRIVER_LIBRARIES}
    ixwebsocket::ixwebsocket
    Threads::Threads)

if(USE_SYSTEM_JSONLIB)
    target_link_libraries(indi_batterymonitor nlohmann_json::nlohmann_json)
endif()

install(TARGETS indi_batterymonitor RUNTIME DESTINATION bin)
install(FILES indi_batterymonitor.xml DESTINATION ${INDI_DATA_DIR})
