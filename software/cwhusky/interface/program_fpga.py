# -*- coding: utf-8 -*-
#
# Copyright (c) 2014-2018, NewAE Technology Inc
# All rights reserved.
#
# Find this and more at newae.com - this file is part of the chipwhisperer
# project, http://www.assembla.com/spaces/chipwhisperer
#
#    This file is part of chipwhisperer.
#
#    chipwhisperer is free software: you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation, either version 3 of the License, or
#    (at your option) any later version.
#
#    chipwhisperer is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU Lesser General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with chipwhisperer.  If not, see <http://www.gnu.org/licenses/>.
#==========================================================================

from . import naeusb as NAE
from .naeusb import packuint32
import time
import sys
import os
import zipfile



import time
import logging

class PhyWhispererUSB(object):

    CMD_FPGA_STATUS = 0x15
    CMD_FPGA_PROGRAM = 0x16
    CMD_CHANGE_PWR = 0x24
    CMD_FPGA_RESET = 0x25

    PWR_SRC_HOST = 0x02
    PWR_SRC_OFF = 0x00
    PWR_SRC_5V = 0x01

    def __init__(self, usb, timeout=200):
        self.sendCtrl = usb.sendCtrl
        self.readCtrl = usb.readCtrl
        self._usb = usb
        self._timeout = timeout

    def isFPGAProgrammed(self):
        """
        Return True/False about FPGA status
        """

        status = self.readCtrl(self.CMD_FPGA_STATUS, dlen=4)

        if status[0] & 0x01:
            return True
        else:
            return False

    def FPGAProgram(self, bitstream=None, exceptOnDoneFailure=True):
        """
        Program FPGA with a bitstream, or if not bitstream passed just erases FPGA
        """

        # Erase the FPGA by toggling PROGRAM pin, setup
        # NAEUSB chip for FPGA programming
        self.sendCtrl(self.CMD_FPGA_PROGRAM, 0xA0)
        time.sleep(0.001)
        self.sendCtrl(self.CMD_FPGA_PROGRAM, 0xA1)

        time.sleep(0.001)

        # Download actual bitstream now if present
        if bitstream:
            # Run the download which should program FPGA
            self._FPGADownloadBitstream(bitstream)

            wait = 5
            while wait > 0:
                # Check the status a few times
                programStatus = self.isFPGAProgrammed()
                if programStatus:
                    break
                time.sleep(0.001)
                wait -= 1

            # Exit FPGA programming mode
            self.sendCtrl(self.CMD_FPGA_PROGRAM, 0xA2)

            if programStatus == False and exceptOnDoneFailure:
                raise IOError("FPGA Done pin failed to go high, bad bitstream?", bitstream)

            return programStatus
        else:
            # No bitstream, exit programming mode
            self.sendCtrl(self.CMD_FPGA_PROGRAM, 0xA2)
            return False

    def _FPGADownloadBitstream(self, fwFileLike):
        """
        Performs actual bitstream download, do not call directly, call FPGAProgram
        """

        transactionBytes = 2048
        t0 = 0

        buffer_ = [None] * int(16 * 1024 * 1024 / transactionBytes)
        size = 0

        # Read entire thing in
        with fwFileLike as f:
            inputStream = f.read()
        streamCnt = 0

        # Might need a few extra CCLKs at end to finish off, and as written elsewhere this is done with DO=1
        # Perhaps micro should add these instead? For now this should be reliable enough (things worked even w/o this it seemed, so this is
        # a just in case item)
        inputStream += bytes([0xff] * 32)

        inputStream = inputStream[0x7C:]

        j = transactionBytes
        for i in range(0, len(buffer_)):
            if j != transactionBytes: break
            buffer_[i] = bytearray(inputStream[streamCnt:(streamCnt + transactionBytes)])
            streamCnt += transactionBytes

            j = len(buffer_[i])

            if j < transactionBytes and j % 64 == 0:
                j += 1
            size += j

        if size < 64 or size % 64 == 0:
            raise ValueError("Invalid file size: " + str(size))

        tries = 1

        while tries > 0:
            # self.resetFpga()
            try:
                # t0 = -Date().getTime()
                bs = 0
                cs = 0

                for i in range(0, len(buffer_)):
                    if i * transactionBytes >= size:
                        break
                    j = size - i * transactionBytes
                    if j > transactionBytes:
                        j = transactionBytes

                    self._usb.writeBulkEP(buffer_[i])

                    bs += j
                    for k in range(0, len(buffer_[i])):
                        cs = (cs + (buffer_[i][k] & 0xff)) & 0xff

                tries = 0
            except IOError as e:
                if tries > 1:
                    print(("Warning: " + str(e) + ": Retrying it ..."))
                else:
                    raise
            tries -= 1
        # time.sleep(0.1)
        return t0

    def changePowerSource(self, src):
        self.sendCtrl(self.CMD_CHANGE_PWR, src)

    def resetFPGA(self):
        self.sendCtrl(self.CMD_FPGA_RESET, 0x00)

    def eraseFW(self, confirm=False):
        # may not work, FW copied from CWLite
        if confirm:
            self.sendCtrl(0x22, 0x03)

def print_usage():
    print("Usage: python program_fpga.py cmd arg")
    print("Commands:\n\tfpgaprog bitstream.bit\n\tchangepwr src")
    exit()

if __name__ == '__main__':
    if len(sys.argv) != 3:
        print_usage()

    usb = NAE.NAEUSB()
    usb.con(idProduct=[0xC521])
    phywhisperer = PhyWhispererUSB(usb)
    if sys.argv[1] == "fpgaprog":
        if not os.path.isfile(sys.argv[2]):
            print("Bitstream file not found")
            print_usage()

        bitstream = open(sys.argv[2], "rb")
        phywhisperer.FPGAProgram(bitstream)
    elif sys.argv[1] == "changepwr":
        new_pwr_src = 0
        try:
            new_pwr_src = int(sys.argv[2])
        except ValueError:
            print("Invalid power source (try 1, 2, or 3)")
            print_usage()
        phywhisperer.changePowerSource(new_pwr_src)
        pass
    else:
        print_usage()
