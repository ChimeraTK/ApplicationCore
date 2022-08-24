// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later

// #define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE LoggingTest

#include "Logging.h"

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/test/included/unit_test.hpp>
#include <boost/thread.hpp>

#include <fstream>
#include <stdlib.h>
using namespace logging;

#include "TestFacility.h"

#include <boost/test/unit_test.hpp>
using namespace boost::unit_test_framework;

struct DummyModule : ChimeraTK::ApplicationModule {
  using ChimeraTK::ApplicationModule::ApplicationModule;
  ChimeraTK::ScalarPushInput<int> input{this, "input", "", "dummy input"};

  boost::shared_ptr<Logger> logger{new Logger(this)};

  void mainLoop() override {
    while(1) {
      input.read();
    }
  }
};

struct TestGroup : public ChimeraTK::ModuleGroup {
  using ChimeraTK::ModuleGroup::ModuleGroup;
  struct A : public ChimeraTK::ModuleGroup {
    using ChimeraTK::ModuleGroup::ModuleGroup;
    DummyModule dummy{this, "Dummy", ""};
  } a{this, "A", ""};
  struct B : public ChimeraTK::ModuleGroup {
    using ChimeraTK::ModuleGroup::ModuleGroup;
    DummyModule dummy{this, "Dummy", ""};
  } b{this, "B", "", ctk::HierarchyModifier::hideThis};
};

/**
 * Define a test app to test the LoggingModule.
 *
 * A temporary directory /tmp/testLogging.XXXXXX will be created. Here XXXXXX will
 * be replaced by a unique string. Once the testApp object is deleted that directory is removed.
 */
struct testApp : public ChimeraTK::Application {
  testApp() : Application("test"), fileCreated(false) {
    char temName[] = "/tmp/testLogging.XXXXXX";
    char* dir_name = mkdtemp(temName);
    dir = std::string(dir_name);
    filename = dir + "/testLogging.log";
  }
  virtual ~testApp() {
    if(!isShutDown) {
      shutdown();
    }
    if(fileCreated) {
      BOOST_CHECK_EQUAL(boost::filesystem::remove(filename.c_str()), true);
    }
    // do not check if removing the folder fails. If the test is run in parallel other instances might have file in the directory
    BOOST_CHECK_EQUAL(boost::filesystem::remove(dir), true);
  }

  DummyModule dummy{this, "Dummy", "Dummy"};

  LoggingModule log{this, "LoggingModule", "LoggingModule test"};

  void initialise() override {
    Application::initialise();
    dumpConnections();
  }

  bool fileCreated;
  std::string dir;
  std::string filename;

  bool isShutDown{false};

  const char* directory = "/tmp/testLogging/";
  const char* fileprefix = "test";
};

struct MultipleModuleApp : public ChimeraTK::Application {
  MultipleModuleApp() : Application("test"){};
  virtual ~MultipleModuleApp() { shutdown(); }

  TestGroup group{this, "MainGroup", ""};
  LoggingModule log{this, "LoggingModule", "LoggingModule test"};

  void initialise() override {
    Application::initialise();
    dumpConnections();
  }
};

BOOST_AUTO_TEST_CASE(testMultipleModules) {
  MultipleModuleApp app;
  ChimeraTK::TestFacility tf;
  tf.setScalarDefault("/LoggingModule/maxTailLength", (uint)1);
  tf.runApplication();
  BOOST_CHECK_EQUAL(app.log.getNumberOfModules(), 2);
  app.group.a.dummy.logger->sendMessage("Message from module a", LogLevel::DEBUG);
  app.group.b.dummy.logger->sendMessage("Message from module b", LogLevel::DEBUG);
  tf.stepApplication();
  std::string ss = tf.readScalar<std::string>("/LoggingModule/logTail");
  BOOST_CHECK_EQUAL(ss.substr(ss.find("->") + 3), std::string("Message from module b\n"));
  app.group.b.dummy.logger->sendMessage("Message from module b", LogLevel::DEBUG);
  app.group.a.dummy.logger->sendMessage("Message from module a", LogLevel::DEBUG);
  tf.stepApplication();
  ss = tf.readScalar<std::string>("/LoggingModule/logTail");
  BOOST_CHECK_EQUAL(ss.substr(ss.find("->") + 3), std::string("Message from module a\n"));
}

