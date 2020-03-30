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
P 1150 950
F 0 "P6" H 1230 992 50  0000 L CNN
F 1 "Conn_01x01" H 1230 901 50  0000 L CNN
F 2 "Connector_PinHeader_2.54mm:PinHeader_1x01_P2.54mm_Vertical" H 1150 950 50  0001 C CNN
F 3 "~" H 1150 950 50  0001 C CNN
	1    1150 950 
	1    0    0    -1  
$EndComp
$Comp
L buffer-rescue:Conn_01x04-Connector_Generic P2
U 1 1 5DCFA861
P 3400 3200
F 0 "P2" H 3400 2750 50  0000 C CNN
F 1 "Conn_01x04" H 3400 2850 50  0000 C CNN
F 2 "Connector_PinHeader_2.54mm:PinHeader_1x04_P2.54mm_Vertical" H 3400 3200 50  0001 C CNN
F 3 "~" H 3400 3200 50  0001 C CNN
	1    3400 3200
	-1   0    0    1   
$EndComp
NoConn ~ 950  950 
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
Text Label 5500 3100 2    50   ~ 0
I_BRED
Text Label 6100 3000 0    50   ~ 0
I_BGREEN
Text Label 6100 3100 0    50   ~ 0
I_BBLUE
$Comp
L Connector_Generic:Conn_01x01 P7
U 1 1 5E6322BC
P 2150 950
F 0 "P7" H 2230 992 50  0000 L CNN
F 1 "Conn_01x01" H 2230 901 50  0000 L CNN
F 2 "Connector_PinHeader_2.54mm:PinHeader_1x01_P2.54mm_Vertical" H 2150 950 50  0001 C CNN
F 3 "~" H 2150 950 50  0001 C CNN
	1    2150 950 
	1    0    0    -1  
$EndComp
NoConn ~ 1950 950 
NoConn ~ 2550 -700
NoConn ~ 3600 3100
NoConn ~ 3600 3200
NoConn ~ 3600 3300
Text Label 5500 3000 2    50   ~ 0
GND
Text Label 5500 3200 2    50   ~ 0
I_RED
Text Label 5500 3300 2    50   ~ 0
I_GREEN
Text Label 5500 3400 2    50   ~ 0
I_BLUE
Text Label 6100 3200 0    50   ~ 0
I_SYNC
Text Label 6100 3300 0    50   ~ 0
I_VSYNC
NoConn ~ 3600 3000
$Comp
L 74xx:74LS245 U1
U 1 1 5E6CBF98
P 5400 4900
F 0 "U1" H 5400 5950 50  0000 C CNN
F 1 "74LS245" H 5400 5850 50  0000 C CNN
F 2 "Package_SO:SOIC-20W_7.5x12.8mm_P1.27mm" H 5400 4900 50  0001 C CNN
F 3 "http://www.ti.com/lit/gpn/sn74LS245" H 5400 4900 50  0001 C CNN
	1    5400 4900
	1    0    0    -1  
$EndComp
Text Label 5400 4100 0    50   ~ 0
VCC
Text Label 5400 5700 0    50   ~ 0
GND
Wire Wire Line
	4700 5700 4900 5700
Wire Wire Line
	4900 5300 4900 5400
Text Label 5900 4400 0    50   ~ 0
I_BLUE
Text Label 7250 5250 0    50   ~ 0
I_VSYNC
Text Label 5900 4500 0    50   ~ 0
I_GREEN
Text Label 4900 4400 2    50   ~ 0
BLUE
Text Label 6650 5350 2    50   ~ 0
VSYNC
Text Label 4900 4500 2    50   ~ 0
GREEN
Text Label 6700 5950 2    50   ~ 0
SYNC
Text Label 4900 4600 2    50   ~ 0
RED
Text Label 4900 4700 2    50   ~ 0
BBLUE
Text Label 4900 4800 2    50   ~ 0
BRED
Text Label 4900 4900 2    50   ~ 0
BGREEN
Text Label 7300 5850 0    50   ~ 0
I_SYNC
Text Label 5900 4600 0    50   ~ 0
I_RED
Text Label 5900 4700 0    50   ~ 0
I_BBLUE
Text Label 5900 4800 0    50   ~ 0
I_BRED
Text Label 5900 4900 0    50   ~ 0
I_BGREEN
$Comp
L Device:C_Small C1
U 1 1 5E6D4BDE
P 4600 5700
F 0 "C1" V 4371 5700 50  0000 C CNN
F 1 "100n" V 4462 5700 50  0000 C CNN
F 2 "Capacitor_SMD:C_0805_2012Metric_Pad1.15x1.40mm_HandSolder" H 4600 5700 50  0001 C CNN
F 3 "~" H 4600 5700 50  0001 C CNN
	1    4600 5700
	0    1    1    0   
