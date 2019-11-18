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
Text Label 4850 6700 2    50   ~ 0
VCC
Text Label 6350 6700 0    50   ~ 0
VANALOG
Text Label 6350 7200 0    50   ~ 0
GND
Text Label 4850 7200 2    50   ~ 0
GND
Wire Wire Line
	10450 6450 10200 6450
$Comp
L Device:C_Small C5
U 1 1 5DD09882
P 5950 6950
F 0 "C5" H 6042 6996 50  0000 L CNN
F 1 "100n" H 6042 6905 50  0000 L CNN
F 2 "Capacitor_SMD:C_0805_2012Metric_Pad1.15x1.40mm_HandSolder" H 5950 6950 50  0001 C CNN
F 3 "~" H 5950 6950 50  0001 C CNN
	1    5950 6950
	1    0    0    -1  
$EndComp
Wire Wire Line
	5950 7050 5950 7200
Wire Wire Line
	5950 7200 6350 7200
Wire Wire Line
	5950 6850 5950 6700
Wire Wire Line
	5950 6700 6200 6700
Connection ~ 5950 6700
$Comp
L power:PWR_FLAG #FLG0103
U 1 1 5DD564AC
P 6200 6700
F 0 "#FLG0103" H 6200 6775 50  0001 C CNN
F 1 "PWR_FLAG" H 6200 6873 50  0000 C CNN
F 2 "" H 6200 6700 50  0001 C CNN
F 3 "~" H 6200 6700 50  0001 C CNN
	1    6200 6700
	1    0    0    -1  
$EndComp
Connection ~ 6200 6700
Wire Wire Line
	6200 6700 6350 6700
Wire Wire Line
	4850 7200 5450 7200
$Comp
L Connector_Generic:Conn_01x01 P6
U 1 1 5DD07D72
P 9600 750
F 0 "P6" H 9680 792 50  0000 L CNN
F 1 "Conn_01x01" H 9680 701 50  0000 L CNN
F 2 "Connector_PinHeader_2.54mm:PinHeader_1x01_P2.54mm_Vertical" H 9600 750 50  0001 C CNN
F 3 "~" H 9600 750 50  0001 C CNN
	1    9600 750 
	1    0    0    -1  
$EndComp
$Comp
L Connector_Generic:Conn_01x01 P7
U 1 1 5DD08736
P 10550 750
F 0 "P7" H 10630 792 50  0000 L CNN
F 1 "Conn_01x01" H 10630 701 50  0000 L CNN
F 2 "Connector_PinHeader_2.54mm:PinHeader_1x01_P2.54mm_Vertical" H 10550 750 50  0001 C CNN
F 3 "~" H 10550 750 50  0001 C CNN
	1    10550 750 
	1    0    0    -1  
$EndComp
Wire Wire Line
	5750 6700 5950 6700
Wire Wire Line
	4850 6700 4950 6700
Connection ~ 9850 6450
Connection ~ 9500 6450
Connection ~ 8450 6450
Connection ~ 8100 6450
Connection ~ 10200 6450
Wire Wire Line
	9050 6450 8800 6450
Connection ~ 9050 6450
Connection ~ 8800 6450
Wire Wire Line
	9850 6450 9500 6450
Wire Wire Line
	9500 6450 9050 6450
Wire Wire Line
	8100 6450 8450 6450
Wire Wire Line
	7800 6450 8100 6450
Wire Wire Line
	8450 6450 8800 6450
Wire Wire Line
	10200 6450 9850 6450
Wire Wire Line
	2400 5600 2400 5700
Connection ~ 2400 5600
Wire Wire Line
	2400 5500 2400 5600
Connection ~ 2400 5500
Wire Wire Line
	2400 5400 2400 5500
Wire Wire Line
	3400 5400 4050 5400
Wire Wire Line
	8850 4450 8850 5500
Wire Wire Line
	9950 4450 8850 4450
Wire Wire Line
	8500 3550 9950 3550
Text Label 2400 5400 2    50   ~ 0
VANALOG
Text Label 3550 5500 0    50   ~ 0
REFSYNC
Text Label 3550 5400 0    50   ~ 0
REF50
Wire Wire Line
	5800 4450 5800 5400
