#!/usr/bin/python3

import os
import logging
import sys

def initDevice(logger=logging.getLogger()):
  logger.info('starting device1 init')

  if os.path.exists('producePythonDeviceInitError1'):
    f = open('producePythonDevice1InitError', 'r')
    logger.error('specific error information')
    raise RuntimeError('error initialising device: '+f.read())

  logger.info('device1 init successful')

  if os.path.exists('producePythonDeviceInitError2'):
    sys.exit(1)

if __name__ == "__main__":
  logging.basicConfig(level=logging.INFO)
  initDevice()