$EndComp
Wire Wire Line
	4500 5700 4250 5700
Wire Wire Line
	4250 5700 4250 4100
Wire Wire Line
	4250 4100 5400 4100
Wire Wire Line
	4900 5400 4900 5700
Connection ~ 4900 5400
Connection ~ 4900 5700
Wire Wire Line
	4900 5700 5400 5700
Wire Wire Line
	7250 2550 5150 2550
Wire Wire Line
	5500 3000 5150 3000
Wire Wire Line
	5150 3000 5150 2550
Connection ~ 5150 2550
Wire Wire Line
	5150 2550 4350 2550
Wire Wire Line
	5000 3100 5500 3100
Wire Wire Line
	5500 3200 5000 3200
Wire Wire Line
	5500 3300 5000 3300
Wire Wire Line
	5500 3400 5000 3400
Wire Wire Line
	6100 3300 6650 3300
Wire Wire Line
	6100 3200 6650 3200
Wire Wire Line
	6100 3100 6650 3100
Wire Wire Line
	6100 3000 6650 3000
Text Label 6100 3400 0    50   ~ 0
I_VCC
$Comp
L Device:D_Schottky D1
U 1 1 5E6C799C
P 7700 4250
F 0 "D1" H 7700 4466 50  0000 C CNN
F 1 "D_Schottky" H 7700 4375 50  0000 C CNN
F 2 "Diode_SMD:D_SOD-123F" H 7700 4250 50  0001 C CNN
F 3 "~" H 7700 4250 50  0001 C CNN
	1    7700 4250
	-1   0    0    -1  
$EndComp
Text Label 7550 4250 2    50   ~ 0
I_VCC
Text Label 7850 4250 0    50   ~ 0
VCC
$Comp
L Connector_Generic:Conn_01x06_EVEN P5
U 1 1 5E712074
P 5900 3100
F 0 "P5" H 5850 2650 50  0000 C CNN
F 1 "Conn_01x06_EVEN" H 5550 2550 50  0000 C CNN
F 2 "Connector_Harwin:Harwin_M20-89006xx_1x06_P2.54mm_Horizontal_EVEN" H 5900 3100 50  0001 C CNN
F 3 "~" H 5900 3100 50  0001 C CNN
	1    5900 3100
	-1   0    0    -1  
$EndComp
$Comp
L Connector_Generic:Conn_01x06_ODD P4
U 1 1 5E713851
P 5700 3100
F 0 "P4" H 5650 2650 50  0000 L CNN
F 1 "Conn_01x06_ODD" H 5100 2550 50  0000 L CNN
F 2 "Connector_Harwin:Harwin_M20-89006xx_1x06_P2.54mm_Horizontal_ODD" H 5700 3100 50  0001 C CNN
F 3 "~" H 5700 3100 50  0001 C CNN
	1    5700 3100
	1    0    0    -1  
$EndComp
Text Label 5500 2900 2    50   ~ 0
I_X1
Text Label 6100 2900 0    50   ~ 0
I_X2
Text Label 5900 5000 0    50   ~ 0
I_X1
Text Label 5900 5100 0    50   ~ 0
I_X2
Text Label 4900 5000 2    50   ~ 0
X1
Text Label 4900 5100 2    50   ~ 0
X2
$Comp
L Device:R_Network05 RN2
U 1 1 5E72603D
P 6850 3100
F 0 "RN2" V 6433 3100 50  0000 C CNN
F 1 "4.7K" V 6524 3100 50  0000 C CNN
F 2 "Resistor_THT:R_Array_SIP6" V 7225 3100 50  0001 C CNN
F 3 "http://www.vishay.com/docs/31509/csc.pdf" H 6850 3100 50  0001 C CNN
	1    6850 3100
	0    1    1    0   
$EndComp
Wire Wire Line
	6100 2900 6650 2900
$Comp
L Device:R_Network05 RN1
U 1 1 5E72B2C1
P 4800 3200
F 0 "RN1" V 4383 3200 50  0000 C CNN
F 1 "4.7K" V 4474 3200 50  0000 C CNN
F 2 "Resistor_THT:R_Array_SIP6" V 5175 3200 50  0001 C CNN
F 3 "http://www.vishay.com/docs/31509/csc.pdf" H 4800 3200 50  0001 C CNN
	1    4800 3200
	0    -1   1    0   
$EndComp
Wire Wire Line
	5000 3000 5100 3000
