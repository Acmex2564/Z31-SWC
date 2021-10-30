# Z31-SWC

Reverse engineering of a 1986 Z31's optical steering wheel controls. 

The system consists of 3 components:
* The buttons & transmitter within the wheel
* The slip ring, optical ring, and head amplifier assembly ("clock spring") 
* The reciever, usually located behind the passenger seat

Only the first two subjects are documented here, since I don't have a reciever module. 

The transmitter module reads the values each of the 7 buttons & volume knob, and transmits the information via the slip ring assembly to the head amplifier. The head amplifier buffers the signal and signals to the reciever module, which then controls the radio and ASCD. 

The transmitter module is based on an NEC uPD652C 4-bit microcomputer. 1kbyte ROM, 32 word RAM. It reads & converts the buttons to 12-bit long serial strings, which flash an IR LED in the slip ring assembly. 

The head amplifier is based on an NEC uPC1251 op-amp and phototransistor. Output is made by pulling down the external data line to the reciever assembly. The reciever contains the pullup for this data line. 
