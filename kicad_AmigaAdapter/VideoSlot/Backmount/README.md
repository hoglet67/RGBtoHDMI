# Amiga Video Slot Adapter

This adapter builds on the ideas from the Amiga Denise adapter V2, adapted for use in big box Amiga video slots.

The main differences are:
- The 7MHZ signal is derived via C1 XNOR C3 since 7MHZ is not available directly from the video slot

The card edge connector used is from Amiga-KiCad-Library (https://github.com/JustinBaldock/Amiga-KiCad-Library), but shortened, because not all signals are used (makes the PCB smaller)

The main difference to the Videoslot V1 version is, that the Raspberry will be mounted in such a way, that the miniHDMI port of the Pi faces the bracket at the back. With a modified bracket this eliminates the need of an HDMI extension cable inside the Amiga. Also this uses the 3V3 regulator of the Amiga

I put much effort into the Layout for good signal integrity.



**Warning:** This is untested, but I am confident, as it is based on the V2 schematics. If you builds this, please let me know so I can remove the warning or fix problems. 

## Installation notes
- Set the Denise jumper according to which Denise is in your Amiga (Super Denise for 8373, Denise for 8362)
- Orient the PCB such that the end with the Raspberry Pi holes faces the rear of the machine
