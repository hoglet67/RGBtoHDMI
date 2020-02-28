EESchema Schematic File Version 4
LIBS:buffer-cache
EELAYER 30 0
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
L Connector_Generic:Conn_01x01 P6
U 1 1 5DD07D72
P 1200 750
F 0 "P6" H 1280 792 50  0000 L CNN
F 1 "Conn_01x01" H 1280 701 50  0000 L CNN
F 2 "Connector_PinHeader_2.54mm:PinHeader_1x01_P2.54mm_Vertical" H 1200 750 50  0001 C CNN
F 3 "~" H 1200 750 50  0001 C CNN
	1    1200 750 
	1    0    0    -1  
$EndComp
$Comp
L Connector_Generic:Conn_01x04 P2
U 1 1 5DCFA861
P 1100 4950
F 0 "P2" H 1100 4500 50  0000 C CNN
F 1 "Conn_01x04" H 1100 4600 50  0000 C CNN
F 2 "Connector_PinHeader_2.54mm:PinHeader_1x04_P2.54mm_Vertical" H 1100 4950 50  0001 C CNN
F 3 "~" H 1100 4950 50  0001 C CNN
	1    1100 4950
	-1   0    0    1   
$EndComp
Text Label 2450 3900 0    50   ~ 0
ASYNC
Text Label 2450 3800 0    50   ~ 0
ABLUE
Text Label 2450 3700 0    50   ~ 0
AGREEN
Text Label 2450 3600 0    50   ~ 0
ARED
NoConn ~ 1000 750 
Text Label 2450 4000 0    50   ~ 0
GND
$Comp
L Connector_Generic:Conn_01x06 P3
U 1 1 5E3F9F6C
P 2250 3800
F 0 "P3" H 2168 3275 50  0000 C CNN
F 1 "Conn_01x06" H 2168 3366 50  0000 C CNN
F 2 "Connector_Molex:Molex_PicoBlade_53048-0610_1x06_P1.25mm_Horizontal" H 2250 3800 50  0001 C CNN
F 3 "~" H 2250 3800 50  0001 C CNN
	1    2250 3800
	-1   0    0    1   
$EndComp
Text Label 2450 3500 0    50   ~ 0
VCC_IN
Text Notes 8950 3200 0    50   ~ 0
NOTE: Connections\nreversed compared\nto RGB to HD pcb
$Comp
L Connector_Generic:Conn_02x06_Odd_Even P1
U 1 1 5E50C60D
P 9250 3800
F 0 "P1" H 9300 4217 50  0000 C CNN
F 1 "Conn_02x06_Odd_Even" H 9300 4126 50  0000 C CNN
F 2 "Connector_PinSocket_2.54mm:PinSocket_2x06_P2.54mm_Horizontal" H 9250 3800 50  0001 C CNN
F 3 "~" H 9250 3800 50  0001 C CNN
	1    9250 3800
	1    0    0    -1  
$EndComp
Text Label 9050 3600 2    50   ~ 0
VCC
Text Label 9050 3700 2    50   ~ 0
VSYNC
Text Label 9050 3800 2    50   ~ 0
SYNC
Text Label 9050 3900 2    50   ~ 0
BBLUE
Text Label 9550 3900 0    50   ~ 0
BRED
Text Label 9550 3800 0    50   ~ 0
RED
Text Label 9550 3700 0    50   ~ 0
GREEN
Text Label 9550 3600 0    50   ~ 0
BLUE
Text Label 9550 4000 0    50   ~ 0
GND
Text Label 9050 4000 2    50   ~ 0
BGREEN
Text Label 9550 4100 0    50   ~ 0
X1
Text Label 9050 4100 2    50   ~ 0
X2
Text Label 1400 2500 2    50   ~ 0
GND
Text Label 1400 2600 2    50   ~ 0
ABLUE
Text Label 1400 2700 2    50   ~ 0
ARED
Text Label 1950 2500 0    50   ~ 0
ASYNC
Text Label 1950 2600 0    50   ~ 0
AGREEN
Text Label 1950 2700 0    50   ~ 0
VCC_IN
$Comp
L Connector_Generic:Conn_01x01 P7
U 1 1 5E6322BC
P 2200 750
F 0 "P7" H 2280 792 50  0000 L CNN
F 1 "Conn_01x01" H 2280 701 50  0000 L CNN
F 2 "Connector_PinHeader_2.54mm:PinHeader_1x01_P2.54mm_Vertical" H 2200 750 50  0001 C CNN
F 3 "~" H 2200 750 50  0001 C CNN
	1    2200 750 
	1    0    0    -1  
