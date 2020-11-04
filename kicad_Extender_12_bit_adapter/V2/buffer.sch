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
L buffer-rescue:Conn_01x04-Connector_Generic P2
U 1 1 5DCFA861
P 4150 2900
F 0 "P2" H 4150 2450 50  0000 C CNN
F 1 "Conn_01x04" H 4150 2550 50  0000 C CNN
F 2 "Connector_PinHeader_2.54mm:PinHeader_1x04_P2.54mm_Vertical" H 4150 2900 50  0001 C CNN
F 3 "~" H 4150 2900 50  0001 C CNN
	1    4150 2900
	-1   0    0    -1  
$EndComp
NoConn ~ 1000 750 
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
B2
Text Label 9550 3900 0    50   ~ 0
R2
Text Label 9550 3800 0    50   ~ 0
R3
Text Label 9550 3700 0    50   ~ 0
G3
Text Label 9550 3600 0    50   ~ 0
B3
Text Label 9550 4000 0    50   ~ 0
GND
Text Label 9050 4000 2    50   ~ 0
G2
Text Label 9550 4100 0    50   ~ 0
R1
Text Label 9050 4100 2    50   ~ 0
B1
Text Label 5500 2900 2    50   ~ 0
R1
Text Label 5500 3100 2    50   ~ 0
R2
Text Label 6100 2900 0    50   ~ 0
B1
Text Label 6100 3000 0    50   ~ 0
G2
Text Label 6100 3100 0    50   ~ 0
B2
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
Wire Wire Line
	2950 1600 4800 1600
Wire Wire Line
	5400 1600 5700 1600
Wire Wire Line
	8500 1600 8500 3600
Wire Wire Line
	8500 3600 9050 3600
NoConn ~ 4350 2900
NoConn ~ 4350 3000
NoConn ~ 4350 3100
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
Text Label 5500 3000 2    50   ~ 0
GND
Text Label 5500 3200 2    50   ~ 0
R3
Text Label 5500 3300 2    50   ~ 0
G3
Text Label 5500 3400 2    50   ~ 0
B3
Text Label 6100 3200 0    50   ~ 0
SYNC
Text Label 6100 3300 0    50   ~ 0
VSYNC
Text Label 6100 3400 0    50   ~ 0
VCC_IN
Text Label 2950 1600 2    50   ~ 0
VCC_IN
NoConn ~ 4350 2800
$Comp
L Connector_Generic:Conn_01x08_odd P4
U 1 1 5F3A7097
P 5700 3000
F 0 "P4" H 5550 2450 50  0000 L CNN
F 1 "Conn_01x08_odd" H 5050 2300 50  0000 L CNN
F 2 "Connector_Harwin:Harwin_M20-89006xx_1x08_P2.54mm_Horizontal_ODD" H 5700 3000 50  0001 C CNN
F 3 "~" H 5700 3000 50  0001 C CNN
	1    5700 3000
	1    0    0    -1  
$EndComp
$Comp
L Connector_Generic:Conn_01x08_even P5
U 1 1 5F3A811A
P 5900 3000
F 0 "P5" H 5850 2450 50  0000 C CNN
F 1 "Conn_01x08_even" H 5600 2300 50  0000 C CNN
F 2 "Connector_Harwin:Harwin_M20-89006xx_1x08_P2.54mm_Horizontal_EVEN" H 5900 3000 50  0001 C CNN
F 3 "~" H 5900 3000 50  0001 C CNN
	1    5900 3000
	-1   0    0    -1  
$EndComp
Text Label 6100 2800 0    50   ~ 0
G1
Text Label 6100 2700 0    50   ~ 0
R0
Text Label 5500 2800 2    50   ~ 0
B0
Text Label 5500 2700 2    50   ~ 0
G0
Text Label 4350 3100 0    50   ~ 0
B0
Text Label 4350 3000 0    50   ~ 0
G0
Text Label 4350 2900 0    50   ~ 0
R0
Text Label 4350 2800 0    50   ~ 0
G1
$EndSCHEMATC
