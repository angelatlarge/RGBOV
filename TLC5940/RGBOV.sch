EESchema Schematic File Version 2  date 2/14/2013 12:11:03 PM
LIBS:kirill
LIBS:power
LIBS:device
LIBS:transistors
LIBS:conn
LIBS:linear
LIBS:regul
LIBS:74xx
LIBS:cmos4000
LIBS:adc-dac
LIBS:memory
LIBS:xilinx
LIBS:special
LIBS:microcontrollers
LIBS:dsp
LIBS:microchip
LIBS:analog_switches
LIBS:motorola
LIBS:texas
LIBS:intel
LIBS:audio
LIBS:interface
LIBS:digital-audio
LIBS:philips
LIBS:display
LIBS:cypress
LIBS:siliconi
LIBS:opto
LIBS:atmel
LIBS:contrib
LIBS:valves
LIBS:atmega16
LIBS:stepper drivers
LIBS:relays
LIBS:RGBOV-cache
EELAYER 25  0
EELAYER END
$Descr User 11000 17000
encoding utf-8
Sheet 1 1
Title ""
Date "14 feb 2013"
Rev ""
Comp ""
Comment1 ""
Comment2 ""
Comment3 ""
Comment4 ""
$EndDescr
Wire Notes Line
	4000 1900 4000 4150
Wire Notes Line
	4000 4150 4150 4150
Connection ~ 6300 3400
Connection ~ 6300 3500
Connection ~ 6300 3100
Connection ~ 6300 3700
Connection ~ 6300 3900
Connection ~ 6300 4100
Wire Notes Line
	6250 2850 6450 2850
Wire Notes Line
	6250 4050 6450 4050
Wire Notes Line
	6250 3450 6450 3450
Wire Wire Line
	4200 4150 4200 5000
Wire Wire Line
	4200 5000 5250 5000
Wire Wire Line
	2550 3400 2950 3400
Wire Wire Line
	2950 3400 2950 3950
Wire Wire Line
	2950 3950 4550 3950
Wire Wire Line
	4550 3450 3650 3450
Wire Wire Line
	3650 3450 3650 5000
Wire Wire Line
	3650 5000 2550 5000
Wire Wire Line
	4550 3650 4100 3650
Wire Wire Line
	4100 3650 4100 3300
Wire Wire Line
	4100 3300 2550 3300
Wire Wire Line
	2900 6200 2900 4600
Wire Wire Line
	2900 4600 2550 4600
Wire Wire Line
	650  3000 650  3300
Connection ~ 3900 7050
Wire Wire Line
	3400 7050 3900 7050
Wire Wire Line
	2550 4800 3900 4800
Wire Wire Line
	3900 4800 3900 8900
Wire Wire Line
	650  5300 650  5200
Wire Wire Line
	2550 3000 3250 3000
Wire Wire Line
	3250 3000 3250 3750
Wire Wire Line
	3250 3750 4550 3750
Wire Wire Line
	2550 4700 2800 4700
Wire Wire Line
	2800 4700 2800 6200
Wire Wire Line
	4550 3250 3200 3250
Wire Wire Line
	3200 3250 3200 3850
Wire Wire Line
	3200 3850 2550 3850
Wire Wire Line
	2550 5300 3700 5300
Wire Wire Line
	3700 5300 3700 3350
Wire Wire Line
	3700 3350 4550 3350
Wire Wire Line
	2550 3500 2900 3500
Wire Wire Line
	2900 3500 2900 4350
Wire Wire Line
	2900 4350 4550 4350
Wire Notes Line
	6250 3150 6450 3150
Wire Notes Line
	6250 3750 6450 3750
Wire Notes Line
	6250 4350 6450 4350
Wire Notes Line
	6450 4350 6450 2850
Connection ~ 6300 4200
Connection ~ 6300 4000
Connection ~ 6300 3800
Connection ~ 6300 3600
Connection ~ 6300 3000
Connection ~ 6300 3200
Connection ~ 6300 3300
Wire Wire Line
	6300 2550 6300 4300
Connection ~ 6300 2900
Text Notes 3400 1300 0    60   ~ 0
IREFpin  must be connected to GND\nthrough an IREF resistor\nThe max channel I of TLC5940 \n= V-IREF/R-IREF * 31.5. \nV-IREF is usually 1.24V, \nso R-IREF=2k2 yields \n17mA channel I
NoConn ~ 5900 4400
NoConn ~ 5900 4700
NoConn ~ 5900 4600
$Comp
L +5V #PWR?
U 1 1 511C6A4D
P 6300 2550
F 0 "#PWR?" H 6300 2640 20  0001 C CNN
F 1 "+5V" H 6300 2640 30  0000 C CNN
	1    6300 2550
	1    0    0    -1  