Text Label 3550 5300 0    50   ~ 0
REF100
Wire Wire Line
	3400 5300 3900 5300
Text Label 2800 4900 1    50   ~ 0
GND
Wire Wire Line
	1000 5300 1700 5300
$Comp
L Analog_DAC:TLC5620 UA1
U 1 1 5DCDF8E6
P 2900 5400
F 0 "UA1" H 3200 6000 50  0000 C CNN
F 1 "TLC5620" H 3200 5900 50  0000 C CNN
F 2 "Package_SO:SOIC-14_3.9x8.7mm_P1.27mm" H 2800 5350 50  0001 C CNN
F 3 "" H 2800 5350 50  0001 C CNN
	1    2900 5400
	1    0    0    -1  
$EndComp
Text Label 2900 4900 1    50   ~ 0
VCC
$Comp
L Connector_Generic:Conn_01x04 P2
U 1 1 5DCFA861
P 800 5200
F 0 "P2" H 718 4775 50  0000 C CNN
F 1 "Conn_01x04" H 718 4866 50  0000 C CNN
F 2 "Connector_PinHeader_2.54mm:PinHeader_1x04_P2.54mm_Vertical" H 800 5200 50  0001 C CNN
F 3 "~" H 800 5200 50  0001 C CNN
	1    800  5200
	-1   0    0    1   
$EndComp
Connection ~ 1750 1550
Wire Wire Line
	1750 1850 1750 1550
Wire Wire Line
	1400 1850 1750 1850
Wire Wire Line
	1650 1950 1650 2700
Connection ~ 1650 1950
Wire Wire Line
	1400 1950 1650 1950
$Comp
L Connector_Generic:Conn_01x02 P4
U 1 1 5DD175CA
P 1200 1950
F 0 "P4" H 1400 1950 50  0000 C CNN
F 1 "Conn_01x02" H 1550 1850 50  0000 C CNN
F 2 "Connector_PinHeader_2.54mm:PinHeader_1x02_P2.54mm_Vertical" H 1200 1950 50  0001 C CNN
F 3 "~" H 1200 1950 50  0001 C CNN
	1    1200 1950
	-1   0    0    1   
$EndComp
Wire Wire Line
	1400 1550 1750 1550
Wire Wire Line
	9850 5750 10200 5750
Connection ~ 9850 5750
Wire Wire Line
	9850 6000 9850 5750
Wire Wire Line
	9850 6200 9850 6450
Wire Wire Line
	9500 6200 9500 6450
Wire Wire Line
	9500 5750 9850 5750
Connection ~ 9500 5750
Wire Wire Line
	9500 5750 9500 6000
Wire Wire Line
	8450 5750 8800 5750
Connection ~ 8450 5750
Wire Wire Line
	8450 6000 8450 5750
Wire Wire Line
	8450 6200 8450 6450
Wire Wire Line
	8100 6200 8100 6450
Wire Wire Line
	8100 5750 8450 5750
Connection ~ 8100 5750
Wire Wire Line
	8100 6000 8100 5750
Wire Wire Line
	7800 5750 8100 5750
Text Label 7650 5750 2    50   ~ 0
VCC
Connection ~ 8800 5750
Wire Wire Line
	10200 5750 10450 5750
Connection ~ 10200 5750
Wire Wire Line
	9050 5750 9500 5750
Connection ~ 9050 5750
Wire Wire Line
	8800 5750 9050 5750
$Comp
L power:PWR_FLAG #FLG0102
U 1 1 5DD55B39
P 7800 6450
F 0 "#FLG0102" H 7800 6525 50  0001 C CNN
F 1 "PWR_FLAG" H 7800 6600 50  0000 C CNN
F 2 "" H 7800 6450 50  0001 C CNN
F 3 "~" H 7800 6450 50  0001 C CNN
	1    7800 6450
	1    0    0    -1  
$EndComp
$Comp
L power:PWR_FLAG #FLG0101
U 1 1 5DD54D82
P 7800 5750
F 0 "#FLG0101" H 7800 5825 50  0001 C CNN
F 1 "PWR_FLAG" H 7800 5923 50  0000 C CNN
F 2 "" H 7800 5750 50  0001 C CNN
F 3 "~" H 7800 5750 50  0001 C CNN
	1    7800 5750
	1    0    0    -1  
