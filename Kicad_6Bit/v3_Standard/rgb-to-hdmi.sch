EESchema Schematic File Version 4
LIBS:rgb-to-hdmi-cache
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
NoConn ~ -500 3250
$Comp
L Jumper:SolderJumper_2_Bridged JP5
U 1 1 5E5C6DC1
P 3250 2350
F 0 "JP5" H 3450 2100 50  0000 R CNN
F 1 "SolderJumper_2_Bridged" H 3850 2200 50  0000 R CNN
F 2 "Jumper:SolderJumper-2_P1.3mm_Bridged_RoundedPad1.0x1.5mm" H 3250 2350 50  0001 C CNN
F 3 "~" H 3250 2350 50  0001 C CNN
	1    3250 2350
	-1   0    0    1   
$EndComp
Text Label 3800 1100 0    60   ~ 0
X2
Text Label 5500 1000 2    60   ~ 0
X1
$Comp
L Device:R_Network05 RN1
U 1 1 5E59DAE8
P 3600 1300
F 0 "RN1" V 3183 1300 50  0000 C CNN
F 1 "4K7" V 3274 1300 50  0000 C CNN
F 2 "Resistor_THT:R_Array_SIP6" V 3975 1300 50  0001 C CNN
F 3 "http://www.vishay.com/docs/31509/csc.pdf" H 3600 1300 50  0001 C CNN
	1    3600 1300
	0    -1   1    0   
$EndComp
$Comp
L Device:R_Network05 RN2
U 1 1 5E59C7B5
P 5700 1200
F 0 "RN2" V 5283 1200 50  0000 C CNN
F 1 "4K7" V 5374 1200 50  0000 C CNN
F 2 "Resistor_THT:R_Array_SIP6" V 6075 1200 50  0001 C CNN
F 3 "http://www.vishay.com/docs/31509/csc.pdf" H 5700 1200 50  0001 C CNN
	1    5700 1200
	0    1    1    0   
$EndComp
Wire Wire Line
	3400 2350 3850 2350
Text Label 6650 5700 2    60   ~ 0
X1
Text Label 7150 5700 0    60   ~ 0
X2
$Comp
L Connector_Generic:Conn_02x01 J2
U 1 1 5E50F51D
P 6850 5700
F 0 "J2" H 6900 5917 50  0001 C CNN
F 1 "Conn_02x01" H 6900 5825 50  0000 C CNN
F 2 "Connector_PinHeader_2.54mm:PinHeader_2x01_P2.54mm_Horizontal" H 6850 5700 50  0001 C CNN
F 3 "~" H 6850 5700 50  0001 C CNN
	1    6850 5700
	1    0    0    -1  
$EndComp
Text Notes 1300 1700 0    60   ~ 0
P19\n
Text Notes 1300 1800 0    60   ~ 0
STB
Text Notes 1300 1900 0    60   ~ 0
CLK
Text Notes 1300 2000 0    60   ~ 0
DAT
Text Label 6450 4300 0    60   ~ 0
GND
Wire Wire Line
	6450 4050 6450 4300
Wire Wire Line
	6450 3850 6450 3650
$Comp
L Device:R_Small R6
U 1 1 5DCDA4CD
P 6450 3950
F 0 "R6" H 6509 3996 50  0000 L CNN
F 1 "10K" H 6509 3905 50  0000 L CNN
F 2 "Resistor_SMD:R_0805_2012Metric_Pad1.15x1.40mm_HandSolder" H 6450 3950 50  0001 C CNN
F 3 "~" H 6450 3950 50  0001 C CNN
	1    6450 3950
	1    0    0    -1  
$EndComp
Wire Wire Line
	5750 3650 6450 3650
