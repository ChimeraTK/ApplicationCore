#!/usr/bin/python3

import os
import logging
import sys

def initDevice(logger=logging.getLogger()):
  logger.info('starting device1 init')

  if os.path.exists('producePythonDevice1InitError'):
    logger.error('error initialising device')
    return 1

  logger.info('device1 init successful')
  return 0


if __name__ == "__main__":
  logging.basicConfig(level=logging.INFO)
  sys.exit(initDevice())
