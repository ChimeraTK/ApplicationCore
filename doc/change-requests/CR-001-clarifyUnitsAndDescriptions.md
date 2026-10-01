# CR-001: clarify unit and description sources

Status: specification

## Background and current state

Currently, when there is more than one source in a variable network for unit or description, the implementation of `ConnectionMaker::checkNetworks()`
simply picks the first non-empty description. So the selected description depends on variable registration order, which is not reliable. 
We need more control over which description/unit appears in the CS.

We also must define how module descriptions enter CS description texts.
Currently, all the DOOCS descriptions properties used by DoocsAdapter are of type D_string, which only supports 80 characters.
A feature request ticket in doocs-serverlib already exists, for in the future use D_text, which supports much longer strings.
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

In the owner hierarchy moduleA.moduleB.outputC, the overall description should be "descriptionOfA - descriptionOfB - descriptionOfC".
If any involved descriptions are empty, the number of dashes is reduced accordingly, e.g. if descriptionOfA="", we get "descriptionOfB - descriptionOfC".
Note, the owner hierarchy does not necessarily conform with the address space hierarchy. Especially, modules consuming data from other
modules will usually adapt the input address from another module, e.g. if `moduleA.moduleD.inputE` reads from 
`moduleA.moduleB.outputC`, both share address `/moduleA/moduleB/outputC`, but inputE description will be "descriptionOfA - descriptionOfD - descriptionOfE".

### How should we specify the prioritization?

* Idea 1: use exclamation marks at the beginning of the strings, to mark priority. The counting of leading exclamation marks defines the priority.
  Leading exclamation marks in a module description also count in.
  These exclamation marks are removed before further string processing.
* Idea 2: instead of positive priorities, use negative ones, indicated by leading question marks.
  A question mark indicates uncertainty whether the description would fit in all use cases.
* We could also combine exclamation marks and question marks.
  
It is important that a server consuming a generic module can overwrite variable descriptions of the generic module, without 
changing the latter's source code. So either, the instantiation of the generic module must allow indicating uncertainty about
its descriptions, or the server's own code must be able to prioritize its own descriptions.

**Open question Q1**: should we allow prioritization of a consumer's variable description over a feeder's variable description?
The consideration of generic modules calls for allowing this, although the other feeder considerations above did not require it.

**Open question Q2**: should we synchronize prioritization of unit and description texts?
Then, the unit string would never have exclamation/question mark prefixes.
It might be confusing if we mix units and descriptions from different sources.
  
### Example

(assuming Q1=Q2=yes)
`moduleA.moduleB.outputC` with descriptionOfA="", descriptionOfB="Oscilloscope", descriptionOfC="ch1" converts into 
description text "Oscilloscope - ch1" with priority=0.
If this is connected to `moduleA.moduleD.inputE` with descriptionD="", descriptionOfE="!phase deviation", converts into 
description text "phase deviation" with priority=1.
CS side description text becomes "phase deviation", so here, differently from default behavior, consumer description is used.
  
### How should we handle remaining ambiguities?

("remaining ambiguity" means, not resolved by explicit prioritization)
For now, just implement a warning. Immediately implementing a `logic_error` would force too much work load on us.
In the long run, we should throw a `logic_error`.

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

We should output a `<description>` tag for each node.
`<description>` should have attributes `direction` ("feeding" or "consuming") and `peer` using same name as `<peer>` tag in `<connections>`.
`<description>` tags should be listed in reverse priority, so the selected one comes first.

Similarly, for units, add the same attributes, and let `<unit>` appear more than once.

## Implementation notes and alternatives considered

## Test plan

## Further work items

