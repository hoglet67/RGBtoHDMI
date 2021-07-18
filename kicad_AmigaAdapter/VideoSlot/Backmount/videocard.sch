EESchema Schematic File Version 4
EELAYER 30 0
EELAYER END
$Descr A4 11693 8268
encoding utf-8
Sheet 1 1
Title "RGBtoHDMI Denise Adapter"
Date ""
Rev "2"
Comp ""
Comment1 ""
Comment2 ""
Comment3 ""
Comment4 ""
$EndDescr
$Comp
L videocard-rescue:Conn_02x20_Odd_Even-Connector_Generic JRaspberryPiZero1
U 1 1 5F2A889B
P 9100 3550
F 0 "JRaspberryPiZero1" H 9150 4667 50  0000 C CNN
F 1 "Conn_02x20_Odd_Even" H 9150 4576 50  0000 C CNN
F 2 "Connector_PinSocket_2.54mm:PinSocket_2x20_P2.54mm_Vertical" H 9100 3550 50  0001 C CNN
F 3 "~" H 9100 3550 50  0001 C CNN
	1    9100 3550
	-1   0    0    -1  
$EndComp
Text Label 4400 3100 0    50   ~ 0
CDAC
Text Label 3000 3200 2    50   ~ 0
R0
Text Label 1000 3300 0    50   ~ 0
R1
Text Label 3000 3300 2    50   ~ 0
R2
Text Label 1000 2500 0    50   ~ 0
R3
Text Label 3000 3600 2    50   ~ 0
B1
Text Label 1000 3700 0    50   ~ 0
B2
Text Label 1000 2300 0    50   ~ 0
B3
Text Label 3000 3400 2    50   ~ 0
G0
Text Label 1000 3500 0    50   ~ 0
G1
Text Label 3000 3500 2    50   ~ 0
G2
Text Label 1000 2400 0    50   ~ 0
G3
Text Label 3150 1700 2    50   ~ 0
CSYNC
$Comp
L videocard-rescue:+5V-power #PWR0103
U 1 1 5F4DBBF7
P 8300 2650
F 0 "#PWR0103" H 8300 2500 50  0001 C CNN
F 1 "+5V" V 8315 2778 50  0000 L CNN
F 2 "" H 8300 2650 50  0001 C CNN
F 3 "" H 8300 2650 50  0001 C CNN
	1    8300 2650
	0    -1   -1   0   
$EndComp
$Comp
L videocard-rescue:GND-power #PWR0104
U 1 1 5F4ED178
P 8300 2850
F 0 "#PWR0104" H 8300 2600 50  0001 C CNN
F 1 "GND" V 8305 2722 50  0000 R CNN
F 2 "" H 8300 2850 50  0001 C CNN
F 3 "" H 8300 2850 50  0001 C CNN
	1    8300 2850
	0    1    1    0   
$EndComp
$Comp
L videocard-rescue:GND-power #PWR0105
U 1 1 5F4F600A
P 9800 3050
F 0 "#PWR0105" H 9800 2800 50  0001 C CNN
F 1 "GND" V 9805 2922 50  0000 R CNN
F 2 "" H 9800 3050 50  0001 C CNN
F 3 "" H 9800 3050 50  0001 C CNN
	1    9800 3050
	0    -1   -1   0   
$EndComp
$Comp
L videocard-rescue:74HC86-74xx U1
U 1 1 5F56435F
P 4200 2000
F 0 "U1" H 4200 2325 50  0000 C CNN
F 1 "74LVC86" H 4200 2234 50  0000 C CNN
F 2 "Package_SO:TSSOP-14_4.4x5mm_P0.65mm" H 4200 2000 50  0001 C CNN
F 3 "http://www.ti.com/lit/gpn/sn74HC86" H 4200 2000 50  0001 C CNN
	1    4200 2000
	1    0    0    -1  
$EndComp
$Comp
L videocard-rescue:74HC86-74xx U1
U 3 1 5F564405
P 8450 1500
F 0 "U1" H 8450 1825 50  0000 C CNN
F 1 "74LVC86" H 8450 1734 50  0000 C CNN
F 2 "Package_SO:TSSOP-14_4.4x5mm_P0.65mm" H 8450 1500 50  0001 C CNN
F 3 "http://www.ti.com/lit/gpn/sn74HC86" H 8450 1500 50  0001 C CNN
	3    8450 1500
	1    0    0    1   
$EndComp
$Comp
L videocard-rescue:74HC86-74xx U1
U 5 1 5F5644A1
P 10500 1550
F 0 "U1" H 10730 1596 50  0000 L CNN
F 1 "74LVC86" H 10730 1505 50  0000 L CNN
F 2 "Package_SO:TSSOP-14_4.4x5mm_P0.65mm" H 10500 1550 50  0001 C CNN
F 3 "http://www.ti.com/lit/gpn/sn74HC86" H 10500 1550 50  0001 C CNN
	5    10500 1550
	1    0    0    -1  
$EndComp
$Comp
L videocard-rescue:74HCT574-74xx U3
U 1 1 5F56452B
P 6400 4250
F 0 "U3" H 6400 5228 50  0000 C CNN
F 1 "74LVC574" H 6400 5137 50  0000 C CNN
F 2 "Package_SO:TSSOP-20_4.4x6.5mm_P0.65mm" H 6400 4250 50  0001 C CNN
F 3 "http://www.ti.com/lit/gpn/sn74HCT574" H 6400 4250 50  0001 C CNN
	1    6400 4250
	1    0    0    -1  
