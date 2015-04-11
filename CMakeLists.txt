set(CMAKE_MODULE_PATH ".")
set(VERSION "0.1")

cmake_minimum_required(VERSION 2.8)
find_package(libusb-1.0 REQUIRED)
include_directories(${LIBUSB_1_INCLUDE_DIRS})

add_library(usb-1.0 SHARED IMPORTED)

file(WRITE "version.h" "#define VERSION \"${VERSION}\"\n")
file(WRITE "test.txt" ${LIBUSB_1_LIBRARIES})

add_library(libminipro STATIC byte_utils.c database.c easyconfig.c fuses.c minipro.c)

add_executable(minipro main.c)
target_link_libraries(minipro libminipro ${LIBUSB_1_LIBRARIES})

add_executable(minipro-query-db minipro-query-db.c)
target_link_libraries(minipro-query-db libminipro)