$EndComp
Connection ~ 7200 3350
Wire Wire Line
	7200 3350 7000 3350
Connection ~ 7200 1450
Wire Wire Line
	7200 1450 7000 1450
Wire Wire Line
	3100 1250 3100 2300
Wire Wire Line
	1400 1250 3100 1250
Wire Wire Line
	2850 1350 2850 2300
Wire Wire Line
	1400 1350 2850 1350
Wire Wire Line
	2600 1450 2600 2300
Wire Wire Line
	1400 1450 2600 1450
Wire Wire Line
	3100 2700 3500 2700
Connection ~ 3100 2700
Wire Wire Line
	3100 2500 3100 2700
Wire Wire Line
	2850 2700 3100 2700
Connection ~ 2850 2700
Wire Wire Line
	2600 2700 2850 2700
Wire Wire Line
	2850 2500 2850 2700
Connection ~ 2600 2700
Wire Wire Line
	1950 2700 2250 2700
Wire Wire Line
	2600 2500 2600 2700
Connection ~ 1950 2700
Wire Wire Line
	1950 2500 1950 2600
Wire Wire Line
	1650 2700 1950 2700
Wire Wire Line
	1650 1650 1650 1950
Wire Wire Line
	1400 1650 1650 1650
$Comp
L Device:R_Small R4
U 1 1 5DCA562F
P 3100 2400
F 0 "R4" H 3159 2446 50  0000 L CNN
F 1 "75R" H 3159 2355 50  0000 L CNN
F 2 "Resistor_SMD:R_0805_2012Metric_Pad1.15x1.40mm_HandSolder" H 3100 2400 50  0001 C CNN
F 3 "~" H 3100 2400 50  0001 C CNN
	1    3100 2400
	1    0    0    -1  
$EndComp
$Comp
L Device:R_Small R3
U 1 1 5DCA4C29
P 2850 2400
F 0 "R3" H 2909 2446 50  0000 L CNN
F 1 "75R" H 2909 2355 50  0000 L CNN
F 2 "Resistor_SMD:R_0805_2012Metric_Pad1.15x1.40mm_HandSolder" H 2850 2400 50  0001 C CNN
F 3 "~" H 2850 2400 50  0001 C CNN
	1    2850 2400
	1    0    0    -1  
$EndComp
$Comp
L Device:R_Small R2
U 1 1 5DCA4199
P 2600 2400
F 0 "R2" H 2659 2446 50  0000 L CNN
F 1 "75R" H 2659 2355 50  0000 L CNN
F 2 "Resistor_SMD:R_0805_2012Metric_Pad1.15x1.40mm_HandSolder" H 2600 2400 50  0001 C CNN
F 3 "~" H 2600 2400 50  0001 C CNN
	1    2600 2400
	1    0    0    -1  
$EndComp
$Comp
L Device:C_Small C1
U 1 1 5DCE58C4
P 8100 6100
F 0 "C1" H 8192 6146 50  0000 L CNN
F 1 "10uF" H 8192 6055 50  0000 L CNN
F 2 "Capacitor_SMD:C_0805_2012Metric_Pad1.15x1.40mm_HandSolder" H 8100 6100 50  0001 C CNN
F 3 "~" H 8100 6100 50  0001 C CNN
	1    8100 6100
	1    0    0    -1  
$EndComp
$Comp
L Device:C_Small C2
U 1 1 5DCE4EBF
P 8450 6100
F 0 "C2" H 8542 6146 50  0000 L CNN
F 1 "100n" H 8542 6055 50  0000 L CNN
F 2 "Capacitor_SMD:C_0805_2012Metric_Pad1.15x1.40mm_HandSolder" H 8450 6100 50  0001 C CNN
F 3 "~" H 8450 6100 50  0001 C CNN
	1    8450 6100
	1    0    0    -1  
$EndComp
$Comp
L Device:C_Small C4
U 1 1 5DCABD73
P 9500 6100
F 0 "C4" H 9592 6146 50  0000 L CNN
F 1 "100n" H 9592 6055 50  0000 L CNN
F 2 "Capacitor_SMD:C_0805_2012Metric_Pad1.15x1.40mm_HandSolder" H 9500 6100 50  0001 C CNN
F 3 "~" H 9500 6100 50  0001 C CNN
	1    9500 6100
	1    0    0    -1  