$EndComp
$Comp
L videocard-rescue:74HCT574-74xx U2
U 1 1 5F56462A
P 6400 2200
F 0 "U2" H 6400 3178 50  0000 C CNN
F 1 "74LVC574" H 6400 3087 50  0000 C CNN
F 2 "Package_SO:TSSOP-20_4.4x6.5mm_P0.65mm" H 6400 2200 50  0001 C CNN
F 3 "http://www.ti.com/lit/gpn/sn74HCT574" H 6400 2200 50  0001 C CNN
	1    6400 2200
	1    0    0    -1  
$EndComp
Wire Wire Line
	5400 4650 5400 2600
Wire Wire Line
	5400 2600 5900 2600
Wire Wire Line
	5400 4650 5900 4650
$Comp
L videocard-rescue:GND-power #PWR0112
U 1 1 5F619B7F
P 5900 3050
F 0 "#PWR0112" H 5900 2800 50  0001 C CNN
F 1 "GND" H 5905 2877 50  0000 C CNN
F 2 "" H 5900 3050 50  0001 C CNN
F 3 "" H 5900 3050 50  0001 C CNN
	1    5900 3050
	1    0    0    -1  
$EndComp
Wire Wire Line
	6400 3000 5900 3000
Wire Wire Line
	5900 3000 5900 3050
Wire Wire Line
	5900 2700 5900 3000
Connection ~ 5900 3000
$Comp
L videocard-rescue:GND-power #PWR0113
U 1 1 5F62DB20
P 5900 5100
F 0 "#PWR0113" H 5900 4850 50  0001 C CNN
F 1 "GND" H 5905 4927 50  0000 C CNN
F 2 "" H 5900 5100 50  0001 C CNN
F 3 "" H 5900 5100 50  0001 C CNN
	1    5900 5100
	1    0    0    -1  
$EndComp
Wire Wire Line
	5900 4750 5900 5050
Wire Wire Line
	6400 5050 5900 5050
Connection ~ 5900 5050
Wire Wire Line
	5900 5050 5900 5100
$Comp
L videocard-rescue:+3.3V-power #PWR0114
U 1 1 5F64C1AF
P 6500 1400
F 0 "#PWR0114" H 6500 1250 50  0001 C CNN
F 1 "+3.3V" V 6515 1528 50  0000 L CNN
F 2 "" H 6500 1400 50  0001 C CNN
F 3 "" H 6500 1400 50  0001 C CNN
	1    6500 1400
	0    1    1    0   
$EndComp
$Comp
L videocard-rescue:+3.3V-power #PWR0115
U 1 1 5F64C235
P 6500 3450
F 0 "#PWR0115" H 6500 3300 50  0001 C CNN
F 1 "+3.3V" V 6515 3578 50  0000 L CNN
F 2 "" H 6500 3450 50  0001 C CNN
F 3 "" H 6500 3450 50  0001 C CNN
	1    6500 3450
	0    1    1    0   
$EndComp
Wire Wire Line
	6400 3450 6500 3450
Wire Wire Line
	6400 1400 6500 1400
Text Label 5700 2300 0    50   ~ 0
B0
Text Label 5700 2200 0    50   ~ 0
B3
Text Label 5700 2000 0    50   ~ 0
R3
Text Label 5700 4450 0    50   ~ 0
R2
Text Label 5700 4350 0    50   ~ 0
R1
Text Label 5700 4250 0    50   ~ 0
G0
Text Label 5700 4150 0    50   ~ 0
G1
Text Label 5700 2100 0    50   ~ 0
G3
Connection ~ 5400 2600
Wire Wire Line
	5700 1000 5700 1700
Wire Wire Line
	5700 1700 5900 1700
Connection ~ 5700 1000
Wire Wire Line
	5700 2100 5900 2100
Wire Wire Line
	5700 2200 5900 2200
Wire Wire Line
	5700 2300 5900 2300
Wire Wire Line
	5700 2400 5900 2400
Wire Wire Line
	5700 1000 9050 1000
Wire Wire Line
	5700 3950 5900 3950
Wire Wire Line
	5700 4050 5900 4050
Wire Wire Line
	5700 4150 5900 4150
Wire Wire Line
	5700 4250 5900 4250
Wire Wire Line
	5700 4350 5900 4350
Wire Wire Line
	5700 4450 5900 4450
Text Label 5700 2400 0    50   ~ 0
CSYNC
Wire Wire Line
	5700 2000 5900 2000
Text Label 7000 4250 0    50   ~ 0
PiG0
Text Label 7000 4150 0    50   ~ 0
PiG1
Text Label 7000 2100 0    50   ~ 0
PiG3
Text Label 7000 2400 0    50   ~ 0
PiCSYNC
Wire Wire Line
	6900 1700 7000 1700
Wire Wire Line
	6900 2000 7000 2000
Wire Wire Line
	6900 2100 7000 2100
Wire Wire Line
	6900 2200 7000 2200
Wire Wire Line
	6900 2300 7000 2300
Wire Wire Line
	6900 2400 7000 2400
Text Label 7000 4350 0    50   ~ 0
PiR1
Text Label 7000 4450 0    50   ~ 0
PiR2
Text Label 7000 2000 0    50   ~ 0
PiR3
Text Label 7000 2200 0    50   ~ 0
PiB3
Text Label 7000 2300 0    50   ~ 0
PiB0
Wire Wire Line
	6900 3950 7000 3950
Wire Wire Line
	6900 4050 7000 4050
Wire Wire Line
	6900 4150 7000 4150
Wire Wire Line
	6900 4250 7000 4250
Wire Wire Line
	6900 4350 7000 4350
Wire Wire Line
	6900 4450 7000 4450
