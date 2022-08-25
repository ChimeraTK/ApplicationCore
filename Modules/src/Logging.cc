/*
 * Logging.cc
 *
 *  Created on: Apr 3, 2018
 *      Author: zenker
 */

#include "Logging.h"

#include "boost/date_time/posix_time/posix_time.hpp"

#include <fstream>
#include <ostream>
#include <sstream>
#include <string>
#include <vector>

using namespace logging;

std::ostream& logging::operator<<(std::ostream& os, const LogLevel& level) {
  switch(level) {
    case LogLevel::DEBUG:
      os << "DEBUG::";
      break;
    case LogLevel::INFO:
      os << "INFO::";
      break;
    case LogLevel::WARNING:
      os << "WARNING::";
      break;
    case LogLevel::ERROR:
      os << "ERROR::";
      break;
    default:
      break;
  }
  return os;
}

std::string logging::getTime() {
  std::string str;
  str.append(boost::posix_time::to_simple_string(boost::posix_time::microsec_clock::local_time()) + " ");
  str.append(" -> ");
  return str;
}

Logger::Logger(ctk::Module* module, const std::string& name, const std::string& description, const std::string& tag)
: VariableGroup(module, name, description),
  message(this, "message", "",
      "Message of the module to the logging System. The leading number indicates the log level "
      "(0: DEBUG, 1: INFO, 2: WARNING, 3: ERROR, 4;SILENT). A leading 5 is used internally for old messages.",
      {tag, module->getName()}),
  alias(this, "alias", "", "Alias used in the message as identifier.", {tag, module->getName()}) {}

void Logger::sendMessage(const std::string& msg, const logging::LogLevel& level) {
  if(message.isInitialised()) {
    while(!msg_buffer.empty()) {
      message = msg_buffer.front();
      message.write();
      msg_buffer.pop();
    }
    message = std::to_string(level) + msg + "\n";
    message.write();
    // set emtpy message to let the LoggingModule know that someone called writeAll() without sending a message
    message = std::to_string(logging::LogLevel::INTERNAL) + msg + "\n";
  }
  else {
    // only use the buffer until ctk initialized the process variables
    msg_buffer.push(std::to_string(level) + msg + "\n");
  }
}

void Logger::prepare() {
  // write initial value in order to bring LoggingModule to mainLoop()
  message.write();
}

LoggingModule::LoggingModule(ctk::EntityOwner* owner, const std::string& name, const std::string& description,
    ctk::HierarchyModifier hierarchyModifier, const std::unordered_set<std::string>& tags)
: ApplicationModule(owner, name, description, hierarchyModifier, tags) {
  for(auto tag : tags) {
    auto virtualLogging = getOwner()->findTag(tag);
    auto list = virtualLogging.getAccessorListRecursive();
    for(auto it = list.begin(); it != list.end(); ++it) {
      // do not add the module itself
      if(it->getOwningModule() == this) continue;
      try {
        // virtualLogging.getQualifiedName() returns the name of the app, e.g. /test and we remove that from the module
        // name , e.g. /test/MyModule
        auto moduleName =
            it->getOwningModule()->getQualifiedName().substr(virtualLogging.getQualifiedName().length() + 1);
        auto msgSource = std::find(sources.begin(), sources.end(), moduleName);
        if(msgSource == sources.end()) {
          // Create new MessageSource
          auto acc = getAccessorPair(moduleName, it->getOwningModule()->getName());
          (*it) >> acc;
          std::cout << "Registered module " << it->getOwningModule()->getQualifiedName() << " for logging."
                    << std::endl;
        }
        else {
          // Connect variables that was not connected when creating the MessageSource
          // On creation either alias or the message could have been connected -> now connect the other one
          if(it->getName() == "alias") {
            (*it) >> msgSource->data.alias;
          }
          else if(it->getName() == "message") {
            (*it) >> msgSource->data.msg;
          }
        }
      }
      catch(ChimeraTK::logic_error& e) {
        std::cerr << "Failed to add logging module: " << it->getOwningModule()->getQualifiedName()
                  << " Error: " << e.what() << std::endl;
      }
    }
  }
  if(sources.empty()) std::cerr << "LoggingModule did not find any module that uses a Logger." << std::endl;
}