Wire Wire Line
	5100 3000 5100 2900
Wire Wire Line
	5100 2900 5500 2900
Wire Wire Line
	4350 2550 4350 3000
Wire Wire Line
	4350 3000 4600 3000
Wire Wire Line
	7250 2550 7250 2900
Wire Wire Line
	7250 2900 7050 2900
$Comp
L 74xx:74LS08 U2
U 1 1 5E73C1B8
P 2150 4500
F 0 "U2" H 2150 4825 50  0000 C CNN
F 1 "74LS08" H 2150 4734 50  0000 C CNN
F 2 "Package_SO:SOIC-14_3.9x8.7mm_P1.27mm" H 2150 4500 50  0001 C CNN
F 3 "http://www.ti.com/lit/gpn/sn74LS08" H 2150 4500 50  0001 C CNN
	1    2150 4500
	1    0    0    -1  
$EndComp
$Comp
L 74xx:74LS08 U2
U 2 1 5E73DD4D
P 2150 5050
F 0 "U2" H 2150 5375 50  0000 C CNN
F 1 "74LS08" H 2150 5284 50  0000 C CNN
F 2 "Package_SO:SOIC-14_3.9x8.7mm_P1.27mm" H 2150 5050 50  0001 C CNN
F 3 "http://www.ti.com/lit/gpn/sn74LS08" H 2150 5050 50  0001 C CNN
	2    2150 5050
	1    0    0    -1  
$EndComp
$Comp
L 74xx:74LS08 U2
U 3 1 5E73F784
P 7000 5950
F 0 "U2" H 7000 6275 50  0000 C CNN
F 1 "74LS08" H 7000 6184 50  0000 C CNN
F 2 "Package_SO:SOIC-14_3.9x8.7mm_P1.27mm" H 7000 5950 50  0001 C CNN
F 3 "http://www.ti.com/lit/gpn/sn74LS08" H 7000 5950 50  0001 C CNN
	3    7000 5950
	-1   0    0    -1  
$EndComp
$Comp
L 74xx:74LS08 U2
U 4 1 5E741666
P 6950 5350
F 0 "U2" H 6950 5675 50  0000 C CNN
F 1 "74LS08" H 6950 5584 50  0000 C CNN
F 2 "Package_SO:SOIC-14_3.9x8.7mm_P1.27mm" H 6950 5350 50  0001 C CNN
F 3 "http://www.ti.com/lit/gpn/sn74LS08" H 6950 5350 50  0001 C CNN
	4    6950 5350
	-1   0    0    -1  
$EndComp
$Comp
L 74xx:74LS08 U2
U 5 1 5E74332A
P 4250 6200
F 0 "U2" H 4480 6246 50  0000 L CNN
F 1 "74LS08" H 4480 6155 50  0000 L CNN
F 2 "Package_SO:SOIC-14_3.9x8.7mm_P1.27mm" H 4250 6200 50  0001 C CNN
F 3 "http://www.ti.com/lit/gpn/sn74LS08" H 4250 6200 50  0001 C CNN
	5    4250 6200
	1    0    0    -1  
$EndComp
Connection ~ 4250 5700
Wire Wire Line
	4250 6700 5400 6700
Wire Wire Line
	5400 6700 5400 5700
Connection ~ 5400 5700
$Comp
L Device:C_Small C2
U 1 1 5E746F52
P 3600 6150
F 0 "C2" H 3692 6196 50  0000 L CNN
F 1 "100n" H 3692 6105 50  0000 L CNN
F 2 "Capacitor_SMD:C_0805_2012Metric_Pad1.15x1.40mm_HandSolder" H 3600 6150 50  0001 C CNN
F 3 "~" H 3600 6150 50  0001 C CNN
	1    3600 6150
	1    0    0    -1  
$EndComp
Wire Wire Line
	4250 5700 3600 5700
Wire Wire Line
	3600 5700 3600 6050
Wire Wire Line
	3600 6250 3600 6700
Wire Wire Line
	3600 6700 4250 6700
Connection ~ 4250 6700
Wire Wire Line
	7300 6050 7300 5850
Wire Wire Line
	7250 5450 7250 5250
Wire Wire Line
	1850 6700 3600 6700
Wire Wire Line
	1850 4400 1850 4600
Connection ~ 1850 4600
Wire Wire Line
	1850 4600 1850 4950
Connection ~ 1850 4950
Wire Wire Line
	1850 4950 1850 5150
Connection ~ 1850 5150
Wire Wire Line
	1850 5150 1850 6700
Connection ~ 3600 6700
$EndSCHEMATC
