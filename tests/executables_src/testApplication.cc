// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#include <chrono>
#include <future>

#define BOOST_TEST_MODULE testApplication

#include "Application.h"
#include "DeviceModule.h"
#include "Multiplier.h"
#include "Pipe.h"
#include "Utilities.h"
#include <libxml++/libxml++.h>

#include <boost/filesystem.hpp>
#include <boost/mpl/list.hpp>
#include <boost/test/included/unit_test.hpp>
#include <boost/thread.hpp>

using namespace boost::unit_test_framework;
namespace ctk = ChimeraTK;

namespace Tests::testApplication {

  /*********************************************************************************************************************/
  /* Application without name */

  struct TestApp : public ctk::Application {
    explicit TestApp(const std::string& name) : ctk::Application(name) {}
    ~TestApp() override { shutdown(); }

    ctk::ConstMultiplier<double> multiplierD{this, "multiplierD", "Some module", 42};
    ctk::ScalarPipe<std::string> pipe{this, "pipeIn", "pipeOut", "unit", "Some pipe module"};
    ctk::ConstMultiplier<uint16_t, uint16_t, 120> multiplierU16{this, "multiplierU16", "Some other module", 42};

    ctk::DeviceModule device{this, "(logicalNameMap?map=empty.xlmap)", "/trigger"};
  };

  /*********************************************************************************************************************/
  /* test trigger by app variable when connecting a polled device register to an
   * app variable */

  BOOST_AUTO_TEST_CASE(testApplicationExceptions) {
    std::cout << "***************************************************************"
                 "******************************************************"
              << std::endl;
    std::cout << "==> testApplicationExceptions" << std::endl;

    // zero length name forbidden
    try {
      TestApp app("");
      BOOST_FAIL("Exception expected.");
    }
    catch(ChimeraTK::logic_error&) {
    }

    // names with spaces and special characters are forbidden
    try {
      TestApp app("With space");
      BOOST_FAIL("Exception expected.");
    }
    catch(ChimeraTK::logic_error&) {
    }
    try {
      TestApp app("WithExclamationMark!");
      BOOST_FAIL("Exception expected.");
    }
    catch(ChimeraTK::logic_error&) {
    }

    // all allowed characters in the name
    {
      TestApp app("AaBbCcDdEeFfGgHhIiJjKkLlMmNnOoPpQqRrSsTtUuVvWwXxYyZz_1234567890");
    }

    // repeated characters are allowed
    {
      TestApp app("AAAAAAA");
    }

    // Two apps at the same time are not allowed
    TestApp app1("FirstInstance");
    try {
      TestApp app2("SecondInstance");
      BOOST_FAIL("Exception expected.");
    }
    catch(ChimeraTK::logic_error&) {
    }
  }

  /*********************************************************************************************************************/
  // Helper function for testXmlGeneration:
  // Obtain a value from an XML node
  std::string getValueFromNode(const xmlpp::Node* node, const std::string& subnodeName) {
    xmlpp::Node* theChild = nullptr;
    for(const auto& child : node->get_children()) {
      if(child->get_name() == subnodeName) {
        theChild = child;
      }
    }
    BOOST_REQUIRE(theChild != nullptr); // requested child tag is there
    auto subChildList = theChild->get_children();
    if(subChildList.empty()) { // special case: no text in the tag -> return empty string
      return "";
    }
    BOOST_REQUIRE_EQUAL(subChildList.size(),
        1); // child tag contains only text (no further sub-tags)
    const xmlpp::TextNode* textNode = dynamic_cast<xmlpp::TextNode*>(subChildList.front());
    BOOST_REQUIRE(textNode != nullptr);
    return textNode->get_content();
  }

  /*********************************************************************************************************************/
  /* test creation of XML file describing the variable tree */

