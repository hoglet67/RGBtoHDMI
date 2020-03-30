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
	10250 5750 9950 5750
Wire Wire Line
	9950 5850 10250 5850
Wire Wire Line
	9950 5850 9950 6000
Wire Wire Line
	9950 6000 9350 6000
Wire Wire Line
	9350 6000 9350 5750
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
NoConn ~ 9400 1450
NoConn ~ 9400 1350
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
$EndSCHEMATC
