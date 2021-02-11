import time
import phywhisperer.usb as pw
phy = pw.Usb()
phy.con(program_fpga=False)
phy.erase_sam3u()
time.sleep(0.5)
phy.program_sam3u("COM7", "phywhisperer-SAM3U1C.bin")
