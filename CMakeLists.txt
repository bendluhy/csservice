cmake_minimum_required(VERSION 3.14)

project(CSService LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 COMPONENTS Core Network REQUIRED)

# Include the common header directory globally
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Shared/Src
)

add_executable(CSService
  main.cpp
  windowsservice.h windowsservice.cpp
  logger.h logger.cpp
  winring0.cpp winring0.h
  appresource.cpp appresource.h
  commandproc.h commandproc.cpp
  registryaccess.h registryaccess.cpp
  wmiaccess.h wmiaccess.cpp
  namedpipeserver.h namedpipeserver.cpp
  launcher.cpp
  launcher.h
  monitor.h monitor.cpp
  ecmemorymirror.h ecmemorymirror.cpp
  securecommandhandler.h securecommandhandler.cpp
  ../../Shared/Src/CommandMessage.h
  ../../Shared/Src/secureprotocol.cpp ../../Shared/Src/secureprotocol.h
)
target_link_libraries(CSService
    Qt6::Core
    Qt6::Network
    advapi32
)
include(GNUInstallDirs)
install(TARGETS CSService
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
