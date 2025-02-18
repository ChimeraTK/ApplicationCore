#!/bin/bash

import os
import logging
import sys

def initDevice(logger=logging.getLogger()):
  if os.path.exists('producePythonDevice2InitError'):
    logger.info('error initialising device2')
    return 1

  logger.info('just a second script')
  return 0

if __name__ == "__main__":
  logging.basicConfig(level=logging.INFO)
  sys.exit(initDevice())