$EndComp
NoConn ~ 2000 750 
NoConn ~ 2550 -700
$Comp
L Connector_Generic:Conn_01x03_EVEN P5
U 1 1 5E57FB0D
P 1750 2600
F 0 "P5" H 1750 2850 50  0000 C CNN
F 1 "Conn_01x03_EVEN" H 1450 2950 50  0000 C CNN
F 2 "Connector_Harwin:Harwin_M20-89003xx_1x03_P2.54mm_Horizontal_Custom_EVEN" H 1750 2600 50  0001 C CNN
F 3 "~" H 1750 2600 50  0001 C CNN
	1    1750 2600
	-1   0    0    1   
$EndComp
$Comp
L Connector_Generic:Conn_01x03_ODD P4
U 1 1 5E580E6E
P 1600 2600
F 0 "P4" H 1550 2350 50  0000 L CNN
F 1 "Conn_01x03_ODD" H 1000 2250 50  0000 L CNN
F 2 "Connector_Harwin:Harwin_M20-89003xx_1x03_P2.54mm_Horizontal_Custom_ODD" H 1600 2600 50  0001 C CNN
F 3 "~" H 1600 2600 50  0001 C CNN
	1    1600 2600
	1    0    0    -1  
$EndComp
$Comp
L Device:D_Schottky D1
U 1 1 5E5D9673
P 5250 1600
F 0 "D1" H 5250 1816 50  0000 C CNN
F 1 "D_Schottky" H 5250 1725 50  0000 C CNN
F 2 "Diode_SMD:D_SOD-123" H 5250 1600 50  0001 C CNN
F 3 "~" H 5250 1600 50  0001 C CNN
	1    5250 1600
	-1   0    0    -1  
$EndComp
$Comp
L Device:R_Small R1
U 1 1 5E5DA0A7
P 3800 5950
F 0 "R1" V 3604 5950 50  0000 C CNN
F 1 "47K" V 3695 5950 50  0000 C CNN
F 2 "Resistor_SMD:R_0805_2012Metric_Pad1.15x1.40mm_HandSolder" H 3800 5950 50  0001 C CNN
F 3 "~" H 3800 5950 50  0001 C CNN
	1    3800 5950
	1    0    0    -1  
$EndComp
$Comp
L Device:R_Small R2
U 1 1 5E5DA7E8
P 4150 5950
F 0 "R2" V 3954 5950 50  0000 C CNN
F 1 "47K" V 4045 5950 50  0000 C CNN
F 2 "Resistor_SMD:R_0805_2012Metric_Pad1.15x1.40mm_HandSolder" H 4150 5950 50  0001 C CNN
F 3 "~" H 4150 5950 50  0001 C CNN
	1    4150 5950
	1    0    0    -1  
$EndComp
$Comp
L Device:R_Small R3
U 1 1 5E5DAF3E
P 4500 5950
F 0 "R3" V 4304 5950 50  0000 C CNN
F 1 "47K" V 4395 5950 50  0000 C CNN
F 2 "Resistor_SMD:R_0805_2012Metric_Pad1.15x1.40mm_HandSolder" H 4500 5950 50  0001 C CNN
F 3 "~" H 4500 5950 50  0001 C CNN
	1    4500 5950
	1    0    0    -1  