Text Label 1850 1700 2    60   ~ 0
DETECT
Text Label 1850 1800 2    60   ~ 0
GPIO22
Text Label 1850 1900 2    60   ~ 0
GPIO1
Text Label 1850 2000 2    60   ~ 0
GPIO0
$Comp
L Connector_Generic:Conn_01x04 P5
U 1 1 5DCE6A21
P 2050 1900
F 0 "P5" H 2130 1892 50  0000 L CNN
F 1 "Conn_01x04" H 2130 1801 50  0000 L CNN
F 2 "Connector_PinSocket_2.54mm:PinSocket_1x04_P2.54mm_Vertical" H 2050 1900 50  0001 C CNN
F 3 "~" H 2050 1900 50  0001 C CNN
	1    2050 1900
	1    0    0    1   
$EndComp
Connection ~ 1650 5050
Wire Wire Line
	1650 5050 1350 5050
Wire Wire Line
	1650 5800 1350 5800
$Comp
L Device:CP1_Small C1
U 1 1 595B5ACB
P 1650 5350
F 0 "C1" H 1660 5420 50  0000 L CNN
F 1 "10uF" H 1660 5270 50  0000 L CNN
F 2 "Capacitor_SMD:C_0805_2012Metric_Pad1.15x1.40mm_HandSolder" H 1650 5350 50  0001 C CNN
F 3 "" H 1650 5350 50  0000 C CNN
	1    1650 5350
	1    0    0    -1  
$EndComp
$Comp
L Device:C_Small C2
U 1 1 595A3251
P 2850 5350
F 0 "C2" H 2860 5420 50  0000 L CNN
F 1 "100nF" H 2860 5270 50  0000 L CNN
F 2 "Capacitor_SMD:C_0805_2012Metric_Pad1.15x1.40mm_HandSolder" H 2850 5350 50  0001 C CNN
F 3 "" H 2850 5350 50  0000 C CNN
	1    2850 5350
	1    0    0    -1  
$EndComp
Text Label 8400 3650 2    60   ~ 0
TDI
Text Label 8700 3650 0    60   ~ 0
GPIO0
$Comp
L Jumper:SolderJumper_2_Bridged JP1
U 1 1 5DAA5F8A
P 8550 3650
F 0 "JP1" H 8550 3855 50  0000 C CNN
F 1 "SolderJumper_2_Bridged" H 8550 3764 50  0000 C CNN
F 2 "Jumper:SolderJumper-2_P1.3mm_Bridged_RoundedPad1.0x1.5mm" H 8550 3650 50  0001 C CNN
F 3 "~" H 8550 3650 50  0001 C CNN
	1    8550 3650
	-1   0    0    -1  
$EndComp
$Comp
L Jumper:SolderJumper_2_Bridged JP2
U 1 1 5DAA5395
P 8550 4000
F 0 "JP2" H 8550 4205 50  0000 C CNN
F 1 "SolderJumper_2_Bridged" H 8550 4114 50  0000 C CNN
F 2 "Jumper:SolderJumper-2_P1.3mm_Bridged_RoundedPad1.0x1.5mm" H 8550 4000 50  0001 C CNN
F 3 "~" H 8550 4000 50  0001 C CNN
	1    8550 4000
	-1   0    0    -1  
$EndComp
$Comp
L Jumper:SolderJumper_2_Bridged JP3
U 1 1 5DAA709E
P 8550 4350
F 0 "JP3" H 8550 4555 50  0000 C CNN
F 1 "SolderJumper_2_Bridged" H 8550 4464 50  0000 C CNN
F 2 "Jumper:SolderJumper-2_P1.3mm_Bridged_RoundedPad1.0x1.5mm" H 8550 4350 50  0001 C CNN
F 3 "~" H 8550 4350 50  0001 C CNN
	1    8550 4350
	-1   0    0    -1  
$EndComp
$Comp
L Jumper:SolderJumper_2_Bridged JP4
U 1 1 5DAA6850
P 8550 4700
F 0 "JP4" H 8550 4905 50  0000 C CNN
F 1 "SolderJumper_2_Bridged" H 8550 4814 50  0000 C CNN
F 2 "Jumper:SolderJumper-2_P1.3mm_Bridged_RoundedPad1.0x1.5mm" H 8550 4700 50  0001 C CNN
F 3 "~" H 8550 4700 50  0001 C CNN
	1    8550 4700
	-1   0    0    -1  
