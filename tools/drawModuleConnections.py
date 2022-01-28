#!/bin/python3

import xml.etree.ElementTree as ET
import sys

ns = "{https://github.com/ChimeraTK/ApplicationCore}"

moduleConnections = {}

def parseVariable(node) :
  varDir = node.find(ns+'direction').text

  feeder = None
  moduleList = set()

  if varDir == 'control_system_to_application' or varDir == 'control_system_to_application_with_return' :
    return
    feeder = 'ControlSystem'
#  else :
#    moduleList.add("ControlSystem")

  for peer in node.find(ns+'connections') :
    peerType = peer.attrib['type']
    peerDir = peer.attrib['direction']
    if peerType == "ApplicationModule" or peerType == "Device" :
      if peerType == "ApplicationModule" :
        peerName = peer.attrib['class']
      else :
        peerName = 'Device:'+peer.attrib['name']
      if peerDir == 'consuming' :
        moduleList.add(peerName)
      else :
        if feeder != None :
          print("ERROR: Found two feeders!")
          print(node.attrib['name'])
          sys.exit(1)
        feeder = peerName

  if feeder == None :
    print("ERROR: No feeders found!")
    print(node.attrib['name'])
    sys.exit(1)

    
  for m in moduleList :
    if not m in moduleConnections.keys():
      moduleConnections[m] = set()
  if not feeder in moduleConnections.keys():
    moduleConnections[feeder] = set()
    
  for m in moduleList :
    moduleConnections[feeder].add(m)


def parseDirectory(subtree) :

  for node in subtree :

    if node.tag == ns+'directory' :
      parseDirectory(node)
    elif node.tag == ns+'variable' :
      parseVariable(node)
    

tree = ET.parse('llrfctrl.xml')
root = tree.getroot()
    
parseDirectory(root)

print("digraph {")

def makeNodeName(label) :
  name = label.replace(':', '_')
  name = name.replace('/', '_')
  name = name.replace('<', '_')
  name = name.replace('>', '_')
  name = name.replace(',', '_')
  name = name.replace(' ', '_')
  return name


def defineNode(label) :
  if not label in nodeIds.keys() :
    nodeIds[label] = makeNodeName(label)
    print(nodeIds[label] + ' [label="'+label+'", style=filled', end="")
    if label.startswith("Device:") :
      print(', fillcolor=silver', end="")
    else :
      print(', fillcolor=lightgreen', end="")
    print(']')


nodeIds = {}
for source in moduleConnections.keys() :
  defineNode(source)
  for target in moduleConnections[source] :
    defineNode(target)


for source in moduleConnections.keys() :
  if source == None :
    continue
  print(nodeIds[source] + ' -> { ', end="")
  for target in moduleConnections[source] :
    print(nodeIds[target]+' ', end="")
  print('}')

print('}')

