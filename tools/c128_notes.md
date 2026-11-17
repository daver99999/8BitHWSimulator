# Commodore 128 (schematic 310378 rev 7A) - working notes

Sources: the four schematic sheets (MPU/MMU/SID/PLA/power, video/clock/reset, I/O, RAM/ROM), the redrawn full
schematic (net labels, easier to read), Jorgen Olsson's rev 7 board image, the 8721 Verilog (`8721-pla.v`).

Release plan (user's choice): release 1 = C128 mode, 40-column VIC-IIe screen, Z80 start-up and hand-over to the
8502 at pin level. Later: C64 mode, the 8563 VDC picture (80 columns), CP/M (Z80) mode.

## ROMs (user's files)
* `complete.252343-04` 32K = first 16K C64 OS (BASIC $A000 + KERNAL $E000, vectors FE43/FCE2/FF48) followed by
  16K C128 KERNAL ($C000-$FFFF, reset vector FF3D). The Z80 BIOS (4K) is inside it at file offset $5000 =
  C128 KERNAL $D000. `z80bios.bin` is a copy of that.
* `basic-4000.318018-04` 16K (BASIC low, $4000-$7FFF), `basic-8000.318019-04` 16K (BASIC high, $8000-$BFFF).
* `characters.390059-01` 8K: first 4K C64 set, second 4K C128 set.
* Boards: U32 = ROM1 (64 OS 16K + C128 kernal as one 23256 or 23128), U33 = ROM2 (BASIC lo), U34 = ROM3 (BASIC hi),
  U35 = ROM4 (KERNAL hi), U36 = function ROM (FROM1), U18 = character ROM.
  (23256 parts: J4/J5/J6/A14 and TA12/TA14 jumpers select which half.)

## 8722 MMU (U7, 48 pins)
 1 VDD(+5V)  2 /RES  3 TA15  4 TA14  5 TA13  6 TA12  7 TA11  8 TA10  9 TA9  10 TA8  11 /CAS1  12 /CAS0
 13 MS2 (I/O SEL)  14 MS1 (ROMBANKHI)  15 MS0 (ROMBANKLO)  16 AEC(GAEC)  17 MUX  18 A0  19 A1  20 A2  21 A3
 22 A5/A4  23 A7/A6  24 A8  25 A9  26 A10  27 A11  28 A12  29 A13  30 A14  31 A15  32 R/W  33 PHI0 (2MHZ)
 34 GND  35-42 D0-D7  43 /Z80EN  44 /FSDIR  45 /GAME  46 /EXROM  47 MS3 (128/64)  48 40/80
 (A7/A6 and A5/A4 come through 74LS32 OR gates, so the MMU registers sit at $D50x and $FF0x.)
 Registers: $D500 CR, $D501-$D504 PCRA-D, $D505 MCR (bit0 = 8502 select, 0 = Z80; bit3 FSDIR; bit4 GAME;
 bit5 EXROM; bit6 = 64 mode; bit7 = 40/80 sense), $D506 RCR, $D507-$D50A page 0/1 pointers, $D50B version.
 $FF00 = LCR (load CR from PCR), $FF01-$FF04 = load CR from PCRA..D.

## 8721 PLA (U11, 48 pins)
 inputs : 1 A15 2 A14 3 A13 4 A12 5 A11 6 A10 7 VICFIX 8 DMAACK(to GND) 9 AEC 10 R/W 11 GAME 12 EXROM 13 /Z80EN
          14 Z80IO 15 MS3 16 MS2 17 MS1 18 MS0 19 MA4(VMA4) 20 MA5(VMA5) 21 BA 22 LORAM 23 HIRAM 25 CHAREN
          26 VA14 27 128/256 (J3: open = 128K ROM) 47 CLK (= /CAS)
 outputs: 30 ROML 31 ROMH 32 CLRBNK 33 FROM1 34 ROM4 35 ROM3 36 ROM2 37 ROM1 38 IOCS 40 DWE 41 CASENB 42 VIC
          43 IOACC(IOCC) 44 GWE 45 COLORAM 46 CHAROM  (SDEN/DIR unconnected: pins 28/29/39)
 24 GND, 48 VCC.  Logic: `8721-pla.v` product terms p0..p86 (DWE latched while CLK high, CASENB latched while
 CLK|p74).

## 8502 (U6): A0-A15 = 7..23 (A0=7 .. A13=20, A14=22, A15=23), D0-D7 = 38..31, PHI0=1 (2MHZ), RDY=2, AEC=5, R/W=39,
 /RES=40, /IRQ=3, /NMI=4, P0 30 LORAM, P1 29 HIRAM, P2 28 CHAREN, P3 27 CASS WRT, P4 26 CASS SENSE, P5 25 CASS MTR,
 P6 24 CAPLK SENSE; VCC 6, GND 21.

## Z80A (U10): A0-A10 = 30..40, A11-A15 = 1..5, D4 7 D3 8 D5 9 D6 10 D2 12 D7 13 D0 14 D1 15, /INT 16 /NMI 17
 /HALT 18 /MREQ 19 /IORQ 20 /RD 21 /WR 22 /BUSACK 23 /WAIT 24 /BUSRQ 25 /RESET 26 /M1 27 /RFSH 28 GND 29 CLK 6 VCC 11.
 Bus latches: U12 74LS373 (system D -> ZD to the Z80, OC = /RD, ENC = D_1MHZ), U13A/B 74LS244 (ZD -> system D, G = /ZWRT).
 U56A 74LS74 (CLK = D_1MHZ) and U31 74LS00 gates make GWR / ZWRT / OLD_WR from /WR and GAEC.

## 8564 VIC-IIe (U21, 48 pins)
 9 /LP  13 /CS(/VIC)  22 /IOACC  43-46 D11..D8  47 D7  1 D6  2 D5  3 D4  4 D3  5 D2  6 D1  7 D0  14 R/W
 39 A7(VA7)  38 A6(VA6)  31 MA11(TA11)  42 MA10  41 MA9  40 MA8  37..32 MA5..MA0 (VMA5..VMA0)
 20 /CAS  19 /RAS  11 /DMARQST  15 /DMAACK (NC)  25 Z80 PHI  26 K0  27 K1  28 K2  18 1MHz  23 2MHz  8? /IRQ  10 BA
 17 SYNC/LUM  16 CHROMA  21 MUX  12 AEC  29 PHI-COLOR  30 PHI-IN (dot clock)  48 VCC  24 GND
 Clock: Y2 14.31818 MHz -> U28 8701 (pin 8 = colour clock, pin 6 = dot clock; J1 NTSC/PAL).

## Keyboard (user's 128keybd.gif; extra-key matrix from commodore-128.org "THREE-KEY ROLLOVER" table)
The 8x8 matrix is the C64's (CIA1 port A = columns driven, port B = rows read). The extra keys are three more
columns, selected by VIC register $D02F bits 0-2 (K0, K1, K2 low), read on CIA1 port B (bit 7 .. bit 0):
  K0: 1(kp) 7(kp) 4(kp) 2(kp) TAB   5(kp) 8(kp) HELP
  K1: 3(kp) 9(kp) 6(kp) ENTER(kp) LINE FEED  -(kp) +(kp) ESC
  K2: NO SCROLL  CURSOR-RIGHT  CURSOR-LEFT  CURSOR-DOWN  CURSOR-UP  .(kp) 0(kp) ALT
CAPS LOCK is a switch on the 8502's P6 (CAPLK SENSE, pin 24); 40/80 DISPLAY is a switch on the MMU's pin 48
(CN5 pins: K0 21, K1 22, K2 23, 40/80 24, CAPLK SENSE 25). The top row of the keyboard (ESC TAB ALT CAPS LOCK,
HELP LINE FEED 40/80 NO SCROLL, four cursor keys, F1 F3 F5 F7) and the keypad are the extra keys.
