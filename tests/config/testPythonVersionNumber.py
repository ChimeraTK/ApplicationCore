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

        self.testError = ac.ScalarOutput(ac.DataType.string, self, "TestError", "", "")

    def mainLoop(self):
        try:
            usedDate = dt.datetime.now() - dt.timedelta(hours=2)
            version1 = ac.VersionNumber()
            version2 = ac.VersionNumber()
            version3 = ac.VersionNumber(usedDate)
            version4 = ac.VersionNumber(None)

            assert version1 < version2, "smaller"
            assert version1 != version2, "not equal"
            assert version1 == version1, "equal"

            assert version4 < version1, "smaller 41"
            assert version4 < version2, "smaller 42"
            assert version4 < version3, "smaller 43"

            assert version3.getTime() == usedDate, "Date roundtrip works"

        except AssertionError as e:
            self.testError.setAndWrite("\n".join(traceback.format_exception(e)))


ac.app.testRunner = TestRunner(ac.app, "VersionTestRunner", "Simple module for testing the VersionNumber binding")
