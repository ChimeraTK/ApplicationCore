# CR-001: clarify unit and description sources

Status: specification

## Background and current state

Currently, when there is more than one source in a variable network for unit or description, the implementation of `ConnectionMaker::checkNetworks()`
simply picks the first non-empty description. So the selected description depends on variable registration order, which is not reliable. 
We need more control over which description/unit appears in the CS.

We also must define how module descriptions enter CS description texts.
Currently, all the DOOCS descriptions properties used by DoocsAdapter are of type `D_string`, which only supports 80 characters.
A feature request ticket in `doocs-serverlib` already exists, requesting that in the future `D_text` should be used, which supports much longer strings.
For now, we have to live with the limitation and see how to get reasonable information into exposed units and descriptions
(as recently implemented in PR setDescriptions of DoocsAdapter).

### Possible considerations of a VariableNetwork

Feeder is one of: device, control system, application module or `ConfigReader` module.
There may be one or more consumers.

* In general, the feeder's description should win. ("in general" means there are exceptions)
  Why?
  - usually, the data source will know most about the data
  - While there may be several consumers, there is only one feeder

* If the feeder's description is empty, a consumer's description should win.
  This introduces ambiguity, if consumers' descriptions deviate. 
  - ignore empty descriptions
  - we can resolve ambiguity by a prioritization mechanism
  - if ambiguity is not resolved, we must warn or exit with error
  
* The server developer should always have the possibility to change the feeder's description.
  - feeder = device: use logical name mapping and `setDescription` plugin (parameters=description,engineeringUnit) (already implemented)
  - feeder = application module: usual process variable constructor
  - feeder = `ConfigReader` module: extend xml configuration syntax
  - feeder = control system:
    We do not want to read pack descriptions set via control system, so we must resolve the ambiguity of possibly more than one consumer.

* As a last resort, units/descriptions can always be forced on the control system side.
  This is already implemented within DoocsAdapter (setDescriptions PR).


## Requirements

### How should module descriptions enter

We define a convention: The module description set in the constructor is not the description of the module's intent 
(which would be described in source code comments),
but a string-snippet to be placed into the description of its input and output variables.

In the owner hierarchy `moduleA.moduleB.outputC`, the overall description should be "descriptionOfA - descriptionOfB - descriptionOfC".
If any involved descriptions are empty, the number of dashes is reduced accordingly, e.g. if descriptionOfA="", we get "descriptionOfB - descriptionOfC".
Note, the owner hierarchy does not necessarily conform with the address space hierarchy. Especially, modules consuming data from other
modules will usually adapt the input address from another module, e.g. if `moduleA.moduleD.inputE` reads from 
`moduleA.moduleB.outputC`, both share address `/moduleA/moduleB/outputC`, but `inputE` description will be "descriptionOfA - descriptionOfD - descriptionOfE".

### How should we specify the prioritization?

* Use exclamation marks at the beginning of the description strings, to mark positive priority. The count of leading exclamation marks defines the priority.
  Leading exclamation marks in a module description also count in.
  These exclamation marks are removed before further string processing.
* Leading question marks are used for negative priorities, analog to exclamation marks.
  A question mark indicates uncertainty whether the description would fit for all use cases.
* It is allowed to combine exclamation marks and question marks.
* For the rare case, that the actual description should begin with a exclamation mark or question mark, allow escapting by backslashes, 
  i.e. `\!` -> `!` and `\?` -> `?`.
  
It is important that a server consuming a generic module can overwrite variable descriptions of the generic module, without 
changing the latter's source code. So either, the instantiation of the generic module must allow indicating uncertainty about
its descriptions, or the server's own code must be able to prioritize its own descriptions.

Quuestion for discussion: should we allow prioritization of a consumer's variable description over a feeder's variable description?
The consideration of generic modules calls for allowing this, although the other feeder considerations above did not require it.
Decision: Yes, a higher priority of a consumer overwrites the description of a feeder.

Question for discussion: should we synchronize prioritization of unit and description texts? Or do we need a different concept?
Decision: 
Handle units differently. They are much more tied to the values, and must match.
If standard SI abbreviations are used, unit texts can be made to match.
Ambiguities how to write down the physical formula still exist, but it is better to force the server developer to fix the problem than to automatically select something.
  
### Example

`moduleA.moduleB.outputC` with descriptionOfA="", descriptionOfB="Oscilloscope", descriptionOfC="ch1" converts into 
description text "Oscilloscope - ch1" with priority=0.
If this is connected to `moduleA.moduleD.inputE` with descriptionD="", descriptionOfE="!phase deviation", converts into 
description text "phase deviation" with priority=1.
CS side description text becomes "phase deviation", so here, differently from default behavior, consumer description is used.
  
### How should we handle remaining ambiguities?

("remaining ambiguity" means, not resolved by explicit prioritization)
For now, just implement a warning. Immediately implementing a `logic_error` would force too much work load on us.
In the long run, we should throw a `logic_error`.
Only warn or throw when involved candidate priorities are >= 0, pick any non-empty description if all candidates have the same negative priority.
Since there is no priority concept for units, any mismatch or units will create a warning, or `logic_error` in the long run.

### ConfigReader

In the server configuration, `ConfigReader` should also allow setting unit and description texts via XML attribute.

We define `unit` and `description` like in the example below. `description` is also allowed on modules.

```XML
<configuration>
  <variable name="varFloat" type="float" value="3.1415" unit="MV/m" description="A scalar with unit and description"/>
  <variable name="intArray" type="int32" unit="mA" description="An array with unit and description">
    <value i="0" v="10"/>
    <value i="1" v="9"/>
  </variable>
  <module name="moduleA" description="descriptionOfA">
    <variable name="inputC" type="int16" value="1" unit="mA" description="descriptionOfC" />
  </module>
</configuration>
```

### xmlGenerator output

We need debugging possibilities to find possibly non-matching candidate texts.
We should put all units/descriptions of a variable network into xmlGenerator output.

Keep the winning `<description>` and `<unit>` as is, but in the `<connections>` list, where tags `<peer>` are listed for the network, add in description and unit information.
Inside every element `<peer>`, include a `<description>` and `<unit>` if they are non-empty, respectively.
Leave the priority markers in the description string, that will help with manual inspection.

## Implementation notes and alternatives considered

## Test plan

## Further work items

