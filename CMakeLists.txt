cmake_minimum_required(VERSION 2.8.4)
set(CMAKE_TOOLCHAIN_FILE ${CMAKE_SOURCE_DIR}/cmake/ArduinoToolchain.cmake)
set(CMAKE_CXX_STANDARD 11)
set(ARDUINO_CXX_FLAGS "${ARDUINO_C_FLAGS} -Os -w -fpermissive -fno-exceptions -ffunction-sections -fdata-sections -fno-threadsafe-statics  -mmcu=atmega328p -DF_CPU=16000000L -DARDUINO=10805 -DARDUINO_AVR_UNO -DARDUINO_ARCH_AVR")
set(PROJECT_NAME temp_controller)

#set(ROOM_MODULE room_module)
#set(SERVER_MODULE server module)

#set(${PROJECT_NAME}_BOARD uno)

# set(ARDUINO_CPU)
project(${PROJECT_NAME})
# set(${PROJECT_NAME}_SRCS)
# set(${PROJECT_NAME}_HDRS)
# set(${<@PROJECT_NAME@>}_HDRS)
# set(${PROJECT_NAME}_LIBS)
# link_directories(${CMAKE_CURRENT_SOURCE_DIR}/)
link_directories(${CMAKE_CURRENT_SOURCE_DIR}/lib)
# add_subdirectory()
#set(${PROJECT_NAME}_SKETCH room_module.ino)
#set(${PROJECT_NAME}_SKETCH server.ino)
# set(${PROJECT_NAME}_PROGRAMMER avrispmkii)
# set(${<@PROJECT_NAME@>}_PROGRAMMER avrispmkii)
#set(${PROJECT_NAME}_PORT COM5)
# set(${PROJECT_NAME}_AFLAGS -v)
# set(${<@PROJECT_NAME@>}_AFLAGS -v)
# set(uno.upload.speed 9600)
generate_arduino_firmware(room_module
        SKETCH ${CMAKE_SOURCE_DIR}/room_module
        BOARD uno
        PORT COM5)

generate_arduino_firmware(server_module
        SKETCH ${CMAKE_SOURCE_DIR}/server
        BOARD uno
        PORT COM21)

generate_arduino_firmware(ether_module
        SKETCH ${CMAKE_SOURCE_DIR}/ethertest
        BOARD uno
        PORT COM19)

