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
    # can we do this somehow in the class definition instead of the constructor body?
    self.myOutput = PyApplicationCore.ScalarOutput(PyApplicationCore.DataType.float32, self, "/Var1" ,"unit", "description")
    self.myInput = PyApplicationCore.ScalarPushInput(PyApplicationCore.DataType.int32, self, "/Var2","unit", "description")

  def mainLoop(self) :
    val = 0
    while True:
      self.myOutput.setAndWrite(val + 0.5)
      val = self.myInput.readAndGet()


# create instance of test module
PyApplicationCore.app.myMod = MyMod(PyApplicationCore.app, "SomeName", "Description")

# Show that getting the appConfig also works on module level
c = PyApplicationCore.appConfig()
tickers = []

# And have an example for passing on a default value
for t in range(0, c.get(PyApplicationCore.DataType.uint32, "numberOfTickers", 0)):
  tickers.append(Foo(f"Ticker{t}", "A Ticker module"))
