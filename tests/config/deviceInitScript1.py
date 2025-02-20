#!/usr/bin/python3

import os
import logging
import sys

def initDevice(logger=logging.getLogger()):
  logger.info('starting device1 init')

  if os.path.exists('producePythonDeviceInitError1'):
    f = open('producePythonDeviceInitError1', 'r')
    logger.error('specific error information')
    raise RuntimeError('error initialising device: '+f.read())

  if os.path.exists('producePythonDeviceInitError2'):
    sys.exit(1)

  logger.info('device1 init successful')

if __name__ == "__main__":
  logging.basicConfig(level=logging.INFO)
  initDevice()