$EndComp
$Comp
L Device:C_Small C3
U 1 1 5DCAB08B
P 9850 6100
F 0 "C3" H 9942 6146 50  0000 L CNN
F 1 "100n" H 9942 6055 50  0000 L CNN
F 2 "Capacitor_SMD:C_0805_2012Metric_Pad1.15x1.40mm_HandSolder" H 9850 6100 50  0001 C CNN
F 3 "~" H 9850 6100 50  0001 C CNN
	1    9850 6100
	1    0    0    -1  
$EndComp
Text Label 10550 4350 0    50   ~ 0
SYNC
Text Label 10550 3450 0    50   ~ 0
VSYNC
Text Label 9400 3350 2    50   ~ 0
ASYNC
Connection ~ 9750 3350
Wire Wire Line
	9750 3350 9400 3350
Wire Wire Line
	9750 3350 9950 3350
Wire Wire Line
	9750 4250 9750 3350
Wire Wire Line
	9950 4250 9750 4250
$Comp
L Connector_Generic:Conn_01x05 P3
U 1 1 5DC7AFF6
P 1200 1450
F 0 "P3" H 1280 1492 50  0000 L CNN
F 1 "Conn_01x05" H 1280 1401 50  0000 L CNN
F 2 "Connector_Molex:Molex_PicoBlade_53048-0510_1x05_P1.25mm_Horizontal" H 1200 1450 50  0001 C CNN
F 3 "~" H 1200 1450 50  0001 C CNN
	1    1200 1450
	-1   0    0    1   
$EndComp
Text Label 10750 5000 0    50   ~ 0
BLUE
Text Label 10750 5100 0    50   ~ 0
GREEN
Text Label 10750 5200 0    50   ~ 0
RED
Text Label 10750 5300 0    50   ~ 0
BRED
Text Label 10750 5400 0    50   ~ 0
GND
Text Label 10250 5400 2    50   ~ 0
BGREEN
Text Label 10250 5300 2    50   ~ 0
BBLUE
Text Label 10250 5200 2    50   ~ 0
SYNC
Text Label 10250 5100 2    50   ~ 0
VSYNC
$Comp
L Comparator:MAX9201 U2
U 5 1 5DB2CA12
P 9150 6100
F 0 "U2" H 9108 6146 50  0000 L CNN
F 1 "MAX9201" H 9108 6055 50  0000 L CNN
F 2 "Package_SO:SOIC-16_3.9x9.9mm_P1.27mm" H 9100 6250 50  0001 C CNN
F 3 "http://ww1.microchip.com/downloads/en/DeviceDoc/DS20002143E.pdf" H 9200 6300 50  0001 C CNN
	5    9150 6100
	1    0    0    -1  
$EndComp
$Comp
L Comparator:MAX9201 U3
U 5 1 5DB2BD87
P 10550 6100
F 0 "U3" H 10508 6146 50  0000 L CNN
F 1 "MAX9201" H 10508 6055 50  0000 L CNN
F 2 "Package_SO:SOIC-16_3.9x9.9mm_P1.27mm" H 10500 6250 50  0001 C CNN
F 3 "http://ww1.microchip.com/downloads/en/DeviceDoc/DS20002143E.pdf" H 10600 6300 50  0001 C CNN
	5    10550 6100
	1    0    0    -1  
$EndComp
$Comp
L Comparator:MAX9201 U3
U 4 1 5DB2AD59
P 10250 3450
F 0 "U3" H 10250 3817 50  0000 C CNN
F 1 "MAX9201" H 10250 3726 50  0000 C CNN
F 2 "Package_SO:SOIC-16_3.9x9.9mm_P1.27mm" H 10200 3600 50  0001 C CNN
F 3 "http://ww1.microchip.com/downloads/en/DeviceDoc/DS20002143E.pdf" H 10300 3650 50  0001 C CNN
	4    10250 3450
	1    0    0    -1  
