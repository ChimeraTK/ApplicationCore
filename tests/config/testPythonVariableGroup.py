#!/usr/bin/python3

import sys
import os
import os.path
import traceback

# fmt: off
# Hack to insert the python path for the locally compiled module in the
# test script
sys.path.insert(0, os.path.abspath(os.path.join(os.curdir, "..")))
import PyApplicationCore as ac  # NOQA
# fmt: on


class MyMod(ac.ApplicationModule):

    class VG2(ac.VariableGroup):
        def __init__(self, owner, name, description):
            super().__init__(owner, name, description)
            self.out2 = ac.ArrayOutput(ac.DataType.int32, self, "out2", "unit", 2,"")

            self.vg3 = ac.VariableGroup(self, "VG3", "inner VG")
            self.vg3.out3 = ac.ArrayOutput(ac.DataType.int32, self.vg3, "out3", "unit", 2,"")

    def __init__(self, owner, name, description):
        super().__init__(owner, name, description)

        self.vg = ac.VariableGroup(self, "VG", "VG using dynamic attributes")
        self.vg.in1 = ac.ArrayPushInput(ac.DataType.int32, self.vg, "in1", "unit", 2, "description")
        self.vg.out1 = ac.ScalarOutput(ac.DataType.int32, self.vg, "out1", "", "")

        self.vg2 = MyMod.VG2(self, "VG2", "VG subclassed by user")

        self.testError = ac.ScalarOutput(ac.DataType.string, self, "testError", "", "")

    def mainLoop(self):
        rag = self.vg.readAnyGroup() # just to test that ReadAnyGroup of VariableGroup is available

        self.vg.out1.setAndWrite(1)

        while True:
            rag.readUntil(self.vg.in1)
            in1val = self.vg.in1.get()
            self.vg2.out2.set(in1val)
            self.vg2.out2.write()
            self.vg2.vg3.out3.set(in1val)
            self.vg2.vg3.out3.write()

            try:
                ok = (in1val[0] != 0)
                assert ok, "check input value"

                # just check that readAll/writeAll functions exist, check with/without default args
                # using writeAll as example, also check that these functions are available on the ApplicationModule (derives from VariableGroup)
                self.writeAll(True)
                self.writeAllDestructively()
                # note, vg2.readAll ist allowed while vg.readAll is not since we already access vg's element via ReadyAnyGroup
                # actually vg2 has no inputs but we don't care here
                self.vg2.readAllNonBlocking()
                self.vg2.readAllLatest()
                self.vg2.readAll(True)

            except AssertionError as e:
                self.testError.setAndWrite("\n".join(traceback.format_exception(e)))


ac.app.userModule = MyMod(ac.app, "UserModule", "module for testing of VariableGroup bindings")
ac.app.someGroup = ac.ModuleGroup(ac.app, "SomeGroup", "Need to test ModuleGroups as well")
ac.app.someGroup.userModule = MyMod(ac.app.someGroup, "UserModuleInGroup", "Inside ModuleGroup")