$EndComp
$Comp
L Device:R_Small R4
U 1 1 5E5DB84E
P 4850 5950
F 0 "R4" V 4654 5950 50  0000 C CNN
F 1 "47K" V 4745 5950 50  0000 C CNN
F 2 "Resistor_SMD:R_0805_2012Metric_Pad1.15x1.40mm_HandSolder" H 4850 5950 50  0001 C CNN
F 3 "~" H 4850 5950 50  0001 C CNN
	1    4850 5950
	1    0    0    -1  
$EndComp
Wire Wire Line
	2450 3500 2950 3500
Wire Wire Line
	2950 3500 2950 1600
Wire Wire Line
	2950 1600 4800 1600
Wire Wire Line
	5400 1600 5700 1600
Wire Wire Line
	8500 1600 8500 3600
Wire Wire Line
	8500 3600 9050 3600
Wire Wire Line
	10350 3600 9550 3600
Wire Wire Line
	10100 3800 9550 3800
Wire Wire Line
	10250 3700 9550 3700
NoConn ~ 1300 4850
NoConn ~ 1300 4950
NoConn ~ 1300 5050
$Comp
L 74xx:74LS08 U1
U 1 1 5E57B4E4
P 5450 4600
F 0 "U1" H 5450 4925 50  0000 C CNN
F 1 "74LS08" H 5450 4834 50  0000 C CNN
F 2 "Package_SO:SOIC-14_3.9x8.7mm_P1.27mm" H 5450 4600 50  0001 C CNN
F 3 "http://www.ti.com/lit/gpn/sn74LS08" H 5450 4600 50  0001 C CNN
	1    5450 4600
	1    0    0    -1  
$EndComp
$Comp
L 74xx:74LS08 U1
U 2 1 5E57F2AA
P 5450 5150
F 0 "U1" H 5450 5475 50  0000 C CNN
F 1 "74LS08" H 5450 5384 50  0000 C CNN
F 2 "Package_SO:SOIC-14_3.9x8.7mm_P1.27mm" H 5450 5150 50  0001 C CNN
F 3 "http://www.ti.com/lit/gpn/sn74LS08" H 5450 5150 50  0001 C CNN
	2    5450 5150
	1    0    0    -1  
$EndComp
$Comp
L 74xx:74LS08 U1
U 4 1 5E5835E4
P 5450 3500
F 0 "U1" H 5450 3825 50  0000 C CNN
F 1 "74LS08" H 5450 3734 50  0000 C CNN
F 2 "Package_SO:SOIC-14_3.9x8.7mm_P1.27mm" H 5450 3500 50  0001 C CNN
F 3 "http://www.ti.com/lit/gpn/sn74LS08" H 5450 3500 50  0001 C CNN
	4    5450 3500
	1    0    0    -1  
$EndComp
$Comp
L 74xx:74LS08 U1
U 5 1 5E585D4B
P 1150 6300
F 0 "U1" H 1380 6346 50  0000 L CNN
F 1 "74LS08" H 1380 6255 50  0000 L CNN
F 2 "Package_SO:SOIC-14_3.9x8.7mm_P1.27mm" H 1150 6300 50  0001 C CNN
F 3 "http://www.ti.com/lit/gpn/sn74LS08" H 1150 6300 50  0001 C CNN
	5    1150 6300
	1    0    0    -1  
$EndComp
Text Label 1150 5800 0    50   ~ 0
VCC
Text Label 1150 6800 0    50   ~ 0
GND
Wire Wire Line
	2450 3600 4850 3600
Wire Wire Line
	5150 3600 5150 3400
Wire Wire Line
	2450 3700 4400 3700
Wire Wire Line
	4400 3700 4400 3950
Wire Wire Line
	5150 4150 5150 3950
Connection ~ 5150 3950
Wire Wire Line
	4250 3800 4250 4500
Wire Wire Line
	5150 5050 5150 5250
Wire Wire Line
	5950 4050 5750 4050
Wire Wire Line
	6200 3500 6000 3500
Wire Wire Line
	3800 5850 3800 3900
