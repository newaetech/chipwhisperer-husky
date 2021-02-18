# Copyright (c) 2019, NewAE Technology Inc
# All rights reserved.
#
#
# Find this and more at newae.com - this file is part of the PhyWhisperer-USB
# project, https://github.com/newaetech/phywhispererusb
#
#    This file is part of PhyWhisperer-USB.
#
#    PhyWhisperer-USB is free software: you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation, either version 3 of the License, or
#    (at your option) any later version.
#
#    PhyWhisperer-USB is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU Lesser General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with PhyWhisperer-USB.  If not, see <http://www.gnu.org/licenses/>.
#=================================================

from .interface import naeusb as NAE
from .interface import program_fpga as LLINT

class Husky:
    """PhyWhisperer-USB Interface"""

    def __init__ (self):
        """ Set up PhyWhisperer-USB device.
        
        Args:
            viewsb: Should only be set to 'True' when this is called by ViewSB.
        """
        pass


    def con(self, bsfile, sn=None):
        """Connect to PhyWhisperer-USB. Raises error if multiple detected

        Args:
            PID (int, optional): USB PID of PhyWhisperer, defaults to 0xC610 (NewAE standard).
            sn (int, option):  Serial Number of PhyWhisperer, required when multiple
                PhyWhisperers are connected.
            program_fpga (bool, option): Specifies whether or not to program the FPGA with
                the default firmware when we connect. Set to False if using custom bitstream.
        """

        self.usb = NAE.NAEUSB()
        self.usb.con(idProduct=[0xACE5], serial_number=sn)
        self._llint = LLINT.PhyWhispererUSB(self.usb)

        with open(bsfile,"rb") as bitstream:
            self._llint.FPGAProgram(bitstream) 

    def adc_reset(self):
        self.adc_reg_write(0x00, 0x02)

    def adc_reg_write(self, addr, data):
        self.usb.sendCtrl(0x26, 0x00, [addr, data])

    def adc_reg_read(self, addr):
        return self.usb.readCtrl(0x26, addr, 1)
    def write_reg(self, address, data):
        """Write a PhyWhisperer register.
        
        Args:
            address: int
            data: bytes
        """
        return self.usb.cmdWriteMem(address, data)


    def read_reg(self, address, size=1):
        """Reads a PhyWhisperer register.
        
        Args:
            address: int
            size: int, number of bytes to read
        Returns:
        """
        return self.usb.cmdReadMem(address, size)




    def close(self):
        """ Terminates our connection to the PhyWhisperer device. """

        self.usb.close()