ctk::RegisterPath LoggingModule::prepareHierarchy(const ctk::RegisterPath& namePrefix) {
  ctk::RegisterPath result;
  // create variable group map for namePrefix if needed
  if(groupMap.find(namePrefix) == groupMap.end()) {
    // search for existing parent (if any)
    auto parentPrefix = namePrefix;
    while(groupMap.find(parentPrefix) == groupMap.end()) {
      if(parentPrefix == "/") break; // no existing parent found
      parentPrefix = std::string(parentPrefix).substr(0, std::string(parentPrefix).find_last_of("/"));
    }
    // create all not-yet-existing parents
    while(true) {
      ctk::EntityOwner* owner = this;
      if(parentPrefix != "/") owner = &groupMap[parentPrefix];
      auto stop = std::string(namePrefix).find_first_of("/", parentPrefix.length() + 1);
      if(stop == std::string::npos) stop = namePrefix.length();
      ctk::RegisterPath name = std::string(namePrefix).substr(parentPrefix.length(), stop - parentPrefix.length());
      result = parentPrefix;
      parentPrefix /= name;
      if(parentPrefix == namePrefix) {
        // do not add the last hierachy level -> will be added by the MessageSource
        break;
      }
      groupMap[parentPrefix] = ctk::VariableGroup(owner, std::string(name).substr(1), "");
    }
  }
  return result;
}

void LoggingModule::broadcastMessage(std::string msg, const bool& isError) {
  if(msg.back() != '\n') {
    msg.append("\n");
  }

  std::string tmpLog = (std::string)logTail;
  if(tailLength == 0 && messageCounter > 20) {
    messageCounter--;
    tmpLog = tmpLog.substr(tmpLog.find_first_of("\n") + 1, tmpLog.length());
  }
  else if(tailLength > 0) {
    while(messageCounter >= tailLength) {
      messageCounter--;
      tmpLog = tmpLog.substr(tmpLog.find_first_of("\n") + 1, tmpLog.length());
    }
  }
  if(targetStream == 0 || targetStream == 2) {
    if(isError)
      std::cerr << msg;
    else
      std::cout << msg;
  }
  if(targetStream == 0 || targetStream == 1) {
    if(file->is_open()) {
      (*file) << msg.c_str();
      file->flush();
    }
  }
  tmpLog.append(msg);
  messageCounter++;
  logTail = tmpLog;
  logTail.write();
}

