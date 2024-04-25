#!/usr/bin/python3

# This is a simple simulator script for an oven.
# It is meant to run in parallel to the demo_example server.
# 
# Both are accessing the same device using the same DMAP file. Since the device is
# backed by the SharedMemoryDummy, it is possible to react to the setpoint provided
# by the demo_example server and provide values back to it. The SharedMemoryDummy
# maps the register map on a shared memory segment, making it possible to access
# the device from multiple applications simultaneously.
#
# Through special registers it also enables us to provide values for registers that
# are declared read-only in the map file.

# The script runs a simple heating simulation that generates the oven temperature from the
# heating current and environment temperature.


import deviceaccess as da
import numpy as np
import random
import time

da.setDMapFilePath('demo_example.dmap')

dev = da.Device('device')
dev.open()

#cooling rate c
c=0.001 #deg /(deg*s)

#heating rate h
h=0.0003 #deg/(A*s)

ovenTemp = 25.
environment = 25.

# Our current setpoint
currentSetpoint = dev.getScalarRegisterAccessor(np.double, "HEATER.CURRENT_SET")

# The readback value of the actual current.
currentReadback = dev.getScalarRegisterAccessor(np.double, "HEATER.CURRENT_READBACK.DUMMY_WRITEABLE")

# The readback of various temperatures
tempratureReadback = dev.getScalarRegisterAccessor(np.double, "SENSORS.TEMPERATURE1.DUMMY_WRITEABLE")
temperatureTop = dev.getScalarRegisterAccessor(np.double, "SENSORS.TEMPERATURE2.DUMMY_WRITEABLE")
temperatureBottom = dev.getScalarRegisterAccessor(np.double, "SENSORS.TEMPERATURE3.DUMMY_WRITEABLE")
temperatureOutside = dev.getScalarRegisterAccessor(np.double, "SENSORS.TEMPERATURE4.DUMMY_WRITEABLE")


while True:
    currentSetpoint.read()
    I = currentSetpoint[0];
    tempChange = 1 * (I*h +                  (environment-ovenTemp)*c)
            #   1s   current*heating rate    deltaT * cooling rate

    ovenTemp = ovenTemp + tempChange
    tempratureReadback.setAndWrite(ovenTemp)

    # Just add some random jitter on the other read-out sensors
    currentReadback.setAndWrite(I + random.gauss(0.0, 0.2))
    temperatureTop.setAndWrite(ovenTemp + random.gauss(0.0, 0.2))
    temperatureBottom.setAndWrite(ovenTemp + random.gauss(0.0, 0.2))
    temperatureOutside.setAndWrite(environment + random.gauss(0.0,  0.2))
    print('change ' + str(tempChange) +', new temp ' +str(ovenTemp))

    time.sleep(1)