Text Label 9500 1500 0    50   ~ 0
PiCLK
Text Label 9900 4250 0    50   ~ 0
PiR3
Text Label 8050 4150 0    50   ~ 0
PiR2
Text Label 9900 3750 0    50   ~ 0
PiR1
Text Label 9900 3550 0    50   ~ 0
PiR0
Text Label 9900 3650 0    50   ~ 0
PiG3
Text Label 8050 3750 0    50   ~ 0
PiG2
Text Label 8050 3850 0    50   ~ 0
PiG1
Text Label 9900 4150 0    50   ~ 0
PiG0
Text Label 9900 4050 0    50   ~ 0
PiB3
Text Label 9900 2950 0    50   ~ 0
PiB2
Text Label 9900 2850 0    50   ~ 0
PiB1
Text Label 9900 2750 0    50   ~ 0
PiB0
Text Notes 9400 2650 0    50   ~ 0
3V3
Text Label 8050 3350 0    50   ~ 0
PiCSYNC
Text Label 9900 3150 0    50   ~ 0
PiCLK
NoConn ~ 9300 3250
$Comp
L videocard-rescue:GND-power #PWR0120
U 1 1 5FD41687
P 10500 2150
F 0 "#PWR0120" H 10500 1900 50  0001 C CNN
F 1 "GND" H 10505 1977 50  0000 C CNN
F 2 "" H 10500 2150 50  0001 C CNN
F 3 "" H 10500 2150 50  0001 C CNN
	1    10500 2150
	1    0    0    -1  
$EndComp
$Comp
L videocard-rescue:+3.3V-power #PWR0121
U 1 1 5FD416C6
P 10500 950
F 0 "#PWR0121" H 10500 800 50  0001 C CNN
F 1 "+3.3V" H 10515 1123 50  0000 C CNN
F 2 "" H 10500 950 50  0001 C CNN
F 3 "" H 10500 950 50  0001 C CNN
	1    10500 950 
	1    0    0    -1  
$EndComp
Wire Wire Line
	10500 950  10500 1050
Wire Wire Line
	10500 2050 10500 2150
$Comp
L videocard-rescue:+5V-power #PWR0122
U 1 1 5FDC364C
P 2950 6500
F 0 "#PWR0122" H 2950 6350 50  0001 C CNN
F 1 "+5V" H 2965 6673 50  0000 C CNN
F 2 "" H 2950 6500 50  0001 C CNN
F 3 "" H 2950 6500 50  0001 C CNN
	1    2950 6500
	1    0    0    -1  
$EndComp
Wire Wire Line
	2950 6500 2950 6550
$Comp
L videocard-rescue:GND-power #PWR0123
U 1 1 5FDD62FC
P 3450 7050
F 0 "#PWR0123" H 3450 6800 50  0001 C CNN
F 1 "GND" H 3455 6877 50  0000 C CNN
F 2 "" H 3450 7050 50  0001 C CNN
F 3 "" H 3450 7050 50  0001 C CNN
	1    3450 7050
	1    0    0    -1  
$EndComp
Wire Wire Line
	3450 7000 4000 7000
Connection ~ 3450 7000
Wire Wire Line
	3450 7000 3450 7050
$Comp
L videocard-rescue:C_Small-Device C4
U 1 1 5FE100A9
P 4000 6800
F 0 "C4" H 4092 6846 50  0000 L CNN
F 1 "1uF" H 4092 6755 50  0000 L CNN
F 2 "Capacitor_SMD:C_0805_2012Metric_Pad1.15x1.40mm_HandSolder" H 4000 6800 50  0001 C CNN
F 3 "~" H 4000 6800 50  0001 C CNN
	1    4000 6800
	1    0    0    -1  
$EndComp
$Comp
L videocard-rescue:C_Small-Device C1
U 1 1 5FE10161
P 4350 6800
F 0 "C1" H 4442 6846 50  0000 L CNN
F 1 "100nF" H 4442 6755 50  0000 L CNN
F 2 "Capacitor_SMD:C_0603_1608Metric_Pad1.05x0.95mm_HandSolder" H 4350 6800 50  0001 C CNN
F 3 "~" H 4350 6800 50  0001 C CNN
	1    4350 6800
	1    0    0    -1  
$EndComp
$Comp
L videocard-rescue:C_Small-Device C2
U 1 1 5FE101EB
P 4800 6800
F 0 "C2" H 4892 6846 50  0000 L CNN
F 1 "100nF" H 4892 6755 50  0000 L CNN
F 2 "Capacitor_SMD:C_0603_1608Metric_Pad1.05x0.95mm_HandSolder" H 4800 6800 50  0001 C CNN
F 3 "~" H 4800 6800 50  0001 C CNN
	1    4800 6800
	1    0    0    -1  
$EndComp
$Comp
L videocard-rescue:C_Small-Device C3
U 1 1 5FE1049B
P 5250 6800
F 0 "C3" H 5342 6846 50  0000 L CNN
F 1 "100nF" H 5342 6755 50  0000 L CNN
F 2 "Capacitor_SMD:C_0603_1608Metric_Pad1.05x0.95mm_HandSolder" H 5250 6800 50  0001 C CNN
F 3 "~" H 5250 6800 50  0001 C CNN
	1    5250 6800
	1    0    0    -1  
$EndComp
Wire Wire Line
	3850 6550 4000 6550
Wire Wire Line
	4000 6550 4000 6700
Wire Wire Line
	4000 6900 4000 7000
Connection ~ 4000 7000
Wire Wire Line
	4000 6550 4350 6550
Wire Wire Line
	4350 6550 4350 6700
Connection ~ 4000 6550
Wire Wire Line
	4350 7000 4350 6900
Wire Wire Line
	4000 7000 4350 7000
Wire Wire Line
	4350 6550 4800 6550
Wire Wire Line
	4800 6550 4800 6700
Connection ~ 4350 6550
Wire Wire Line
	4800 6900 4800 7000
Wire Wire Line
	4800 7000 4350 7000