Wire Wire Line
	4500 5850 4500 3950
Wire Wire Line
	4400 3950 4500 3950
Connection ~ 4500 3950
Wire Wire Line
	4500 3950 5150 3950
Wire Wire Line
	4850 5850 4850 3600
Connection ~ 4850 3600
Wire Wire Line
	4850 3600 5150 3600
Wire Wire Line
	3800 6050 3800 6400
Wire Wire Line
	3800 6400 4150 6400
Wire Wire Line
	4150 6400 4150 6050
Wire Wire Line
	4150 6400 4500 6400
Wire Wire Line
	4500 6400 4500 6050
Connection ~ 4150 6400
Wire Wire Line
	4500 6400 4850 6400
Wire Wire Line
	4850 6400 4850 6050
Connection ~ 4500 6400
Text Label 4850 6400 0    50   ~ 0
GND
Wire Wire Line
	2450 3800 4150 3800
Wire Wire Line
	4150 5850 4150 3800
Connection ~ 4150 3800
Wire Wire Line
	4150 3800 4250 3800
Wire Wire Line
	3950 3900 3950 5050
Wire Wire Line
	2450 3900 3800 3900
Wire Wire Line
	3950 5050 5150 5050
Connection ~ 3800 3900
Wire Wire Line
	3800 3900 3950 3900
Wire Wire Line
	5300 3050 4850 3050
Wire Wire Line
	4850 3050 4850 3600
Wire Wire Line
	4500 2750 4500 3950
Wire Wire Line
	5300 2400 4150 2400
Wire Wire Line
	4150 2400 4150 3800
Connection ~ 5150 4500
Wire Wire Line
	4250 4500 5150 4500
Wire Wire Line
	5150 4500 5150 4700
Wire Wire Line
	10100 4400 10100 3800
Wire Wire Line
	6200 4400 6200 3500
Wire Wire Line
	10250 4500 10250 3700
Wire Wire Line
	5950 4500 5950 4050
Wire Wire Line
	10350 4600 10350 3600
$Comp
L 74xx:74LS08 U1
U 3 1 5E581AF8
P 5450 4050
F 0 "U1" H 5450 4375 50  0000 C CNN
F 1 "74LS08" H 5450 4284 50  0000 C CNN
F 2 "Package_SO:SOIC-14_3.9x8.7mm_P1.27mm" H 5450 4050 50  0001 C CNN
F 3 "http://www.ti.com/lit/gpn/sn74LS08" H 5450 4050 50  0001 C CNN
	3    5450 4050
	1    0    0    -1  
$EndComp
Wire Wire Line
	7600 5150 7600 3800
Wire Wire Line
	5750 5150 7150 5150
Wire Wire Line
	6200 4400 10100 4400
Wire Wire Line
	5950 4500 6450 4500
Wire Wire Line
	5750 4600 6750 4600
Wire Wire Line
	7600 3800 9050 3800
Wire Wire Line
	5300 2050 3800 2050
Wire Wire Line
	3800 2050 3800 3900
Connection ~ 5150 3600
Connection ~ 5150 5050
Wire Wire Line
	5600 3050 6000 3050
Wire Wire Line
	6000 3050 6000 3500
Connection ~ 6000 3500
Wire Wire Line
	6000 3500 5750 3500
Wire Wire Line
	5600 2750 6450 2750
Wire Wire Line
	6450 2750 6450 4500
Connection ~ 6450 4500
Wire Wire Line
	6450 4500 10250 4500
Wire Wire Line
	5600 2400 6750 2400
Wire Wire Line
	6750 2400 6750 4600
Connection ~ 6750 4600
Wire Wire Line
	6750 4600 10350 4600
Wire Wire Line
	5600 2050 7150 2050
Wire Wire Line
	7150 2050 7150 5150
Connection ~ 7150 5150
Wire Wire Line
	7150 5150 7600 5150
