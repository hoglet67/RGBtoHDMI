EESchema Schematic File Version 4
LIBS:analog-cache
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
L Connector_Generic:Conn_02x05_Odd_Even J3
U 1 1 5DB26BD3
P 10100 4200
F 0 "J3" H 10150 4617 50  0000 C CNN
F 1 "Conn_02x05_Odd_Even" H 10150 4526 50  0000 C CNN
F 2 "Connector_PinSocket_2.54mm:PinSocket_2x05_P2.54mm_Horizontal" H 10100 4200 50  0001 C CNN
F 3 "~" H 10100 4200 50  0001 C CNN
	1    10100 4200
	1    0    0    -1  
$EndComp
Text Label 1950 3000 2    50   ~ 0
GND
Text Label 1950 3200 2    50   ~ 0
ARED
Text Label 1950 3300 2    50   ~ 0
AGREEN
Text Label 1950 3400 2    50   ~ 0
ABLUE
Text Label 1950 3100 2    50   ~ 0
ASYNC
Wire Wire Line
	5600 2800 5350 2800
Wire Wire Line
	5350 2800 5350 1900
Wire Wire Line
	5350 1900 5600 1900
Wire Wire Line
	5600 4700 5350 4700
Wire Wire Line
	5350 4700 5350 3800
Wire Wire Line
	5350 3800 5600 3800
Wire Wire Line
	8100 2900 7850 2900
Wire Wire Line
	7850 2900 7850 2000
Wire Wire Line
	7850 2000 8100 2000
Text Label 5150 1900 2    50   ~ 0
ARED
Wire Wire Line
	5350 1900 5150 1900
Connection ~ 5350 1900
Wire Wire Line
	5350 3800 5150 3800
Connection ~ 5350 3800
Text Label 5150 3800 2    50   ~ 0
AGREEN
Wire Wire Line
	7550 2000 7850 2000
Connection ~ 7850 2000
Text Label 7550 2000 2    50   ~ 0
ABLUE
Text Label 6200 2000 0    50   ~ 0
RED
Text Label 6200 2900 0    50   ~ 0
BRED
Text Label 6200 3900 0    50   ~ 0
GREEN
Text Label 6200 4800 0    50   ~ 0
BGREEN
Text Label 8700 2100 0    50   ~ 0
BLUE
Text Label 8700 3000 0    50   ~ 0
BBLUE
Text Label 9900 4000 2    50   ~ 0
VCC
Text Label 2200 5350 2    50   ~ 0
VCC
Text Label 2200 5950 2    50   ~ 0
GND
Text Label 2150 6800 2    50   ~ 0
VCC
Text Label 2150 7400 2    50   ~ 0
GND
$Comp
L Connector_Generic:Conn_01x03 J1
U 1 1 5DB1E4F3
P 1000 5650
F 0 "J1" H 918 5967 50  0000 C CNN
F 1 "Conn_01x03" H 918 5876 50  0000 C CNN
F 2 "Connector_PinHeader_2.54mm:PinHeader_1x03_P2.54mm_Horizontal" H 1000 5650 50  0001 C CNN
F 3 "~" H 1000 5650 50  0001 C CNN
	1    1000 5650
	-1   0    0    -1  
$EndComp
Wire Wire Line
	1200 5650 2200 5650
Wire Wire Line
	1200 5550 1400 5550
Wire Wire Line
	1400 5550 1400 5500
Wire Wire Line
	1400 5500 2200 5500
Wire Wire Line
	1200 5750 1400 5750
Wire Wire Line
	1400 5750 1400 5800
Wire Wire Line
	1400 5800 2200 5800
Wire Wire Line
	3100 5500 3700 5500
Wire Wire Line
	3700 2100 5600 2100
Wire Wire Line
	3700 2400 7400 2400
Wire Wire Line
	7400 2400 7400 2200
Wire Wire Line
	7400 2200 8100 2200
Connection ~ 3700 2400
Wire Wire Line
	3700 2400 3700 2100
Wire Wire Line
	5600 4000 3700 4000
Connection ~ 3700 4000
Wire Wire Line
	3700 4000 3700 2400
Wire Wire Line
	3700 4000 3700 5500
Wire Wire Line
	3100 5650 4500 5650
Wire Wire Line
	4500 3000 5600 3000
Wire Wire Line
	4500 5650 4500 4900
Wire Wire Line
	4500 3300 7400 3300
