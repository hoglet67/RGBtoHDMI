EESchema Schematic File Version 4
LIBS:Clock_fixer-cache
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
Text GLabel 3800 6550 0    50   Input ~ 0
5V
Text GLabel 3800 7550 0    50   Input ~ 0
GND
$Comp
L Device:C_Small C1
U 1 1 628F4DF4
P 4950 7050
F 0 "C1" H 5042 7096 50  0000 L CNN
F 1 "100n" H 5042 7005 50  0000 L CNN
F 2 "Capacitor_SMD:C_0603_1608Metric" H 4950 7050 50  0001 C CNN
F 3 "~" H 4950 7050 50  0001 C CNN
	1    4950 7050
	1    0    0    -1  
$EndComp
$Comp
L Device:C_Small C2
U 1 1 628F5899
P 6300 7050
F 0 "C2" H 6392 7096 50  0000 L CNN
F 1 "100n" H 6392 7005 50  0000 L CNN
F 2 "Capacitor_SMD:C_0603_1608Metric" H 6300 7050 50  0001 C CNN
F 3 "~" H 6300 7050 50  0001 C CNN
	1    6300 7050
	1    0    0    -1  
$EndComp
Wire Wire Line
	4950 6950 4950 6550
Connection ~ 4950 6550
Wire Wire Line
	4950 7150 4950 7550
Connection ~ 4950 7550
Wire Wire Line
	6300 7150 6300 7550
Wire Wire Line
	6300 6950 6300 6550
Wire Wire Line
	10050 3800 8550 3800
Wire Wire Line
	10050 3700 8900 3700
Wire Wire Line
	1700 3200 1600 3200
Connection ~ 1700 3200
Wire Wire Line
	1700 3500 1700 3200
$Comp
L Oscillator:IQXO-70 X1
U 1 1 628CAE0E
P 2000 3500
F 0 "X1" H 2344 3546 50  0000 L CNN
F 1 "IQXO-70" H 2344 3455 50  0000 L CNN
F 2 "Oscillator:Oscillator_SMD_IQD_IQXO70-4Pin_7.5x5.0mm_HandSoldering" H 2675 3175 50  0001 C CNN
F 3 "http://www.iqdfrequencyproducts.com/products/details/iqxo-70-11-30.pdf" H 1900 3500 50  0001 C CNN
	1    2000 3500
	1    0    0    -1  
$EndComp
$Comp
L Connector_Generic:Conn_01x03 J1
U 1 1 628AC6FF
P 10250 3700
F 0 "J1" H 10330 3742 50  0000 L CNN
F 1 "Conn_01x03" H 10330 3651 50  0000 L CNN
F 2 "Connector_PinHeader_2.54mm:PinHeader_1x03_P2.54mm_Vertical" H 10250 3700 50  0001 C CNN
F 3 "~" H 10250 3700 50  0001 C CNN
	1    10250 3700
	1    0    0    -1  
$EndComp
Text GLabel 1400 7150 0    50   Input ~ 0
GND
Text GLabel 1450 4950 0    50   Input ~ 0
5V
Wire Wire Line
	2300 7150 1400 7150
Wire Wire Line
	2300 4950 1450 4950
$Comp
L Memory_EPROM:27C128 U6
U 1 1 62875686
P 2300 6050
F 0 "U6" H 2300 7331 50  0000 C CNN
F 1 "27C128" H 2300 7240 50  0000 C CNN
F 2 "Package_DIP:DIP-28_W15.24mm_Socket" H 2300 6050 50  0001 C CNN
F 3 "http://ww1.microchip.com/downloads/en/devicedoc/11003L.pdf" H 2300 6050 50  0001 C CNN
	1    2300 6050
	1    0    0    -1  
$EndComp
Connection ~ 5200 3500
Wire Wire Line
	5200 3600 5200 3500
Wire Wire Line
	5350 3600 5200 3600
Wire Wire Line
	5200 3400 5350 3400
Wire Wire Line
	5200 3500 5200 3400
$Comp
L Clock_fixer-rescue:74AHC02-74xx U3
U 4 1 6280D371
P 8200 1400
F 0 "U3" H 8200 1725 50  0000 C CNN
F 1 "74AHC02" H 8200 1634 50  0000 C CNN
F 2 "Package_SO:TSSOP-14_4.4x5mm_P0.65mm" H 8200 1400 50  0001 C CNN
F 3 "http://www.ti.com/lit/gpn/sn74hc02" H 8200 1400 50  0001 C CNN
	4    8200 1400
	1    0    0    -1  
