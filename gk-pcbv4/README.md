# gkv4 PCBs #

This directory contains PDF schematics, KiCAD projects and Gerbers for the gkv4 PCBs.

There are 4 separate PCBs included:

- gk: This is the main board for the gkv4 and includes the high speed components.  It is implemented as a 6 layer board with impedance control requirements specified in the PCBnew file.
- pmic: This is a standalone 1 inch square board that contains the STPMIC25 power management chip and associated passive components.  It is implemented as a 4 layer board and additionally provided as a panelized board.  It is mounted using LGA/castellated holes onto the main board.
- controls: This is a daughter board containing the bulk of the controls for the front of the gk.  It aligns using M3 bolt holes with the main board and sits in front of it to ensure the thumb sticks are at the appropriate height relative to the screen.  It connects to the main board via a 2x10 pin 1.27mm pitch cable (ARM Cortex 20 pin Trace Cable).  It is a 2 layer PCB.
- controls-trigger: This is a single/two layer PCB simply containing the pads for the two back trigger buttons.  It is provided as a separate board to allow a certain amount of vertical separation from the controls board such that the triggers (which rotate rather than push) have better alignment with the pads.

For the various button pads a 5mm high rubber contact e.g. Xbox controller ABXY contact is expected.