$Comp
L Jumper:SolderJumper_2_Open JP1
U 1 1 5E6139A7
P 5450 2050
F 0 "JP1" H 5450 2255 50  0000 C CNN
F 1 "SolderJumper_2_Open" H 5450 2164 50  0000 C CNN
F 2 "Jumper:SolderJumper-2_P1.3mm_Open_RoundedPad1.0x1.5mm" H 5450 2050 50  0001 C CNN
F 3 "~" H 5450 2050 50  0001 C CNN
	1    5450 2050
	1    0    0    -1  
$EndComp
$Comp
L Jumper:SolderJumper_2_Open JP2
U 1 1 5E614667
P 5450 2400
F 0 "JP2" H 5450 2605 50  0000 C CNN
F 1 "SolderJumper_2_Open" H 5450 2514 50  0000 C CNN
F 2 "Jumper:SolderJumper-2_P1.3mm_Open_RoundedPad1.0x1.5mm" H 5450 2400 50  0001 C CNN
F 3 "~" H 5450 2400 50  0001 C CNN
	1    5450 2400
	1    0    0    -1  
$EndComp
Wire Wire Line
	5300 2750 4500 2750
$Comp
L Jumper:SolderJumper_2_Open JP3
U 1 1 5E615EFD
P 5450 2750
F 0 "JP3" H 5450 2955 50  0000 C CNN
F 1 "SolderJumper_2_Open" H 5450 2864 50  0000 C CNN
F 2 "Jumper:SolderJumper-2_P1.3mm_Open_RoundedPad1.0x1.5mm" H 5450 2750 50  0001 C CNN
F 3 "~" H 5450 2750 50  0001 C CNN
	1    5450 2750
	1    0    0    -1  
$EndComp
$Comp
L Jumper:SolderJumper_2_Open JP4
U 1 1 5E616975
P 5450 3050
F 0 "JP4" H 5450 3255 50  0000 C CNN
F 1 "SolderJumper_2_Open" H 5450 3164 50  0000 C CNN
F 2 "Jumper:SolderJumper-2_P1.3mm_Open_RoundedPad1.0x1.5mm" H 5450 3050 50  0001 C CNN
F 3 "~" H 5450 3050 50  0001 C CNN
	1    5450 3050
	1    0    0    -1  
$EndComp
$Comp
L Jumper:SolderJumper_2_Open JP5
U 1 1 5E65C51A
P 5250 1250
F 0 "JP5" H 5250 1455 50  0000 C CNN
F 1 "SolderJumper_2_Open" H 5250 1364 50  0000 C CNN
F 2 "Jumper:SolderJumper-2_P1.3mm_Open_RoundedPad1.0x1.5mm" H 5250 1250 50  0001 C CNN
F 3 "~" H 5250 1250 50  0001 C CNN
	1    5250 1250
	1    0    0    -1  
$EndComp
Wire Wire Line
	5100 1250 4800 1250
Wire Wire Line
	4800 1250 4800 1600
Connection ~ 4800 1600
Wire Wire Line
	4800 1600 5100 1600
Wire Wire Line
	5400 1250 5700 1250
Wire Wire Line
	5700 1250 5700 1600
Connection ~ 5700 1600
Wire Wire Line
	5700 1600 8500 1600
$Comp
L Device:R_Small R5
U 1 1 5E57FE4F
P 1500 4350
F 0 "R5" H 1559 4396 50  0000 L CNN
F 1 "1K" H 1559 4305 50  0000 L CNN
F 2 "Resistor_SMD:R_0805_2012Metric_Pad1.15x1.40mm_HandSolder" H 1500 4350 50  0001 C CNN
F 3 "~" H 1500 4350 50  0001 C CNN
	1    1500 4350
	1    0    0    -1  
$EndComp
Wire Wire Line
	1300 4750 1500 4750
Wire Wire Line
	1500 4750 1500 4450
Wire Wire Line
	1500 4250 1500 3900
Text Label 1500 3900 0    50   ~ 0
VCC
Text Notes 850  4200 0    50   ~ 0
R5 Not Fitted
$EndSCHEMATC
