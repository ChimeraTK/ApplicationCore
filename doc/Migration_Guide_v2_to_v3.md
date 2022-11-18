ApplicationCore 2.x -> 3.0 migration guide {#migration_guide_v2_to_v3}
====================================================================

## General ##

* Some header files have been removed. Make sure to build against a clean install directory without any stray old header files!

### Old-style servers (defineConnections still overridden) ###

* The ControlSystemModule does not exist any more. All PVs are connected to the control system automatically.
* Do not use connectTo() and operator>> for individual connections. Connected variables must have same fully qualified name in the directory hierarchy (formerly known as virtual hierarchy).
* The DeviceModule works now like the previous ConnectingDeviceModule, i.e. it publishes all registers in its neighbour directory. Use as many DeviceModules as necessary for the same device (e.g. if different triggers are used).
* Use fully qualified variable or module names (ModuleGroup, ApplicationModule, VariableGroup) to connect to variables of other modules.
* Avoid that technique (point 3) to connect to device registers whose names do not match the application structure. Instead change the logical name mapping file such that it matches the application structure.
* Also check points mentioned for new-style servers!

### New-style servers (no defineConnections) ###

* The DMAP file path needs to be set earlier than before, namely before the first (Connecting)DeviceModule is constructed. Create an instance of ChimeraTK::SetDMapFilePath as first data member of the application to set the path early enough.
* Signature of device recovery handlers changed (argument is now of the type ChimeraTK::Device&).
* Recommended: Switch from deprecated ConnectingDeviceModule to DeviceModule (type alias for compatibility exists).
* Recommended: Switch from HierarchyModifyingGroup and ModifyHierarchy to plain VaraibleGroup and plain accessors, which now accept qualified paths (compatibility layers exist).
* Path names of device status variables changed in some cases when using a CDD instead of an alias name, since the name is more strictly stripped of special characters. See ChimeraTK::Utilities::stripName() for a function to strip the special characters programmatically.
* /Devices/.../deviceBecameFunctional is now of the type ChimeraTK::Void.
* When specifying a trigger in DeviceModule constructor that is not part of another typed network (i.e. just connected to the control system), its type wil be ChimeraTK::Void
* HierarchyModifyingGroup (now VariableGroup): consecutive slashes are now treated differently, they move to root and are no longer treated just as a single slash. Example: a VariableGroup with the name `some/path//with/two/slashes` will have the same effect as a VariableGroup `/with/two/slashes`.