Wire Wire Line
	7400 3300 7400 3100
Wire Wire Line
	7400 3100 8100 3100
Connection ~ 4500 3300
Wire Wire Line
	4500 3300 4500 3000
Wire Wire Line
	4500 4900 5600 4900
Connection ~ 4500 4900
Wire Wire Line
	4500 4900 4500 3300
Wire Wire Line
	3100 5800 7050 5800
Text Label 1750 5500 2    50   ~ 0
GPIO22
Text Label 1750 5650 2    50   ~ 0
GPIO20
Text Label 1700 5800 2    50   ~ 0
GPIO0
$Comp
L pspice:CAP C2
U 1 1 5DB4F048
P 5250 7050
F 0 "C2" H 5428 7096 50  0000 L CNN
F 1 "CAP" H 5428 7005 50  0000 L CNN
F 2 "Capacitor_SMD:C_0805_2012Metric_Pad1.15x1.40mm_HandSolder" H 5250 7050 50  0001 C CNN
F 3 "~" H 5250 7050 50  0001 C CNN
	1    5250 7050
	1    0    0    -1  
$EndComp
Wire Wire Line
	6000 6750 6000 6850
Wire Wire Line
	5250 6800 5250 6750
Connection ~ 5250 6750
Wire Wire Line
	5250 6750 6000 6750
Wire Wire Line
	4550 6850 4550 6750
Wire Wire Line
	4550 6750 5250 6750
Wire Wire Line
	4550 7350 4550 7450
Text Notes 3200 5450 0    50   ~ 0
100% ref
Text Notes 3950 5600 0    50   ~ 0
50% ref
Text Notes 4750 5750 0    50   ~ 0
Monochrome ref
$Comp
L Comparator:MAX9201 U2
U 1 1 5DB24975
P 5900 2000
F 0 "U2" H 5900 2367 50  0000 C CNN
F 1 "MAX9201" H 5900 2276 50  0000 C CNN
F 2 "Package_SO:TSSOP-16_4.4x5mm_P0.65mm" H 5850 2150 50  0001 C CNN
F 3 "http://ww1.microchip.com/downloads/en/DeviceDoc/DS20002143E.pdf" H 5950 2200 50  0001 C CNN
	1    5900 2000
	1    0    0    -1  
$EndComp
$Comp
L Comparator:MAX9201 U2
U 2 1 5DB258DB
P 5900 2900
F 0 "U2" H 5900 3267 50  0000 C CNN
F 1 "MAX9201" H 5900 3176 50  0000 C CNN
F 2 "Package_SO:TSSOP-16_4.4x5mm_P0.65mm" H 5850 3050 50  0001 C CNN
F 3 "http://ww1.microchip.com/downloads/en/DeviceDoc/DS20002143E.pdf" H 5950 3100 50  0001 C CNN
	2    5900 2900
	1    0    0    -1  
$EndComp
$Comp
L Comparator:MAX9201 U2
U 3 1 5DB2652A
P 8400 2100
F 0 "U2" H 8400 2467 50  0000 C CNN
F 1 "MAX9201" H 8400 2376 50  0000 C CNN
F 2 "Package_SO:TSSOP-16_4.4x5mm_P0.65mm" H 8350 2250 50  0001 C CNN
F 3 "http://ww1.microchip.com/downloads/en/DeviceDoc/DS20002143E.pdf" H 8450 2300 50  0001 C CNN
	3    8400 2100
	1    0    0    -1  
$EndComp
$Comp
L Comparator:MAX9201 U2
U 4 1 5DB2730F
P 8400 3000
F 0 "U2" H 8400 3367 50  0000 C CNN
F 1 "MAX9201" H 8400 3276 50  0000 C CNN
F 2 "Package_SO:TSSOP-16_4.4x5mm_P0.65mm" H 8350 3150 50  0001 C CNN
F 3 "http://ww1.microchip.com/downloads/en/DeviceDoc/DS20002143E.pdf" H 8450 3200 50  0001 C CNN
	4    8400 3000
	1    0    0    -1  
$EndComp
$Comp
L Comparator:MAX9201 U3
U 1 1 5DB28596
P 5900 3900
F 0 "U3" H 5900 4267 50  0000 C CNN
F 1 "MAX9201" H 5900 4176 50  0000 C CNN
F 2 "Package_SO:TSSOP-16_4.4x5mm_P0.65mm" H 5850 4050 50  0001 C CNN
F 3 "http://ww1.microchip.com/downloads/en/DeviceDoc/DS20002143E.pdf" H 5950 4100 50  0001 C CNN
	1    5900 3900
	1    0    0    -1  
