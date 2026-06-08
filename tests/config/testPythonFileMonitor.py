#!/usr/bin/python3

import sys
import os
import os.path
import time

# fmt: off
# Hack to insert the python path for the locally compiled module in the
# test script
sys.path.insert(0, os.path.abspath(os.path.join(os.curdir, "..")))
import PyApplicationCore  # NOQA
# fmt: on

class MyMod(PyApplicationCore.ApplicationModule) :

  def __init__(self, owner, name, description) :
    super().__init__(owner, name, description)
    self.myOutput = PyApplicationCore.ScalarOutput(PyApplicationCore.DataType.float32, self, "/Var1" ,"unit", "description")
    self.myInput = PyApplicationCore.ScalarPushInput(PyApplicationCore.DataType.int32, self, "/Var2","unit", "description")

  def mainLoop(self) :
    val = 0
    while True:
      self.myOutput.setAndWrite(val + 0.5)
      val = self.myInput.readAndGet()


# create instance of test module
PyApplicationCore.app.myMod = MyMod(PyApplicationCore.app, "SomeName", "Description")
