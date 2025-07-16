#!/usr/bin/python3

import sys
import os
import os.path
import traceback
from numpy import testing as t

# fmt: off
# Hack to insert the python path for the locally compiled module in the
# test script
sys.path.insert(0, os.path.abspath(os.path.join(os.curdir, "..")))
import PyApplicationCore as ac  # NOQA
# fmt: on


class MyMod(ac.ApplicationModule):

    def __init__(self, owner, name, description):
        super().__init__(owner, name, description)

        self.testError = ac.ScalarOutput(ac.DataType.string, self, "testError", "", "")

    def mainLoop(self):
        try:
            config = self.appConfig()

            # Getting scalars and arrays with default
            assert (config.get(ac.DataType.string, "stringScalar") == "a string scalar")
            assert (config.get(ac.DataType.string, "thisDoesNotExist", "a default") == "a default")

            assert (config.getArray(ac.DataType.string, "stringArray") == ["a", "string", "array"])
            assert (config.getArray(ac.DataType.string, "thisDoesNotExists", ["a", "default"]) == ["a", "default"])

            # Some smoketest for type conversions

            t.assert_allclose(config.get(ac.DataType.float32, "floatScalar"), 815.4711)
            t.assert_allclose(config.get(
                ac.DataType.float32, "thisDoesNotExist", 47.11), 47.11)

            t.assert_allclose(config.getArray(ac.DataType.float32, "floatArray"),
                              [0.5, 0.6, 1.5, 1.6, 1.7, 1.8])
            t.assert_allclose(config.getArray(ac.DataType.float32, "thisDoesNotExist",
                                              [4.5, 4.6, 5.0]), [4.5, 4.6, 5.0])
        except ac.LogicError as e:
            self.testError.setAndWrite("\n".join(traceback.format_exception(e)))
        except AssertionError as e:
            self.testError.setAndWrite("\n".join(traceback.format_exception(e)))


ac.app.userModule = MyMod(ac.app, "UserModule", "module for testing of appConfig bindings")