$EndComp
Wire Wire Line
	3650 5650 3650 5800
Wire Wire Line
	3650 5200 3650 5350
Wire Wire Line
	3650 5050 3850 5050
Wire Wire Line
	3650 5800 3850 5800
Wire Wire Line
	2300 5050 2850 5050
Wire Wire Line
	2850 5050 3300 5050
Wire Wire Line
	3300 5050 3650 5050
Wire Wire Line
	3300 5800 3650 5800
Wire Wire Line
	2850 5800 3300 5800
Wire Wire Line
	2300 5800 2850 5800
Wire Wire Line
	10050 3900 10200 3900
Wire Wire Line
	10050 4500 10200 4500
Wire Wire Line
	10050 5100 10200 5100
Wire Wire Line
	9350 4500 9350 5100
Wire Wire Line
	9350 5100 9350 5300
Text Label 8700 4350 0    60   ~ 0
GPIO24
Text Label 8700 4700 0    60   ~ 0
GPIO1
Text Label 8400 4350 2    60   ~ 0
TDO
Text Label 8400 4700 2    60   ~ 0
TMS
Text Label 8400 4000 2    60   ~ 0
TCK
Text Label 8700 4000 0    60   ~ 0
GPIO20
Text Label 5750 5800 0    60   ~ 0
TDO
Text Label 5750 5650 0    60   ~ 0
TMS
Text Label 5750 5500 0    60   ~ 0
TDI
Text Label 5750 5350 0    60   ~ 0
TCK
Text Label 8500 5750 2    60   ~ 0
TMS
Text Label 8500 5650 2    60   ~ 0
TDI
Text Label 8500 5550 2    60   ~ 0
TDO
Text Label 8500 5450 2    60   ~ 0
TCK
Text Label 8500 5350 2    60   ~ 0
GND
Text Label 8500 5250 2    60   ~ 0
3V3
$Comp
L rgb-to-hdmi-rescue:Conn_01x06 P3
U 1 1 5DA9B6A9
P 8700 5450
F 0 "P3" H 8700 5750 50  0000 C CNN
F 1 "Conn_01x06" H 8700 5050 50  0000 C CNN
F 2 "Connector_PinHeader_2.54mm:PinHeader_1x06_P2.54mm_Vertical" H 8700 5450 50  0001 C CNN
F 3 "" H 8700 5450 50  0001 C CNN
	1    8700 5450
	1    0    0    -1  
$EndComp
Text Label 3400 1100 2    60   ~ 0
GND
Text Label 5900 1000 0    60   ~ 0
GND
Wire Wire Line
	4350 1500 3800 1500
Wire Wire Line
	4850 1100 5500 1100
$Comp
L rgb-to-hdmi-rescue:Conn_02x05_Odd_Even P2
U 1 1 5DAA3151
P 4550 1300
F 0 "P2" H 4600 1600 50  0000 C CNN
F 1 "Conn_02x05_Odd_Even" H 4600 1000 50  0000 C CNN
F 2 "Connector_PinHeader_2.54mm:PinHeader_2x05_P2.54mm_Horizontal" H 4550 1300 50  0001 C CNN
F 3 "" H 4550 1300 50  0001 C CNN
	1    4550 1300
	1    0    0    -1  
$EndComp
Wire Wire Line
	1650 5800 2300 5800
Wire Wire Line
	4850 1400 5500 1400
Wire Wire Line
	4850 1300 5500 1300
Wire Wire Line
	4850 1200 5500 1200
Wire Wire Line
	4350 1400 3800 1400
Wire Wire Line
	4350 1300 3800 1300
Wire Wire Line
	4350 1200 3800 1200
Wire Wire Line
	3850 5500 3650 5500
Wire Wire Line
	3650 5500 3650 5650