$EndComp
$Comp
L Clock_fixer-rescue:74AHC02-74xx U3
U 2 1 6280989C
P 7450 4750
F 0 "U3" H 7450 5075 50  0000 C CNN
F 1 "74AHC02" H 7450 4984 50  0000 C CNN
F 2 "Package_SO:TSSOP-14_4.4x5mm_P0.65mm" H 7450 4750 50  0001 C CNN
F 3 "http://www.ti.com/lit/gpn/sn74hc02" H 7450 4750 50  0001 C CNN
	2    7450 4750
	1    0    0    -1  
$EndComp
$Comp
L Clock_fixer-rescue:74AHC02-74xx U3
U 1 1 628073AA
P 5650 3500
F 0 "U3" H 5650 3825 50  0000 C CNN
F 1 "74AHC02" H 5650 3734 50  0000 C CNN
F 2 "Package_SO:TSSOP-14_4.4x5mm_P0.65mm" H 5650 3500 50  0001 C CNN
F 3 "http://www.ti.com/lit/gpn/sn74hc02" H 5650 3500 50  0001 C CNN
	1    5650 3500
	1    0    0    -1  
$EndComp
Text Notes 4650 6100 0    118  ~ 0
Synchronous divide by 8 with hsync reset
Text GLabel 1600 3800 0    50   Input ~ 0
GND
Text GLabel 1600 3200 0    50   Input ~ 0
5V
Wire Wire Line
	2000 3800 1600 3800
Wire Wire Line
	2000 3200 1700 3200
Text GLabel 8900 3700 0    50   Input ~ 0
!HSYNC
Text GLabel 9200 3800 3    50   Input ~ 0
6Mhz
Text GLabel 8550 3050 0    50   Input ~ 0
16Mhz
Wire Wire Line
	8550 3600 10050 3600
Wire Wire Line
	8900 6200 8900 3700
Wire Wire Line
	4500 6200 8900 6200
Wire Wire Line
	4500 5550 4500 6200
Wire Wire Line
	4950 5550 4500 5550
Text GLabel 4050 5850 0    50   Input ~ 0
GND
Wire Wire Line
	5450 5850 4750 5850
Text GLabel 4150 4250 0    50   Input ~ 0
5V
Connection ~ 4350 4250
Wire Wire Line
	4350 4250 4150 4250
Wire Wire Line
	4350 4250 5450 4250
Connection ~ 4350 5150
Wire Wire Line
	4350 5250 4350 5150
Wire Wire Line
	4950 5250 4350 5250
Connection ~ 4350 5050
Wire Wire Line
	4350 5150 4350 5050
Wire Wire Line
	4950 5150 4350 5150
Wire Wire Line
	4950 5050 4350 5050
Text Label 4150 3500 0    50   ~ 0
CLK48
Connection ~ 7050 4750
Wire Wire Line
	7050 4850 7150 4850
Wire Wire Line
	7050 4750 7050 4850
Wire Wire Line
	7050 4650 7150 4650
Wire Wire Line
	7050 4750 7050 4650
Wire Wire Line
	5950 4750 6800 4750
$Comp
L 74xx:74LS163 U4
U 1 1 628120EC
P 5450 5050
F 0 "U4" H 5450 6031 50  0000 C CNN
F 1 "74VHC163" H 5450 5940 50  0000 C CNN
F 2 "Package_SO:TSSOP-16_4.4x5mm_P0.65mm" H 5450 5050 50  0001 C CNN
F 3 "http://www.ti.com/lit/gpn/sn74LS163" H 5450 5050 50  0001 C CNN
	1    5450 5050
	1    0    0    -1  
$EndComp
Text Notes 2900 750  0    118  ~ 0
Synchronous divide by 3 with 50:50 mark space ratio
$Comp
L 74xx:74LS163 U1
U 1 1 6281A10C
P 4750 2000
F 0 "U1" H 4750 2981 50  0000 C CNN
F 1 "74VHC163" H 4750 2890 50  0000 C CNN
F 2 "Package_SO:TSSOP-16_4.4x5mm_P0.65mm" H 4750 2000 50  0001 C CNN
F 3 "http://www.ti.com/lit/gpn/sn74LS163" H 4750 2000 50  0001 C CNN
	1    4750 2000
	1    0    0    -1  
$EndComp
Text GLabel 3950 1200 0    50   Input ~ 0
5V
Text GLabel 3100 2800 0    50   Input ~ 0
GND
Wire Wire Line
	2750 850  5450 850 