$EndComp
$Comp
L Comparator:MAX9201 U3
U 2 1 5DB291EF
P 5900 4800
F 0 "U3" H 5900 5167 50  0000 C CNN
F 1 "MAX9201" H 5900 5076 50  0000 C CNN
F 2 "Package_SO:TSSOP-16_4.4x5mm_P0.65mm" H 5850 4950 50  0001 C CNN
F 3 "http://ww1.microchip.com/downloads/en/DeviceDoc/DS20002143E.pdf" H 5950 5000 50  0001 C CNN
	2    5900 4800
	1    0    0    -1  
$EndComp
$Comp
L Comparator:MAX9201 U3
U 3 1 5DB29FFF
P 8400 4000
F 0 "U3" H 8400 4367 50  0000 C CNN
F 1 "MAX9201" H 8400 4276 50  0000 C CNN
F 2 "Package_SO:TSSOP-16_4.4x5mm_P0.65mm" H 8350 4150 50  0001 C CNN
F 3 "http://ww1.microchip.com/downloads/en/DeviceDoc/DS20002143E.pdf" H 8450 4200 50  0001 C CNN
	3    8400 4000
	1    0    0    -1  
$EndComp
$Comp
L Comparator:MAX9201 U3
U 4 1 5DB2AD59
P 8400 4900
F 0 "U3" H 8400 5267 50  0000 C CNN
F 1 "MAX9201" H 8400 5176 50  0000 C CNN
F 2 "Package_SO:TSSOP-16_4.4x5mm_P0.65mm" H 8350 5050 50  0001 C CNN
F 3 "http://ww1.microchip.com/downloads/en/DeviceDoc/DS20002143E.pdf" H 8450 5100 50  0001 C CNN
	4    8400 4900
	1    0    0    -1  
$EndComp
$Comp
L Comparator:MAX9201 U3
U 5 1 5DB2BD87
P 4000 7100
F 0 "U3" H 3958 7146 50  0000 L CNN
F 1 "MAX9201" H 3958 7055 50  0000 L CNN
F 2 "Package_SO:TSSOP-16_4.4x5mm_P0.65mm" H 3950 7250 50  0001 C CNN
F 3 "http://ww1.microchip.com/downloads/en/DeviceDoc/DS20002143E.pdf" H 4050 7300 50  0001 C CNN
	5    4000 7100
	1    0    0    -1  
$EndComp
$Comp
L Comparator:MAX9201 U2
U 5 1 5DB2CA12
P 3000 7100
F 0 "U2" H 2958 7146 50  0000 L CNN
F 1 "MAX9201" H 2958 7055 50  0000 L CNN
F 2 "Package_SO:TSSOP-16_4.4x5mm_P0.65mm" H 2950 7250 50  0001 C CNN
F 3 "http://ww1.microchip.com/downloads/en/DeviceDoc/DS20002143E.pdf" H 3050 7300 50  0001 C CNN
	5    3000 7100
	1    0    0    -1  
$EndComp
Wire Wire Line
	2150 6750 2650 6750
Connection ~ 2650 6750
Wire Wire Line
	2650 6750 2900 6750
Connection ~ 2900 6750
Wire Wire Line
	2900 6750 3650 6750
Connection ~ 3650 6750
Wire Wire Line
	3650 6750 3900 6750
Connection ~ 3900 6750
Wire Wire Line
	3900 6750 4550 6750
Wire Wire Line
	2150 7450 2650 7450
Connection ~ 2650 7450
Wire Wire Line
	2650 7450 2900 7450
Connection ~ 2900 7450
Wire Wire Line
	2900 7450 3650 7450
Connection ~ 3650 7450
Wire Wire Line
	3650 7450 3900 7450
Connection ~ 3900 7450
Wire Wire Line
	3900 7450 4550 7450
Connection ~ 4550 6750
Wire Wire Line
	4550 7450 5250 7450
Wire Wire Line
	5250 7450 6000 7450
Connection ~ 5250 7450
Wire Wire Line
	5250 7300 5250 7450
Wire Wire Line
	6000 7450 6000 7350
