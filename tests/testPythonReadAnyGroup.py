import sys
import os
import os.path
import numpy as np
import traceback

# fmt: off
# Hack to insert the python path for the locally compiled module in the
# test script
sys.path.insert(0, os.path.abspath(os.path.join(os.curdir, "..")))
import PyApplicationCore as ac  # NOQA
from PyApplicationCore import DataType as dt  # NOQA
# fmt: on


class ReadAnyGroupMod1(ac.ApplicationModule):

    def __init__(self, owner, name, description):
        super().__init__(owner, name, description)

        self.input1 = ac.ScalarPushInput(dt.int32, self, "in1", "", "")
        self.input2 = ac.ArrayPushInput(dt.int32, self, "in2", "", 4, "")
        self.input3 = ac.ScalarPushInput(dt.int32, self, "in3", "", "")
        self.output = ac.ScalarOutput(dt.string, self, "output", "", "")
        self.testError = ac.ScalarOutput(dt.string, self, "testError", "", "")

    def mainLoop(self):
        try:
            # Manually create a ReadAnyGroup from two of the three inputs
            foo = [self.input1, self.input2]
            group = ac.ReadAnyGroup()
            group.add(self.input1)
            group.add(self.input2)
            group.finalise()

            id = group.readAny()
            assert id == self.input1.getId()
            assert self.input1.get() == 12
            self.output.setAndWrite("step1")

            id = group.readAny()
            assert id == self.input2.getId()
            assert (self.input2.get() == [24, 24, 24, 24]).all()
            self.output.setAndWrite("step2")

            assert self.input3.readAndGet() == 36
            assert not group.readAnyNonBlocking().isValid()

            self.output.setAndWrite("step3")

            group.readUntil(self.input1)
            self.output.setAndWrite("step4")

            group.readUntil(self.input2)
            self.output.setAndWrite("step5")

            group.readUntilAll(*foo)
            self.output.setAndWrite("step6")

            group.readUntil(self.input1.getId())
            self.output.setAndWrite("step7")

        except AssertionError as e:
            self.testError.setAndWrite("\n".join(traceback.format_exception(e)))
            self.output.setAndWrite("error")


ac.app.readAnyGroupMod1 = ReadAnyGroupMod1(ac.app, "UserModule", "Test Module for ReadAnyGroup")