$EndComp
$Comp
L Comparator:MAX9201 U3
U 3 1 5DB29FFF
P 10250 4350
F 0 "U3" H 10250 4717 50  0000 C CNN
F 1 "MAX9201" H 10250 4626 50  0000 C CNN
F 2 "Package_SO:SOIC-16_3.9x9.9mm_P1.27mm" H 10200 4500 50  0001 C CNN
F 3 "http://ww1.microchip.com/downloads/en/DeviceDoc/DS20002143E.pdf" H 10300 4550 50  0001 C CNN
	3    10250 4350
	1    0    0    -1  
$EndComp
$Comp
L Comparator:MAX9201 U3
U 2 1 5DB291EF
P 7750 3450
F 0 "U3" H 7750 3817 50  0000 C CNN
F 1 "MAX9201" H 7750 3726 50  0000 C CNN
F 2 "Package_SO:SOIC-16_3.9x9.9mm_P1.27mm" H 7700 3600 50  0001 C CNN
F 3 "http://ww1.microchip.com/downloads/en/DeviceDoc/DS20002143E.pdf" H 7800 3650 50  0001 C CNN
	2    7750 3450
	1    0    0    -1  
$EndComp
$Comp
L Comparator:MAX9201 U3
U 1 1 5DB28596
P 10250 1550
F 0 "U3" H 10250 1917 50  0000 C CNN
F 1 "MAX9201" H 10250 1826 50  0000 C CNN
F 2 "Package_SO:SOIC-16_3.9x9.9mm_P1.27mm" H 10200 1700 50  0001 C CNN
F 3 "http://ww1.microchip.com/downloads/en/DeviceDoc/DS20002143E.pdf" H 10300 1750 50  0001 C CNN
	1    10250 1550
	1    0    0    -1  
$EndComp
$Comp
L Comparator:MAX9201 U2
U 4 1 5DB2730F
P 10250 2450
F 0 "U2" H 10250 2817 50  0000 C CNN
F 1 "MAX9201" H 10250 2726 50  0000 C CNN
F 2 "Package_SO:SOIC-16_3.9x9.9mm_P1.27mm" H 10200 2600 50  0001 C CNN
F 3 "http://ww1.microchip.com/downloads/en/DeviceDoc/DS20002143E.pdf" H 10300 2650 50  0001 C CNN
	4    10250 2450
	1    0    0    -1  
$EndComp
$Comp
L Comparator:MAX9201 U2
U 3 1 5DB2652A
P 7750 4350
F 0 "U2" H 7750 4717 50  0000 C CNN
F 1 "MAX9201" H 7750 4626 50  0000 C CNN
F 2 "Package_SO:SOIC-16_3.9x9.9mm_P1.27mm" H 7700 4500 50  0001 C CNN
F 3 "http://ww1.microchip.com/downloads/en/DeviceDoc/DS20002143E.pdf" H 7800 4550 50  0001 C CNN
	3    7750 4350
	1    0    0    -1  
$EndComp
$Comp
L Comparator:MAX9201 U2
U 2 1 5DB258DB
P 7750 2450
F 0 "U2" H 7750 2817 50  0000 C CNN
F 1 "MAX9201" H 7750 2726 50  0000 C CNN
F 2 "Package_SO:SOIC-16_3.9x9.9mm_P1.27mm" H 7700 2600 50  0001 C CNN
F 3 "http://ww1.microchip.com/downloads/en/DeviceDoc/DS20002143E.pdf" H 7800 2650 50  0001 C CNN
	2    7750 2450
	1    0    0    -1  
$EndComp
$Comp
L Comparator:MAX9201 U2
U 1 1 5DB24975
P 7750 1550
F 0 "U2" H 7750 1917 50  0000 C CNN
F 1 "MAX9201" H 7750 1826 50  0000 C CNN
F 2 "Package_SO:SOIC-16_3.9x9.9mm_P1.27mm" H 7700 1700 50  0001 C CNN
F 3 "http://ww1.microchip.com/downloads/en/DeviceDoc/DS20002143E.pdf" H 7800 1750 50  0001 C CNN
	1    7750 1550
	1    0    0    -1  
