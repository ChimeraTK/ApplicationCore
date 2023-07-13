#!/bin/env python3
import deviceaccess as da
import time

# Set DMAP file name
da.setDMapFilePath("demo_example.dmap")

# Open the oven device directly, without logical name mapping, to access all registers
dev = da.Device("device")
dev.open()

# reset the device
print("Resetting oven device...")
dev.write("BOARD.RESET_N", 0)
time.sleep(0.1)
dev.write("BOARD.RESET_N", 1)

# (inverted) reset line is released now. Additional initialisation would go here.
print("Reset done, device is now ready.")
