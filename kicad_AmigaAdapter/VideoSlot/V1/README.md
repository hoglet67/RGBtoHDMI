# Amiga Video Slot Adapter

This adapter builds on the ideas from the Amiga Denise adapter V2, adapted for use in big box Amiga video slots.

The main differences are:
- The 7MHZ signal is derived via C1 XNOR C3 since 7MHZ is not available directly from the video slot
- Two button connectors allow for both a rear slot cover bracket button and another to be used, mounted wherever desired

The card edge connector used is from Amiga-KiCad-Library (https://github.com/JustinBaldock/Amiga-KiCad-Library). Approximately 1mm of space is left between the fingers and the PCB edge to allow for beveling.

## Assembly
- [Interactive BOM](bom/ibom.html)

## Installation notes
- Set the Denise jumper according to which Denise is in your Amiga (Super Denise for 8373, Denise for 8362)
- Orient the PCB such that the end with bracket mounting holes faces the rear of the machine

## Version History
- V1.1:
    - Routing changes that eliminate "sparkling" on certain image patterns
    - Excessive overclocking and the extra 47pf capacitor should no longer be necessary
    - Make use of unused gates on the original 74LVC86 instead of adding another
- V1.01 Added spot for 47pf capacitor (C6) to ensure no sparkling on image, and grounded unused inputs on U5 per data sheet recommendation.
- V1.00 Initial version.
