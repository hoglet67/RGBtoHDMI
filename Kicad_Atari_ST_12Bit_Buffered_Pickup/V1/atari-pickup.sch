EESchema Schematic File Version 4
LIBS:atari-pickup-cache
EELAYER 30 0
EELAYER END
$Descr A4 11693 8268
encoding utf-8
Sheet 1 1
Title "Atari ST Buffered Shifter pickup"
Date ""
Rev "3"
Comp ""
Comment1 ""
Comment2 ""
Comment3 ""
Comment4 ""
$EndDescr
$Comp
L atari-pickup-rescue:atari-pickup-rescue_shifter-rescue--atari-pickup-rescue U1
U 1 1 5B7FD182
P 2650 1700
F 0 "U1" H 2675 1775 50  0000 C CNN
F 1 "SHIFTER" H 2675 1684 50  0000 C CNN
F 2 "footprints:socket_embedded_40pin" H 2650 1700 50  0001 C CNN
F 3 "" H 2650 1700 50  0001 C CNN
	1    2650 1700
	1    0    0    -1  
$EndComp
NoConn ~ 2250 3550
NoConn ~ 3100 2750
NoConn ~ 3100 2650
NoConn ~ 3100 2550
NoConn ~ 3100 2450
NoConn ~ 2250 3250
NoConn ~ 2250 3150
NoConn ~ 2250 3050
NoConn ~ 2250 2650
NoConn ~ 2250 2550
Text GLabel 8450 2100 0    50   Input ~ 0
G0
Text GLabel 8450 2200 0    50   Input ~ 0
B0
Text GLabel 8450 2300 0    50   Input ~ 0
R1
Text GLabel 8450 2500 0    50   Input ~ 0
R2
Text GLabel 8450 2600 0    50   Input ~ 0
MONO
Text GLabel 8450 2700 0    50   Input ~ 0
MONO
Text GLabel 8450 2800 0    50   Input ~ 0
MONO
Text GLabel 9300 2700 2    50   Input ~ 0
VSYNC
Wire Wire Line
	8450 2100 8650 2100
Wire Wire Line
	8450 2200 8650 2200
Wire Wire Line
	8450 2300 8650 2300
Wire Wire Line
	8450 2500 8650 2500
Wire Wire Line
	8450 2600 8650 2600
Wire Wire Line
	8450 2700 8650 2700
Wire Wire Line
	8450 2800 8650 2800
Wire Wire Line
	9150 2100 9300 2100
Wire Wire Line
	9150 2200 9300 2200
Wire Wire Line
	9150 2300 9300 2300
Wire Wire Line
	9150 2400 9300 2400
Wire Wire Line
	9150 2500 9300 2500
Wire Wire Line
	9150 2600 9300 2600
Wire Wire Line
	9150 2700 9300 2700
$Comp
L Connector_Generic:Conn_01x02 J1
U 1 1 60099CF0
P 8750 3950
F 0 "J1" H 8830 3942 50  0000 L CNN
F 1 "Conn_01x02" H 8830 3851 50  0000 L CNN
F 2 "Connector_PinHeader_2.54mm:PinHeader_1x02_P2.54mm_Horizontal" H 8750 3950 50  0001 C CNN
F 3 "~" H 8750 3950 50  0001 C CNN
	1    8750 3950
	1    0    0    -1  
$EndComp
Text GLabel 3100 2950 2    50   Input ~ 0
R0_I
NoConn ~ 2250 3450
NoConn ~ 2250 3350
NoConn ~ 2250 2950
NoConn ~ 2250 2850
NoConn ~ 2250 2750
NoConn ~ 2250 2450
NoConn ~ 2250 2350
NoConn ~ 2250 2250
NoConn ~ 2250 2150
NoConn ~ 2250 2050
NoConn ~ 2250 1950
NoConn ~ 2250 1850
NoConn ~ 3100 1950
NoConn ~ 3100 2050
NoConn ~ 3100 2150
NoConn ~ 3100 2250
NoConn ~ 3100 2350
Text GLabel 8550 3950 0    50   Input ~ 0
SYNC_I
Text GLabel 8550 4050 0    50   Input ~ 0
VSYNC_I
$Comp
L power:GND #PWR0101
U 1 1 5B8000C2
P 5150 5900
F 0 "#PWR0101" H 5150 5650 50  0001 C CNN
F 1 "GND" H 5155 5727 50  0000 C CNN
F 2 "" H 5150 5900 50  0001 C CNN
F 3 "" H 5150 5900 50  0001 C CNN
	1    5150 5900
	1    0    0    -1  