Connection ~ 4350 7000
Wire Wire Line
	4800 6550 5250 6550
Wire Wire Line
	5250 6550 5250 6700
Connection ~ 4800 6550
Wire Wire Line
	5250 6900 5250 7000
Wire Wire Line
	5250 7000 4800 7000
Connection ~ 4800 7000
Wire Wire Line
	4000 6450 4000 6550
$Comp
L videocard-rescue:PWR_FLAG-power #FLG0101
U 1 1 5FECE8FC
P 2800 7000
F 0 "#FLG0101" H 2800 7075 50  0001 C CNN
F 1 "PWR_FLAG" V 2800 7128 50  0000 L CNN
F 2 "" H 2800 7000 50  0001 C CNN
F 3 "~" H 2800 7000 50  0001 C CNN
	1    2800 7000
	0    -1   -1   0   
$EndComp
$Comp
L videocard-rescue:PWR_FLAG-power #FLG0102
U 1 1 5FEE4F7C
P 3850 6550
F 0 "#FLG0102" H 3850 6625 50  0001 C CNN
F 1 "PWR_FLAG" V 3850 6678 50  0000 L CNN
F 2 "" H 3850 6550 50  0001 C CNN
F 3 "~" H 3850 6550 50  0001 C CNN
	1    3850 6550
	0    -1   -1   0   
$EndComp
Wire Wire Line
	2800 6550 2950 6550
Wire Wire Line
	2800 7000 3450 7000
NoConn ~ 8800 3450
Text Notes 8450 2650 0    50   ~ 0
5V
Text Notes 8450 2750 0    50   ~ 0
5V
Text Notes 8450 2850 0    50   ~ 0
Ground
Text Notes 8450 2950 0    50   ~ 0
GPIO14
Text Notes 8450 3050 0    50   ~ 0
GPIO15
Text Notes 8450 3150 0    50   ~ 0
GPIO18
Text Notes 8450 3250 0    50   ~ 0
Ground
Text Notes 8450 3350 0    50   ~ 0
GPIO23
Text Notes 8450 3450 0    50   ~ 0
GPIO24
Text Notes 8450 3550 0    50   ~ 0
Ground
Text Notes 8450 3650 0    50   ~ 0
GPIO25
Text Notes 8450 3750 0    50   ~ 0
GPIO8
Text Notes 8450 3850 0    50   ~ 0
GPIO7
Text Notes 8450 3950 0    50   ~ 0
GPIO1
Text Notes 8450 4150 0    50   ~ 0
GPIO12
Text Notes 8450 4250 0    50   ~ 0
Ground
Text Notes 8450 4350 0    50   ~ 0
GPIO16
Text Notes 8450 4550 0    50   ~ 0
GPIO21
Text Notes 8450 4450 0    50   ~ 0
GPIO20
Text Notes 8450 4050 0    50   ~ 0
Ground
Text Notes 9400 2750 0    50   ~ 0
GPIO2
Text Notes 9400 2850 0    50   ~ 0
GPIO3
Text Notes 9400 2950 0    50   ~ 0
GPIO4
Text Notes 9400 3050 0    50   ~ 0
Ground
Text Notes 9400 3150 0    50   ~ 0
GPIO17
Text Notes 9400 3250 0    50   ~ 0
GPIO27
Text Notes 9400 3350 0    50   ~ 0
GPIO22
Text Notes 9400 3450 0    50   ~ 0
3V3
Text Notes 9400 3550 0    50   ~ 0
GPIO10
Text Notes 9400 3650 0    50   ~ 0
GPIO9
Text Notes 9400 3750 0    50   ~ 0
GPIO11
Text Notes 9400 3850 0    50   ~ 0
Ground
Text Notes 9400 3950 0    50   ~ 0
GPIO0
Text Notes 9400 4050 0    50   ~ 0
GPIO5
Text Notes 9400 4150 0    50   ~ 0
GPIO6
Text Notes 9400 4250 0    50   ~ 0
GPIO13
Text Notes 9400 4350 0    50   ~ 0
GPIO19
Text Notes 9400 4450 0    50   ~ 0
GPIO26
Text Notes 9400 4550 0    50   ~ 0
Ground
Wire Wire Line
	8300 2850 8800 2850
Wire Wire Line
	8800 2650 8350 2650
Wire Wire Line
	8800 2750 8350 2750
Wire Wire Line
	8350 2750 8350 2650
Connection ~ 8350 2650
Wire Wire Line
	8350 2650 8300 2650
Wire Wire Line
	9300 3050 9800 3050
Wire Wire Line
	9300 3150 9900 3150
NoConn ~ 8800 2950
NoConn ~ 8800 3050
NoConn ~ 9300 3350
NoConn ~ 8800 4550
NoConn ~ 8800 3650
Wire Wire Line
	8050 3350 8800 3350
Wire Wire Line
	8050 3750 8800 3750
Wire Wire Line
	8050 3850 8800 3850
Wire Wire Line
	8050 4150 8800 4150
NoConn ~ 8800 3950
Wire Wire Line
	9300 4250 9900 4250
Wire Wire Line
	9300 4150 9900 4150
Wire Wire Line
	9300 4050 9900 4050
NoConn ~ 9300 3950
Wire Wire Line
	9300 3750 9900 3750
Wire Wire Line
	9300 3650 9900 3650
Wire Wire Line
	9300 3550 9900 3550
$Comp
L videocard-rescue:R_Small-Device R1
U 1 1 5FA8375F
P 8150 3150
F 0 "R1" V 7954 3150 50  0000 C CNN
F 1 "3k3" V 8045 3150 50  0000 C CNN
F 2 "Resistor_SMD:R_0805_2012Metric_Pad1.15x1.40mm_HandSolder" H 8150 3150 50  0001 C CNN
F 3 "~" H 8150 3150 50  0001 C CNN
	1    8150 3150
	0    1    1    0   