Wire Wire Line
	5450 850  5450 1600
Wire Wire Line
	5450 1600 5250 1600
Wire Wire Line
	4250 2300 3950 2300
Wire Wire Line
	3950 2300 3950 3500
Connection ~ 3950 3500
Wire Wire Line
	3950 3500 5200 3500
Text Label 6500 3350 0    50   ~ 0
!CLK48
$Comp
L 74xx:74LS163 U2
U 1 1 62881229
P 7100 2000
F 0 "U2" H 7100 2981 50  0000 C CNN
F 1 "74VHC163" H 7100 2890 50  0000 C CNN
F 2 "Package_SO:TSSOP-16_4.4x5mm_P0.65mm" H 7100 2000 50  0001 C CNN
F 3 "http://www.ti.com/lit/gpn/sn74LS163" H 7100 2000 50  0001 C CNN
	1    7100 2000
	1    0    0    -1  
$EndComp
Wire Wire Line
	7100 2800 6250 2800
Wire Wire Line
	6250 2800 6250 2200
Wire Wire Line
	6250 2000 6600 2000
Connection ~ 4750 1200
Wire Wire Line
	4750 1200 6350 1200
Connection ~ 6350 1200
Wire Wire Line
	6350 1200 7100 1200
Wire Wire Line
	6600 2500 6350 2500
Wire Wire Line
	5950 3500 6500 3500
Wire Wire Line
	6500 3500 6500 2300
Wire Wire Line
	6500 2300 6600 2300
Wire Wire Line
	5450 1600 5850 1600
Wire Wire Line
	5850 1600 5850 1500
Wire Wire Line
	5850 1500 6600 1500
Connection ~ 5450 1600
Wire Wire Line
	5850 1500 5850 900 
Wire Wire Line
	5850 900  7750 900 
Wire Wire Line
	7750 900  7750 1300
Wire Wire Line
	7750 1300 7900 1300
Connection ~ 5850 1500
Wire Wire Line
	7600 1500 7900 1500
Wire Wire Line
	8500 1400 8550 1400
Wire Wire Line
	8550 1400 8550 3600
Wire Wire Line
	3800 6550 4950 6550
Wire Wire Line
	4950 6550 5700 6550
Wire Wire Line
	4950 7550 5700 7550
Wire Wire Line
	3800 7550 4950 7550
Wire Wire Line
	4250 2500 3800 2500
$Comp
L Clock_fixer-rescue:74AHC02-74xx U3
U 3 1 6280AFE2
P 3500 2500
F 0 "U3" H 3500 2825 50  0000 C CNN
F 1 "74AHC02" H 3500 2734 50  0000 C CNN
F 2 "Package_SO:TSSOP-14_4.4x5mm_P0.65mm" H 3500 2500 50  0001 C CNN
F 3 "http://www.ti.com/lit/gpn/sn74hc02" H 3500 2500 50  0001 C CNN
	3    3500 2500
	1    0    0    -1  
$EndComp
Wire Wire Line
	3200 2600 2950 2600
Wire Wire Line
	2950 2600 2950 2500
Wire Wire Line
	2950 2400 3200 2400
Wire Wire Line
	2950 2500 2750 2500
Wire Wire Line
	2750 2500 2750 850 
Connection ~ 2950 2500
Wire Wire Line
	2950 2500 2950 2400
$Comp
L 74xx:74HC02 U3
U 5 1 628F1E6D
P 5700 7050
F 0 "U3" H 5930 7096 50  0000 L CNN
F 1 "74AHC02" H 5930 7005 50  0000 L CNN
F 2 "Package_SO:TSSOP-14_4.4x5mm_P0.65mm" H 5700 7050 50  0001 C CNN
F 3 "http://www.ti.com/lit/gpn/sn74hc02" H 5700 7050 50  0001 C CNN
	5    5700 7050
	1    0    0    -1  
$EndComp
Connection ~ 5700 6550
Wire Wire Line
	5700 6550 6300 6550
Connection ~ 5700 7550
Wire Wire Line
	5700 7550 6300 7550
Connection ~ 6250 2800
Wire Wire Line
	3100 2800 3850 2800
Connection ~ 4750 2800
Wire Wire Line
	4750 2800 6250 2800
Wire Wire Line
	2300 3500 3500 3500
Wire Wire Line
	3500 3950 6500 3950
Wire Wire Line
	6500 3950 6500 3500
