# Amiga Video Slot Adapter

This adapter builds on the ideas from the Amiga Denise adapter V2, adapted for use in big box Amiga video slots.

The main differences are:
- The 7MHZ signal is derived via C1 XNOR C3 using an additional 74LVC86 since 7MHZ is not available directly from the video slot
- Two button connectors allow for both a rear slot cover bracket button and another to be used, mounted wherever desired

The card edge connector used is from Amiga-KiCad-Library (https://github.com/JustinBaldock/Amiga-KiCad-Library). Approximately 1mm of space is left between the fingers and the PCB edge to allow for beveling.

## Recommendations for builders & resellers
- To ensure the best possible picture on all Amigas, builders should include C6 (47 pf capacitor). 
- V1.0 boards can easily be updated to include C6 by soldering the capacitor between pins 9 and 11 of the Pi connector on the bottom side of the board
- Try the default software profile first, and if any glitches appear on the image update the software profile settings to include the below settings:
    - Settings Menu->Overclock CPU: 40
    - Settings Menu->Overclock Core: 170
    - Sampling Menu->Sync Edge: Trailing with +ve PixClk
- A heat sink on the Pi is also highly recommended. 

## Assembly
- [Interactive BOM](bom/ibom.html)

## Installation notes
- Set the Denise jumper according to which Denise is in your Amiga (Super Denise for 8373, Denise for 8362)
- Orient the PCB such that the end with bracket mounting holes faces the rear of the machine

## Version History
- V1.01 Added spot for 47pf capacitor (C6) to ensure no sparkling on image, and grounded unused inputs on U5 per data sheet recommendation.
- V1.00 Initial version.
