#!/usr/bin/python3

import sys
import os
import os.path
import numpy as np
from typing import Callable
import time
import traceback

# fmt: off
# Hack to insert the python path for the locally compiled module in the
# test script
sys.path.insert(0, os.path.abspath(os.path.join(os.curdir, "..")))
import PyApplicationCore as ac  # NOQA
# fmt: on


class MyMod(ac.ApplicationModule):

    def __init__(self, owner, name, description):
        super().__init__(owner, name, description)

        # can we do this somehow in the class definition instead of the constructor body?
        self.myOutput1 = ac.ArrayOutput(ac.DataType.int32, self, "ArrayOut1", "SomeUnit", 10, "my fancy description")
        self.myOutput2 = ac.ArrayOutput(ac.DataType.int32, self, "ArrayOut2", "SomeUnit", 10, "my fancy description")
        self.myOutputRB = ac.ArrayOutputPushRB(
            ac.DataType.int32,
            self,
            "ArrayOutRB",
            "SomeUnit",
            2,
            "my fancy description")

        self.myInput1 = ac.ArrayPushInput(ac.DataType.int32, self, "ArrayIn1", "unit", 2, "description")
        self.myInput2 = ac.ArrayPushInput(ac.DataType.int32, self, "ArrayIn2", "unit", 5, "description")
        self.myInput3 = ac.ArrayPushInput(ac.DataType.float32, self, "ArrayIn3", "unit", 2, "description")
        self.myInputPoll = ac.ArrayPollInput(ac.DataType.int32, self, "ArrayInPOLL", "unit", 2, "description")
        self.myInputWB = ac.ArrayPushInputWB(ac.DataType.int32, self, "ArrayInWB", "unit", 1, "description")

        # these outputs are used to transport subject-to-test data to the C++ side for checking in the BOOST test case
        self.testError = ac.ScalarOutput(ac.DataType.string, self, "TestError", "", "")

    def prepare(self):
        self.myOutputRB.setAndWrite([42, 43])

    def mainLoop(self):
        try:
            assert self.myOutput1.getName() == "/SomeName/ArrayOut1"
            assert self.myOutput1.getUnit() == "SomeUnit"
            assert "Root for Python Modules - Module's description - my fancy description" in self.myOutput1.getDescription()
            assert self.myOutput1.getValueType() == ac.DataType.int32
            assert self.myOutput1.getVersionNumber() == ac.VersionNumber(None)
            assert self.myOutput1.isReadOnly() == False
            assert self.myOutput1.isReadable() == False
            assert self.myOutput1.isWriteable()
            assert str(self.myOutput1.getId()).startswith('0x')
            assert self.myOutput1.getId() != self.myOutput2.getId()
            assert self.myOutput1.dataValidity() == ac.DataValidity.ok

            while True:
                val = self.myInput1.get()
                valSum = sum(val)
                self.myOutput1.setAndWrite(range(valSum, valSum + 10))

                valcopy = np.array(self.myInput1, copy=True)
                assert (self.myInput1 == valcopy).all()

                valcopy[0] = 3

                assert ((self.myInput1 == valcopy) == [False, True]).all()
                assert ((self.myInput1 != valcopy) == [True, False]).all()
                assert ((self.myInput1 <= valcopy) == [False, True]).all()
                assert ((self.myInput1 < valcopy) == [False, False]).all()
                assert ((self.myInput1 >= valcopy) == [True, True]).all()
                assert ((self.myInput1 > valcopy) == [True, False]).all()

                assert ((valcopy == self.myInput1) == [False, True]).all()
                assert ((valcopy != self.myInput1) == [True, False]).all()
                assert ((valcopy >= self.myInput1) == [False, True]).all()
                assert ((valcopy > self.myInput1) == [False, False]).all()
                assert ((valcopy <= self.myInput1) == [True, True]).all()
                assert ((valcopy < self.myInput1) == [True, False]).all()

                assert ((self.myInput1 == self.myInput1) == [True, True]).all()
                assert ((self.myInput1 == self.myInputPoll) == [False, False]).all()

                # input arrays: test support both for numpy arrays and python lists; implementation may differ
                self.myInput1.set(np.array([12, 23], dtype=np.int32))
                assert self.myInput1[0] == 12
                assert self.myInput1[1] == 23
                assert ((self.myInput1 == [12, 23]) == [True, True]).all()

                self.myInput1.set([123, 234])
                assert self.myInput1[0] == 123
                assert self.myInput1[1] == 234
                assert ((self.myInput1 == [123, 234]) == [True, True]).all()

                # check that np.float32 is correctly taken over (bug regression)
                self.myInput3.set(np.array([1.1, 6.6], dtype=np.float32))
                assert self.myInput3[0] == np.float32(1.1)
                assert self.myInput3[1] == np.float32(6.6)
                self.myInput3.set([1.2, 6.7])
                assert self.myInput3[0] == np.float32(1.2)
                assert self.myInput3[1] == np.float32(6.7)
                self.myInput3.set([np.float64(1.1), np.float64(6.6)])
                assert self.myInput3[0] == np.float32(1.1)
                assert self.myInput3[1] == np.float32(6.6)
                self.myInput3.set([np.float32(1.1), np.float32(6.6)])
                # note, index access returns type 'float' and not 'np.float32'
                # TODO discuss - is this acceptable?
                print ("input3: type,val=", type(self.myInput3.get()),
                    type(self.myInput3.get()[0]),
                    self.myInput3.get(), type(self.myInput3[0]))
                assert self.myInput3[0] == np.float32(1.1)
                assert self.myInput3[1] == np.float32(6.6)
                # test array slicing (separate implementation)
                self.myInput3[0:2:1] = 1.3
                assert self.myInput3[0] == np.float32(1.3)
                assert self.myInput3[1] == np.float32(1.3)
                self.myInput3[0:2:1] = np.float32(1.5)  # 1.5 is precisely representable as float32
                assert self.myInput3[0] == 1.5
                assert self.myInput3[1] == 1.5
                # test array index access (separate implementation)
                self.myInput3[1] = 6.5
                assert self.myInput3[0] == 1.5
                assert self.myInput3[1] == 6.5
                self.myInput3[1] = np.float32(6.75)  # 6.75 is precisely representable as float32
                assert self.myInput3[1] == 6.75

                # TODO probably it will also make sense to go over all other types, including bool...


                # calling members of np.array is directly possible
                assert self.myInput1.shape == (2,)

                # val is not a deep copy
                assert val[0] == self.myInput1[0]
                val[0] = 4
                assert val[0] == self.myInput1[0]
                self.myInput1[0] = 3
                assert self.myInput1[0] == 3
                assert val[0] == self.myInput1[0]
                assert val[0] == 3

                val = self.myInput2.readAndGet()
                valSum = sum(val)
                self.myOutput2.set(range(valSum, valSum + 10))
                self.myOutput2.write()

                for i in range(0, 5):
                    self.myInputPoll.read()  # non-blocking (poll)
                    assert (self.myInputPoll == [43, 2]).all()

                self.myInputWB.read()
                assert self.myInputWB == 15
                self.myInputWB.setAndWrite([28])
                assert self.myInputWB == 28

                self.myOutputRB.setAndWrite([120, 121])
                self.myOutputRB.read()
                assert (self.myOutputRB == [130, 131]).all()

                self.myInput1.read()

        except AssertionError as e:
            print("\n".join(traceback.format_exception(e)))
            sys.stdout.flush()
            self.testError.setAndWrite("\n".join(traceback.format_exception(e)))

# We cannot check the return channel of a ArrayOutputPushRB through the control system, so we use a second
# module for this purpose


class MySecondMod(ac.ApplicationModule):
    def __init__(self, owner, name, description):
        super().__init__(owner, name, description)

        self.input = ac.ArrayPushInputWB(ac.DataType.int32, self, "/SomeName/ArrayOutRB", "", 2, "")

        # these outputs are used to transport subject-to-test data to the C++ side for checking in the BOOST test case
        self.testError = ac.ScalarOutput(ac.DataType.string, self, "TestError", "", "")

    def mainLoop(self):
        try:
            assert (self.input == [42, 43]).all()  # initial value as set in prepare()
            self.input.read()
            assert (self.input == [120, 121]).all()
            self.input.setAndWrite([130, 131])
        except AssertionError as e:
            self.testError.setAndWrite("\n".join(traceback.format_exception(e)))


ac.app.mod = MyMod(ac.app, "SomeName", "Module's description")
ac.app.mod2 = MySecondMod(ac.app, "Foo", "Bar")
