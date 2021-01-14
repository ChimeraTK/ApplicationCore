#!/usr/bin/python3

import mtca4u
import time, math

mtca4u.set_dmap_location('example2.dmap')
d=mtca4u.Device('oven_raw')

#cooling rate c
c=0.001 #deg /(deg*s)

#heating rate h
h=0.0003 #deg/(A*s)

ovenTemp = 25.
environment = 25.

gains = d.read('APP.0','SUPPLY_ADCS_GAIN')

for i in range(4):
    if gains[i] == 0:
        gains[i] = 1

d.write('APP.0','SUPPLY_ADCS_GAIN',gains)

while True:
    I = d.read('APP.0','HEATING_CURRENT')[0]
    if d.read('APP.0','POWER') == 0:
        I = 0
        
    tempChange = 1 * (I*h +                  (environment-ovenTemp)*c)
            #   1s   current*heating rate    deltaT * cooling rate

    ovenTemp = ovenTemp + tempChange
    tempRaw = ovenTemp / 65 * math.pow(2,16) / 10 # 65 V/degC, 10V on 16 bits
    d.write('APP.0','TEMPERATURE_ADC.DUMMY_WRITEABLE',tempRaw)
    print('change ' + str(tempChange) +', new temp ' +str(ovenTemp))

    gains = d.read('APP.0','SUPPLY_ADCS_GAIN')
    voltagesRaw = [24./650.*pow(2,16)*gains[0],
                   400./650.*pow(2,16)*gains[1],
                   400./650.*pow(2,16)*gains[2],
                   400./650.*pow(2,16)*gains[3] ]
    d.write('APP.0','SUPPLY_ADCS.DUMMY_WRITEABLE', voltagesRaw)

    time.sleep(1)
