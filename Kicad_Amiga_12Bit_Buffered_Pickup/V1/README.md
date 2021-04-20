# C128 adapter

This is an adapater to be plugged between the VIC-IIe and its socket in a Commodore 128.
Using this the, C64 Video Enchancement Board should in theory be able to work for the 40 columns mode 
display of the C128.

## Building

Use the provided gerber files (gerber.zip) to have a the PCB manufactured. Use the provided 
bill of material (c218adapter_bom.ods) to order the necessary parts and solder everything
according to the silk-screen placement markings. Be careful to get the orientation of
the various ICs right. Pin 1 is always marked specifically.

## IC socket

The VIC-IIe has 48 pins, so a 48-pin socket would be needed. Unluckily, there are no such sockets
to be found anywhere that have those long solder tails needed to perfectly fit the IC socket in
the C128 board. So I made the design to use one 40-pin socket together with one 8-pin socket.
To make things more complicated still, the 8-pin socket is of the narrow variant, so you 
have so split it to halves before mounting.

## Installation into the C128
The adapter board should directly fit under the VIC-IIe without relocating any components
(it was quite difficult to design the board in this way).

Installing the FPGA board will be pretty difficult, because it does not really fit into the space 
of the removed RF modulator. You will have to remove parts of the metal can of
the graphics chips and also some small parts of the DIN connector.
As the original mod was designed with only the C64 in mind, you are pretty much
on your own here. Good luck!