  BOOST_AUTO_TEST_CASE(testXmlGeneration) {
    std::cout << "***************************************************************"
                 "******************************************************"
              << std::endl;
    std::cout << "==> testXmlGeneration" << std::endl;

    // delete XML file if already existing
    boost::filesystem::remove("TestAppInstance.xml");

    // create app which exports some properties and generate its XML file
    TestApp app("TestAppInstance");
    app.generateXML();

    // validate the XML file
    xmlpp::XsdValidator validator("application.xsd");
    validator.validate("TestAppInstance.xml");

    // parse XML file
    xmlpp::DomParser parser;
    try {
      parser.parse_file("TestAppInstance.xml");
    }
    catch(xmlpp::exception& e) {
      throw std::runtime_error(std::string("ConfigReader: Error opening the config file "
                                           "'TestAppInstance.xml': ") +
          e.what());
    }

    // get root element
    auto* const root = parser.get_document()->get_root_node();
    BOOST_CHECK_EQUAL(root->get_name(), "application");

    // parsing loop
    bool found_pipeIn{false};
    bool found_multiplierDIn{false};
    bool found_multiplierDOut{false};
    bool found_multiplierU16In{false};
    bool found_multiplierU16Out{false};
    bool found_pipeOut{false};
    bool foundTrigger{false};

    bool foundDeviceStatus{false};
    bool foundDeviceMessage{false};
    bool foundBecameFunctional{false};

    for(const auto& child : root->get_children()) {
      // cast into element, ignore if not an element (e.g. comment)
      const auto* element = dynamic_cast<const xmlpp::Element*>(child);
      if(!element) {
        continue;
      }

      if(element->get_name() == "variable") {
        // obtain attributes from the element
        auto* xname = element->get_attribute("name");
        BOOST_REQUIRE(xname != nullptr);
        std::string name(xname->get_value());

        // obtain values from sub-elements
        std::string value_type = getValueFromNode(element, "value_type");
        std::string direction = getValueFromNode(element, "direction");
        std::string unit = getValueFromNode(element, "unit");
        std::string description = getValueFromNode(element, "description");
        std::string numberOfElements = getValueFromNode(element, "numberOfElements");

        // check if variables are described correctly
        if(name == "pipeOut") {
          found_pipeIn = true;
          BOOST_TEST(value_type == "string");
          BOOST_TEST(direction == "application_to_control_system");
          BOOST_TEST(unit == "unit");
          BOOST_TEST(description == "Some pipe module");
          BOOST_TEST(numberOfElements == "1");
        }
        else if(name == "pipeIn") {
          found_pipeOut = true;
          BOOST_TEST(value_type == "string");
          BOOST_CHECK_EQUAL(direction, "control_system_to_application");
          BOOST_TEST(unit == "unit");
          BOOST_TEST(description == "Some pipe module");
          BOOST_TEST(numberOfElements == "1");
        }
        else if(name == "trigger") {
          foundTrigger = true;
          BOOST_TEST(value_type == "Void");
          BOOST_CHECK_EQUAL(direction, "control_system_to_application");
          BOOST_TEST(unit == "n./a.");
          BOOST_TEST(description == "");
          BOOST_TEST(numberOfElements == "0");
        }
        else {
          BOOST_ERROR("Wrong variable name found.");
        }
      }
      else if(element->get_name() == "directory") {
        auto* xname = element->get_attribute("name");
        BOOST_REQUIRE(xname != nullptr);
        std::string name(xname->get_value());

        for(const auto& subchild : child->get_children()) {
          const auto* element2 = dynamic_cast<const xmlpp::Element*>(subchild);
          if(!element2) {
            continue;
          }

          if(element2->get_name() == "directory") {
            auto* xname2 = element2->get_attribute("name");
            BOOST_REQUIRE(xname2 != nullptr);
            std::string name2(xname2->get_value());
            BOOST_REQUIRE(name2 == ctk::Utilities::escapeName(app.device.getDeviceAliasOrURI(), false));

            for(const auto& devicechild : element2->get_children()) {
              const auto* deviceChildElement = dynamic_cast<const xmlpp::Element*>(devicechild);
              if(!deviceChildElement) {
                continue;
              }
              if(deviceChildElement->get_name() == "variable") {
                // obtain attributes from the element
                auto* xname3 = deviceChildElement->get_attribute("name");
                BOOST_REQUIRE(xname3 != nullptr);
                std::string name3(xname3->get_value());

                // obtain values from sub-elements
                std::string value_type = getValueFromNode(deviceChildElement, "value_type");
                std::string direction = getValueFromNode(deviceChildElement, "direction");
                std::string unit = getValueFromNode(deviceChildElement, "unit");
                std::string description = getValueFromNode(deviceChildElement, "description");
                std::string numberOfElements = getValueFromNode(deviceChildElement, "numberOfElements");
                if(name3 == "status") {
                  foundDeviceStatus = true;
                  BOOST_CHECK_EQUAL(value_type, "int32");
                  BOOST_CHECK_EQUAL(description, "Error status of the device - Error status of the device");
                  BOOST_CHECK_EQUAL(numberOfElements, "1");
                  BOOST_CHECK_EQUAL(direction, "application_to_control_system");
                  BOOST_CHECK_EQUAL(unit, "");
                }
                else if(name3 == "status_message") {
                  foundDeviceMessage = true;
                  BOOST_CHECK_EQUAL(value_type, "string");
                  BOOST_CHECK_EQUAL(description, "Error status of the device - status message");
                  BOOST_CHECK_EQUAL(numberOfElements, "1");
                  BOOST_CHECK_EQUAL(direction, "application_to_control_system");
                  BOOST_CHECK_EQUAL(unit, "");
                }
                else if(name3 == "deviceBecameFunctional") {
                  foundBecameFunctional = true;
                  BOOST_CHECK_EQUAL(value_type, "Void");
                  BOOST_CHECK_EQUAL(description, "");
                  BOOST_CHECK_EQUAL(numberOfElements, "1");
                  BOOST_CHECK_EQUAL(direction, "application_to_control_system");
                  BOOST_CHECK_EQUAL(unit, "");
                }
                else {
                  BOOST_ERROR("Unexpected variable " + name2);
                }
              }
              else {
                BOOST_ERROR("Wrong tag " + element2->get_name() + " found");
              }
            }
          }
          else if(element2->get_name() == "variable") {
            // obtain attributes from the element
            auto* xname2 = element2->get_attribute("name");
            BOOST_REQUIRE(xname2 != nullptr);
            std::string name2(xname2->get_value());

            // obtain values from sub-elements
            std::string value_type = getValueFromNode(element2, "value_type");
            std::string direction = getValueFromNode(element2, "direction");
            std::string unit = getValueFromNode(element2, "unit");
            std::string description = getValueFromNode(element2, "description");
            std::string numberOfElements = getValueFromNode(element2, "numberOfElements");

            if(name2 == "input") {
              if(name == "multiplierD") {
                found_multiplierDIn = true;
                BOOST_CHECK_EQUAL(value_type, "double");
                BOOST_CHECK_EQUAL(description, "Some module");
                BOOST_CHECK_EQUAL(numberOfElements, "1");
              }
              else if(name == "multiplierU16") {
                found_multiplierU16In = true;
                BOOST_CHECK_EQUAL(value_type, "uint16");
                BOOST_CHECK_EQUAL(description, "Some other module");
                BOOST_CHECK_EQUAL(numberOfElements, "120");
              }
              else {
                BOOST_ERROR("Wrong Directory name found");
              }
              BOOST_CHECK_EQUAL(direction, "control_system_to_application");
              BOOST_CHECK_EQUAL(unit, "");
            }
            else if(name2 == "output") {
              if(name == "multiplierD") {
                found_multiplierDOut = true;
                BOOST_CHECK_EQUAL(value_type, "double");
                BOOST_CHECK_EQUAL(description, "Some module");
                BOOST_CHECK_EQUAL(numberOfElements, "1");
              }
              else if(name == "multiplierU16") {
                found_multiplierU16Out = true;
                BOOST_CHECK_EQUAL(value_type, "uint16");
                BOOST_CHECK_EQUAL(description, "Some other module");
                BOOST_CHECK_EQUAL(numberOfElements, "120");
              }
              else {
                BOOST_ERROR("Wrong Directory name found");
              }
              BOOST_CHECK_EQUAL(direction, "application_to_control_system");
              BOOST_CHECK_EQUAL(unit, "");
            }
            else {
              BOOST_ERROR("Wrong variable name found.");
            }
          }
        }
      }
      else {
        BOOST_ERROR("Wrong tag found.");
      }
    }

    BOOST_CHECK(found_pipeIn);
    BOOST_CHECK(found_pipeOut);
    BOOST_CHECK(found_multiplierDIn);
    BOOST_CHECK(found_multiplierDOut);
    BOOST_CHECK(found_multiplierU16In);
    BOOST_CHECK(found_multiplierU16Out);
    BOOST_CHECK(foundTrigger);
    BOOST_CHECK(foundDeviceMessage);
    BOOST_CHECK(foundDeviceStatus);
    BOOST_CHECK(foundBecameFunctional);
  }

  BOOST_AUTO_TEST_CASE(testDOTGeneration) {
    std::cout << "***************************************************************"
                 "******************************************************"
              << std::endl;
    std::cout << "==> tesDOTGeneration" << std::endl;

    // delete DOT file if already existing
    boost::filesystem::remove("TestAppInstance.dot");

    // create app which exports some properties and generate its XML file
    TestApp app("TestAppInstance");
    app.generateDOT();

    // check existence
    bool found_dot{boost::filesystem::exists("TestAppInstance.dot")};

    BOOST_CHECK(found_dot);
  }

} // namespace Tests::testApplication