$EndComp
Text Notes 6350 4150 0    60   ~ 0
R
Text Notes 6350 4250 0    60   ~ 0
G
Text Notes 6350 4350 0    60   ~ 0
B
$Comp
L LED D?
U 1 1 511C69DC
P 6100 4100
F 0 "D?" H 6100 4200 50  0000 C CNN
F 1 "LED" H 6100 4000 50  0000 C CNN
	1    6100 4100
	-1   0    0    1   
$EndComp
$Comp
L LED D?
U 1 1 511C69DB
P 6100 4200
F 0 "D?" H 6100 4300 50  0000 C CNN
F 1 "LED" H 6100 4100 50  0000 C CNN
	1    6100 4200
	-1   0    0    1   
$EndComp
$Comp
L LED D?
U 1 1 511C69DA
P 6100 4300
F 0 "D?" H 6100 4400 50  0000 C CNN
F 1 "LED" H 6100 4200 50  0000 C CNN
	1    6100 4300
	-1   0    0    1   
$EndComp
$Comp
L LED D?
U 1 1 511C69D6
P 6100 3500
F 0 "D?" H 6100 3600 50  0000 C CNN
F 1 "LED" H 6100 3400 50  0000 C CNN
	1    6100 3500
	-1   0    0    1   
$EndComp
Text Notes 6350 3550 0    60   ~ 0
R
Text Notes 6350 3650 0    60   ~ 0
G
Text Notes 6350 3750 0    60   ~ 0
B
$Comp
L LED D?
U 1 1 511C69D5
P 6100 3600
F 0 "D?" H 6100 3700 50  0000 C CNN
F 1 "LED" H 6100 3500 50  0000 C CNN
	1    6100 3600
	-1   0    0    1   
$EndComp
$Comp
L LED D?
U 1 1 511C69D4
P 6100 3700
F 0 "D?" H 6100 3800 50  0000 C CNN
F 1 "LED" H 6100 3600 50  0000 C CNN
	1    6100 3700
	-1   0    0    1   
$EndComp
$Comp
L LED D?
U 1 1 511C69D3
P 6100 4000
F 0 "D?" H 6100 4100 50  0000 C CNN
F 1 "LED" H 6100 3900 50  0000 C CNN
	1    6100 4000
	-1   0    0    1   
$EndComp
$Comp
L LED D?
U 1 1 511C69D2
P 6100 3900
F 0 "D?" H 6100 4000 50  0000 C CNN
F 1 "LED" H 6100 3800 50  0000 C CNN
	1    6100 3900
	-1   0    0    1   
$EndComp
$Comp
L LED D?
U 1 1 511C69D1
P 6100 3800
F 0 "D?" H 6100 3900 50  0000 C CNN
F 1 "LED" H 6100 3700 50  0000 C CNN
	1    6100 3800
	-1   0    0    1   
$EndComp
Text Notes 6350 4050 0    60   ~ 0
B
Text Notes 6350 3950 0    60   ~ 0
G
Text Notes 6350 3850 0    60   ~ 0
R
Text Notes 6350 3250 0    60   ~ 0
R
Text Notes 6350 3350 0    60   ~ 0
G
Text Notes 6350 3450 0    60   ~ 0
B
$Comp
L LED D?
U 1 1 511C69C9
P 6100 3200
F 0 "D?" H 6100 3300 50  0000 C CNN
F 1 "LED" H 6100 3100 50  0000 C CNN
	1    6100 3200
	-1   0    0    1   
$EndComp
$Comp
L LED D?
U 1 1 511C69C8
P 6100 3300
F 0 "D?" H 6100 3400 50  0000 C CNN
F 1 "LED" H 6100 3200 50  0000 C CNN
	1    6100 3300
	-1   0    0    1   
$EndComp
$Comp
L LED D?
U 1 1 511C69C7
P 6100 3400
F 0 "D?" H 6100 3500 50  0000 C CNN
F 1 "LED" H 6100 3300 50  0000 C CNN
	1    6100 3400
	-1   0    0    1   
$EndComp
$Comp
L LED D?
U 1 1 511C69B0
P 6100 3100
F 0 "D?" H 6100 3200 50  0000 C CNN
F 1 "LED" H 6100 3000 50  0000 C CNN
	1    6100 3100
	-1   0    0    1   
$EndComp
$Comp
L LED D?
U 1 1 511C69AA
P 6100 3000
F 0 "D?" H 6100 3100 50  0000 C CNN
F 1 "LED" H 6100 2900 50  0000 C CNN
	1    6100 3000
	-1   0    0    1   