$EndComp
$Comp
L videocard-rescue:GND-power #PWR0106
U 1 1 5FA83876
P 7850 3250
F 0 "#PWR0106" H 7850 3000 50  0001 C CNN
F 1 "GND" H 7855 3077 50  0000 C CNN
F 2 "" H 7850 3250 50  0001 C CNN
F 3 "" H 7850 3250 50  0001 C CNN
	1    7850 3250
	1    0    0    -1  
$EndComp
Wire Wire Line
	7850 3150 8050 3150
Wire Wire Line
	8250 3150 8800 3150
NoConn ~ 8800 4450
NoConn ~ 9300 4350
Wire Wire Line
	9300 2750 9900 2750
Wire Wire Line
	9300 2850 9900 2850
Wire Wire Line
	9300 2950 9900 2950
Wire Wire Line
	8000 1500 8050 1500
Wire Wire Line
	6900 3750 7000 3750
Wire Wire Line
	6900 3850 7000 3850
Wire Wire Line
	5700 3750 5900 3750
Wire Wire Line
	5700 3850 5900 3850
Wire Wire Line
	5850 1800 5850 1900
Wire Wire Line
	5850 1900 5900 1900
Connection ~ 5850 1800
Wire Wire Line
	5850 1800 5900 1800
NoConn ~ 6900 1800
NoConn ~ 6900 1900
$Comp
L videocard-rescue:+3.3V-power #PWR0107
U 1 1 5F35F4D0
P 7350 1350
F 0 "#PWR0107" H 7350 1200 50  0001 C CNN
F 1 "+3.3V" H 7365 1523 50  0000 C CNN
F 2 "" H 7350 1350 50  0001 C CNN
F 3 "" H 7350 1350 50  0001 C CNN
	1    7350 1350
	1    0    0    -1  
$EndComp
Wire Wire Line
	7400 1600 7000 1600
Wire Wire Line
	7000 1600 7000 1700
Wire Wire Line
	7350 1350 7350 1400
Wire Wire Line
	7350 1400 7400 1400
$Comp
L videocard-rescue:Conn_01x02-Connector_Generic JButton1
U 1 1 5F3EA598
P 8800 5450
F 0 "JButton1" V 8673 5530 50  0000 L CNN
F 1 "Conn_01x02" V 8764 5530 50  0000 L CNN
F 2 "Connector_PinHeader_2.54mm:PinHeader_1x02_P2.54mm_Horizontal" H 8800 5450 50  0001 C CNN
F 3 "~" H 8800 5450 50  0001 C CNN
	1    8800 5450
	0    1    1    0   
$EndComp
$Comp
L videocard-rescue:GND-power #PWR0109
U 1 1 5F3F766C
P 8900 5050
F 0 "#PWR0109" H 8900 4800 50  0001 C CNN
F 1 "GND" V 8905 4922 50  0000 R CNN
F 2 "" H 8900 5050 50  0001 C CNN
F 3 "" H 8900 5050 50  0001 C CNN
	1    8900 5050
	0    -1   -1   0   
$EndComp
Wire Wire Line
	8800 5250 8800 5050
Wire Wire Line
	8800 5050 8900 5050
Wire Wire Line
	8700 5250 8700 4850
Wire Wire Line
	8800 4350 8300 4350
Wire Wire Line
	8300 4350 8300 4850
Wire Wire Line
	8300 4850 8700 4850
NoConn ~ 9300 4450
Wire Wire Line
	8750 1500 9050 1500
Wire Wire Line
	9050 1500 9050 1000
Connection ~ 9050 1500
Wire Wire Line
	9050 1500 9500 1500
$Comp
L videocard-rescue:GND-power #PWR0108
U 1 1 5F704071
P 8150 1700
F 0 "#PWR0108" H 8150 1450 50  0001 C CNN
F 1 "GND" H 8155 1527 50  0000 C CNN
F 2 "" H 8150 1700 50  0001 C CNN
F 3 "" H 8150 1700 50  0001 C CNN
	1    8150 1700
	1    0    0    -1  
$EndComp
Wire Wire Line
	8050 1500 8050 1400
Wire Wire Line
	8050 1400 8150 1400
Wire Wire Line
	8150 1700 8150 1600
Wire Wire Line
	5350 2500 5400 2500
Wire Wire Line
	5400 2500 5400 2600
Wire Wire Line
	4750 2400 4750 1000
Wire Wire Line
	4750 1000 5700 1000
$Comp
L videocard-rescue:Jumper_3_Bridged12-Jumper JP1
U 1 1 5F7E06DA
P 4600 2600
F 0 "JP1" V 4700 2450 50  0000 L CNN
F 1 "Jumper_3_Bridged12" H 4300 2700 50  0000 L CNN
F 2 "Connector_PinHeader_2.54mm:PinHeader_1x03_P2.54mm_Vertical" H 4600 2600 50  0001 C CNN
F 3 "~" H 4600 2600 50  0001 C CNN
	1    4600 2600
	0    -1   -1   0   
$EndComp
Wire Wire Line
	900  3900 1150 3900
Text Label 900  3900 0    50   ~ 0
CDAC
$Comp
L videocard-rescue:GND-power #PWR0111
U 1 1 60153149
P 3200 3700
F 0 "#PWR0111" H 3200 3450 50  0001 C CNN
F 1 "GND" V 3200 3500 50  0000 C CNN
F 2 "" H 3200 3700 50  0001 C CNN
F 3 "" H 3200 3700 50  0001 C CNN
	1    3200 3700
	1    0    0    -1  
$EndComp
Wire Wire Line
	2850 3700 3200 3700