Wire Wire Line
	3650 5650 3850 5650
Connection ~ 3650 5650
Wire Wire Line
	1650 5050 2300 5050
Wire Wire Line
	3650 5050 3650 5200
Wire Wire Line
	3650 5350 3850 5350
Wire Wire Line
	3850 5200 3650 5200
Connection ~ 3650 5200
Connection ~ 3650 5050
Connection ~ 3650 5800
Connection ~ 2300 5050
Wire Wire Line
	2850 5050 2850 5250
Connection ~ 2850 5050
Wire Wire Line
	3300 5050 3300 5250
Connection ~ 3300 5050
Wire Wire Line
	3300 5800 3300 5450
Connection ~ 3300 5800
Wire Wire Line
	2850 5800 2850 5450
Connection ~ 2850 5800
Connection ~ 2300 5800
Wire Wire Line
	10250 5750 9950 5750
Wire Wire Line
	9950 5850 10250 5850
Wire Wire Line
	2300 5800 2300 5450
Wire Wire Line
	2300 5050 2300 5250
Wire Wire Line
	1650 5050 1650 5250
Wire Wire Line
	1650 5800 1650 5450
Wire Wire Line
	9950 3900 10050 3900
Wire Wire Line
	10050 3900 10050 3800
Wire Wire Line
	10050 3600 10050 3500
Connection ~ 10050 3900
Wire Wire Line
	9950 5100 10050 5100
Wire Wire Line
	10050 5100 10050 5000
Wire Wire Line
	9950 4500 10050 4500
Wire Wire Line
	10050 4500 10050 4400
Connection ~ 10050 4500
Connection ~ 10050 5100
Wire Wire Line
	10050 4200 10050 4100
Wire Wire Line
	10050 4800 10050 4700
Wire Wire Line
	9350 3900 9350 4500
Connection ~ 9350 4500
Connection ~ 9350 5100
Wire Wire Line
	9950 5850 9950 6000
Wire Wire Line
	9950 6000 9350 6000
Wire Wire Line
	9350 6000 9350 5750
Connection ~ 1650 5800
Wire Wire Line
	2300 3150 2300 3350
Wire Wire Line
	2000 3150 2000 3350
Wire Wire Line
	2000 3650 2000 3550
Wire Wire Line
	2000 2850 2000 2750
Wire Wire Line
	2000 2750 1850 2750
Wire Wire Line
	2300 2850 2300 2600
Wire Wire Line
	2300 2600 1850 2600
Wire Wire Line
	2300 3550 2300 3650
Wire Wire Line
	2300 3650 2000 3650
Text Label 3850 2500 2    60   ~ 0
VSYNC
Text Label 4850 1500 0    60   ~ 0
VCC
Text Label 4850 1400 0    60   ~ 0
VSYNC
Text Label 4850 1300 0    60   ~ 0
SYNC
Text Label 4850 1200 0    60   ~ 0
BBLUE
Text Label 4850 1100 0    60   ~ 0
BGREEN
Text Label 4350 1500 2    60   ~ 0
BLUE
Text Label 4350 1400 2    60   ~ 0
GREEN
Text Label 4350 1300 2    60   ~ 0
RED
Text Label 4350 1200 2    60   ~ 0
BRED
Text Label 4350 1100 2    60   ~ 0
GND
Text Label 3850 4850 2    60   ~ 0
X2
Text Label 2250 3650 2    60   ~ 0
GND
Text Label 1850 2600 2    60   ~ 0
GPIO25
Text Label 3850 3250 2    60   ~ 0
GPIO6
Text Label 3850 3100 2    60   ~ 0
GPIO13
Text Label 3850 2650 2    60   ~ 0
GPIO25
Text Label 3850 2350 2    60   ~ 0
X3
Text Label 10200 5100 0    60   ~ 0
GPIO19
Text Label 10200 4500 0    60   ~ 0
GPIO26
Text Label 1850 2750 2    60   ~ 0
GPIO27
$Comp
L Device:R_Small R4
U 1 1 5B15A7CF
P 2000 3450
F 0 "R4" H 2030 3470 50  0000 L CNN
F 1 "1K" H 2030 3410 50  0000 L CNN
F 2 "Resistor_SMD:R_0805_2012Metric_Pad1.15x1.40mm_HandSolder" H 2000 3450 50  0001 C CNN
F 3 "" H 2000 3450 50  0000 C CNN
	1    2000 3450
	1    0    0    -1  
