# Z31-SWC

Reverse engineering of a 1986 Z31's optical steering wheel controls. 

The system consists of 3 components:
* The buttons & transmitter within the wheel
* The slip ring, optical ring, and head amplifier assembly ("clock spring") 
* The reciever, usually located behind the passenger seat

Only the first two subjects are documented here, since I don't have a reciever module. 

The transmitter module reads the values each of the 7 buttons & volume knob, and transmits the information via the slip ring assembly to the head amplifier. The head amplifier buffers the signal and signals to the reciever module, which then controls the radio and ASCD. 

The transmitter module is based on an NEC uPD652C 4-bit microcomputer. 1kbyte ROM, 32 word RAM. It reads & converts the buttons to 12-bit long serial strings, which flash an IR LED in the slip ring assembly. Bit widths are 1ms. 

As I capture them, the frames are laid out as follows:
0-11, from L->R

Resting State: 0101000XXXXX
Bit 0    : Start of Frame, Always Low ("0" here)
Bits 1-3 : Cruise Controls
Bits 4-6 : Media Controls
Bits 7-11: Volume Control

The actual buttons are:
Cruise Buttons (1-3):
None	: 101
Resume	: 011
Accel	: 010
Set		: 100

Media Buttons (4-6):
None	: 000
SW		: 001
Tape	: 010
FM/AM	: 100
Scan	: 011


The head amplifier is based on an NEC uPC1251 op-amp and phototransistor. Output is made by pulling down the external data line to the reciever assembly. The reciever contains the pullup for this data line. 

The goal of this project is to replace the reciever assembly. The replacement reciever should be able to control the ASCD (4x 12V active high outputs), and make all outputs available over CAN. Maybe some other SWC outputs? This has been a sticking point in the Z31 community for ages, since there are no available kits to retain SWC with a modern head unit. 