$Comp
L pspice:CAP C3
U 1 1 5DB4F8EC
P 6000 7100
F 0 "C3" H 6178 7146 50  0000 L CNN
F 1 "CAP" H 6178 7055 50  0000 L CNN
F 2 "Capacitor_SMD:C_0805_2012Metric_Pad1.15x1.40mm_HandSolder" H 6000 7100 50  0001 C CNN
F 3 "~" H 6000 7100 50  0001 C CNN
	1    6000 7100
	1    0    0    -1  
$EndComp
$Comp
L pspice:CAP C1
U 1 1 5DB4E691
P 4550 7100
F 0 "C1" H 4728 7146 50  0000 L CNN
F 1 "CAP" H 4728 7055 50  0000 L CNN
F 2 "Capacitor_SMD:C_0805_2012Metric_Pad1.15x1.40mm_HandSolder" H 4550 7100 50  0001 C CNN
F 3 "~" H 4550 7100 50  0001 C CNN
	1    4550 7100
	1    0    0    -1  
$EndComp
Connection ~ 4550 7450
Text Label 1800 4250 2    50   ~ 0
GND
Text Label 2200 4150 0    50   ~ 0
ARED
Text Label 2200 4050 0    50   ~ 0
AGREEN
Text Label 2200 4250 0    50   ~ 0
ASYNC
Text Label 2200 3950 0    50   ~ 0
ABLUE
Text Label 9900 4100 2    50   ~ 0
VSYNC
Text Label 9900 4200 2    50   ~ 0
SYNC
Text Label 9900 4300 2    50   ~ 0
BBLUE
Text Label 9900 4400 2    50   ~ 0
BGREEN
Text Label 10400 4400 0    50   ~ 0
GND
Text Label 10400 4300 0    50   ~ 0
BRED
Text Label 10400 4200 0    50   ~ 0
RED
Text Label 10400 4100 0    50   ~ 0
GREEN
Text Label 10400 4000 0    50   ~ 0
BLUE
$Comp
L Connector_Generic:Conn_01x05 J2
U 1 1 5DC7AFF6
P 2150 3200
F 0 "J2" H 2230 3242 50  0000 L CNN
F 1 "Conn_01x05" H 2230 3151 50  0000 L CNN
F 2 "Connector_PinHeader_2.54mm:PinHeader_1x05_P2.54mm_Horizontal" H 2150 3200 50  0001 C CNN
F 3 "~" H 2150 3200 50  0001 C CNN
	1    2150 3200
	1    0    0    -1  
$EndComp
$Comp
L Device:R_Network04 RN1
U 1 1 5DC80AE0
P 2000 4050
F 0 "RN1" V 2325 4050 50  0000 C CNN
F 1 "R_Network04" V 2234 4050 50  0000 C CNN
F 2 "Resistor_THT:R_Array_SIP5" V 2275 4050 50  0001 C CNN
F 3 "http://www.vishay.com/docs/31509/csc.pdf" H 2000 4050 50  0001 C CNN
	1    2000 4050
	0    -1   -1   0   
$EndComp
Wire Wire Line
	8100 4800 7900 4800
Wire Wire Line
	7900 4800 7900 3900
Wire Wire Line
	7900 3900 8100 3900
Wire Wire Line
	7900 3900 7550 3900
Connection ~ 7900 3900
Text Label 7550 3900 2    50   ~ 0
ASYNC
Text Label 8700 4000 0    50   ~ 0
VSYNC
Text Label 8700 4900 0    50   ~ 0
SYNC
$Comp
L Analog_DAC:DAC084S085 U1
U 1 1 5DC7B737
P 2650 5650
F 0 "U1" H 2650 6215 50  0000 C CNN
F 1 "DAC084S085" H 2650 6124 50  0000 C CNN
F 2 "Package_SO:VSSOP-10_3x3mm_P0.5mm" H 2650 5650 50  0001 C CNN
F 3 "" H 2650 5650 50  0001 C CNN
	1    2650 5650
	1    0    0    -1  
$EndComp
Text Label 3100 5350 0    50   ~ 0
VCC
Wire Wire Line
	7050 5800 7050 4100
Wire Wire Line
	7050 4100 8100 4100
Wire Wire Line
	3100 5950 7900 5950
Wire Wire Line
	7900 5950 7900 5000
Wire Wire Line
	7900 5000 8100 5000
Text Notes 7350 5900 0    50   ~ 0
Sync ref
$EndSCHEMATC