$EndComp
Text Label 1050 5300 0    50   ~ 0
GPIO0_DAT
Text Label 1050 5200 0    50   ~ 0
GPIO1_CLK
Text Label 1050 5100 0    50   ~ 0
GPIO22_STB
Connection ~ 5800 4450
Wire Wire Line
	5800 4450 7450 4450
Wire Wire Line
	9250 2550 9950 2550
Wire Wire Line
	5800 2550 7450 2550
Wire Wire Line
	7450 3550 5550 3550
Wire Wire Line
	9250 1650 9950 1650
Wire Wire Line
	5550 1650 7450 1650
Wire Wire Line
	1000 5200 1850 5200
Text Label 7650 6450 2    50   ~ 0
GND
Text Label 2900 5900 3    50   ~ 0
GND
Text Label 10250 5000 2    50   ~ 0
VCC
Text Label 10550 2450 0    50   ~ 0
BBLUE
Text Label 10550 1550 0    50   ~ 0
BLUE
Text Label 8050 4350 0    50   ~ 0
BGREEN
Text Label 8050 3450 0    50   ~ 0
GREEN
Text Label 8050 2450 0    50   ~ 0
BRED
Text Label 8050 1550 0    50   ~ 0
RED
Text Label 9400 1450 2    50   ~ 0
ABLUE
Connection ~ 9700 1450
Wire Wire Line
	9400 1450 9700 1450
Text Label 7000 3350 2    50   ~ 0
AGREEN
Text Label 7000 1450 2    50   ~ 0
ARED
Wire Wire Line
	9700 1450 9950 1450
Wire Wire Line
	9700 2350 9700 1450
Wire Wire Line
	9950 2350 9700 2350
Wire Wire Line
	7200 3350 7450 3350
Wire Wire Line
	7200 4250 7200 3350
Wire Wire Line
	7450 4250 7200 4250
Wire Wire Line
	7200 1450 7450 1450
Wire Wire Line
	7200 2350 7200 1450
Wire Wire Line
	7450 2350 7200 2350
Text Label 3500 1550 0    50   ~ 0
ASYNC
Text Label 3500 1250 0    50   ~ 0
ABLUE
Text Label 3500 1350 0    50   ~ 0
AGREEN
Text Label 3500 1450 0    50   ~ 0
ARED
Text Label 3500 2700 0    50   ~ 0
GND
$Comp
L Connector_Generic:Conn_02x05_Odd_Even P1
U 1 1 5DB26BD3
P 10450 5200
F 0 "P1" H 10500 5617 50  0000 C CNN
F 1 "Conn_02x05_Odd_Even" H 10500 5526 50  0000 C CNN
F 2 "Connector_PinSocket_2.54mm:PinSocket_2x05_P2.54mm_Horizontal" H 10450 5200 50  0001 C CNN
F 3 "~" H 10450 5200 50  0001 C CNN
	1    10450 5200
	1    0    0    -1  
$EndComp
Wire Wire Line
	1950 2250 1950 2300
Wire Wire Line
	3500 5600 3400 5600
$Comp
L Jumper:SolderJumper_2_Open JP1
U 1 1 5DFC21F6
P 1950 2450
F 0 "JP1" V 1904 2518 50  0000 L CNN
F 1 "Bypass" H 1850 2300 50  0000 L CNN
F 2 "Jumper:SolderJumper-2_P1.3mm_Open_RoundedPad1.0x1.5mm" H 1950 2450 50  0001 C CNN
F 3 "~" H 1950 2450 50  0001 C CNN
	1    1950 2450
	0    1    1    0   
$EndComp
Wire Wire Line
	8500 3550 8500 5300
Wire Wire Line
	8500 5300 5550 5300
Connection ~ 3900 5300
Connection ~ 1950 2600
Wire Wire Line
	1950 2600 1950 2700
Wire Wire Line
	1750 1550 1950 1550
$Comp
L Device:R_Small R1
U 1 1 5DCA386F
P 1950 1900
F 0 "R1" H 2009 1946 50  0000 L CNN
F 1 "75R" H 2009 1855 50  0000 L CNN
F 2 "Resistor_SMD:R_0805_2012Metric_Pad1.15x1.40mm_HandSolder" H 1950 1900 50  0001 C CNN
F 3 "~" H 1950 1900 50  0001 C CNN
	1    1950 1900
	1    0    0    -1  
