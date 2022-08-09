# Hey Emacs, this is a -*- makefile -*-
#----------------------------------------------------------------------------
#
# Makefile for ChipWhisperer SimpleSerial-AES Program
#
#----------------------------------------------------------------------------
# On command line:
#
# make all = Make software.
#
# make clean = Clean out built project files.
#
# make coff = Convert ELF to AVR COFF.
#
# make extcoff = Convert ELF to AVR Extended COFF.
#
# make program = Download the hex file to the device, using avrdude.
#                Please customize the avrdude settings below first!
#
# make debug = Start either simulavr or avarice as specified for debugging, 
#              with avr-gdb or avr-insight as the front end for debugging.
#
# make filename.s = Just compile filename.c into the assembler code only.
#
# make filename.i = Create a preprocessed source file for use in submitting
#                   bug reports to the GCC project.
#
# To rebuild project do "make clean" then "make all".
#----------------------------------------------------------------------------

# Target file name (without extension).
# This is the base name of the compiled .hex file.
TARGET = ChipWhisperer-Husky

# List C source files here.
# Header files (.h) are automatically pulled in.
SRC += main.c naeusb/usb_xmem.c naeusb/fpga_program.c naeusb/V2Protocol.c naeusb/V2ProtocolParams.c  ui.c
SRC += naeusb/XMEGANVM.c naeusb/XPROGNewAE.c naeusb/XPROGTarget.c naeusb/ISPProtocol.c naeusb/ISPTarget.c 
SRC += naeusb/XPROGTimeout.c naeusb/circbuffer.c
SRC += spi_driver.c cdci6214.c naeusb/udc.c
SRC += naeusb/naeusb.c naeusb/naeusb_default.c naeusb/naeusb_openadc.c naeusb/naeusb_usart.c naeusb_husky.c
SRC += naeusb/naeusb_mpsse.c
LINKERFILE = naeusb/flash_sam3u4.ld
# -----------------------------------------------------------------------------
OPT=2

include naeusb/makefile.cw

