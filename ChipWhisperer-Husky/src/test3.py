import time
import phywhisperer.usb as pw
phy = pw.Usb()
phy.program_sam3u("COM7", "phywhisperer-SAM3U1C.bin")