$EndComp
$Comp
L Device:LED D1
U 1 1 5B15A735
P 2000 3000
F 0 "D1" H 2000 3100 50  0000 C CNN
F 1 "LED" H 2000 2900 50  0000 C CNN
F 2 "LED_THT:LED_D1.8mm_W3.3mm_H2.4mm" H 2000 3000 50  0001 C CNN
F 3 "" H 2000 3000 50  0001 C CNN
	1    2000 3000
	0    -1   -1   0   
$EndComp
$Comp
L Device:LED D2
U 1 1 5B15A088
P 2300 3000
F 0 "D2" H 2300 3100 50  0000 C CNN
F 1 "LED" H 2300 2900 50  0000 C CNN
F 2 "LED_THT:LED_D1.8mm_W3.3mm_H2.4mm" H 2300 3000 50  0001 C CNN
F 3 "" H 2300 3000 50  0001 C CNN
	1    2300 3000
	0    -1   -1   0   
$EndComp
$Comp
L Device:R_Small R5
U 1 1 5B159FA8
P 2300 3450
F 0 "R5" H 2330 3470 50  0000 L CNN
F 1 "1K" H 2330 3410 50  0000 L CNN
F 2 "Resistor_SMD:R_0805_2012Metric_Pad1.15x1.40mm_HandSolder" H 2300 3450 50  0001 C CNN
F 3 "" H 2300 3450 50  0000 C CNN
	1    2300 3450
	1    0    0    -1  
$EndComp
$Comp
L power:PWR_FLAG #FLG02
U 1 1 5B15303A
P 1650 5800
F 0 "#FLG02" H 1650 5875 50  0001 C CNN
F 1 "PWR_FLAG" H 1650 5950 50  0000 C CNN
F 2 "" H 1650 5800 50  0001 C CNN
F 3 "" H 1650 5800 50  0001 C CNN
	1    1650 5800
	-1   0    0    1   
$EndComp
$Comp
L power:PWR_FLAG #FLG01
U 1 1 5B152FF8
P 2300 5050
F 0 "#FLG01" H 2300 5125 50  0001 C CNN
F 1 "PWR_FLAG" H 2300 5200 50  0000 C CNN
F 2 "" H 2300 5050 50  0001 C CNN
F 3 "" H 2300 5050 50  0001 C CNN
	1    2300 5050
	1    0    0    -1  
$EndComp
Text Label 1350 5050 2    60   ~ 0
3V3
Text Label 1350 5800 2    60   ~ 0
GND
Text Label 3850 4700 2    60   ~ 0
BBLUE
Text Label 3850 4550 2    60   ~ 0
BGREEN
Text Label 3850 4400 2    60   ~ 0
BRED
Text Label 5750 3250 0    60   ~ 0
GPIO24
Text Label 5750 3650 0    60   ~ 0
DETECT
Text Label 5750 3800 0    60   ~ 0
GPIO23
Text Label 5750 3950 0    60   ~ 0
X4
Text Label 3850 2200 2    60   ~ 0
X1
$Comp
L rgb-to-hdmi-rescue:SW_PUSH SW4
U 1 1 595B7AD4
P 9650 5750
F 0 "SW4" H 9800 5860 50  0000 C CNN
F 1 "SW_PUSH" H 9650 5670 50  0000 C CNN
F 2 "Button_Switch_THT:SW_PUSH_6mm_H5mm" H 9650 5750 50  0001 C CNN
F 3 "" H 9650 5750 50  0000 C CNN
	1    9650 5750
	1    0    0    -1  
