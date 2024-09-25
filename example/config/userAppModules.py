#!/usr/bin/python3

import sys
import os
import os.path
import traceback

import PyApplicationCore as ac

# It is now possible to define ApplicationCore application modules in Python. This relies on system-installed python interpreter
# and python modules. The implementation makes use of pybind11, in order to bind C++ library functions. The function signatures
# were chosen to match their C++ origin as far as possible.
# Limitations are: 
# In python, there can be only a single interpreter instance used commonly by all threads. Although each 
# application module gets its own thread - like when you implement it in C++ - you should be aware that they share the same global
# python context, in particular they share import statements and may block each other, in case you do not release the global 
# interpreter lock.


# application module that can be used as drop-in replacement C++-defined SetpointRamp
class SetpointRamp(ac.ApplicationModule):

    # we must define __init__ in order to define owner, name, and decription of this application module
    # this is also the place to define inputs and outputs as ChimerTK accessors.
    def __init__(self, owner):
        super().__init__(owner, "SetpointRamp", "Slow ramping of temperator setpoint")

        self.operatorSetpoint = ac.ScalarPollInput(ac.DataType.float32, self, "operatorSetpoint", "degC", "description")
        
        self.ctrl = ac.VariableGroup(self, "/ControlUnit/Controller", "")
        # elements of the VariableGroup can be added as dynamic attributes
        # (alternatively, it would be possible to sub-class VariableGroup)
        self.ctrl.actualSetpoint = ac.ScalarOutput(ac.DataType.float32, self.ctrl, "temperatureSetpoint", "degC", "...")
        
        self.trigger = ac.ScalarPushInput(ac.DataType.uint64, self, "/Timer/tick", "", "...")
        
    def mainLoop(self):
        # at this point, initial values for inputs are already available
        
        maxStep = 1  # choose a different step-size than in C++ example so we can see some difference
        while True:            
            # note, scalar accessors automatically convert to their corresponding python value type (float in our case), while 
            # array accessors automatically convert to numpy arrays.
            # note, += operator is (yet?) available, so need to use .set()
            # syntax like self.ctrl.actualSetpoint = self.ctrl.actualSetpoint+1 does _not_ work since it overwrites the accessor reference by a float.
            self.ctrl.actualSetpoint.set( self.ctrl.actualSetpoint + max( min(self.operatorSetpoint - self.ctrl.actualSetpoint, maxStep), -maxStep) )
            
            # print("new setpoint value : " + str(self.ctrl.actualSetpoint))
            self.writeAll()
            
            self.readAll() # waits until next trigger received, then reads operatorSetpoint

# Finally, instantiate user-defined application modules and reference them somehow from ac.app so they don't get deleted
# owner = ac.app means the module is mounted at the root of the process variable household.
# Differently from usual python semantics, the owner will manage lifetime of its owned elements.
ac.app.module1 = SetpointRamp(ac.app)

# we could also put the whole application module 'SetpointRamp' into a module group 'SomeGroup' like this:
#ac.app.someGroup = ac.ModuleGroup(ac.app, "SomeGroup", "group description")
#ac.app.someGroup.module1 = SetpointRamp(ac.app.someGroup)