$EndComp
$Comp
L atari-pickup-rescue:LVC245-parts-rescue-atari-pickup-rescue-atari-pickup-rescue U2
U 1 1 5B800CBC
P 5650 1250
F 0 "U2" H 5625 1325 50  0000 C CNN
F 1 "HC245" H 5625 1234 50  0000 C CNN
F 2 "Package_SO:TSSOP-20_4.4x6.5mm_P0.65mm" H 5650 1250 50  0001 C CNN
F 3 "" H 5650 1250 50  0001 C CNN
	1    5650 1250
	1    0    0    -1  
$EndComp
$Comp
L atari-pickup-rescue:LVC245-parts-rescue-atari-pickup-rescue-atari-pickup-rescue U3
U 1 1 5B800E39
P 5650 3250
F 0 "U3" H 5625 3325 50  0000 C CNN
F 1 "HC245" H 5625 3234 50  0000 C CNN
F 2 "Package_SO:TSSOP-20_4.4x6.5mm_P0.65mm" H 5650 3250 50  0001 C CNN
F 3 "" H 5650 3250 50  0001 C CNN
	1    5650 3250
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR0102
U 1 1 5B80BDC9
P 5150 4450
F 0 "#PWR0102" H 5150 4200 50  0001 C CNN
F 1 "GND" H 5155 4277 50  0000 C CNN
F 2 "" H 5150 4450 50  0001 C CNN
F 3 "" H 5150 4450 50  0001 C CNN
	1    5150 4450
	1    0    0    -1  
$EndComp
Wire Wire Line
	5300 4300 5150 4300
Wire Wire Line
	5150 4300 5150 4450
Text GLabel 6100 1900 2    50   Input ~ 0
VSYNC_I
Wire Wire Line
	5950 4100 6100 4100
Wire Wire Line
	5950 3400 6050 3400
Wire Wire Line
	6050 3400 6050 3350
Text GLabel 6100 1800 2    50   Input ~ 0
MONO_I
Wire Wire Line
	5950 4000 6100 4000
Text GLabel 6100 2000 2    50   Input ~ 0
SYNC_I
Wire Wire Line
	6100 3900 5950 3900
Text GLabel 6100 4300 2    50   Input ~ 0
B2_I
Text GLabel 6100 2100 2    50   Input ~ 0
R2_I
Wire Wire Line
	5950 3600 6100 3600
Wire Wire Line
	5950 3700 6100 3700
Wire Wire Line
	5950 3800 6100 3800
$Comp
L power:GND #PWR0103
U 1 1 5B8187F3
P 6250 3300
F 0 "#PWR0103" H 6250 3050 50  0001 C CNN
F 1 "GND" H 6255 3127 50  0000 C CNN
F 2 "" H 6250 3300 50  0001 C CNN
F 3 "" H 6250 3300 50  0001 C CNN
	1    6250 3300
	1    0    0    -1  
$EndComp
Wire Wire Line
	5950 3500 6100 3500
Wire Wire Line
	6150 3500 6150 3300
Wire Wire Line
	6150 3300 6250 3300
Wire Wire Line
	5950 1400 6050 1400
Wire Wire Line
	6050 1400 6050 1300
$Comp
L power:GND #PWR0104
U 1 1 5B83310F
P 5250 2400
F 0 "#PWR0104" H 5250 2150 50  0001 C CNN
F 1 "GND" H 5255 2227 50  0000 C CNN
F 2 "" H 5250 2400 50  0001 C CNN
F 3 "" H 5250 2400 50  0001 C CNN
	1    5250 2400
	1    0    0    -1  
$EndComp
Wire Wire Line
	5250 2400 5250 2300
Wire Wire Line
	5250 2300 5300 2300
$Comp
L power:GND #PWR0105
U 1 1 5B8361CF
P 6200 1300
F 0 "#PWR0105" H 6200 1050 50  0001 C CNN
F 1 "GND" H 6205 1127 50  0000 C CNN
F 2 "" H 6200 1300 50  0001 C CNN
F 3 "" H 6200 1300 50  0001 C CNN
	1    6200 1300
	1    0    0    -1  
$EndComp
Wire Wire Line
	5950 1500 6050 1500
Wire Wire Line
	6100 1500 6100 1300
Wire Wire Line
	6100 1300 6200 1300
Text GLabel 6100 4100 2    50   Input ~ 0
G2_I
Text GLabel 6100 4200 2    50   Input ~ 0
B1_I
Text GLabel 6100 2200 2    50   Input ~ 0
R1_I
Text GLabel 6100 4000 2    50   Input ~ 0
G1_I
Text GLabel 6100 3800 2    50   Input ~ 0
B0_I
Text GLabel 6100 2300 2    50   Input ~ 0
R0_I
Text GLabel 6100 3900 2    50   Input ~ 0
G0_I
Wire Wire Line
	5950 1800 6100 1800
