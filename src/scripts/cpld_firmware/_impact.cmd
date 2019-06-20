setMode -bs
setMode -bs
setMode -bs
setMode -bs
setCable -port xsvf -file "/home/dmb/atom/RGBtoHDMI/src/scripts/cpld_firmware/atom/ATOM_CPLD_v24.xsvf"
addDevice -p 1 -file "/home/dmb/atom/RGBtoHDMI/vhdl_atom/RGBtoHDMI.jed"
Program -p 1 -e -v 
setMode -bs
deleteDevice -position 1
setMode -bs
setMode -ss
setMode -sm
setMode -hw140
setMode -spi
setMode -acecf
setMode -acempm
setMode -pff
