import datetime as dt
import os.path
import sys
import traceback


# fmt: off
# Hack to insert the python path for the locally compiled module in the
# test script
sys.path.insert(0, os.path.abspath(os.path.join(os.curdir, "..")))
import PyApplicationCore as ac  # NOQA
# fmt: on


class TestRunner(ac.ApplicationModule):
    def __init__(self, owner, name, description):
        super().__init__(owner, name, description)

        self.in1 = ac.ScalarPushInputWB(
            ac.DataType.int32, self, "in1", "", "First validated input"
        )
        self.in2 = ac.ArrayPushInputWB(
            ac.DataType.int32, self, "in2", "", 5, "First validated input"
        )
        self.testError = ac.ScalarOutput(ac.DataType.string, self, "TestError", "", "")
        self.validator = ac.UserInputValidator()
        self.errorFunctionCalled = ac.VoidOutput(self, "errorFunctionCalled", "")

    def prepare(self):
        self.validator.add(
            f"From Python: ({self.getName()}) in1 needs to be smaller than 10",
            lambda: self.in1.get() < 10,
            self.in1,
        )
        self.validator.add(
            f"From Python: ({self.getName()}) Sum of in2 needs to be smaller than 10",
            lambda: sum(self.in2.get()) < 10,
            self.in2,
        )
        self.validator.add(
            f"From Python: ({self.getName()}) Elements in in2 need to be smaller than in1",
            lambda: all(v <= self.in1.get() for v in self.in2.get()),
            self.in1,
            self.in2,
        )

        # This is abusing that the return value of print is None so it is false and we do the write
        self.validator.setErrorFunction(
            lambda x: print(x) or self.errorFunctionCalled.write()
        )
        self.validator.setFallback(self.in1, 7)
        self.validator.setFallback(self.in2, [1, 1, 1, 1, 1])

    def mainLoop(self):
        try:

            group = self.readAnyGroup()
            change = ac.TransferElementID()
            assert self.validator.validate(change)
            assert self.in1.get() == 7, "Using the set Fall-back value"
            assert (self.in2.get() == [1, 1, 1, 1, 1]).all(), "Using the set fall-back value"

            change = group.readAny()
            assert not self.validator.validate(change)
            assert self.in1.get() == 8, "Setting correct value"

            change = group.readAny()
            assert self.validator.validate(change)
            assert self.in1.get() == 8, "Keeping previous correct value"

            change = group.readAny()
            assert not self.validator.validate(change)
            assert (self.in2.get() == [2, 2, 2, 2, 1]).all(), "Setting a new value"

            change = group.readAny()
            assert self.validator.validate(change)
            assert (self.in2.get() == [2, 2, 2, 2, 1]).all(), "Using the previous value"

            change = group.readAny()
            assert self.validator.validate(change)
            assert (self.in2.get() == [2, 2, 2, 2, 1]).all(), "Using the previous value"

        except AssertionError as e:
            self.testError.setAndWrite("\n".join(traceback.format_exception(e)))


ac.app.testRunner = TestRunner(
    ac.app,
    "UserInputValidatorTestRunner",
    "Simple module for testing the UserInputValidator binding",
)