BOOST_AUTO_TEST_CASE(testAlias) {
  testApp app;
  ChimeraTK::TestFacility tf;
  tf.setScalarDefault("/LoggingModule/maxTailLength", (uint)1);
  tf.runApplication();
  BOOST_CHECK_EQUAL(app.log.getNumberOfModules(), 1);
  app.dummy.logger->sendMessage("TestMessage", LogLevel::DEBUG);
  tf.stepApplication();
  std::string ss = tf.readScalar<std::string>("/LoggingModule/logTail");
  BOOST_CHECK_EQUAL(ss.substr(ss.find("LoggingModule:") + 14, 5), std::string("Dummy"));
  auto alias = tf.getScalar<std::string>("/Dummy/Logging/alias");
  alias = "NewName";
  alias.write();
  app.dummy.logger->sendMessage("TestMessage", LogLevel::DEBUG);
  tf.stepApplication();
  // write twice to be sure the alias is set
  app.dummy.logger->sendMessage("TestMessage", LogLevel::DEBUG);
  tf.stepApplication();
  ss = tf.readScalar<std::string>("/LoggingModule/logTail");
  BOOST_CHECK_EQUAL(ss.substr(ss.find("LoggingModule:") + 14, 7), std::string("NewName"));
}

BOOST_AUTO_TEST_CASE(testAlias_withHierachies) {
  MultipleModuleApp app;
  ChimeraTK::TestFacility tf;
  tf.setScalarDefault("/LoggingModule/maxTailLength", (uint)1);
  tf.runApplication();
  BOOST_CHECK_EQUAL(app.log.getNumberOfModules(), 2);
  app.group.a.dummy.logger->sendMessage("TestMessage", LogLevel::DEBUG);
  tf.stepApplication();
  std::string ss = tf.readScalar<std::string>("/LoggingModule/logTail");
  BOOST_CHECK_EQUAL(ss.substr(ss.find("LoggingModule:") + 14, 25), std::string("MainGroup/A/Dummy/Logging"));

  app.group.b.dummy.logger->sendMessage("TestMessage", LogLevel::DEBUG);
  tf.stepApplication();
  ss = tf.readScalar<std::string>("/LoggingModule/logTail");
  BOOST_CHECK_EQUAL(ss.substr(ss.find("LoggingModule:") + 14, 25), std::string("MainGroup/B/Dummy/Logging"));

  auto aliasA = tf.getScalar<std::string>("/MainGroup/A/Dummy/Logging/alias");
  aliasA = "NewName";
  aliasA.write();
  app.group.a.dummy.logger->sendMessage("TestMessage", LogLevel::DEBUG);
  tf.stepApplication();
  // write twice to be sure the alias is set
  app.group.a.dummy.logger->sendMessage("TestMessage", LogLevel::DEBUG);
  tf.stepApplication();
  ss = tf.readScalar<std::string>("/LoggingModule/logTail");
  BOOST_CHECK_EQUAL(ss.substr(ss.find("LoggingModule:") + 14, 7), std::string("NewName"));

  // check if internal variable is removed from CS
  BOOST_CHECK_THROW(tf.getScalar<std::string>("/MainGroup/B/Dummy/Logging/alias"), ctk::logic_error);
  auto aliasB = tf.getScalar<std::string>("/MainGroup/Dummy/Logging/alias");
  aliasB = "NewName";
  aliasB.write();
  app.group.b.dummy.logger->sendMessage("TestMessage", LogLevel::DEBUG);
  tf.stepApplication();
  // write twice to be sure the alias is set
  app.group.b.dummy.logger->sendMessage("TestMessage", LogLevel::DEBUG);
  tf.stepApplication();
  ss = tf.readScalar<std::string>("/LoggingModule/logTail");
  BOOST_CHECK_EQUAL(ss.substr(ss.find("LoggingModule:") + 14, 7), std::string("NewName"));
}

BOOST_AUTO_TEST_CASE(testLogMsg) {
  testApp app;
  ChimeraTK::TestFacility tf;
  tf.setScalarDefault("/LoggingModule/maxTailLength", (uint)1);
  tf.runApplication();
  app.dummy.logger->sendMessage("test", LogLevel::DEBUG);
  tf.stepApplication();
  std::string ss = tf.readScalar<std::string>("/LoggingModule/logTail");
  BOOST_CHECK_EQUAL(ss.substr(ss.find("->") + 3), std::string("test\n"));
}