$EndComp
Wire Wire Line
	1950 1800 1950 1550
Connection ~ 1950 1550
Wire Wire Line
	1950 1550 2250 1550
Wire Wire Line
	1950 2000 1950 2250
Connection ~ 1950 2250
$Comp
L Device:R_Small R5
U 1 1 5E133363
P 2250 2150
F 0 "R5" H 2309 2196 50  0000 L CNN
F 1 "10K" H 2309 2105 50  0000 L CNN
F 2 "Resistor_SMD:R_0805_2012Metric_Pad1.15x1.40mm_HandSolder" H 2250 2150 50  0001 C CNN
F 3 "~" H 2250 2150 50  0001 C CNN
	1    2250 2150
	1    0    0    -1  
$EndComp
Wire Wire Line
	2250 2250 2250 2700
Connection ~ 2250 2700
Wire Wire Line
	2250 2700 2600 2700
Wire Wire Line
	2250 2050 2250 1550
Connection ~ 2250 1550
Wire Wire Line
	2250 1550 3500 1550
$Comp
L Analog_Switch:TS5A3166DBVR U5
U 1 1 5DD31595
P 1650 3700
F 0 "U5" H 850 3900 50  0000 C CNN
F 1 "TS5A3166DBVR" H 850 3800 50  0000 C CNN
F 2 "Package_TO_SOT_SMD:SOT-23-5_HandSoldering" H 1600 3550 50  0001 C CNN
F 3 " http://www.ti.com/lit/ds/symlink/ts5a3166.pdf" H 1650 3800 50  0001 C CNN
	1    1650 3700
	-1   0    0    -1  
$EndComp
Wire Wire Line
	7650 5750 7800 5750
Connection ~ 7800 5750
Wire Wire Line
	7800 6450 7650 6450
Connection ~ 7800 6450
Wire Wire Line
	1950 3700 1950 2700
Wire Wire Line
	1250 2250 1250 3700
Wire Wire Line
	1250 3700 1350 3700
Wire Wire Line
	1250 2250 1950 2250
Text Label 1550 4000 3    50   ~ 0
GND
$Comp
L DAC084S085:DAC084S085 UB1
U 1 1 5DD04B37
P 2900 6950
F 0 "UB1" H 2900 7515 50  0000 C CNN
F 1 "DAC084S085" H 2900 7424 50  0000 C CNN
F 2 "Package_SO:VSSOP-10_3x3mm_P0.5mm" H 2900 6950 50  0001 C CNN
F 3 "" H 2900 6950 50  0001 C CNN
	1    2900 6950
	1    0    0    -1  
$EndComp
Wire Wire Line
	2450 6650 2400 6650
Wire Wire Line
	2450 6800 2000 6800
Wire Wire Line
	2000 6800 2000 5100
Connection ~ 2000 5100
Wire Wire Line
	2000 5100 2400 5100
Wire Wire Line
	1000 5100 2000 5100
Wire Wire Line
	2450 6950 1850 6950
Wire Wire Line
	1850 6950 1850 5200
Connection ~ 1850 5200
Wire Wire Line
	1850 5200 2400 5200
Wire Wire Line
	2450 7100 1700 7100
Wire Wire Line
	1700 7100 1700 5300
Connection ~ 1700 5300
Wire Wire Line
	1700 5300 2400 5300
Wire Wire Line
	3400 6200 2400 6200
Wire Wire Line
	2400 6200 2400 5700
Connection ~ 2400 5700
Wire Wire Line
	3350 6800 3900 6800
Wire Wire Line
	3900 6800 3900 5300
Wire Wire Line
	3350 6950 4050 6950
Wire Wire Line
	4050 6950 4050 5400
Connection ~ 4050 5400
Wire Wire Line
	4050 5400 5800 5400
Wire Wire Line
	3350 7100 4200 7100
Wire Wire Line
	3350 7250 3500 7250
Wire Wire Line
	3500 7250 3500 5600
Wire Wire Line
	3350 6650 3400 6650
Wire Wire Line
	3400 6650 3400 6200
