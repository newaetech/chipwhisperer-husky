import chipwhisperer as cw
prog = cw.SAMFWLoader(None)
prog.program("COM7", "ChipWhisperer-Husky-SAM3U1C.bin")