$EndComp
Text Label 9350 5300 2    60   ~ 0
GND
Text Label 10050 4700 0    60   ~ 0
3V3
Text Label 10050 4100 0    60   ~ 0
3V3
$Comp
L Device:R_Small R3
U 1 1 595B74A8
P 10050 4900
F 0 "R3" H 10080 4920 50  0000 L CNN
F 1 "1K" H 10080 4860 50  0000 L CNN
F 2 "Resistor_SMD:R_0805_2012Metric_Pad1.15x1.40mm_HandSolder" H 10050 4900 50  0001 C CNN
F 3 "" H 10050 4900 50  0000 C CNN
	1    10050 4900
	1    0    0    -1  
$EndComp
$Comp
L Device:R_Small R2
U 1 1 595B744A
P 10050 4300
F 0 "R2" H 10080 4320 50  0000 L CNN
F 1 "1K" H 10080 4260 50  0000 L CNN
F 2 "Resistor_SMD:R_0805_2012Metric_Pad1.15x1.40mm_HandSolder" H 10050 4300 50  0001 C CNN
F 3 "" H 10050 4300 50  0000 C CNN
	1    10050 4300
	1    0    0    -1  
$EndComp
$Comp
L rgb-to-hdmi-rescue:SW_PUSH SW3
U 1 1 595B6785
P 9650 5100
F 0 "SW3" H 9800 5210 50  0000 C CNN
F 1 "SW_PUSH" H 9650 5020 50  0000 C CNN
F 2 "Button_Switch_THT:SW_PUSH_6mm_H5mm" H 9650 5100 50  0001 C CNN
F 3 "" H 9650 5100 50  0000 C CNN
	1    9650 5100
	1    0    0    -1  
$EndComp
$Comp
L rgb-to-hdmi-rescue:SW_PUSH SW2
U 1 1 595B6852
P 9650 4500
F 0 "SW2" H 9800 4610 50  0000 C CNN
F 1 "SW_PUSH" H 9650 4420 50  0000 C CNN
F 2 "Button_Switch_THT:SW_PUSH_6mm_H5mm" H 9650 4500 50  0001 C CNN
F 3 "" H 9650 4500 50  0000 C CNN
	1    9650 4500
	1    0    0    -1  
$EndComp
Text Label 10200 3900 0    60   ~ 0
GPIO16
Text Label 10050 3500 0    60   ~ 0
3V3
$Comp
L Device:R_Small R1
U 1 1 595B6CB3
P 10050 3700
F 0 "R1" H 10080 3720 50  0000 L CNN
F 1 "1K" H 10080 3660 50  0000 L CNN
F 2 "Resistor_SMD:R_0805_2012Metric_Pad1.15x1.40mm_HandSolder" H 10050 3700 50  0001 C CNN
F 3 "" H 10050 3700 50  0000 C CNN
	1    10050 3700
	1    0    0    -1  
$EndComp
$Comp
L rgb-to-hdmi-rescue:SW_PUSH SW1
U 1 1 595B68BB
P 9650 3900
F 0 "SW1" H 9800 4010 50  0000 C CNN
F 1 "SW_PUSH" H 9650 3820 50  0000 C CNN
F 2 "Button_Switch_THT:SW_PUSH_6mm_H5mm" H 9650 3900 50  0001 C CNN
F 3 "" H 9650 3900 50  0000 C CNN
	1    9650 3900
	1    0    0    -1  
