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
P 10050 4150
F 0 "J3" H 10100 4567 50  0000 C CNN
F 1 "Conn_02x05_Odd_Even" H 10100 4476 50  0000 C CNN
F 2 "Connector_PinSocket_2.54mm:PinSocket_2x05_P2.54mm_Horizontal" H 10050 4150 50  0001 C CNN
F 3 "~" H 10050 4150 50  0001 C CNN
	1    10050 4150
	1    0    0    -1  
$EndComp
Text Label 1900 2950 2    50   ~ 0
GND
Text Label 1900 3150 2    50   ~ 0
ARED
Text Label 1900 3250 2    50   ~ 0
AGREEN
Text Label 1900 3350 2    50   ~ 0
ABLUE
Text Label 1900 3050 2    50   ~ 0
ASYNC
Wire Wire Line
	5550 2750 5300 2750
Wire Wire Line
	5300 2750 5300 1850
Wire Wire Line
	5300 1850 5550 1850
Wire Wire Line
	5550 4650 5300 4650
Wire Wire Line
	5300 4650 5300 3750
Wire Wire Line
	5300 3750 5550 3750
Wire Wire Line
	8050 2850 7800 2850
Wire Wire Line
	7800 2850 7800 1950
Wire Wire Line
	7800 1950 8050 1950
Text Label 5100 1850 2    50   ~ 0
ARED
Wire Wire Line
	5300 1850 5100 1850
Connection ~ 5300 1850
Wire Wire Line
	5300 3750 5100 3750
Connection ~ 5300 3750
Text Label 5100 3750 2    50   ~ 0
AGREEN
Wire Wire Line
	7500 1950 7800 1950
Connection ~ 7800 1950
Text Label 7500 1950 2    50   ~ 0
ABLUE
Wire Wire Line
	8050 3850 7500 3850
Wire Wire Line
	8050 4750 7500 4750
Text Label 7500 3850 2    50   ~ 0
ASYNC
Text Label 7500 4750 2    50   ~ 0
ASYNC
Text Label 6150 1950 0    50   ~ 0
RED
Text Label 6150 2850 0    50   ~ 0
BRED
Text Label 6150 3850 0    50   ~ 0
GREEN
Text Label 6150 4750 0    50   ~ 0
BGREEN
Text Label 8650 2050 0    50   ~ 0
BLUE
Text Label 8650 2950 0    50   ~ 0
BBLUE
Text Label 8650 3950 0    50   ~ 0
SYNC
Text Label 8650 4850 0    50   ~ 0
VSYNC
Text Label 9850 3950 2    50   ~ 0
VCC
$Comp
L analog-rescue:BH2220FVM-bh2220fvm U1
U 1 1 5DAFDDD1
P 2600 5600
F 0 "U1" H 2600 6165 50  0000 C CNN
F 1 "BH2220FVM" H 2600 6074 50  0000 C CNN
F 2 "Package_SO:MSOP-8_3x3mm_P0.65mm" H 2600 5600 50  0001 C CNN
F 3 "" H 2600 5600 50  0001 C CNN
	1    2600 5600
	1    0    0    -1  
$EndComp
Text Label 2150 5300 2    50   ~ 0
VCC
Text Label 2150 5900 2    50   ~ 0
GND
Text Label 2150 6800 2    50   ~ 0
VCC
Text Label 2150 7400 2    50   ~ 0
GND
$Comp
L Connector_Generic:Conn_01x03 J1
U 1 1 5DB1E4F3
P 950 5600
F 0 "J1" H 868 5917 50  0000 C CNN
F 1 "Conn_01x03" H 868 5826 50  0000 C CNN
F 2 "Connector_PinHeader_2.54mm:PinHeader_1x03_P2.54mm_Horizontal" H 950 5600 50  0001 C CNN
F 3 "~" H 950 5600 50  0001 C CNN
	1    950  5600
	-1   0    0    -1  
$EndComp
Wire Wire Line
	1150 5600 2150 5600
Wire Wire Line
	1150 5500 1350 5500
Wire Wire Line
	1350 5500 1350 5450
Wire Wire Line
	1350 5450 2150 5450
Wire Wire Line
	1150 5700 1350 5700
Wire Wire Line
	1350 5700 1350 5750
Wire Wire Line
	1350 5750 2150 5750
