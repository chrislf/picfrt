include_directories(../lib/Config)
include_directories(../lib/GUI)
include_directories(../lib/e-Paper)
include_directories(../ext/freetype-2.9/include)

add_library(log ./log.c ./log.h)

#add_library(fta STATIC IMPORTED)
#set_target_properties(fta PROPERTIES IMPORTED_LOCATION /nix/store/kgkdfsi8cl5a4hch3chsajv4mw1pwyrh-freetype-arm-none-eabi-2.11.1/lib/libfreetype.a)

# Generate the link library
add_library(src ./liberation_serif_regular_arm.o ./EPD_7in5_V2_test.c ./ImageData.c ./ImageData.h)
target_link_libraries(src PUBLIC Config log freetype)