$EndComp
$Comp
L R-US R1
U 1 1 511C6957
P 4400 4050
F 0 "R1" H 4400 4050 60  0000 C CNN
F 1 "2K2" H 4400 3850 60  0000 C CNN
	1    4400 4050
	1    0    0    -1  
$EndComp
Text Notes 2850 4400 1    60   ~ 0
Parallel SIN
Text Notes 2750 6350 0    60   ~ 0
to BTH
$Comp
L +5V #PWR?
U 1 1 511C6518
P 5250 2650
F 0 "#PWR?" H 5250 2740 20  0001 C CNN
F 1 "+5V" H 5250 2740 30  0000 C CNN
	1    5250 2650
	1    0    0    -1  
$EndComp
$Comp
L GND #PWR?
U 1 1 511C6511
P 5250 5050
F 0 "#PWR?" H 5000 5200 60  0001 C CNN
F 1 "GND" H 5150 4750 60  0001 C CNN
	1    5250 5050
	1    0    0    -1  
$EndComp
$Comp
L TLC5940 IC2
U 1 1 511C6504
P 5250 3750
F 0 "IC2" H 4850 4900 60  0000 C CNN
F 1 "TLC5940" H 5250 3800 60  0000 C CNN
	1    5250 3750
	1    0    0    -1  
$EndComp
$Comp
L GND #PWR?
U 1 1 510AA641
P 650 5350
F 0 "#PWR?" H 400 5500 60  0001 C CNN
F 1 "GND" H 550 5050 60  0001 C CNN
	1    650  5350
	1    0    0    -1  
$EndComp
$Comp
L +5V #PWR?
U 1 1 510AA60F
P 650 3000
F 0 "#PWR?" H 650 3090 20  0001 C CNN
F 1 "+5V" H 650 3090 30  0000 C CNN
	1    650  3000
	0    -1   -1   0   
$EndComp
$Comp
L +5V #PWR?
U 1 1 510AA608
P 2650 7050
F 0 "#PWR?" H 2650 7140 20  0001 C CNN
F 1 "+5V" H 2650 7140 30  0000 C CNN
	1    2650 7050
	0    -1   -1   0   
$EndComp
$Comp
L R-US R?
U 1 1 510AA5F3
P 2850 6950
F 0 "R?" H 2850 6950 60  0000 C CNN
F 1 "330" H 2850 6750 60  0000 C CNN
	1    2850 6950
	1    0    0    -1  
$EndComp
$Comp
L LED D?
U 1 1 510AA5E5
P 3200 7050
F 0 "D?" H 3200 7150 50  0000 C CNN
F 1 "LED" H 3200 6950 50  0000 C CNN
	1    3200 7050
	1    0    0    -1  
$EndComp
Text Notes 3950 8800 0    60   ~ 0
Hall effect switch
$Comp
L NPN Q?
U 1 1 510AA5BD
P 3900 9100
F 0 "Q?" H 3730 9210 60  0000 C CNN
F 1 "NPN" H 3900 9000 60  0000 L CNN
	1    3900 9100
	1    0    0    -1  
$EndComp
$Comp
L GND #PWR?
U 1 1 5105A0C7
P 3900 9350
F 0 "#PWR?" H 3900 9350 30  0001 C CNN
F 1 "GND" H 3900 9280 30  0001 C CNN
	1    3900 9350
	1    0    0    -1  
$EndComp
NoConn ~ 2550 3700
NoConn ~ 2550 3600
NoConn ~ 2550 4450
Text Notes 6500 3950 0    60   ~ 0
3
Text Notes 6500 4250 0    60   ~ 0
4
Text Notes 6500 3650 0    60   ~ 0
2
Text Notes 6500 3350 0    60   ~ 0
1
Text Notes 6500 3050 0    60   ~ 0
0
Text Notes 6350 3150 0    60   ~ 0
B
Text Notes 6350 3050 0    60   ~ 0
G
Text Notes 6350 2950 0    60   ~ 0
R
$Comp
L LED D?
U 1 1 50FE3B99
P 6100 2900
F 0 "D?" H 6100 3000 50  0000 C CNN
F 1 "LED" H 6100 2800 50  0000 C CNN
	1    6100 2900
	-1   0    0    1   
$EndComp
$Comp
L ATMEGA328-P IC1
U 1 1 50FE39DE
P 1550 4100
F 0 "IC1" H 850 5350 50  0000 L BNN
F 1 "ATMEGA328-P" H 1800 2700 50  0000 L BNN
F 2 "DIL28" H 950 2750 50  0001 C CNN
	1    1550 4100
	1    0    0    -1  
$EndComp
$EndSCHEMATC
