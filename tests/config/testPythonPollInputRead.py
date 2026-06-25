#!/usr/bin/python3

import sys
import os
import traceback

# fmt: off
sys.path.insert(0, os.path.abspath(os.path.join(os.curdir, "..")))
import PyApplicationCore as ac  # NOQA
# fmt: on


class PollInputTestMod(ac.ApplicationModule):

    def __init__(self, owner, name, description):
        super().__init__(owner, name, description)

        # Scalar poll input — read() should delegate to readLatest() and never block
        self.scalarPollIn = ac.ScalarPollInput(
            ac.DataType.int32, self, "/ScalarPollIn", "", "")

        # Array poll input — read() should delegate to readLatest() and never block
        self.arrayPollIn = ac.ArrayPollInput(
            ac.DataType.int32, self, "/ArrayPollIn", "", 5, "")

        # Trigger to step the mainLoop
        self.end = ac.ScalarPushInput(
            ac.DataType.int32, self, "/End", "", "")

        # Error reporting
        self.testError = ac.ScalarOutput(
            ac.DataType.string, self, "/TestError", "", "")

    def mainLoop(self):
        while True:
            try:
                # read() on poll inputs must NOT block even if no new data is available.
                # These calls should return immediately by delegating to readLatest().
                self.scalarPollIn.read()
                self.arrayPollIn.read()

                self.testError.setAndWrite("")
            except Exception as e:
                self.testError.setAndWrite("\n".join(traceback.format_exception(e)))

            self.end.readAndGet()


ac.app.pollInputTestMod = PollInputTestMod(
    ac.app, "PollInputTestModule", "Test ScalarPollInput and ArrayPollInput read() delegation")
