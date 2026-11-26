# Apple IIe (820-0064-B): continuity checks of the netlist against the real board

Board unpowered, meter on beep. 76 checks, 26 November 2026. The netlist is the Bit Preserve capture of 050-0051-D; the chips named here are the ones the capture was most likely to get wrong (units drawn on several sheets, pins corrected by hand, and the RAM address wiring). Passives, power and ground were not checked one by one, apart from the oscillator.

## Errors found (both fixed in the netlist)
| Chip | Capture | Real board | Effect |
|---|---|---|---|
| UC1 (74LS109) pins 13, 14 | pin 14 on pin 7 (/Q of the first flip-flop), pin 13 on 7M | pin 14 on pin 6 (7M), pin 13 on pin 7 | CREF changes when 7M falls; the long cycle is 16 14M clocks and a line is 912 clocks (it was 12 and 908) |
| UB8 (7402) pins 5, 6 | PH1 on pin 6, X4/X5 on pin 5 | PH1 on pin 5, X4/X5 on pin 6 | none in behaviour (the two inputs of a NOR are interchangeable); the netlist now matches the board |

## Confirmed as in the netlist
- ROM sockets: UE8 is the CD ROM (342-0135-B), UE10 the EF ROM (342-0134-A) (stated by David).
- UC1: pins 1, 2, 5, 11, 15 on the pulled-up net; pin 3 on ground; pins 4 and 12 on 14M; pin 6 to UD1 pin 2 (7M); pin 10 to UD1 pin 3 (3.58M); 14 to 7 open.
- Oscillator: UE1 pin 11 to UC1 pin 12, UF5 pin 7 (14M); UE1 pin 12 to R34 and OSC2 pin 3; Y1 to R35 and R37 (not directly to UE1 pin 12); R37 to ground; R36 to R38, C83 and the transistors Q4 and Q6; R38 to the +12V side; C83 to ground.
- UE1 (74LS125): pin 13 on ~CLKEN (R28), pin 1 on PH1 (R42, UD1 pin 14), pin 4 on DMA (UB1 pin 1), pin 5 to UC4 pin 34 (R/W), pin 6 on R/W (UE4 pin 14, UE5 pin 14, UB8 pin 9), pin 3 on the RAM write line (UF6 and UF13 pin 3), pin 10 to UE4 pin 17 (~EN80); pins 2, 6 and 9 joined; pin 8 not joined to pin 9.
- UB8 (7402): pin 4 to UC4 pin 37; pin 3 on ground; pin 10 to UB2 pin 19; pin 8 to UD1 pin 15 (PH0); pin 13 to the video amplifier; pin 12 to UC1 pin 10; pin 11 to UE5 pin 37; pin 2 to UE4 pin 16; pin 1 to UE1 pin 4. Pin 5 did not beep to X4, X5, DMA, ground or +5V; pin 6 beeps to X4.
- UC5 (74LS10): pin 1 to UB5 pin 6 (CXXX); pin 2 to UB3 pin 12 (A11); pin 13 to UD1 pin 15 (PH0); pins 4 and 5 joined (~ALTVID, to RP3); pins 10 and 11 joined (~FRCTXT); pin 9 to UE5 pin 2 (GR); pin 8 to UD1 pin 7; pin 6 to R7; pin 3 to UF5 pin 13 (~SEROUT); pin 12 not joined to pin 13.
- UF4 (video ROM): pin 20 to UE5 pin 38 (~WNDW); pin 22 to UE5 pin 35 (~RA9); 20 and 22 not joined.
- RAM: UF8 pins 2 and 14 joined, and to UD3 pin 7 (MD2); the address bus is reversed into the 6664s (UE4 pin 6 to UF8 pin 9, pin 13 to pin 5, pin 11 to pin 6).

## Not checked
The pin-to-net mapping of the MMU (UE4), IOU (UE5) and timing HAL (UD1) beyond the pins above, the slots, the keyboard connector and encoder, the game connector and the power connectors.

## Closed
- X4 and X5: X5 is bridged on this board (David), as in the netlist, so UB8 pin 6 joins the DMA net through it; X4 is open. UB8 pin 5 does not beep to either jumper because it is on PH1, which fits the corrected pin order.