$Comp
L videocard-rescue:GND-power #PWR0116
U 1 1 60161DEE
P 3200 4200
F 0 "#PWR0116" H 3200 3950 50  0001 C CNN
F 1 "GND" V 3200 4000 50  0000 C CNN
F 2 "" H 3200 4200 50  0001 C CNN
F 3 "" H 3200 4200 50  0001 C CNN
	1    3200 4200
	1    0    0    -1  
$EndComp
Wire Wire Line
	2850 4200 3200 4200
$Comp
L videocard-rescue:GND-power #PWR0118
U 1 1 6016F73E
P 3200 4700
F 0 "#PWR0118" H 3200 4450 50  0001 C CNN
F 1 "GND" V 3200 4500 50  0000 C CNN
F 2 "" H 3200 4700 50  0001 C CNN
F 3 "" H 3200 4700 50  0001 C CNN
	1    3200 4700
	1    0    0    -1  
$EndComp
Wire Wire Line
	2850 4700 3200 4700
$Comp
L videocard-rescue:GND-power #PWR0119
U 1 1 6017D97F
P 850 3200
F 0 "#PWR0119" H 850 2950 50  0001 C CNN
F 1 "GND" H 850 3250 50  0000 C CNN
F 2 "" H 850 3200 50  0001 C CNN
F 3 "" H 850 3200 50  0001 C CNN
	1    850  3200
	1    0    0    -1  
$EndComp
Wire Wire Line
	850  3200 1150 3200
$Comp
L videocard-rescue:GND-power #PWR0125
U 1 1 601A7E30
P 850 3400
F 0 "#PWR0125" H 850 3150 50  0001 C CNN
F 1 "GND" H 850 3450 50  0000 C CNN
F 2 "" H 850 3400 50  0001 C CNN
F 3 "" H 850 3400 50  0001 C CNN
	1    850  3400
	1    0    0    -1  
$EndComp
Wire Wire Line
	850  3400 1150 3400
$Comp
L videocard-rescue:GND-power #PWR0126
U 1 1 601B5E16
P 850 3600
F 0 "#PWR0126" H 850 3350 50  0001 C CNN
F 1 "GND" H 850 3650 50  0000 C CNN
F 2 "" H 850 3600 50  0001 C CNN
F 3 "" H 850 3600 50  0001 C CNN
	1    850  3600
	1    0    0    -1  
$EndComp
Wire Wire Line
	850  3600 1150 3600
Wire Wire Line
	1150 2500 1000 2500
Wire Wire Line
	1150 2200 1000 2200
Wire Wire Line
	750  1700 1150 1700
Wire Wire Line
	750  1900 1150 1900
$Comp
L videocard-rescue:GND-power #PWR0129
U 1 1 60230DF2
P 750 2100
F 0 "#PWR0129" H 750 1850 50  0001 C CNN
F 1 "GND" H 750 1950 50  0000 C CNN
F 2 "" H 750 2100 50  0001 C CNN
F 3 "" H 750 2100 50  0001 C CNN
	1    750  2100
	1    0    0    -1  
$EndComp
Wire Wire Line
	750  2100 1150 2100
$Comp
L videocard-rescue:GND-power #PWR0130
U 1 1 6024153B
P 3300 1600
F 0 "#PWR0130" H 3300 1350 50  0001 C CNN
F 1 "GND" H 3300 1650 50  0000 C CNN
F 2 "" H 3300 1600 50  0001 C CNN
F 3 "" H 3300 1600 50  0001 C CNN
	1    3300 1600
	-1   0    0    -1  
$EndComp
Wire Wire Line
	3300 1600 2850 1600
$Comp
L videocard-rescue:GND-power #PWR0131
U 1 1 60273473
P 3300 2000
F 0 "#PWR0131" H 3300 1750 50  0001 C CNN
F 1 "GND" H 3300 2050 50  0000 C CNN
F 2 "" H 3300 2000 50  0001 C CNN
F 3 "" H 3300 2000 50  0001 C CNN
	1    3300 2000
	-1   0    0    -1  
$EndComp
Wire Wire Line
	3300 2000 2850 2000
$Comp
L videocard-rescue:GND-power #PWR0132
U 1 1 602839C1
P 3300 2200
F 0 "#PWR0132" H 3300 1950 50  0001 C CNN
F 1 "GND" H 3300 2250 50  0000 C CNN
F 2 "" H 3300 2200 50  0001 C CNN
F 3 "" H 3300 2200 50  0001 C CNN
	1    3300 2200
	-1   0    0    -1  
$EndComp
Wire Wire Line
	3300 2200 2850 2200
$Comp
L videocard-rescue:GND-power #PWR0133
U 1 1 602944CA
P 3300 2600
F 0 "#PWR0133" H 3300 2350 50  0001 C CNN
F 1 "GND" H 3300 2650 50  0000 C CNN
F 2 "" H 3300 2600 50  0001 C CNN
F 3 "" H 3300 2600 50  0001 C CNN
	1    3300 2600
	-1   0    0    -1  
$EndComp
Wire Wire Line
	3300 2600 2850 2600
Wire Wire Line
	2850 1300 3150 1300
$Comp
L videocard-rescue:+5V-power #PWR0134
U 1 1 602B73C0
P 3150 1300
F 0 "#PWR0134" H 3150 1150 50  0001 C CNN
F 1 "+5V" H 3165 1473 50  0000 C CNN
F 2 "" H 3150 1300 50  0001 C CNN
F 3 "" H 3150 1300 50  0001 C CNN
	1    3150 1300
	1    0    0    -1  
$EndComp
Wire Wire Line
	2850 1400 3150 1400
Wire Wire Line
	3150 1400 3150 1300
