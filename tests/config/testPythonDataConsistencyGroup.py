import sys
import os
import os.path
import traceback
import threading

# fmt: off
# Hack to insert the python path for the locally compiled module in the
# test script
sys.path.insert(0, os.path.abspath(os.path.join(os.curdir, "..")))
import PyApplicationCore as ac  # NOQA
# fmt: on

class Sender(ac.ApplicationModule):

    def __init__(self, owner, name, description):
        super().__init__(owner, name, description)

        self.out1 = ac.ScalarOutput(ac.DataType.int32, self, "in1", "", "")
        self.out2 = ac.ScalarOutput(ac.DataType.int32, self, "in2", "", "")
        self.out3 = ac.ScalarOutput(ac.DataType.int32, self, "in3", "", "")

        self.letsStart = ac.ScalarPushInput(ac.DataType.int32, self, "letsStart", "", "")

    def prepare(self):
        self.out1.write()
        self.out2.write()
        self.out3.write()

    def mainLoop(self):
        sys.stdout.flush()

        self.setCurrentVersionNumber(ac.VersionNumber())
        self.out1.write()
        self.out3.write()
        self.out2.write()

        self.setCurrentVersionNumber(ac.VersionNumber())
        self.out1.write()
        self.out3.write()
        self.setCurrentVersionNumber(ac.VersionNumber())
        self.out2.write()

        # for last part of test, with MatchingMode.historized
        self.out2.write()
        self.out1.write()


class Receiver(ac.ApplicationModule):

    def __init__(self, owner, name, description):
        super().__init__(owner, name, description)

        self.in1 = ac.ScalarPushInput(ac.DataType.int32, self, "in1", "", "")
        self.in2 = ac.ScalarPushInput(ac.DataType.int32, self, "in2", "", "")
        self.in3 = ac.ScalarPushInput(ac.DataType.int32, self, "in3", "", "")

        self.letsStart = ac.ScalarOutput(ac.DataType.int32, self, "letsStart", "", "")

        self.testError = ac.ScalarOutput(ac.DataType.string, self, "testError", "", "")


    def mainLoop(self):

        try:
            sys.stdout.flush()
            rag = self.readAnyGroup()

            dg = ac.DataConsistencyGroup(self.in1, self.in2)
            dg.add(self.in3)

            sys.stdout.flush()
            self.letsStart.write()

            change = rag.readAny()
            isValid = dg.update(change)
            assert not isValid
            change = rag.readAny()
            isValid = dg.update(change)
            assert not isValid
            change = rag.readAny()
            isValid = dg.update(change)
            assert isValid


            change = rag.readAny()
            isValid = dg.update(change)
            assert not isValid
            change = rag.readAny()
            isValid = dg.update(change)
            assert not isValid
            change = rag.readAny()
            isValid = dg.update(change)
            assert not isValid

            # Note, it is fine to create a DataConsistencyGroup with MatchingMode.historized after MatchingMode.exact,
            # but the other way round would not be valid.
            dgh = ac.DataConsistencyGroup(self.in1, self.in2, mode=ac.MatchingMode.historized, histLen = 1)
            change = rag.readAny()
            isValid = dgh.update(change)
            assert isValid

            self.testError.setAndWrite("ok")


        except AssertionError as e:
            print("Exception: "+"\n".join(traceback.format_exception(e)))
            sys.stdout.flush()
            self.testError.setAndWrite("\n".join(traceback.format_exception(e)))



ac.app.sender = Sender(ac.app, "UserModule", "")
ac.app.receiver = Receiver(ac.app, "UserModule", "")
