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

    def __init__(self, owner, name, description):
        super().__init__(owner, name, description)

        self.testError = ac.ScalarOutput(ac.DataType.string, self, "testError", "", "")

    def mainLoop(self):
        try:
            assert self.getName() == "UserModule"
            assert self.getDataValidity() == ac.DataValidity.ok
            old = self.getDataFaultCounter()
            self.incrementDataFaultCounter()
            assert self.getDataFaultCounter() == old + 1
            assert self.getDataValidity() == ac.DataValidity.faulty
            self.decrementDataFaultCounter()
            assert self.getDataValidity() == ac.DataValidity.ok
            assert self.getDataFaultCounter() == old

            # see if getting a config works and it is our own config
            conf = self.appConfig()
            try:
                assert conf.get(ac.DataType.string, "PythonModules/AppModule/path") == "testPythonApplicationModule"
            except RuntimeError:
                assert False

        except AssertionError as e:
            self.testError.setAndWrite("\n".join(traceback.format_exception(e)))


class SecondMod(ac.ApplicationModule):
    def __init__(self, owner, name, description):
        super().__init__(owner, name, description)
        self.disable()

    def mainLoop(self):
        pass


ac.app.myMod = MyMod(ac.app,"UserModule", "module for testing of VariableGroup bindings")
ac.app.secondMod = SecondMod(ac.app,"DisabledMod", "module should be disabled")
