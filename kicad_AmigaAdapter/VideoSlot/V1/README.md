# Amiga Video Slot Adapter

This adapter builds on the ideas from the Amiga Denise adapter V2, adapted for use in big box Amiga video slots.

The main differences are:
- The 7MHZ signal is derived via C1 XNOR C3 using an additional 74LVC86 since 7MHZ is not available directly from the video slot
- Two button connectors allow for both a rear slot cover bracket button and another to be used, mounted wherever desired

The card edge connector used is from Amiga-KiCad-Library (https://github.com/JustinBaldock/Amiga-KiCad-Library). Approximately 1mm of space is left between the fingers and the PCB edge to allow for beveling.

Installation notes:
- Set the Denise jumper according to which Denise is in your Amiga (Super Denise for 8373, Denise for 8362)
- Orient the PCB such that the end with bracket mounting holes faces the rear of the machine

<iframe
  src="https://htmlpreview.github.io/?https://github.com/bissonex/RGBtoHDMI/blob/amiga_video_slot_add_bom/kicad_AmigaAdapter/VideoSlot/V1/bom/ibom.html"
  style="width:100%; height:300px;"
></iframe>