Text Label 2450 7250 2    50   ~ 0
GND
Text Label 2400 6650 2    50   ~ 0
VCC
$Comp
L Regulator_Linear:MCP1754S-3302xCB U4
U 1 1 5DDED86E
P 5450 6700
F 0 "U4" H 5450 6942 50  0000 C CNN
F 1 "MCP1754S-3302xCB" H 5450 6851 50  0000 C CNN
F 2 "Package_TO_SOT_SMD:SOT-23_Handsoldering" H 5450 6925 50  0001 C CNN
F 3 "http://ww1.microchip.com/downloads/en/DeviceDoc/20002276C.pdf" H 5450 6700 50  0001 C CNN
	1    5450 6700
	1    0    0    -1  
$EndComp
Wire Wire Line
	5950 7200 5450 7200
Connection ~ 5950 7200
Connection ~ 5450 7200
Wire Wire Line
	5450 7150 5450 7200
Wire Wire Line
	5450 7000 5450 7200
$Comp
L Jumper:SolderJumper_2_Open JP2
U 1 1 5DE1A29D
P 5450 6300
F 0 "JP2" H 5450 6505 50  0000 C CNN
F 1 "Bypass regulator" H 5450 6414 50  0000 C CNN
F 2 "Jumper:SolderJumper-2_P1.3mm_Open_RoundedPad1.0x1.5mm" H 5450 6300 50  0001 C CNN
F 3 "~" H 5450 6300 50  0001 C CNN
	1    5450 6300
	1    0    0    -1  
$EndComp
Wire Wire Line
	4950 6700 4950 6300
Wire Wire Line
	4950 6300 5300 6300
Connection ~ 4950 6700
Wire Wire Line
	4950 6700 5150 6700
Wire Wire Line
	5600 6300 5950 6300
Wire Wire Line
	5950 6300 5950 6700
Text Label 2150 3800 0    50   ~ 0
TERMINATE
Wire Wire Line
	5550 1650 5550 2000
Wire Wire Line
	5800 2550 5800 2950
Wire Wire Line
	9250 2000 5550 2000
Wire Wire Line
	9250 1650 9250 2000
Connection ~ 5550 2000
Wire Wire Line
	5550 2000 5550 3550
Wire Wire Line
	9250 2950 5800 2950
Wire Wire Line
	9250 2550 9250 2950
Connection ~ 5800 2950
Wire Wire Line
	5800 2950 5800 4450
NoConn ~ 1000 5000
Text Notes 9200 5200 0    50   ~ 0
NOTE: Connections\nreversed compared\nto RGB to HD pcb
Text Label 2600 3000 2    50   ~ 0
VANALOG
Text Label 3000 3000 0    50   ~ 0
VCC
Wire Wire Line
	2800 3150 2800 3300
Wire Wire Line
	2800 3300 1550 3300
Wire Wire Line
	1550 3300 1550 3400
Connection ~ 3500 5600
Wire Wire Line
	1950 3800 3500 3800
Wire Wire Line
	3500 3800 3500 5600
$Comp
L Jumper:SolderJumper_3_Bridged12 JP3
U 1 1 5DD334FB
P 2800 3000
F 0 "JP3" H 2800 3205 50  0000 C CNN
F 1 "SolderJumper_3_Bridged12" H 2800 3114 50  0000 C CNN
F 2 "Jumper:SolderJumper-3_P1.3mm_Bridged12_RoundedPad1.0x1.5mm" H 2800 3000 50  0001 C CNN
F 3 "~" H 2800 3000 50  0001 C CNN
	1    2800 3000
	1    0    0    -1  
$EndComp
Wire Wire Line
	5550 3550 5550 5300
Connection ~ 5550 3550
Connection ~ 5550 5300
Wire Wire Line
	5550 5300 3900 5300
NoConn ~ 10350 750 
NoConn ~ 9400 750 
Wire Wire Line
	3400 5500 4200 5500
Connection ~ 4200 5500
Wire Wire Line
	4200 5500 8850 5500
Wire Wire Line
	4200 7100 4200 5500
Wire Wire Line
	3100 1250 3500 1250
Connection ~ 3100 1250
Wire Wire Line
	2850 1350 3500 1350
Connection ~ 2850 1350
Wire Wire Line
	2600 1450 3500 1450
Connection ~ 2600 1450
$EndSCHEMATC