Wire Wire Line
	3050 5450 3650 5450
Wire Wire Line
	3650 2050 5550 2050
Wire Wire Line
	3650 2350 7350 2350
Wire Wire Line
	7350 2350 7350 2150
Wire Wire Line
	7350 2150 8050 2150
Connection ~ 3650 2350
Wire Wire Line
	3650 2350 3650 2050
Wire Wire Line
	5550 3950 3650 3950
Connection ~ 3650 3950
Wire Wire Line
	3650 3950 3650 2350
Wire Wire Line
	3650 3950 3650 5450
Wire Wire Line
	3050 5600 4450 5600
Wire Wire Line
	4450 2950 5550 2950
Wire Wire Line
	4450 5600 4450 4850
Wire Wire Line
	4450 3250 7350 3250
Wire Wire Line
	7350 3250 7350 3050
Wire Wire Line
	7350 3050 8050 3050
Connection ~ 4450 3250
Wire Wire Line
	4450 3250 4450 2950
Wire Wire Line
	4450 4850 5550 4850
Connection ~ 4450 4850
Wire Wire Line
	4450 4850 4450 3250
Wire Wire Line
	3050 5750 7000 5750
Wire Wire Line
	7000 5750 7000 4950
Wire Wire Line
	7000 4050 8050 4050
Wire Wire Line
	8050 4950 7000 4950
Connection ~ 7000 4950
Wire Wire Line
	7000 4950 7000 4050
Text Label 1700 5450 2    50   ~ 0
GPIO22
Text Label 1700 5600 2    50   ~ 0
GPIO20
Text Label 1650 5750 2    50   ~ 0
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
Text Notes 3150 5400 0    50   ~ 0
100% ref
Text Notes 3900 5550 0    50   ~ 0
50% ref
Text Notes 4700 5700 0    50   ~ 0
Sync ref
$Comp
L Comparator:MAX9201 U2
U 1 1 5DB24975
P 5850 1950
F 0 "U2" H 5850 2317 50  0000 C CNN
F 1 "MAX9201" H 5850 2226 50  0000 C CNN
F 2 "Package_SO:TSSOP-16_4.4x5mm_P0.65mm" H 5800 2100 50  0001 C CNN
F 3 "http://ww1.microchip.com/downloads/en/DeviceDoc/DS20002143E.pdf" H 5900 2150 50  0001 C CNN
	1    5850 1950
	1    0    0    -1  
$EndComp
$Comp
L Comparator:MAX9201 U2
U 2 1 5DB258DB
P 5850 2850
F 0 "U2" H 5850 3217 50  0000 C CNN
F 1 "MAX9201" H 5850 3126 50  0000 C CNN
F 2 "Package_SO:TSSOP-16_4.4x5mm_P0.65mm" H 5800 3000 50  0001 C CNN
F 3 "http://ww1.microchip.com/downloads/en/DeviceDoc/DS20002143E.pdf" H 5900 3050 50  0001 C CNN
	2    5850 2850
	1    0    0    -1  
$EndComp
$Comp
L Comparator:MAX9201 U2
U 3 1 5DB2652A
P 8350 2050
F 0 "U2" H 8350 2417 50  0000 C CNN
F 1 "MAX9201" H 8350 2326 50  0000 C CNN
F 2 "Package_SO:TSSOP-16_4.4x5mm_P0.65mm" H 8300 2200 50  0001 C CNN
F 3 "http://ww1.microchip.com/downloads/en/DeviceDoc/DS20002143E.pdf" H 8400 2250 50  0001 C CNN
	3    8350 2050
	1    0    0    -1  
$EndComp
$Comp
L Comparator:MAX9201 U2
U 4 1 5DB2730F
P 8350 2950
F 0 "U2" H 8350 3317 50  0000 C CNN
F 1 "MAX9201" H 8350 3226 50  0000 C CNN
F 2 "Package_SO:TSSOP-16_4.4x5mm_P0.65mm" H 8300 3100 50  0001 C CNN
F 3 "http://ww1.microchip.com/downloads/en/DeviceDoc/DS20002143E.pdf" H 8400 3150 50  0001 C CNN
	4    8350 2950
	1    0    0    -1  