Connection ~ 3150 1300
Text Label 1000 2200 0    50   ~ 0
B0
Wire Wire Line
	1150 3300 1000 3300
Wire Wire Line
	1150 3500 1000 3500
Wire Wire Line
	2850 3200 3000 3200
Wire Wire Line
	2850 3300 3000 3300
Wire Wire Line
	2850 3400 3000 3400
Wire Wire Line
	2850 3500 3000 3500
Wire Wire Line
	2850 3600 3000 3600
Wire Wire Line
	1150 3700 1000 3700
Wire Wire Line
	1000 2300 1150 2300
Wire Wire Line
	1000 2400 1150 2400
Wire Wire Line
	2850 1700 3150 1700
$Comp
L videocard-rescue:MountingHole-Mechanical-raspberrypi_zerow_uhat-rescue H1
U 1 1 601472EE
P 3950 4800
F 0 "H1" H 4050 4846 50  0000 L CNN
F 1 "MountingHole" H 4050 4755 50  0000 L CNN
F 2 "lib:MountingHole_2.7mm_M2.5_uHAT_RPi" H 3950 4800 50  0001 C CNN
F 3 "~" H 3950 4800 50  0001 C CNN
	1    3950 4800
	1    0    0    -1  
$EndComp
$Comp
L videocard-rescue:MountingHole-Mechanical-raspberrypi_zerow_uhat-rescue H2
U 1 1 601472F4
P 3950 5000
F 0 "H2" H 4050 5046 50  0000 L CNN
F 1 "MountingHole" H 4050 4955 50  0000 L CNN
F 2 "lib:MountingHole_2.7mm_M2.5_uHAT_RPi" H 3950 5000 50  0001 C CNN
F 3 "~" H 3950 5000 50  0001 C CNN
	1    3950 5000
	1    0    0    -1  
$EndComp
Wire Wire Line
	4600 3100 4600 2850
NoConn ~ 2850 3800
NoConn ~ 2850 3900
NoConn ~ 2850 4000
NoConn ~ 2850 4100
NoConn ~ 2850 4300
NoConn ~ 2850 4400
NoConn ~ 2850 4500
NoConn ~ 2850 4600
NoConn ~ 2850 4800
NoConn ~ 2850 4900
NoConn ~ 1150 4900
NoConn ~ 1150 4800
NoConn ~ 1150 4700
NoConn ~ 1150 4600
NoConn ~ 1150 4500
NoConn ~ 1150 4400
NoConn ~ 1150 4300
NoConn ~ 1150 4200
NoConn ~ 1150 4100
NoConn ~ 1150 3800
NoConn ~ 1150 2700
NoConn ~ 1150 2600
NoConn ~ 1150 2000
NoConn ~ 1150 1800
NoConn ~ 1150 1600
NoConn ~ 1150 1500
NoConn ~ 1150 1400
NoConn ~ 1150 1200
NoConn ~ 2850 1200
NoConn ~ 2850 1500
NoConn ~ 2850 1800
NoConn ~ 2850 1900
NoConn ~ 2850 2100
NoConn ~ 2850 2300
NoConn ~ 2850 2400
NoConn ~ 2850 2500
Wire Wire Line
	750  1700 750  1900
Connection ~ 750  2100
Connection ~ 750  1900
Wire Wire Line
	750  1900 750  2100
$Comp
L videocard-rescue:PWR_FLAG-power #FLG0103
U 1 1 603BAE45
P 2800 6550
F 0 "#FLG0103" H 2800 6625 50  0001 C CNN
F 1 "PWR_FLAG" V 2800 6678 50  0000 L CNN
F 2 "" H 2800 6550 50  0001 C CNN
F 3 "~" H 2800 6550 50  0001 C CNN
	1    2800 6550
	0    -1   -1   0   
$EndComp
Wire Wire Line
	1150 4000 900  4000
Text Label 3150 2700 2    50   ~ 0
~C1
Text Label 900  4000 0    50   ~ 0
~C3
Wire Wire Line
	4600 3100 4400 3100
Wire Wire Line
	4600 2000 4600 2350
Wire Wire Line
	4600 2000 4500 2000
Wire Wire Line
	3900 2100 3750 2100
Text Label 3750 2100 0    50   ~ 0
~C1
Text Label 3750 1900 0    50   ~ 0
~C3
Wire Wire Line
	2850 2700 3600 2700
$Comp
L videocard-rescue:A2000_Video_Slot_Phys-amiga-conn J1
U 1 1 6015E384
P 2000 3000
F 0 "J1" H 2000 5215 50  0000 C CNN
F 1 "A2000_Video_Slot_Phys" H 2000 5124 50  0000 C CNN
F 2 "amiga-conn:A2000_Video_Slot_Short" H 2100 3700 50  0001 C CNN
F 3 "" H 2100 3700 50  0001 C CNN
	1    2000 3000
	1    0    0    -1  
$EndComp
NoConn ~ 1150 2800
NoConn ~ 1150 1100
NoConn ~ 1150 1300
NoConn ~ 2850 1100
NoConn ~ 2850 2800
Wire Wire Line
	8300 3550 8800 3550
Wire Wire Line
	8800 4050 8300 4050
Wire Wire Line
	9300 3850 9800 3850
$Comp
L videocard-rescue:GND-power #PWR0101
U 1 1 6018DAAA
P 9800 3850
F 0 "#PWR0101" H 9800 3600 50  0001 C CNN
F 1 "GND" V 9805 3722 50  0000 R CNN
F 2 "" H 9800 3850 50  0001 C CNN
F 3 "" H 9800 3850 50  0001 C CNN
	1    9800 3850
	0    -1   -1   0   
