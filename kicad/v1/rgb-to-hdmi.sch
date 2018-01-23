EESchema Schematic File Version 2
LIBS:power
LIBS:device
LIBS:transistors
LIBS:conn
LIBS:linear
LIBS:regul
LIBS:74xx
LIBS:cmos4000
LIBS:adc-dac
LIBS:memory
LIBS:xilinx
LIBS:microcontrollers
LIBS:dsp
LIBS:microchip
LIBS:analog_switches
LIBS:motorola
LIBS:texas
LIBS:intel
LIBS:audio
LIBS:interface
LIBS:digital-audio
LIBS:philips
LIBS:display
LIBS:cypress
LIBS:siliconi
LIBS:opto
LIBS:atmel
LIBS:contrib
LIBS:valves
LIBS:xc9572xl
LIBS:rgb-to-hdmi-cache
EELAYER 25 0
EELAYER END
$Descr A4 11693 8268
encoding utf-8
Sheet 1 1
Title ""
Date ""
Rev ""
Comp ""
Comment1 ""
Comment2 ""
Comment3 ""
Comment4 ""
$EndDescr
$Comp
L CONN_02X20 P1
U 1 1 574468E6
P 8650 3550
F 0 "P1" H 8650 4600 50  0000 C CNN
F 1 "CONN_02X20" V 8650 3550 50  0000 C CNN
F 2 "Socket_Strips:Socket_Strip_Straight_2x20" H 8650 2600 50  0001 C CNN
F 3 "" H 8650 2600 50  0000 C CNN
	1    8650 3550
	1    0    0    -1  
$EndComp
$Comp
L XC9572XL-VQFP44 U1
U 1 1 595A11EC
P 5750 3600
F 0 "U1" H 5750 1400 60  0000 C CNN
F 1 "XC9572XL-VQFP44" H 5750 1550 60  0000 C CNN
F 2 "Housings_QFP:LQFP-44_10x10mm_Pitch0.8mm" H 5750 3600 60  0001 C CNN
F 3 "" H 5750 3600 60  0001 C CNN
	1    5750 3600
	1    0    0    -1  
$EndComp
$Comp
L CONN_01X06 P4
U 1 1 595A124F
P 1550 6750
F 0 "P4" H 1550 7100 50  0000 C CNN
F 1 "CONN_01X06" V 1650 6750 50  0000 C CNN
F 2 "footprints:DIN45322" H 1550 6750 50  0001 C CNN
F 3 "" H 1550 6750 50  0000 C CNN
	1    1550 6750
	1    0    0    -1  
$EndComp
$Comp
L CONN_01X06 P5
U 1 1 595A12E7
P 3000 6750
F 0 "P5" H 3000 7100 50  0000 C CNN
F 1 "CONN_01X06" V 3100 6750 50  0000 C CNN
F 2 "Pin_Headers:Pin_Header_Straight_1x06_Pitch2.54mm" H 3000 6750 50  0001 C CNN
F 3 "" H 3000 6750 50  0000 C CNN
	1    3000 6750
	1    0    0    -1  
$EndComp
$Comp
L CONN_01X06 P6
U 1 1 595A1366
P 4650 6750
F 0 "P6" H 4650 7100 50  0000 C CNN
F 1 "CONN_01X06" V 4750 6750 50  0000 C CNN
F 2 "Pin_Headers:Pin_Header_Straight_1x06_Pitch2.54mm" H 4650 6750 50  0001 C CNN
F 3 "" H 4650 6750 50  0000 C CNN
	1    4650 6750
	1    0    0    -1  
$EndComp
$Comp
L CONN_01X02 P2
U 1 1 595A13C3
P 2800 3400
F 0 "P2" H 2800 3550 50  0000 C CNN
F 1 "CONN_01X02" V 2900 3400 50  0000 C CNN
F 2 "Pin_Headers:Pin_Header_Straight_1x02_Pitch2.54mm" H 2800 3400 50  0001 C CNN
F 3 "" H 2800 3400 50  0000 C CNN
	1    2800 3400
	1    0    0    -1  
$EndComp
$Comp
L CP1_Small C1
U 1 1 595A1518
P 1350 5450
F 0 "C1" H 1360 5520 50  0000 L CNN
F 1 "CP1_Small" H 1360 5370 50  0000 L CNN
F 2 "Capacitors_Tantalum_SMD:CP_Tantalum_Case-C_EIA-6032-28_Hand" H 1350 5450 50  0001 C CNN
F 3 "" H 1350 5450 50  0000 C CNN
	1    1350 5450
	1    0    0    -1  
