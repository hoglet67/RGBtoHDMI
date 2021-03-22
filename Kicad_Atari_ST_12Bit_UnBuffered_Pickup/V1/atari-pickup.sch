EESchema Schematic File Version 4
LIBS:atari-pickup-cache
EELAYER 30 0
EELAYER END
$Descr A4 11693 8268
encoding utf-8
Sheet 1 1
Title "Atari ST Unbuffered shifter pickup"
Date ""
Rev "3"
Comp ""
Comment1 ""
Comment2 ""
Comment3 ""
Comment4 ""
$EndDescr
$Comp
L atari-pickup-rescue:shifter-rescue--atari-pickup-rescue U1
U 1 1 5B7FD182
P 2500 2050
F 0 "U1" H 2525 2125 50  0000 C CNN
F 1 "SHIFTER" H 2525 2034 50  0000 C CNN
F 2 "footprints:socket_embedded_40pin" H 2500 2050 50  0001 C CNN
F 3 "" H 2500 2050 50  0001 C CNN
	1    2500 2050
	1    0    0    -1  
$EndComp
NoConn ~ 2100 4000
NoConn ~ 2100 3900
NoConn ~ 2950 3100
NoConn ~ 2950 3000
NoConn ~ 2950 2900
NoConn ~ 2950 2800
NoConn ~ 2100 3600
NoConn ~ 2100 3500
NoConn ~ 2100 3400
NoConn ~ 2100 3000
NoConn ~ 2100 2900
Text GLabel 8450 2700 0    50   Input ~ 0
G0
Text GLabel 8450 2800 0    50   Input ~ 0
B0
Text GLabel 8450 2900 0    50   Input ~ 0
R1
Text GLabel 8450 3000 0    50   Input ~ 0
GND
Text GLabel 8450 3100 0    50   Input ~ 0
R2
Text GLabel 8450 3200 0    50   Input ~ 0
R3
Text GLabel 8450 3300 0    50   Input ~ 0
G3
Text GLabel 8450 3400 0    50   Input ~ 0
B3
Text GLabel 9300 2700 2    50   Input ~ 0
R0
Text GLabel 9300 2800 2    50   Input ~ 0
G1
Text GLabel 9300 2900 2    50   Input ~ 0
B1
Text GLabel 9300 3000 2    50   Input ~ 0
G2
Text GLabel 9300 3100 2    50   Input ~ 0
B2
Text GLabel 9300 3200 2    50   Input ~ 0
SYNC
Text GLabel 9300 3300 2    50   Input ~ 0
VSYNC
Text GLabel 9300 3400 2    50   Input ~ 0
VCC
Wire Wire Line
	8450 2700 8650 2700
Wire Wire Line
	8450 2800 8650 2800
Wire Wire Line
	8450 2900 8650 2900
Wire Wire Line
	8450 3000 8650 3000
Wire Wire Line
	8450 3100 8650 3100
Wire Wire Line
	8450 3200 8650 3200
Wire Wire Line
	8450 3300 8650 3300
Wire Wire Line
	8450 3400 8650 3400
Wire Wire Line
	9150 2700 9300 2700
Wire Wire Line
	9150 2800 9300 2800
Wire Wire Line
	9150 2900 9300 2900
Wire Wire Line
	9150 3000 9300 3000
Wire Wire Line
	9150 3100 9300 3100
Wire Wire Line
	9150 3200 9300 3200
Wire Wire Line
	9150 3300 9300 3300
Wire Wire Line
	9150 3400 9300 3400
$Comp
L Connector_Generic:Conn_02x08_Odd_Even J2
U 1 1 60090BEB
P 8850 3000
F 0 "J2" H 8900 3517 50  0000 C CNN
F 1 "Conn_02x08_Odd_Even" H 8900 3426 50  0000 C CNN
F 2 "Connector_PinHeader_2.54mm:PinHeader_2x08_P2.54mm_Horizontal" H 8850 3000 50  0001 C CNN
F 3 "~" H 8850 3000 50  0001 C CNN
	1    8850 3000
	1    0    0    -1  
$EndComp
$Comp
L Connector_Generic:Conn_01x02 J1
U 1 1 60099CF0
P 8750 4550
F 0 "J1" H 8830 4542 50  0000 L CNN
F 1 "Conn_01x02" H 8830 4451 50  0000 L CNN
F 2 "Connector_PinHeader_2.54mm:PinHeader_1x02_P2.54mm_Horizontal" H 8750 4550 50  0001 C CNN
F 3 "~" H 8750 4550 50  0001 C CNN
	1    8750 4550
	1    0    0    -1  
$EndComp
Text GLabel 2950 2200 2    50   Input ~ 0
VCC
Text GLabel 2950 3500 2    50   Input ~ 0
R2
Text GLabel 2950 3400 2    50   Input ~ 0
R1
Text GLabel 2950 3300 2    50   Input ~ 0
R0
Text GLabel 2950 3600 2    50   Input ~ 0
G0
Text GLabel 2950 3700 2    50   Input ~ 0
G1
Text GLabel 2950 3800 2    50   Input ~ 0
G2
Text GLabel 2950 4100 2    50   Input ~ 0
B2
Text GLabel 2950 4000 2    50   Input ~ 0
B1
Text GLabel 2950 3900 2    50   Input ~ 0
B0
Wire Wire Line
	2950 3200 3350 3200
Wire Wire Line
	3350 3200 3350 3050
Wire Wire Line
	3350 3050 3450 3050
Wire Wire Line
	3350 3200 3450 3200
Connection ~ 3350 3200
Wire Wire Line
	3350 3200 3350 3350
Wire Wire Line
	3350 3350 3450 3350
Text GLabel 3450 3050 2    50   Input ~ 0
R3
Text GLabel 3450 3200 2    50   Input ~ 0
G3
Text GLabel 3450 3350 2    50   Input ~ 0
B3
Text GLabel 2100 4100 0    50   Input ~ 0
GND
NoConn ~ 2100 3800
NoConn ~ 2100 3700
NoConn ~ 2100 3300
NoConn ~ 2100 3200
NoConn ~ 2100 3100
NoConn ~ 2100 2800
NoConn ~ 2100 2700
NoConn ~ 2100 2600
NoConn ~ 2100 2500
NoConn ~ 2100 2400
NoConn ~ 2100 2300
NoConn ~ 2100 2200
NoConn ~ 2950 2300
NoConn ~ 2950 2400
NoConn ~ 2950 2500
NoConn ~ 2950 2600
NoConn ~ 2950 2700
Text GLabel 8550 4550 0    50   Input ~ 0
SYNC
Text GLabel 8550 4650 0    50   Input ~ 0
VSYNC
$EndSCHEMATC