void LoggingModule::mainLoop() {
  file.reset(new std::ofstream());
  messageCounter = 0;
  std::stringstream greeter;
  greeter << getName() << " " << getTime() << "There are " << sources.size()
          << " modules registered for logging:" << std::endl;
  broadcastMessage(greeter.str());
  for(auto module = sources.begin(); module != sources.end(); module++) {
    broadcastMessage(std::string("\t - ") + module->sendingModule);
    id_list[module->data.msg.getId()] = &(*module);
  }
  auto group = readAnyGroup();
  std::string msg;
  MessageSource* currentSender;
  LogLevel level;

  while(1) {
    // we skip the initial value since it is empty anyway and set in Logger::prepare
    auto id = group.readAny();
    if(id_list.count(id) == 0) {
      throw ChimeraTK::logic_error("Cannot find  element id"
                                   "when updating logging variables.");
    }
    try {
      currentSender = id_list.at(id);
      msg = (std::string)(currentSender->data.msg);
    }
    catch(std::out_of_range& e) {
      throw ChimeraTK::logic_error("Cannot find  element id"
                                   "when updating logging variables.");
    }
    try {
      level = static_cast<LogLevel>(std::strtoul(&msg.at(0), NULL, 0));
    }
    catch(std::out_of_range& e) {
      throw ChimeraTK::logic_error("Cannot find  message level"
                                   "when updating logging variables.");
    }
    // if log level is INTERANEL someone called writeAll() in a module containing the Logger -> ignore
    if(level == LogLevel::INTERNAL) {
      continue;
    }
    if(targetStream == 4) continue;
    LogLevel setLevel = static_cast<LogLevel>((uint)logLevel);
    std::string tmpStr = msg;
    // remove message level
    tmpStr = tmpStr.substr(1, tmpStr.size());
    std::stringstream ss;
    if(((std::string)currentSender->data.alias).empty()) {
      ss << level << getName() << ":" << currentSender->sendingModule << " " << getTime() << tmpStr;
    }
    else {
      ss << level << getName() << ":" << (std::string)currentSender->data.alias << " " << getTime() << tmpStr;
    }
    if(targetStream == 0 || targetStream == 1) {
      if(!((std::string)logFile).empty() && !file->is_open()) {
        std::stringstream ss_file;
        file->open((std::string)logFile, std::ofstream::out | std::ofstream::app);
        if(!file->is_open() && setLevel <= LogLevel::ERROR) {
          ss_file << LogLevel::ERROR << getName() << " " << getTime()
                  << "Failed to open log file for writing: " << (std::string)logFile << std::endl;
          broadcastMessage(ss_file.str(), true);
        }
        else if(file->is_open() && setLevel <= LogLevel::INFO) {
          ss_file << LogLevel::INFO << getName() << " " << getTime()
                  << "Opened log file for writing: " << (std::string)logFile << std::endl;
          broadcastMessage(ss_file.str());
        }
      }
    }
    if(level >= setLevel) {
      if(level < LogLevel::ERROR)
        broadcastMessage(ss.str());
      else
        broadcastMessage(ss.str(), true);
    }
  }
}

ctk::VariableNetworkNode LoggingModule::getAccessorPair(const ctk::RegisterPath& namePrefix, const std::string& name) {
  auto it = std::find_if(sources.begin(), sources.end(),
      boost::bind(&MessageSource::sendingModule, boost::placeholders::_1) == (std::string)namePrefix);
  if(it == sources.end()) {
    auto result = prepareHierarchy(namePrefix);
    if(result == "/") {
      sources.emplace_back(MessageSource{namePrefix, this});
    }
    else {
      sources.emplace_back(MessageSource{namePrefix, &groupMap[result]});
    }
  }
  else {
    throw ChimeraTK::logic_error(
        "Cannot add logging for module " + namePrefix + " since logging was already added for this module.");
  }
  return sources.back().data.msg;
}

void LoggingModule::terminate() {
  if((file.get() != nullptr) && (file->is_open())) file->close();
  ApplicationModule::terminate();
}

void LoggingModule::findTagAndAppendToModule(ctk::VirtualModule& virtualParent, const std::string& tag,
    bool eliminateAllHierarchies, bool eliminateFirstHierarchy, bool negate, ctk::VirtualModule& root) const {
  // Change behaviour to exclude the auto-generated inputs which are connected to the data sources. Otherwise those
  // variables might get published twice to the control system, if findTag(".*") is used to connect the entire
  // application to the control system.
  // This is a temporary solution. In future, instead the inputs should be generated at the same place in the
  // hierarchy as the source variable, and the connetion should not be made by the module itself. This currently would
  // be complicated to implement, since it is difficult to find the correct virtual name for the variables.

  struct MyVirtualModule : ctk::VirtualModule {
    using ctk::VirtualModule::VirtualModule;
    using ctk::VirtualModule::findTagAndAppendToModule;
  };

  MyVirtualModule tempParent("tempRoot", "", ModuleType::ApplicationModule);
  MyVirtualModule tempRoot("tempRoot", "", ModuleType::ApplicationModule);
  ctk::EntityOwner::findTagAndAppendToModule(
      tempParent, "_logging_internal", eliminateAllHierarchies, eliminateFirstHierarchy, true, tempRoot);
  tempParent.findTagAndAppendToModule(virtualParent, tag, false, true, negate, root);
  tempRoot.findTagAndAppendToModule(root, tag, false, true, negate, root);
}