$EndComp
Text Label 8400 2700 2    60   ~ 0
GPIO2
Text Label 8400 2800 2    60   ~ 0
GPIO3
Text Label 8400 2900 2    60   ~ 0
GPIO4
Text Label 8400 3000 2    60   ~ 0
GND
Text Label 8400 3100 2    60   ~ 0
GPIO17
Text Label 8400 3500 2    60   ~ 0
GPIO10
Text Label 8400 3600 2    60   ~ 0
GPIO9
Text Label 8400 3700 2    60   ~ 0
GPIO11
Text Label 8400 3800 2    60   ~ 0
GND
Text Label 8400 4000 2    60   ~ 0
GPIO5
Text Label 8400 4100 2    60   ~ 0
GPIO6
Text Label 8400 4200 2    60   ~ 0
GPIO13
Text Label 8400 4300 2    60   ~ 0
GPIO19
Text Label 8400 4400 2    60   ~ 0
GPIO26
Text Label 8400 4500 2    60   ~ 0
GND
Text Label 8900 2600 0    60   ~ 0
VCC
Text Label 8900 2700 0    60   ~ 0
VCC
Text Label 8900 2800 0    60   ~ 0
GND
Text Label 8900 2900 0    60   ~ 0
TxD
Text Label 8900 3000 0    60   ~ 0
RxD
Text Label 8900 3100 0    60   ~ 0
GPIO18
Text Label 8900 3500 0    60   ~ 0
GND
Text Label 8900 3200 0    60   ~ 0
GND
Text Label 8900 3700 0    60   ~ 0
GPIO8
Text Label 8900 3800 0    60   ~ 0
GPIO7
Text Label 8900 4000 0    60   ~ 0
GND
Text Label 8900 4100 0    60   ~ 0
GPIO12
Text Label 8900 4200 0    60   ~ 0
GND
Text Label 8900 4300 0    60   ~ 0
GPIO16
Text Label 8900 4400 0    60   ~ 0
GPIO20
Text Label 8900 4500 0    60   ~ 0
GPIO21
NoConn ~ 8400 3400
NoConn ~ 8400 3900
NoConn ~ 8900 3900
NoConn ~ 8900 3600
$Comp
L CONN_01X03 P3
U 1 1 595A288D
P 9700 2900
F 0 "P3" H 9700 3100 50  0000 C CNN
F 1 "CONN_01X03" V 9800 2900 50  0000 C CNN
F 2 "Pin_Headers:Pin_Header_Straight_1x03_Pitch2.54mm" H 9700 2900 50  0001 C CNN
F 3 "" H 9700 2900 50  0000 C CNN
	1    9700 2900
	1    0    0    -1  
$EndComp
Text Label 2600 5150 0    60   ~ 0
3V3
Text Label 6700 5450 0    60   ~ 0
TCK
Text Label 6700 5600 0    60   ~ 0
TDI
Text Label 6700 5750 0    60   ~ 0
TMS
Text Label 6700 5900 0    60   ~ 0
TDO
Text Label 4450 6500 2    60   ~ 0
3V3
Text Label 4450 6600 2    60   ~ 0
GND
Text Label 4450 6700 2    60   ~ 0
TCK
Text Label 4450 6800 2    60   ~ 0
TDO
Text Label 4450 6900 2    60   ~ 0
TDI
Text Label 4450 7000 2    60   ~ 0
TMS
Text Label 1150 5150 2    60   ~ 0
VCC
$Comp
L C_Small C4
U 1 1 595A3174
P 3250 5450
F 0 "C4" H 3260 5520 50  0000 L CNN
F 1 "C_Small" H 3260 5370 50  0000 L CNN
F 2 "Capacitors_ThroughHole:C_Disc_D3.0mm_W1.6mm_P2.50mm" H 3250 5450 50  0001 C CNN
F 3 "" H 3250 5450 50  0000 C CNN
	1    3250 5450
	1    0    0    -1  
