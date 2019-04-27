# ======================================================================
# Property format for Profiles
# ======================================================================
#
# sampling06 (or just sampling): Sample points for other modes
#
# sampling7: Sample points for mode 7
#
# These properties use a common syntax: an list of comma seperated numbers
# corresponding in order to the following parameters:
#
#                   ------- Range -------     --- Default Value ---
#                   Mode 0..6      Mode 7     Mode 0..6      Mode 7
#     - All Offsets      0..5        0..7             0           0
#     -    A Offset      0..5        0..7             0           0
#     -    B Offset      0..5        0..7             0           0
#     -    C Offset      0..5        0..7             0           0
#     -    D Offset      0..5        0..7             0           0
#     -    E Offset      0..5        0..7             0           0
#     -    F Offset      0..5        0..7             0           0
#     -        Half      0..1        0..1             0           0
#     -     Divider    6 or 8      6 or 8             6           8
#     -         Mux      0..1        0..1             0           0
#     -       Delay     0..15       0..15             0           4
#     - Sample Mode      0..3        0..3             0           0
#
# Any number of these parameters can be specified but typically you would specify:
#
#     - A single value: All Offsets (which sets Offsets A..F to that value)
#                         -- or --
#     - 12 comma seperated values: 0,A,B,C,D,E,F,Half,Divisor,Mux,Delay,Sample Mode
#
# Here's a brief description of each of sampling parameter:
#     -  A Offset: set the sub-pixel sampling point for the 1st Pixel, 7th Pixel, etc.
#     -       ...
#     -    F Offset: set the sub-pixel sampling point for the 6st Pixel, 8th Pixel, etc.
#     -        Half: enables an additional half pixel delay
#     -     Divider: sets wheher the cpld samples every 6 or 8 clocks
#     -         Mux: selects direct input (0) or via tha 74LS08 buffer (1)
#     -       Delay: enables an additional N-pixel delay (CPLDv2 and later)
#     - Sample Mode: 0 = normal rate sampling,   3 bits/pixel
#                    1 = double rate sampling,   6 bits/pixel (CPLDv3 and later)
#                    2 = half-odd rate sampling, 3 bits/pixel (CPLDv4 and later)
#                    3 = half-odd rate sampling, 3 bits/pixel (CPLDv4 and later)
#
# The basic sampling06 and sampling7 values can be copied from the Calibration Summary screen:
#     - Select Mode 6 or Mode 7 screen (as appropriate)
#     - Type *HELP to get some text on the screen
#     - Press Auto Calibation (right button)
#     - Select: Feature Menu/Info/Calibration Summary
#     - For A..F use the six "Offset" values
#     - For Half use the "Half" value
#     - For Divider use 0 for modes 0..6 and 1 for mode 7
#     - For Mux use 0 (unless you have an Electron)
#     - For Delay use the "Delay" value
#     - For Sample Mode use 0
#
# geometry06: (or just geometry): Geometry/mode configuration for other modes
#
# geometry7: Geometry/mode configuration for mode 7
#
# These properties use a common syntax: an list of comma seperated numbers
# corresponding in order to the following parameters:
#
#                         ------- Range -------     --- Default Value ---
#                                                   Mode 0..6      Mode 7
#     -        H Offset         0         100            32          24 | (CPLDv2)
#     -        V Offset         0          39            21          18 |
#     -         H Width         1         100            84          63 |
#     -        V Height         1         300           270         270 | These don't
#     -        FB Width       250         800           672         504 | need to be
#     -       FB Height       180         600           270         270 | changed for
#      FB Double Height         0           1             1           1 | a Beeb or
#     -   FB Bits/pixel         4    or     8             8           4 | a Master or
#     -           Clock  10000000   100000000      96000000    96000000 | an Electron
#     -     Line Length      1000        9999          6144        6144 |
#     - Clock tolerance         0      100000          5000        5000 |
#     -  Pixel sampling         0           5             0           0 |
#
# Note: for CPLDv1, the H Offset defaults are 0 and 0
# Note: for CPLDv3, the H Offset defaults are 38 and 32
#
# Any number of these parameters can be specified but typically you would specify
# 7 comma seperated values:
#     - H Offset,V Offset,H Width,V Height, FB Width, FB Height, FB Height x2
#
# Values outside of the ranges above may or may not work!
#
# Here's a brief description of each of sampling parameter:
#     -         H Offset: defines where to start capturing a line (units are 4-pixels)
#     -         V Offset: defines which line to start capturing (units are scan lines within the field)
#     -          H Width: defines how much of a line to capture (units are 8-pixels)
#     -         V Height: defines how many lines to capture (units are scan lines within the the field)
#     -         FB Width: defines the width of the frame buffer to capture into (in pixels)
#     -        FB Height: defines the height of the frame buffer to capture into (in pixels)
#     - FB Double Height: if 1, doubles the frame buffer height
#     -    FB Bits/pixel: defines the number of bits per pixel in the frame buffer (4 or 8)
#     -            Clock: the nominal sampling clock fed to the CPLD (in Hz)
#     -      Line Length: the length of a horizontal line (in sampling clocks)
#     -  Clock tolerance: the maximum correctable error in the sampling clock (in ppm)
#     -   Pixel sampling: 0=Normal     Every pixel is captured
#                         1=Odd        Odd pixels only are captured, and then duplicated
#                         2=Even       Even pixels only are captured, and then duplicated
#                         3=Half Odd   Odd pixels only are captured
#                         4=Half Even  Even pixels only are captured
#                         5=Double     Every pixel is captured, and the duplicated
#
# info: the default info screen
#     - 0 is the calibration summary
#     - 1 is the calibration detail
#     - 2 is the calibration raw
#     - 3 is the firmware version
#
# palette: colour palette number
#     - 0 is RGB        (Beeb)
#     - 1 is RGBI       (IBM PC with CGA)
#     - 2 is RGBICGA    (IBM PC with CGA, colour 6 is brown rather than dark yellow)
#     - 3 is RrGgBb     (IBM PC with EGA)
#     - 4 is Inverse
#     - 5 is Mono 1
#     - 6 is Mono 2
#     - 7 is Just Red
#     - 8 is Just Green
#     - 9 is Just Blue
#     - 10 is Not Red
#     - 11 is Not Green
#     - 12 is Not Blue
#     - 13 is Atom Colour Normal    (Atom CPLD, default)
#     - 14 is Atom Colour Extended  (Atom CPLD, adds dark green/orange text background)
#     - 15 is Atom Colour Acorn     (Atom CPLD, no orange, just like the Acorn Colour Card)
#     - 16 is Atom Mono             (Atom CPLD, monochrome)
#     - 17 is Atom Experimental     (Normal CPLD, tries to add orange using 6-bit capture mode)
#
# deinterlace: algorithm used for mode 7 deinterlacing
#     - 0 is None
#     - 1 is Simple Bob
#     - 2 is Simple Motion adaptive 1
#     - 3 is Simple Motion adaptive 2
#     - 4 is Simple Motion adaptive 3
#     - 5 is Simple Motion adaptive 4
#     - 6 is Advanced Motion adaptive (needs CPLDv2)
#
# scalines: show visible scanlines in modes 0..6
#     - 0 is scanlines off
#     - 1 is scanlines on
#
# vsynctype: indicates the interface is connected to an Electron
#     - 0 is Model B/Master and other computers
#     - 1 is Electron
#
# vsync: indicates the position of the HDMI vsync
#     - 0 is vsync indicator off
#     - 1 is vsync indicator on
#
# vlockmode: controls the (HDMI) vertical locking mode
#     - 0 is Unlocked
#     - 1 is 2000ppm Slow
#     - 2 is 500ppm Slow
#     - 3 is Locked (Exact) [ i.e. Genlocked ]
#     - 4 is 500ppm Fast
#     - 5 is 2000ppm Fast
#
#  When the HDMI clock is fast the vsync indicator moves up.
#  When the HDMI clock is slow the vsync indicator moves down.
#
# vlockline: sets the target vsync line when vlockmode is set to 3 - Locked (Exact)
#     - range is currently 5 to 265, with 5 being right at the top
#
# nbuffers: controls how many buffers are used in Mode 0..6
#     - 0 = single buffered
#     - 1 = double buffered
#     - 2 = triple buffered
#     - 3 = quadruple buffered
#
# debug: enables debug mode
#     - 0 is debug off
#     - 1 is debug on
#
# autoswitch: enables or disables auto switching between different timing settings
#     - 0 is disabled
#     - 1 is switching between modes0-6 and mode 7 on BBC micro / Master 128 or Electron with a Mode 7 expansion board
#     - 2 is switching between PC CGA and EGA modes
#
#
# actionmap: specifies how keys presses are bound to actions
#     - The default is 012453
#     - The format is <SW1 short><SW2 short><SW3 short><SW1 long><SW2 long><SW3 long>
#     - The values of the digits indicate the action:
#     -    0 = Launch Menu
#     -    1 = Screen Capture
#     -    2 = HDMI Clock Calibration
#     -    3 = Auto Calibration
#     -    4 = Toggle Scanlines
#     -    5 = Spare
#     -    6 = Spare
#     -    7 = Spare
#
# keymap: specifies how keys are used for menu navigation
#     - The default is 12323
#     - The individual digits numbers correspond to the following actions:
#     -    key_enter
#     -    key_menu_up
#     -    key_menu_down
#     -    key_value_dec
#     -    key_value_inc
#     - Key SW1 is 1, key SW2 is 2, etc
#
# return: specifies the position of the return link in menus
#     - 0 is at the start
#     - 1 is at the end (the default)
