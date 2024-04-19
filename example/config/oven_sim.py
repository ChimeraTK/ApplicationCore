#!/usr/bin/python3

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

currentSetpoint = dev.getScalarRegisterAccessor(np.double, "HEATER.CURRENT_SET")
currentReadback = dev.getScalarRegisterAccessor(np.double, "HEATER.CURRENT_READBACK.DUMMY_WRITEABLE")
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