$EndComp
$Comp
L C_Small C3
U 1 1 595A31EE
P 4250 5450
F 0 "C3" H 4260 5520 50  0000 L CNN
F 1 "C_Small" H 4260 5370 50  0000 L CNN
F 2 "Capacitors_ThroughHole:C_Disc_D3.0mm_W1.6mm_P2.50mm" H 4250 5450 50  0001 C CNN
F 3 "" H 4250 5450 50  0000 C CNN
	1    4250 5450
	1    0    0    -1  
$EndComp
$Comp
L C_Small C5
U 1 1 595A3251
P 3800 5450
F 0 "C5" H 3810 5520 50  0000 L CNN
F 1 "C_Small" H 3810 5370 50  0000 L CNN
F 2 "Capacitors_ThroughHole:C_Disc_D3.0mm_W1.6mm_P2.50mm" H 3800 5450 50  0001 C CNN
F 3 "" H 3800 5450 50  0000 C CNN
	1    3800 5450
	1    0    0    -1  
$EndComp
Text Label 1350 6500 2    60   ~ 0
RED
Text Label 1350 6600 2    60   ~ 0
GREEN
Text Label 1350 6700 2    60   ~ 0
BLUE
Text Label 1350 6800 2    60   ~ 0
SYNC
Text Label 1350 6900 2    60   ~ 0
GND
Text Label 1350 7000 2    60   ~ 0
VCC
Text Label 2800 6500 2    60   ~ 0
RED
Text Label 2800 6600 2    60   ~ 0
GREEN
Text Label 2800 6700 2    60   ~ 0
BLUE
Text Label 2800 6800 2    60   ~ 0
SYNC
Text Label 2800 6900 2    60   ~ 0
GND
Text Label 2800 7000 2    60   ~ 0
VCC
Text Label 4800 3750 2    60   ~ 0
GPIO2
Text Label 6700 4650 0    60   ~ 0
GPIO3
Text Label 6700 4500 0    60   ~ 0
GPIO4
Text Label 6700 4350 0    60   ~ 0
GPIO18
Text Label 6700 4200 0    60   ~ 0
GPIO17
Text Label 6700 3500 0    60   ~ 0
GPIO10
Text Label 6700 3200 0    60   ~ 0
GPIO9
Text Label 6700 3050 0    60   ~ 0
GPIO11
Text Label 6700 2900 0    60   ~ 0
GPIO8
Text Label 6700 2750 0    60   ~ 0
GPIO7
Text Label 6700 2600 0    60   ~ 0
GPIO5
Text Label 6700 2450 0    60   ~ 0
GPIO12
Text Label 6700 2300 0    60   ~ 0
GPIO6
Text Label 4800 3500 2    60   ~ 0
GPIO13
Text Label 4800 3350 2    60   ~ 0
GPIO16
Text Label 4800 3200 2    60   ~ 0
GPIO20
Text Label 4800 2900 2    60   ~ 0
GPIO21
Text Label 4800 3050 2    60   ~ 0
GPIO26
Text Label 4800 2750 2    60   ~ 0
GPIO19
Text Label 4800 3900 2    60   ~ 0
SYNC
Text Label 4800 4050 2    60   ~ 0
BLUE
Text Label 4800 4200 2    60   ~ 0
GREEN
Text Label 4800 4350 2    60   ~ 0
RED
NoConn ~ 4800 4500
NoConn ~ 4800 2450
NoConn ~ 4800 2600
Wire Wire Line
	4800 5600 4600 5600
Wire Wire Line
	4600 5600 4600 5900
Wire Wire Line
	4600 5750 4800 5750
Wire Wire Line
	800  5900 4800 5900
Connection ~ 4600 5750
Wire Wire Line
	2150 5150 4800 5150
Wire Wire Line
	4600 5150 4600 5450
Wire Wire Line
	4600 5450 4800 5450
Wire Wire Line
	4800 5300 4600 5300
Connection ~ 4600 5300
Connection ~ 4600 5150
Wire Wire Line
	8900 3000 9500 3000
Wire Wire Line
	8900 2900 9500 2900
Wire Wire Line
	8900 2800 9500 2800
Wire Wire Line
	1750 5450 1750 5900
Connection ~ 4600 5900
Connection ~ 2150 5900
Connection ~ 1750 5900
Connection ~ 1350 5900
Wire Wire Line
	800  5150 1350 5150
Connection ~ 3250 5150
Wire Wire Line
	3800 5350 3800 5150
Connection ~ 3800 5150
Wire Wire Line
	4250 5350 4250 5150