Wire Wire Line
	5950 1900 6100 1900
Wire Wire Line
	5950 2000 6100 2000
Wire Wire Line
	5950 2100 6100 2100
Wire Wire Line
	5950 2200 6100 2200
Wire Wire Line
	5950 2300 6100 2300
Text GLabel 9300 2100 2    50   Input ~ 0
R0
Text GLabel 9300 2200 2    50   Input ~ 0
G1
Text GLabel 9300 2300 2    50   Input ~ 0
B1
Text GLabel 9300 2400 2    50   Input ~ 0
G2
Text GLabel 9300 2500 2    50   Input ~ 0
B2
Text GLabel 9300 2600 2    50   Input ~ 0
SYNC
$Comp
L power:GND #PWR0106
U 1 1 5B8F098E
P 7950 3000
F 0 "#PWR0106" H 7950 2750 50  0001 C CNN
F 1 "GND" H 7955 2827 50  0000 C CNN
F 2 "" H 7950 3000 50  0001 C CNN
F 3 "" H 7950 3000 50  0001 C CNN
	1    7950 3000
	1    0    0    -1  
$EndComp
Wire Wire Line
	5150 5750 5150 5900
$Comp
L Device:C_Small C1
U 1 1 5B996DF5
P 5750 5550
F 0 "C1" H 5842 5596 50  0000 L CNN
F 1 "100nF" H 5842 5505 50  0000 L CNN
F 2 "Capacitor_SMD:C_0603_1608Metric_Pad1.05x0.95mm_HandSolder" H 5750 5550 50  0001 C CNN
F 3 "~" H 5750 5550 50  0001 C CNN
	1    5750 5550
	1    0    0    -1  
$EndComp
Wire Wire Line
	5750 5750 5750 5650
Wire Wire Line
	5750 5450 5750 5300
Wire Wire Line
	5750 5750 6150 5750
Connection ~ 5750 5750
$Comp
L Device:C_Small C2
U 1 1 5B9AE9C3
P 6150 5550
F 0 "C2" H 6242 5596 50  0000 L CNN
F 1 "100nF" H 6242 5505 50  0000 L CNN
F 2 "Capacitor_SMD:C_0603_1608Metric_Pad1.05x0.95mm_HandSolder" H 6150 5550 50  0001 C CNN
F 3 "~" H 6150 5550 50  0001 C CNN
	1    6150 5550
	1    0    0    -1  
$EndComp
Wire Wire Line
	6150 5750 6150 5650
Wire Wire Line
	5750 5300 6150 5300
Wire Wire Line
	6150 5300 6150 5450
Wire Wire Line
	5150 5750 5750 5750
Wire Wire Line
	7950 2400 7950 3000
$Comp
L power:+5V #PWR0107
U 1 1 600AC970
P 6050 3350
F 0 "#PWR0107" H 6050 3200 50  0001 C CNN
F 1 "+5V" H 6065 3523 50  0000 C CNN
F 2 "" H 6050 3350 50  0001 C CNN
F 3 "" H 6050 3350 50  0001 C CNN
	1    6050 3350
	1    0    0    -1  
$EndComp
$Comp
L power:+5V #PWR0109
U 1 1 600B0E6C
P 9850 2800
F 0 "#PWR0109" H 9850 2650 50  0001 C CNN
F 1 "+5V" H 9865 2973 50  0000 C CNN
F 2 "" H 9850 2800 50  0001 C CNN
F 3 "" H 9850 2800 50  0001 C CNN
	1    9850 2800
	1    0    0    -1  
$EndComp
$Comp
L power:+5V #PWR0110
U 1 1 600B300B
P 6050 1300
F 0 "#PWR0110" H 6050 1150 50  0001 C CNN
F 1 "+5V" H 6065 1473 50  0000 C CNN
F 2 "" H 6050 1300 50  0001 C CNN
F 3 "" H 6050 1300 50  0001 C CNN
	1    6050 1300
	1    0    0    -1  
$EndComp
Wire Wire Line
	5950 4200 6100 4200
