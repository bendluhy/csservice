cmake_minimum_required(VERSION 3.14)

project(CSService LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_TOOLCHAIN_FILE "C:/vcpkg/scripts/buildsystems/vcpkg.cmake")
set(Protobuf_DIR "C:/vcpkg/installed/x64-windows/share/protobuf")
set(protobuf_DIR "C:/vcpkg/installed/x64-windows/share/protobuf")
set(Protobuf_SRC_ROOT_FOLDER "C:/vcpkg/installed/x64-windows")

find_package(Qt6 REQUIRED COMPONENTS
    Core
    Network
    Protobuf
)

add_executable(CSService
    src/main.cpp

    src/windowsservice.cpp
    src/windowsservice.h

    src/logger.cpp
    src/logger.h

    src/appresource.cpp
    src/appresource.h

    src/commandproc.cpp
    src/commandproc.h

    src/registryaccess.cpp
    src/registryaccess.h

    src/wmiaccess.cpp
    src/wmiaccess.h

    src/namedpipeserver.cpp
    src/namedpipeserver.h

    src/launcher.cpp
    src/launcher.h

    src/monitor.cpp
    src/monitor.h

    src/ecmemorymirror.cpp
    src/ecmemorymirror.h

    src/securecommandhandler.cpp
    src/securecommandhandler.h

    src/eccommunication/ecmanager.cpp
    src/eccommunication/ecmanager.h

    src/eccommunication/emiio.cpp
    src/eccommunication/emiio.h

    src/eccommunication/emithread.cpp
    src/eccommunication/emithread.h

    src/eccommunication/host_ec_cmds.h
    src/eccommunication/appstd.h
    src/eccommunication/portio.h
    src/eccommunication/portio.cpp

    ../../Shared/Src/CommandMessage.h
    ../../Shared/Src/secureprotocol.cpp
    ../../Shared/Src/secureprotocol.h
    src/os/os.h src/os/os.cpp
    src/action/actioncommandqueue.h src/action/actioncommandqueue.cpp
    src/bezel/bezel.h src/bezel/bezel.cpp
)

qt_add_protobuf(CSService
    PROTO_FILES
        ../../Shared/Proto/command.proto
)

target_include_directories(CSService PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Shared/Src
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/action
    ${CMAKE_CURRENT_SOURCE_DIR}/src/bezel
    ${CMAKE_CURRENT_SOURCE_DIR}/src/eccommunication
    ${CMAKE_CURRENT_SOURCE_DIR}/src/os

)

target_link_libraries(CSService PRIVATE
    Qt6::Core
    Qt6::Network
    Qt6::Protobuf
    advapi32
    PowrProf
)

include(GNUInstallDirs)

install(TARGETS CSService
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