Connection ~ 4250 5150
Wire Wire Line
	4250 5550 4250 5900
Connection ~ 4250 5900
Wire Wire Line
	3800 5550 3800 5900
Connection ~ 3800 5900
Connection ~ 3250 5900
Wire Wire Line
	2600 3350 2300 3350
Wire Wire Line
	2300 3450 2600 3450
Wire Wire Line
	3250 5550 3250 5900
Wire Wire Line
	3250 5150 3250 5350
Text Label 2800 5900 2    60   ~ 0
GND
$Comp
L PWR_FLAG #FLG01
U 1 1 595A4F7A
P 800 6000
F 0 "#FLG01" H 800 6095 50  0001 C CNN
F 1 "PWR_FLAG" H 800 6180 50  0000 C CNN
F 2 "" H 800 6000 50  0000 C CNN
F 3 "" H 800 6000 50  0000 C CNN
	1    800  6000
	-1   0    0    1   
$EndComp
$Comp
L PWR_FLAG #FLG02
U 1 1 595A532E
P 800 4950
F 0 "#FLG02" H 800 5045 50  0001 C CNN
F 1 "PWR_FLAG" H 800 5130 50  0000 C CNN
F 2 "" H 800 4950 50  0000 C CNN
F 3 "" H 800 4950 50  0000 C CNN
	1    800  4950
	1    0    0    -1  
$EndComp
NoConn ~ 8400 2600
NoConn ~ 5150 2650
NoConn ~ 10550 2000
NoConn ~ -500 3250
NoConn ~ 6350 2600
$Comp
L LD1117S33TR U2
U 1 1 595A1621
P 1750 5200
F 0 "U2" H 1750 5450 50  0000 C CNN
F 1 "LD1117S33TR" H 1750 5400 50  0000 C CNN
F 2 "TO_SOT_Packages_SMD:SOT-223" H 1750 5300 50  0000 C CNN
F 3 "" H 1750 5200 50  0000 C CNN
	1    1750 5200
	1    0    0    -1  
$EndComp
Wire Wire Line
	2150 5150 2150 5350
Wire Wire Line
	2150 5550 2150 5900
Wire Wire Line
	1350 5550 1350 5900
Wire Wire Line
	1350 5150 1350 5350
$Comp
L CP1_Small C2
U 1 1 595B5ACB
P 2150 5450
F 0 "C2" H 2160 5520 50  0000 L CNN
F 1 "CP1_Small" H 2160 5370 50  0000 L CNN
F 2 "Capacitors_Tantalum_SMD:CP_Tantalum_Case-C_EIA-6032-28_Hand" H 2150 5450 50  0001 C CNN
F 3 "" H 2150 5450 50  0000 C CNN
	1    2150 5450
	1    0    0    -1  
$EndComp
Wire Wire Line
	800  4950 800  5150
Wire Wire Line
	800  5900 800  6000
$Comp
L SW_PUSH SW1
U 1 1 595B68BB
P 2000 1500
F 0 "SW1" H 2150 1610 50  0000 C CNN
F 1 "SW_PUSH" H 2000 1420 50  0000 C CNN
F 2 "Buttons_Switches_ThroughHole:SW_PUSH_6mm_h5mm" H 2000 1500 50  0001 C CNN
F 3 "" H 2000 1500 50  0000 C CNN
	1    2000 1500
	1    0    0    -1  
$EndComp
$Comp
L R_Small R1
U 1 1 595B6CB3
P 2400 1300
F 0 "R1" H 2430 1320 50  0000 L CNN
F 1 "4K7" H 2430 1260 50  0000 L CNN
F 2 "Resistors_ThroughHole:R_Axial_DIN0207_L6.3mm_D2.5mm_P10.16mm_Horizontal" H 2400 1300 50  0001 C CNN
F 3 "" H 2400 1300 50  0000 C CNN
	1    2400 1300
	1    0    0    -1  
$EndComp
Wire Wire Line
	2300 1500 2550 1500
Wire Wire Line
	2400 1500 2400 1400
Wire Wire Line
	2400 1200 2400 1100
