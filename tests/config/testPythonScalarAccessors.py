#!/usr/bin/python3

import sys
import os
import os.path
import traceback
import math

# fmt: off
# Hack to insert the python path for the locally compiled module in the
# test script
sys.path.insert(0, os.path.abspath(os.path.join(os.curdir, "..")))
import PyApplicationCore as ac  # NOQA
# fmt: on


class MyMod(ac.ApplicationModule):

    def __init__(self, owner, name, description):
        super().__init__(owner, name, description)

        self.v0 = ac.ScalarOutput(ac.DataType.int32, self, "/Test/V0", "", "")
        self.v1 = ac.ScalarOutput(ac.DataType.int32, self, "/Test/V1", "", "")
        self.v2 = ac.ScalarOutput(ac.DataType.int32, self, "/Test/V2", "", "")
        self.v3 = ac.ScalarOutput(ac.DataType.int32, self, "/Test/V3", "", "")
        self.v4 = ac.ScalarOutput(ac.DataType.int32, self, "/Test/V4", "", "")
        self.v5 = ac.ScalarOutput(ac.DataType.float32, self, "/Test/V5", "", "")
        self.v6 = ac.ScalarOutput(ac.DataType.float32, self, "/Test/V6", "", "")
        self.v7 = ac.ScalarOutput(ac.DataType.float32, self, "/Test/V7", "", "")
        self.v8 = ac.ScalarOutput(ac.DataType.string, self, "/Test/V8", "", "")
        self.end = ac.ScalarPushInput(ac.DataType.int32, self, "/Test/end", "", "")

        self.result = ac.ScalarOutput(ac.DataType.string, self, "/Test/Result", "", "")

    def mainLoop(self):

        self.v0.setAndWrite(0)
        self.v1.setAndWrite(1)
        self.v2.setAndWrite(2)
        self.v3.setAndWrite(1)
        self.v4.setAndWrite(42)
        self.v5.setAndWrite(-33.21)
        self.v6.setAndWrite(11.7832)
        self.v7.setAndWrite(2.75)
        self.v8.setAndWrite("some string")
        self.result.setAndWrite("")

        while True:
            try:

                # check comparisons between accessorts

                assert self.v1 == self.v3
                assert self.v1 < self.v2
                assert self.v2 > self.v1
                assert self.v1 != self.v2
                assert self.v1 >= self.v3
                assert self.v2 >= self.v3
                assert self.v1 <= self.v2
                assert self.v1 <= self.v3

                assert not (self.v1 != self.v1)
                assert not (self.v1 > self.v2)
                assert not (self.v2 < self.v1)
                assert not (self.v1 == self.v2)
                assert not (self.v2 <= self.v3)
                assert not (self.v1 >= self.v2)

                # accessors and python objects / ints

                assert self.v1 == 1
                assert self.v1 < 3
                assert self.v2 > -1
                assert self.v1 != 2
                assert self.v1 >= 1
                assert self.v2 >= 1
                assert self.v1 <= 2
                assert self.v1 <= 1

                assert not (self.v1 != 1)
                assert not (self.v1 > 2)
                assert not (self.v2 < 1)
                assert not (self.v1 == 2)
                assert not (self.v2 <= 1)
                assert not (self.v1 >= 2)

                # switch directions

                assert 2 != self.v1
                assert 3 > self.v1
                assert 1 < self.v2
                assert 1 == self.v1
                assert -33 <= self.v2
                assert 2 <= self.v2
                assert 2 >= self.v1
                assert 1 >= self.v1

                assert not (100 == self.v1)
                assert not (3 < self.v1)
                assert not (-1 > self.v2)
                assert not (1 != self.v1)
                assert not (-12 >= self.v1)
                assert not (22 <= self.v1)

                # check math functions
                # assert @ : can't test matmul with scalars
                # check math with accessors on the left

                assert (self.v1 + 41) == 42
                assert (self.v1 - 41) == -40
                assert (self.v4 / 7) == 6
                assert (self.v4 // 10) == 4
                assert (self.v4 % 10) == 2
                assert divmod(self.v4, 4) == (10, 2)
                assert (self.v2 ** 3) == 8
                assert (self.v2 << 3) == 16
                assert (self.v4 >> 2) == 10
                assert (self.v4 & 2) == 2
                assert (self.v4 ^ 2) == 40
                assert (self.v4 | 1) == 43

                # check math with accessors on the right

                assert (41 + self.v1) == 42
                assert (41 - self.v1) == 40
                assert (84 / self.v4) == 2
                assert (7 // self.v2) == 3
                assert (7 % self.v2) == 1
                assert divmod(42, self.v2) == (21, 0)
                assert (3 ** self.v2) == 9
                assert (3 << self.v2) == 12
                assert (42 >> self.v2) == 10
                assert (2 & self.v4) == 2
                assert (2 ^ self.v4) == 40
                assert (1 | self.v4) == 43

                # TODO: check math with two accessors

                # check unary operations
                assert -self.v1 == -1
                eps = 0.00001
                assert -self.v5 - 33.21 <= eps
                assert +self.v4 == self.v4
                assert abs(self.v5) - 33.21 <= eps
                assert ~self.v1 == -2
                assert ~self.v4 == -43

                assert isinstance(int(self.v1), int)
                assert int(self.v5) == -33
                assert isinstance(float(self.v2), float)

                assert math.trunc(self.v6) == 11
                assert math.trunc(self.v5) == -33
                assert math.floor(self.v6) == 11
                assert math.floor(self.v5) == -34
                assert math.ceil(self.v6) == 12
                assert math.ceil(self.v5) == -33
                assert round(self.v6) == 12
                assert round(self.v6, 1) == 11.8
                assert round(self.v6, 2) == 11.78

                assert str(self.v7) == "2.75"
                assert str(self.v8) == "some string"
                assert bool(self.v7) == True
                assert bool(self.v0) == False

            except Exception as e:
                self.result.setAndWrite("\n".join(traceback.format_exception(e)))

            self.result.setAndWrite("Scalar test did not produce any Python Errors")

            self.end.readAndGet()


ac.app.myMod = MyMod(ac.app, "ScalarTestModule", "Module to check scalar comparison and math operators")