Text GLabel 5300 3800 0    50   Input ~ 0
G0
Text GLabel 5300 2200 0    50   Input ~ 0
R0
Text GLabel 5300 3700 0    50   Input ~ 0
B0
Text GLabel 5300 3900 0    50   Input ~ 0
G1
Text GLabel 5300 2100 0    50   Input ~ 0
R1
Text GLabel 5300 4100 0    50   Input ~ 0
B1
Text GLabel 5300 4000 0    50   Input ~ 0
G2
Text GLabel 5300 2000 0    50   Input ~ 0
R2
Text GLabel 5300 4200 0    50   Input ~ 0
B2
Text GLabel 5300 1900 0    50   Input ~ 0
SYNC
Text GLabel 5300 1800 0    50   Input ~ 0
VSYNC
Text GLabel 5300 1700 0    50   Input ~ 0
MONO
$Comp
L power:+5V #PWR0112
U 1 1 6011638C
P 6150 5300
F 0 "#PWR0112" H 6150 5150 50  0001 C CNN
F 1 "+5V" H 6165 5473 50  0000 C CNN
F 2 "" H 6150 5300 50  0001 C CNN
F 3 "" H 6150 5300 50  0001 C CNN
	1    6150 5300
	1    0    0    -1  
$EndComp
Wire Wire Line
	9150 2800 9850 2800
Text GLabel 3100 3350 2    50   Input ~ 0
G1_I
Text GLabel 3100 3450 2    50   Input ~ 0
G2_I
Text GLabel 3600 2850 2    50   Input ~ 0
MONO_I
Text GLabel 3100 3250 2    50   Input ~ 0
G0_I
Text GLabel 3100 3750 2    50   Input ~ 0
B2_I
Text GLabel 3100 3650 2    50   Input ~ 0
B1_I
Text GLabel 3100 3550 2    50   Input ~ 0
B0_I
Text GLabel 3100 3150 2    50   Input ~ 0
R2_I
Text GLabel 3100 3050 2    50   Input ~ 0
R1_I
NoConn ~ 2250 3650
Wire Wire Line
	7950 2400 8650 2400
$Comp
L Connector_Generic:Conn_02x08_Odd_Even J2
U 1 1 60090BEB
P 8850 2400
F 0 "J2" H 8900 2917 50  0000 C CNN
F 1 "Conn_02x08_Odd_Even" H 8900 2826 50  0000 C CNN
F 2 "Connector_PinHeader_2.54mm:PinHeader_2x08_P2.54mm_Horizontal" H 8850 2400 50  0001 C CNN
F 3 "~" H 8850 2400 50  0001 C CNN
	1    8850 2400
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR0113
U 1 1 60088546
P 1900 3850
F 0 "#PWR0113" H 1900 3600 50  0001 C CNN
F 1 "GND" H 1905 3677 50  0000 C CNN
F 2 "" H 1900 3850 50  0001 C CNN
F 3 "" H 1900 3850 50  0001 C CNN
	1    1900 3850
	1    0    0    -1  
$EndComp
$Comp
L power:+5V #PWR0114
U 1 1 6008AACF
P 3300 1700
F 0 "#PWR0114" H 3300 1550 50  0001 C CNN
F 1 "+5V" H 3315 1873 50  0000 C CNN
F 2 "" H 3300 1700 50  0001 C CNN
F 3 "" H 3300 1700 50  0001 C CNN
	1    3300 1700
	1    0    0    -1  
$EndComp
Wire Wire Line
	1900 3850 1900 3750
Wire Wire Line
	1900 3750 2250 3750
Wire Wire Line
	3300 1700 3300 1850
Wire Wire Line
	3300 1850 3100 1850
Wire Wire Line
	3100 2850 3600 2850
Wire Wire Line
	5300 1400 5300 1000
Wire Wire Line
	5300 1000 6250 1000
Wire Wire Line
	6250 1000 6250 1300
Wire Wire Line
	6250 1300 6200 1300
Connection ~ 6200 1300
Wire Wire Line
	5300 3400 5300 3000
Wire Wire Line
	5300 3000 6300 3000
Wire Wire Line
	6300 3000 6300 3300
Wire Wire Line
	6300 3300 6250 3300
Connection ~ 6250 3300
Wire Wire Line
	5950 4300 6100 4300
Wire Wire Line
	6100 3700 6100 3600
Connection ~ 6100 3500
Wire Wire Line
	6100 3500 6150 3500
Connection ~ 6100 3600
Wire Wire Line
	6100 3600 6100 3500
NoConn ~ 5300 3500
NoConn ~ 5300 3600
Wire Wire Line
	5950 1600 6050 1600
Wire Wire Line
	6050 1600 6050 1500
Connection ~ 6050 1500
Wire Wire Line
	6050 1500 6100 1500
Wire Wire Line
	5950 1700 6050 1700
Wire Wire Line
	6050 1700 6050 1600
Connection ~ 6050 1600
NoConn ~ 5300 1500
NoConn ~ 5300 1600
Connection ~ 6150 5300
$EndSCHEMATC