Text Label 2400 1100 0    60   ~ 0
3V3
Connection ~ 2400 1500
Text Label 2550 1500 0    60   ~ 0
SW1
$Comp
L SW_PUSH SW2
U 1 1 595B6852
P 2000 2100
F 0 "SW2" H 2150 2210 50  0000 C CNN
F 1 "SW_PUSH" H 2000 2020 50  0000 C CNN
F 2 "Buttons_Switches_ThroughHole:SW_PUSH_6mm_h5mm" H 2000 2100 50  0001 C CNN
F 3 "" H 2000 2100 50  0000 C CNN
	1    2000 2100
	1    0    0    -1  
$EndComp
$Comp
L SW_PUSH SW3
U 1 1 595B6785
P 2000 2700
F 0 "SW3" H 2150 2810 50  0000 C CNN
F 1 "SW_PUSH" H 2000 2620 50  0000 C CNN
F 2 "Buttons_Switches_ThroughHole:SW_PUSH_6mm_h5mm" H 2000 2700 50  0001 C CNN
F 3 "" H 2000 2700 50  0000 C CNN
	1    2000 2700
	1    0    0    -1  
$EndComp
$Comp
L R_Small R2
U 1 1 595B744A
P 2400 1900
F 0 "R2" H 2430 1920 50  0000 L CNN
F 1 "4K7" H 2430 1860 50  0000 L CNN
F 2 "Resistors_ThroughHole:R_Axial_DIN0207_L6.3mm_D2.5mm_P10.16mm_Horizontal" H 2400 1900 50  0001 C CNN
F 3 "" H 2400 1900 50  0000 C CNN
	1    2400 1900
	1    0    0    -1  
$EndComp
$Comp
L R_Small R3
U 1 1 595B74A8
P 2400 2500
F 0 "R3" H 2430 2520 50  0000 L CNN
F 1 "4K7" H 2430 2460 50  0000 L CNN
F 2 "Resistors_ThroughHole:R_Axial_DIN0207_L6.3mm_D2.5mm_P10.16mm_Horizontal" H 2400 2500 50  0001 C CNN
F 3 "" H 2400 2500 50  0000 C CNN
	1    2400 2500
	1    0    0    -1  
$EndComp
Wire Wire Line
	2300 2700 2550 2700
Wire Wire Line
	2400 2700 2400 2600
Wire Wire Line
	2300 2100 2550 2100
Wire Wire Line
	2400 2100 2400 2000
Connection ~ 2400 2100
Text Label 2550 2100 0    60   ~ 0
SW2
Connection ~ 2400 2700
Text Label 2550 2700 0    60   ~ 0
SW3
Wire Wire Line
	2400 1800 2400 1700
Text Label 2400 1700 0    60   ~ 0
3V3
Wire Wire Line
	2400 2400 2400 2300
Text Label 2400 2300 0    60   ~ 0
3V3
Wire Wire Line
	1700 1500 1700 2900
Connection ~ 1700 2100
Connection ~ 1700 2700
Text Label 1700 2900 2    60   ~ 0
GND
$Comp
L SW_PUSH SW4
U 1 1 595B7AD4
P 2000 3350
F 0 "SW4" H 2150 3460 50  0000 C CNN
F 1 "SW_PUSH" H 2000 3270 50  0000 C CNN
F 2 "Buttons_Switches_ThroughHole:SW_PUSH_6mm_h5mm" H 2000 3350 50  0001 C CNN
F 3 "" H 2000 3350 50  0000 C CNN
	1    2000 3350
	1    0    0    -1  
$EndComp
Wire Wire Line
	2300 3450 2300 3600
Wire Wire Line
	2300 3600 1700 3600
Wire Wire Line
	1700 3600 1700 3350
Text Label 4800 4800 2    60   ~ 0
SW2
Text Label 4800 4950 2    60   ~ 0
SW1
Text Label 4800 2300 2    60   ~ 0
SW3
NoConn ~ 4800 4650
NoConn ~ 5800 2200
Text Label 8400 3200 2    60   ~ 0
GPIO27
Text Label 8400 3300 2    60   ~ 0
GPIO22
Text Label 8900 3300 0    60   ~ 0
GPIO23
Text Label 8900 3400 0    60   ~ 0
GPIO24
Text Label 6700 4050 0    60   ~ 0
GPIO27
Text Label 6700 3900 0    60   ~ 0
GPIO23
Text Label 6700 3750 0    60   ~ 0
GPIO22
Text Label 6700 3350 0    60   ~ 0
GPIO24
$EndSCHEMATC