BOOST_AUTO_TEST_CASE(testLogfileFails) {
  testApp app;
  ChimeraTK::TestFacility tf;

  auto logFile = tf.getScalar<std::string>("/LoggingModule/logFile");
  tf.runApplication();
  auto tmpStr = app.filename;
  tmpStr.replace(tmpStr.find("testLogging"), 18, "wrongFolder");
  logFile = tmpStr;
  logFile.write();
  // message not considered here but used to step through the application
  app.dummy.logger->sendMessage("test", LogLevel::DEBUG);
  tf.stepApplication();
  std::string ss = (std::string)tf.readScalar<std::string>("/LoggingModule/logTail");
  std::vector<std::string> strs;
  boost::split(strs, ss, boost::is_any_of("\n"), boost::token_compress_on);
  BOOST_CHECK_EQUAL(
      strs.at(2).substr(strs.at(2).find("->") + 3), std::string("Failed to open log file for writing: ") + tmpStr);
}

BOOST_AUTO_TEST_CASE(testLogfile) {
  testApp app;
  ChimeraTK::TestFacility tf;

  auto logFile = tf.getScalar<std::string>("/LoggingModule/logFile");

  if(!boost::filesystem::is_directory("/tmp/testLogging/")) boost::filesystem::create_directory("/tmp/testLogging/");
  tf.runApplication();
  logFile = app.filename;
  logFile.write();
  app.fileCreated = true;
  // message not considered here but used to step through the application
  app.dummy.logger->sendMessage("test", LogLevel::DEBUG);
  tf.stepApplication();
  std::fstream file;
  file.open(app.filename.c_str());
  BOOST_CHECK_EQUAL(file.good(), true);
  if(file.good()) app.fileCreated = true;
  std::string line;

  std::getline(file, line);
  BOOST_CHECK_EQUAL(line.substr(line.find("->") + 3), std::string("Opened log file for writing: ") + app.filename);
  std::getline(file, line);
  BOOST_CHECK_EQUAL(line.substr(line.find("->") + 3), std::string("test"));
}

BOOST_AUTO_TEST_CASE(testLogging) {
  testApp app;
  ChimeraTK::TestFacility tf;

  auto logLevel = tf.getScalar<uint>("/LoggingModule/logLevel");
  auto tailLength = tf.getScalar<uint>("/LoggingModule/maxTailLength");

  tf.runApplication();
  logLevel = 0;
  logLevel.write();
  tailLength = 2;
  tailLength.write();
  app.dummy.logger->sendMessage("1st test message", LogLevel::DEBUG);
  tf.stepApplication();
  app.dummy.logger->sendMessage("2nd test message", LogLevel::DEBUG);
  tf.stepApplication();
  auto tail = tf.readScalar<std::string>("/LoggingModule/logTail");
  std::vector<std::string> result;
  boost::algorithm::split(result, tail, boost::is_any_of("\n"));
  // result length should be 3 not 2, because new line is used to split, which
  // results in 3 items although there are only two messages.
  BOOST_CHECK_EQUAL(result.size(), 3);

  /**** Test log level ****/
  logLevel = 2;
  logLevel.write();
  app.dummy.logger->sendMessage("3rd test message", LogLevel::DEBUG);
  tf.stepApplication();
  tail = tf.readScalar<std::string>("/LoggingModule/logTail");
  boost::algorithm::split(result, tail, boost::is_any_of("\n"));
  // should still be 3 because log level was too low!
  BOOST_CHECK_EQUAL(result.size(), 3);

  /**** Test tail length ****/
  tailLength = 3;
  tailLength.write();
  //  tf.stepApplication();
  app.dummy.logger->sendMessage("4th test message", LogLevel::ERROR);
  tf.stepApplication();
  tail = tf.readScalar<std::string>("/LoggingModule/logTail");
  boost::algorithm::split(result, tail, boost::is_any_of("\n"));
  BOOST_CHECK_EQUAL(result.size(), 4);

  app.dummy.logger->sendMessage("5th test message", LogLevel::ERROR);
  tf.stepApplication();
  tail = tf.readScalar<std::string>("/LoggingModule/logTail");
  boost::algorithm::split(result, tail, boost::is_any_of("\n"));
  // should still be 4 because tailLength is 3!
  BOOST_CHECK_EQUAL(result.size(), 4);
}