$EndComp
$Comp
L Comparator:MAX9201 U3
U 1 1 5DB28596
P 5850 3850
F 0 "U3" H 5850 4217 50  0000 C CNN
F 1 "MAX9201" H 5850 4126 50  0000 C CNN
F 2 "Package_SO:TSSOP-16_4.4x5mm_P0.65mm" H 5800 4000 50  0001 C CNN
F 3 "http://ww1.microchip.com/downloads/en/DeviceDoc/DS20002143E.pdf" H 5900 4050 50  0001 C CNN
	1    5850 3850
	1    0    0    -1  
$EndComp
$Comp
L Comparator:MAX9201 U3
U 2 1 5DB291EF
P 5850 4750
F 0 "U3" H 5850 5117 50  0000 C CNN
F 1 "MAX9201" H 5850 5026 50  0000 C CNN
F 2 "Package_SO:TSSOP-16_4.4x5mm_P0.65mm" H 5800 4900 50  0001 C CNN
F 3 "http://ww1.microchip.com/downloads/en/DeviceDoc/DS20002143E.pdf" H 5900 4950 50  0001 C CNN
	2    5850 4750
	1    0    0    -1  
$EndComp
$Comp
L Comparator:MAX9201 U3
U 3 1 5DB29FFF
P 8350 3950
F 0 "U3" H 8350 4317 50  0000 C CNN
F 1 "MAX9201" H 8350 4226 50  0000 C CNN
F 2 "Package_SO:TSSOP-16_4.4x5mm_P0.65mm" H 8300 4100 50  0001 C CNN
F 3 "http://ww1.microchip.com/downloads/en/DeviceDoc/DS20002143E.pdf" H 8400 4150 50  0001 C CNN
	3    8350 3950
	1    0    0    -1  
$EndComp
$Comp
L Comparator:MAX9201 U3
U 4 1 5DB2AD59
P 8350 4850
F 0 "U3" H 8350 5217 50  0000 C CNN
F 1 "MAX9201" H 8350 5126 50  0000 C CNN
F 2 "Package_SO:TSSOP-16_4.4x5mm_P0.65mm" H 8300 5000 50  0001 C CNN
F 3 "http://ww1.microchip.com/downloads/en/DeviceDoc/DS20002143E.pdf" H 8400 5050 50  0001 C CNN
	4    8350 4850
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
$Comp
L Device:R_Network04 RN1
U 1 1 5DB3C087
P 2150 4050
F 0 "RN1" V 2575 4050 50  0000 C CNN
F 1 "75R" V 2484 4050 50  0000 C CNN
F 2 "Resistor_THT:R_Array_SIP6" V 2525 4050 50  0001 C CNN
F 3 "http://www.vishay.com/docs/31509/csc.pdf" H 2150 4050 50  0001 C CNN
	1    2150 4050
	0    -1   -1   0   
$EndComp
Text Label 1950 4250 2    50   ~ 0
GND
Text Label 2350 4050 0    50   ~ 0
ARED
Text Label 2350 4150 0    50   ~ 0
AGREEN
Text Label 2350 3950 0    50   ~ 0
ASYNC
Text Label 2350 4250 0    50   ~ 0
ABLUE
Text Label 9850 4050 2    50   ~ 0
VSYNC
Text Label 9850 4150 2    50   ~ 0
SYNC
Text Label 9850 4250 2    50   ~ 0
BBLUE
Text Label 9850 4350 2    50   ~ 0
BGREEN
Text Label 10350 4350 0    50   ~ 0
GND
Text Label 10350 4250 0    50   ~ 0
BRED
Text Label 10350 4150 0    50   ~ 0
RED
Text Label 10350 4050 0    50   ~ 0
GREEN
Text Label 10350 3950 0    50   ~ 0
BLUE
$Comp
L Connector_Generic:Conn_01x05 J2
U 1 1 5DB2E6CA
P 2100 3150
F 0 "J2" H 2180 3192 50  0000 L CNN
F 1 "Conn_01x05" H 2180 3101 50  0000 L CNN
F 2 "Connector_PinHeader_2.54mm:PinHeader_1x05_P2.54mm_Horizontal" H 2100 3150 50  0001 C CNN
F 3 "~" H 2100 3150 50  0001 C CNN
	1    2100 3150
	1    0    0    -1  
$EndComp
$EndSCHEMATC