$EndComp
Text Label 3850 4250 2    60   ~ 0
GPIO18
Text Label 3850 4100 2    60   ~ 0
RED
Text Label 3850 3950 2    60   ~ 0
GREEN
Text Label 3850 3800 2    60   ~ 0
BLUE
Text Label 3850 2950 2    60   ~ 0
GPIO20
Text Label 3850 2800 2    60   ~ 0
GPIO21
Text Label 3850 3400 2    60   ~ 0
GPIO12
Text Label 5750 2200 0    60   ~ 0
GPIO5
Text Label 5750 2350 0    60   ~ 0
GPIO1
Text Label 5750 2500 0    60   ~ 0
GPIO0
Text Label 5750 2650 0    60   ~ 0
GPIO7
Text Label 5750 2800 0    60   ~ 0
GPIO8
Text Label 5750 2950 0    60   ~ 0
GPIO11
Text Label 5750 3100 0    60   ~ 0
GPIO9
Text Label 5750 3400 0    60   ~ 0
GPIO10
Text Label 5750 4100 0    60   ~ 0
GPIO17
Text Label 5750 4250 0    60   ~ 0
SYNC
Text Label 5750 4400 0    60   ~ 0
GPIO4
Text Label 5750 4550 0    60   ~ 0
GPIO3
Text Label 3850 3650 2    60   ~ 0
GPIO2
$Comp
L Device:C_Small C3
U 1 1 595A31EE
P 3300 5350
F 0 "C3" H 3310 5420 50  0000 L CNN
F 1 "100nF" H 3310 5270 50  0000 L CNN
F 2 "Capacitor_SMD:C_0805_2012Metric_Pad1.15x1.40mm_HandSolder" H 3300 5350 50  0001 C CNN
F 3 "" H 3300 5350 50  0000 C CNN
	1    3300 5350
	1    0    0    -1  
$EndComp
$Comp
L Device:C_Small C4
U 1 1 595A3174
P 2300 5350
F 0 "C4" H 2310 5420 50  0000 L CNN
F 1 "100nF" H 2310 5270 50  0000 L CNN
F 2 "Capacitor_SMD:C_0805_2012Metric_Pad1.15x1.40mm_HandSolder" H 2300 5350 50  0001 C CNN
F 3 "" H 2300 5350 50  0000 C CNN
	1    2300 5350
	1    0    0    -1  
$EndComp
$Comp
L rgb-to-hdmi-rescue:CONN_01X02 P4
U 1 1 595A13C3
P 10450 5800
F 0 "P4" H 10450 5950 50  0000 C CNN
F 1 "CONN_01X02" V 10550 5800 50  0000 C CNN
F 2 "Connector_PinSocket_2.54mm:PinSocket_1x02_P2.54mm_Vertical_Custom" H 10450 5800 50  0001 C CNN
F 3 "" H 10450 5800 50  0000 C CNN
	1    10450 5800
	1    0    0    -1  
$EndComp
$Comp
L xc9572xl:XC9572XL-VQFP44 U1
U 1 1 595A11EC
P 4800 3500
F 0 "U1" H 4800 1300 60  0000 C CNN
F 1 "XC9572XL-VQFP44" H 4800 1450 60  0000 C CNN
F 2 "Package_QFP:LQFP-44_10x10mm_P0.8mm" H 4800 3500 60  0001 C CNN
F 3 "" H 4800 3500 60  0001 C CNN
	1    4800 3500
	1    0    0    -1  
$EndComp
$Comp
L Jumper:SolderJumper_2_Bridged JP6
U 1 1 5E522250
P 6700 4650
F 0 "JP6" H 6600 4350 50  0000 C CNN
F 1 "SolderJumper_2_Bridged" H 6550 4450 50  0000 C CNN
F 2 "Jumper:SolderJumper-2_P1.3mm_Bridged_RoundedPad1.0x1.5mm" H 6700 4650 50  0001 C CNN
F 3 "~" H 6700 4650 50  0001 C CNN
	1    6700 4650
	1    0    0    -1  
$EndComp
Wire Wire Line
	6300 4650 6550 4650
Wire Wire Line
	5750 3950 6300 3950
Wire Wire Line
	6300 3950 6300 4650
Text Label 3100 2350 2    60   ~ 0
GPIO26
Text Label 6850 4650 0    60   ~ 0
GPIO19
NoConn ~ 9400 1450
NoConn ~ 9400 1350
Wire Wire Line
	9800 1050 9800 1150