$EndComp
$Comp
L videocard-rescue:GND-power #PWR0102
U 1 1 6018E087
P 8300 3550
F 0 "#PWR0102" H 8300 3300 50  0001 C CNN
F 1 "GND" V 8305 3422 50  0000 R CNN
F 2 "" H 8300 3550 50  0001 C CNN
F 3 "" H 8300 3550 50  0001 C CNN
	1    8300 3550
	0    1    1    0   
$EndComp
$Comp
L videocard-rescue:GND-power #PWR0110
U 1 1 6018E493
P 8300 4050
F 0 "#PWR0110" H 8300 3800 50  0001 C CNN
F 1 "GND" V 8305 3922 50  0000 R CNN
F 2 "" H 8300 4050 50  0001 C CNN
F 3 "" H 8300 4050 50  0001 C CNN
	1    8300 4050
	0    1    1    0   
$EndComp
Wire Wire Line
	9300 4550 9800 4550
$Comp
L videocard-rescue:GND-power #PWR0127
U 1 1 60197EFD
P 9800 4550
F 0 "#PWR0127" H 9800 4300 50  0001 C CNN
F 1 "GND" V 9805 4422 50  0000 R CNN
F 2 "" H 9800 4550 50  0001 C CNN
F 3 "" H 9800 4550 50  0001 C CNN
	1    9800 4550
	0    -1   -1   0   
$EndComp
Wire Wire Line
	8800 3250 7850 3250
Wire Wire Line
	7850 3250 7850 3150
Connection ~ 7850 3250
Text Label 7000 3750 0    50   ~ 0
PiB1
Text Label 7000 3850 0    50   ~ 0
PiB2
Text Label 5700 3750 0    50   ~ 0
B1
Text Label 5700 3850 0    50   ~ 0
B2
$Comp
L videocard-rescue:+3.3V-power #PWR0124
U 1 1 5FEB8043
P 4000 6450
F 0 "#PWR0124" H 4000 6300 50  0001 C CNN
F 1 "+3.3V" H 4015 6623 50  0000 C CNN
F 2 "" H 4000 6450 50  0001 C CNN
F 3 "" H 4000 6450 50  0001 C CNN
	1    4000 6450
	1    0    0    -1  
$EndComp
$Comp
L videocard-rescue:+3.3V-power #PWR0128
U 1 1 601A2CC9
P 9900 2650
F 0 "#PWR0128" H 9900 2500 50  0001 C CNN
F 1 "+3.3V" H 9915 2823 50  0000 C CNN
F 2 "" H 9900 2650 50  0001 C CNN
F 3 "" H 9900 2650 50  0001 C CNN
	1    9900 2650
	1    0    0    -1  
$EndComp
Wire Wire Line
	9300 2650 9900 2650
Text Label 8050 4350 0    50   ~ 0
PiR2
Wire Wire Line
	8800 4250 8300 4250
$Comp
L videocard-rescue:GND-power #PWR0135
U 1 1 601B0BE8
P 8300 4250
F 0 "#PWR0135" H 8300 4000 50  0001 C CNN
F 1 "GND" V 8305 4122 50  0000 R CNN
F 2 "" H 8300 4250 50  0001 C CNN
F 3 "" H 8300 4250 50  0001 C CNN
	1    8300 4250
	0    1    1    0   
$EndComp
Text Notes 9400 3450 0    50   ~ 0
3V3
$Comp
L videocard-rescue:+3.3V-power #PWR0136
U 1 1 601BAA75
P 9850 3450
F 0 "#PWR0136" H 9850 3300 50  0001 C CNN
F 1 "+3.3V" H 9865 3623 50  0000 C CNN
F 2 "" H 9850 3450 50  0001 C CNN
F 3 "" H 9850 3450 50  0001 C CNN
	1    9850 3450
	1    0    0    -1  
$EndComp
Wire Wire Line
	9300 3450 9850 3450
Text Label 7000 3950 0    50   ~ 0
PiR0
Text Label 5700 3950 0    50   ~ 0
R0
Text Label 7000 4050 0    50   ~ 0
PiG2
Text Label 5700 4050 0    50   ~ 0
G2
$Comp
L videocard-rescue:GND-power #PWR0117
U 1 1 6036017E
P 5600 1800
F 0 "#PWR0117" H 5600 1550 50  0001 C CNN
F 1 "GND" H 5605 1627 50  0000 C CNN
F 2 "" H 5600 1800 50  0001 C CNN
F 3 "" H 5600 1800 50  0001 C CNN
	1    5600 1800
	1    0    0    -1  
$EndComp
Wire Wire Line
	5600 1800 5850 1800
Wire Wire Line
	3750 1900 3900 1900
$Comp
L videocard-rescue:74HC86-74xx U1
U 2 1 5F5643BE
P 5050 2500
F 0 "U1" H 5050 2825 50  0000 C CNN
F 1 "74LVC86" H 5050 2734 50  0000 C CNN
F 2 "Package_SO:TSSOP-14_4.4x5mm_P0.65mm" H 5050 2500 50  0001 C CNN
F 3 "http://www.ti.com/lit/gpn/sn74HC86" H 5050 2500 50  0001 C CNN
	2    5050 2500
	1    0    0    -1  
$EndComp
$Comp
L videocard-rescue:74HC86-74xx U1
U 4 1 5F564458
P 7700 1500
F 0 "U1" H 7700 1825 50  0000 C CNN
F 1 "74LVC86" H 7700 1734 50  0000 C CNN
F 2 "Package_SO:TSSOP-14_4.4x5mm_P0.65mm" H 7700 1500 50  0001 C CNN
F 3 "http://www.ti.com/lit/gpn/sn74HC86" H 7700 1500 50  0001 C CNN
	4    7700 1500
	1    0    0    1   
$EndComp
$EndSCHEMATC
