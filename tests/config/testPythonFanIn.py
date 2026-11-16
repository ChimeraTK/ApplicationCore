import os.path
import sys

sys.path.insert(0, os.path.abspath(os.path.join(os.curdir, "..")))
import PyApplicationCore as ac  # NOQA


class FanInTest(ac.ApplicationModule):
    def __init__(self, owner, name, description):
        super().__init__(owner, name, description)
        self.scalar = ac.ScalarFanIn(
            ac.DataType.int32,
            self,
            "scalar",
            "",
            "",
            lambda changed, inputs: sum(input.get() for input in inputs),
            ["scalarA", "scalarB"],
        )
        self.result = ac.ScalarOutput(ac.DataType.string, self, "result", "", "")

    def mainLoop(self):
        group = self.readAnyGroup()
        group.readAny()
        self.result.setAndWrite(
            f"{self.scalar.get()}"
        )


ac.app.fanInTest = FanInTest(ac.app, "FanInTest", "Python FanIn test")
