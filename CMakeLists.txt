cmake_minimum_required(VERSION 3.12)
include(pico_sdk_import.cmake)
project(Pico_ePaper_Code)

add_compile_options( -mthumb-interwork )

pico_sdk_init()

add_subdirectory("./ext/freetype-2.9")

# add a compilation subdirectory
add_subdirectory(lib/Config)
add_subdirectory(lib/e-Paper)
add_subdirectory(lib/Fonts)
add_subdirectory(lib/GUI)
add_subdirectory(src)


# add a header directory
include_directories(src)
include_directories(./lib/Config)
include_directories(./lib/GUI)
include_directories("./ext/freetype-2.9/include")

# generate an executable file
add_executable(epd main.c ./ext/freetype-2.9/include/ft2build.h)

# disable usb output, enable uart output
pico_enable_stdio_usb(epd 0)
pico_enable_stdio_uart(epd 1)

# create map/bin/hex/uf2 file etc.
pico_add_extra_outputs(epd)

target_link_libraries(epd src ePaper GUI Fonts Config pico_stdlib hardware_spi)