Wire Wire Line
	9400 1050 9800 1050
Wire Wire Line
	9800 1000 9800 1050
Wire Wire Line
	9800 1150 9400 1150
Connection ~ 9800 1050
Text Label 8900 2350 2    60   ~ 0
GPIO0
Text Label 9400 2350 0    60   ~ 0
GPIO1
Text Label 9400 2050 0    60   ~ 0
GPIO25
$Comp
L power:PWR_FLAG #FLG0101
U 1 1 5E56D45A
P 9800 1000
F 0 "#FLG0101" H 9800 1075 50  0001 C CNN
F 1 "PWR_FLAG" H 9800 1150 50  0000 C CNN
F 2 "" H 9800 1000 50  0001 C CNN
F 3 "" H 9800 1000 50  0001 C CNN
	1    9800 1000
	1    0    0    -1  
$EndComp
Text Label 8900 1850 2    60   ~ 0
3V3
Text Label 8900 1050 2    60   ~ 0
3V3
Text Label 9400 1850 0    60   ~ 0
GPIO24
Text Label 9400 1750 0    60   ~ 0
GPIO23
Text Label 8900 1750 2    60   ~ 0
GPIO22
Text Label 8900 1650 2    60   ~ 0
GPIO27
Text Label 9400 2950 0    60   ~ 0
GPIO21
Text Label 9400 2850 0    60   ~ 0
GPIO20
Text Label 9400 2750 0    60   ~ 0
GPIO16
Text Label 9400 2650 0    60   ~ 0
GND
Text Label 9400 2550 0    60   ~ 0
GPIO12
Text Label 9400 2450 0    60   ~ 0
GND
Text Label 9400 2250 0    60   ~ 0
GPIO7
Text Label 9400 2150 0    60   ~ 0
GPIO8
Text Label 9400 1650 0    60   ~ 0
GND
Text Label 9400 1950 0    60   ~ 0
GND
Text Label 9400 1550 0    60   ~ 0
GPIO18
Text Label 9400 1450 0    60   ~ 0
RxD
Text Label 9400 1350 0    60   ~ 0
TxD
Text Label 9400 1250 0    60   ~ 0
GND
Text Label 9400 1150 0    60   ~ 0
VCC
Text Label 9400 1050 0    60   ~ 0
VCC
Text Label 8900 2950 2    60   ~ 0
GND
Text Label 8900 2850 2    60   ~ 0
GPIO26
Text Label 8900 2750 2    60   ~ 0
GPIO19
Text Label 8900 2650 2    60   ~ 0
GPIO13
Text Label 8900 2550 2    60   ~ 0
GPIO6
Text Label 8900 2450 2    60   ~ 0
GPIO5
Text Label 8900 2250 2    60   ~ 0
GND
Text Label 8900 2150 2    60   ~ 0
GPIO11
Text Label 8900 2050 2    60   ~ 0
GPIO9
Text Label 8900 1950 2    60   ~ 0
GPIO10
Text Label 8900 1550 2    60   ~ 0
GPIO17
Text Label 8900 1450 2    60   ~ 0
GND
Text Label 8900 1350 2    60   ~ 0
GPIO4
Text Label 8900 1250 2    60   ~ 0
GPIO3
Text Label 8900 1150 2    60   ~ 0
GPIO2
$Comp
L rgb-to-hdmi-rescue:CONN_02X20 P1
U 1 1 5E56D485
P 9150 2000
F 0 "P1" H 9150 3050 50  0000 C CNN
F 1 "CONN_02X20" V 9150 2000 50  0000 C CNN
F 2 "Connector_PinSocket_2.54mm:PinSocket_2x20_P2.54mm_Vertical_Custom" H 9150 1050 50  0001 C CNN
F 3 "" H 9150 1050 50  0000 C CNN
	1    9150 2000
	1    0    0    -1  
$EndComp
$EndSCHEMATC
