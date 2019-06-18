
#   Part of the Raspberry-Pi Bare Metal Tutorials
#   Copyright (c) 2015, Brian Sidebotham
#   All rights reserved.

#   Redistribution and use in source and binary forms, with or without
#   modification, are permitted provided that the following conditions are met:

#   1. Redistributions of source code must retain the above copyright notice,
#       this list of conditions and the following disclaimer.

#   2. Redistributions in binary form must reproduce the above copyright notice,
#       this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.

#   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
#   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
#   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
#   ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
#   LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
#   CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
#   SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
#   INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
#   CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
#   ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
#   POSSIBILITY OF SUCH DAMAGE.

# CMake build environment for the Valvers Raspberry-Pi bare metal tutorials

# CMake 2.8.10 Documentation: http://www.cmake.org/cmake/help/v2.8.10/cmake.html

cmake_minimum_required( VERSION 2.8 )

# Mark the language as C so that CMake doesn't try to test the C++
# cross-compiler's ability to compile a simple program because that will fail
project( rgb_to_hdmi C ASM )

set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS}" )

# Add any C compiler flags here. The C flags concerned with processor tuning
# are in the toolchain files toolchain-arm-none-eabi-bcm2835.cmake, etc.
set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2" )
set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall" )
set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -nostartfiles" )
#set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fomit-frame-pointer " )
#set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ffixed-ip" )

set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-delete-null-pointer-checks -fdata-sections -ffunction-sections ")

set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --specs=nano.specs --specs=nosys.specs -u _printf_float" )

# Set the linker flags so that we use our "custom" linker script

#set( CMAKE_EXE_LINKER_FLAGS "-Wl,--gc-sections -Wl,--print-gc-sections -Wl,-unresolved-symbols=ignore-in-object-files -Wl,-T,${PROJECT_SOURCE_DIR}/rpi.ld" )

set( CMAKE_EXE_LINKER_FLAGS "-Wl,--gc-sections -Wl,-T,${PROJECT_SOURCE_DIR}/rpi.ld" )


file( GLOB core_files
    armc-start.S
    armc-cstartup.c
    armc-cstubs.c
    rgb_to_hdmi.c
    rgb_to_fb.S
    capture_line_mode7_4bpp.S
    capture_line_default_4bpp_8bpp.S
    capture_line_default_sixbits_4bpp_8bpp.S
    capture_line_default_double_4bpp_8bpp.S
    capture_line_default_sixbits_double_4bpp_8bpp.S
    capture_line_fast_4bpp_8bpp.S
    capture_line_fast_sixbits_4bpp_8bpp.S
    capture_line_ntsc_sixbits_4bpp_8bpp.S
    capture_line_inband_4bpp_8bpp.S
    capture_line_oddeven_4bpp_8bpp.S
    capture_line_half_4bpp_8bpp.S
    defs.h
    arm-exception.c
    cache.c
    cache.h
    rpi-gpio.c
    rpi-gpio.h
    rpi-aux.c
    rpi-aux.h
    rpi-interrupts.c
    rpi-interrupts.h
    rpi-mailbox.c
    rpi-mailbox.h
    rpi-mailbox-interface.c
    rpi-mailbox-interface.h
    info.c
    info.h
    logging.c
    logging.h
    cpld.h
    cpld_normal.h
    cpld_normal.c
    cpld_atom.h
    cpld_atom.c
    geometry.h
    geometry.c
    osd.h
    osd.c
    saa5050_font.h
    saa5050_font.c
    8x8_font.h
    8x8_font.c
    # File system functions
    filesystem.c
    filesystem.h
    # LodePNG Library
    lodepng.c
    lodepng.h
    # Tiny PNG Output Library
    tiny_png_out.c
    tiny_png_out.h
    # FatFS + EMMC driver
    fatfs/block.h
    fatfs/diskio.c
    fatfs/diskio.h
    fatfs/ff.c
    fatfs/ffconf.h
    fatfs/ff.h
    fatfs/integer.h
    fatfs/options/ccsbcs.c
    fatfs/sdcard.c
    # FaTFS needs the rpi-systemtimer package
    rpi-systimer.c
    rpi-systimer.h
    # Xilinx Embedded CPLD programming
    jtag/lenval.c
    jtag/lenval.h
    jtag/micro.c
    jtag/micro.h
    jtag/ports.c
    jtag/ports.h
    jtag/update_cpld.c
    jtag/update_cpld.h
)


if( ${DEBUG} )

    set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DDEBUG=1 " )

endif()

add_executable( rgb-to-hdmi
    ${core_files}
)

target_link_libraries (rgb-to-hdmi m)

add_custom_command(
    TARGET rgb-to-hdmi POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} ./rgb-to-hdmi -O binary ${KERNEL_NAME}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Convert the ELF output file to a binary image" )

# Generate a header file with the current git version in it

include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_custom_target(gitversion ALL)

add_custom_command(
    TARGET gitversion
    COMMAND bash version.sh
)

add_dependencies(rgb-to-hdmi gitversion)