Connection ~ 6500 3500
Wire Wire Line
	6600 2100 6250 2100
Connection ~ 6250 2100
Wire Wire Line
	6250 2100 6250 2000
Wire Wire Line
	6600 2200 6250 2200
Connection ~ 6250 2200
Wire Wire Line
	6250 2200 6250 2100
Wire Wire Line
	6350 1200 6350 2500
Wire Wire Line
	6250 2000 6250 1800
Wire Wire Line
	6250 1800 6600 1800
Connection ~ 6250 2000
Wire Wire Line
	6250 1800 6250 1700
Wire Wire Line
	6250 1600 6600 1600
Connection ~ 6250 1800
Wire Wire Line
	6600 1700 6250 1700
Connection ~ 6250 1700
Wire Wire Line
	6250 1700 6250 1600
Wire Wire Line
	3950 1200 4050 1200
Wire Wire Line
	3850 2800 3850 1800
Wire Wire Line
	3850 1800 4250 1800
Connection ~ 3850 2800
Wire Wire Line
	3850 2800 4750 2800
Wire Wire Line
	3850 1800 3850 1700
Wire Wire Line
	3850 1500 4250 1500
Connection ~ 3850 1800
Wire Wire Line
	4250 1600 3850 1600
Connection ~ 3850 1600
Wire Wire Line
	3850 1600 3850 1500
Wire Wire Line
	4250 1700 3850 1700
Connection ~ 3850 1700
Wire Wire Line
	3850 1700 3850 1600
Wire Wire Line
	4250 2200 4050 2200
Wire Wire Line
	4050 2200 4050 2100
Connection ~ 4050 1200
Wire Wire Line
	4050 1200 4750 1200
Wire Wire Line
	4250 2100 4050 2100
Connection ~ 4050 2100
Wire Wire Line
	4050 2100 4050 2000
Wire Wire Line
	4250 2000 4050 2000
Connection ~ 4050 2000
Wire Wire Line
	4050 2000 4050 1200
Wire Wire Line
	4350 4250 4350 5050
Wire Wire Line
	4950 4850 4750 4850
Wire Wire Line
	4750 4850 4750 5850
Connection ~ 4750 5850
Wire Wire Line
	4750 5850 4050 5850
Wire Wire Line
	4750 4550 4950 4550
Connection ~ 4750 4850
Wire Wire Line
	4950 4650 4750 4650
Wire Wire Line
	4750 4550 4750 4650
Connection ~ 4750 4650
Wire Wire Line
	4750 4650 4750 4750
Wire Wire Line
	4950 4750 4750 4750
Connection ~ 4750 4750
Wire Wire Line
	4750 4750 4750 4850
$Comp
L Jumper:SolderJumper_3_Bridged12 JP1
U 1 1 6298700D
P 3500 3750
F 0 "JP1" V 3546 3818 50  0000 L CNN
F 1 "SolderJumper_3_Bridged12" V 3455 3818 50  0000 L CNN
F 2 "Jumper:SolderJumper-3_P1.3mm_Bridged12_RoundedPad1.0x1.5mm" H 3500 3750 50  0001 C CNN
F 3 "~" H 3500 3750 50  0001 C CNN
	1    3500 3750
	0    1    -1   0   
$EndComp
Wire Wire Line
	3500 3550 3500 3500
Connection ~ 3500 3500
Wire Wire Line
	3500 3500 3950 3500
Wire Wire Line
	3350 3750 3200 3750
Wire Wire Line
	3200 3750 3200 5350
Wire Wire Line
	3200 5350 4950 5350
$Comp
L Jumper:SolderJumper_3_Bridged12 JP2
U 1 1 6299665B
P 8400 3800
F 0 "JP2" V 8446 3868 50  0000 L CNN
F 1 "SolderJumper_3_Bridged12" V 8355 3868 50  0000 L CNN
F 2 "Jumper:SolderJumper-3_P1.3mm_Bridged12_RoundedPad1.0x1.5mm" H 8400 3800 50  0001 C CNN
F 3 "~" H 8400 3800 50  0001 C CNN
	1    8400 3800
	0    -1   -1   0   
$EndComp
Wire Wire Line
	7750 4750 8400 4750
Wire Wire Line
	8400 4750 8400 4000
Wire Wire Line
	6800 4750 6800 3600
Wire Wire Line
	6800 3600 8400 3600
Connection ~ 6800 4750
Wire Wire Line
	6800 4750 7050 4750
$EndSCHEMATC
