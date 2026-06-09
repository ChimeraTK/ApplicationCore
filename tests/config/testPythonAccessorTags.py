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

    def __init__(self, owner, name, description):
        super().__init__(owner, name, description)

        # Test individual accessor types with single tags
        # The tags will be verified from C++ side via the model API
        self.sOut = ac.ScalarOutput(ac.DataType.int32, self, "sOut", "", "", tags={"scalarOutput"})
        self.aOut = ac.ArrayOutput(ac.DataType.int32, self, "aOut", "", 2, "", tags={"arrayOutput"})
        self.sIn = ac.ScalarPushInput(ac.DataType.int32, self, "sIn", "", "", tags={"scalarPushInput"})
        self.aIn = ac.ArrayPushInput(ac.DataType.int32, self, "aIn", "", 2, "", tags={"arrayPushInput"})
        self.sInWB = ac.ScalarPushInputWB(ac.DataType.int32, self, "sInWB", "", "", tags={"scalarPushInputWB"})
        self.aInWB = ac.ArrayPushInputWB(ac.DataType.int32, self, "aInWB", "", 1, "", tags={"arrayPushInputWB"})
        self.sPoll = ac.ScalarPollInput(ac.DataType.int32, self, "sPoll", "", "", tags={"scalarPollInput"})
        self.aPoll = ac.ArrayPollInput(ac.DataType.int32, self, "aPoll", "", 2, "", tags={"arrayPollInput"})
        self.sPushRB = ac.ScalarOutputPushRB(ac.DataType.int32, self, "sPushRB", "", "", tags={"scalarOutputPushRB"})
        self.aPushRB = ac.ArrayOutputPushRB(ac.DataType.int32, self, "aPushRB", "", 2, "", tags={"arrayOutputPushRB"})
        self.sRevRec = ac.ScalarOutputReverseRecovery(ac.DataType.int32, self, "sRevRec", "", "", tags={"scalarRevRec"})
        self.aRevRec = ac.ArrayOutputReverseRecovery(
            ac.DataType.int32, self, "aRevRec", "", 2, "", tags={"arrayRevRec"})

        # Accessor with no tags (default empty set)
        self.sNoTags = ac.ScalarOutput(ac.DataType.int32, self, "sNoTags", "", "")

        # Accessor with the alternative DataType.TheType constructor variant, testing two tags
        self.sTwoTags = ac.ScalarOutput(ac.DataType.TheType.int32, self, "sTwoTags", "", "", tags={"tagA", "tagB"})

    def mainLoop(self):
        pass


ac.app.mod = MyMod(ac.app, "TagTestModule", "Module to test accessor tags")
