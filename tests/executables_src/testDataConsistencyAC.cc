// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later

#define BOOST_TEST_MODULE testDataConsistencyAC

#include "Application.h"
#include "ApplicationModule.h"
#include "ArrayAccessor.h"
#include "ScalarAccessor.h"
#include "TestFacility.h"
#include "VoidAccessor.h"

#include <ChimeraTK/DataConsistencyGroup.h>
#include <ChimeraTK/DataConsistencyGroupHistorizedMatcher.h>

#include <boost/test/included/unit_test.hpp>

namespace Tests::testDataConsistencyAC {

  using namespace boost::unit_test_framework;

  using MatchingMode = ChimeraTK::DataConsistencyGroup::MatchingMode;

  /********************************************************************************************************************/

  struct ModuleA : ChimeraTK::ApplicationModule {
    explicit ModuleA(ChimeraTK::ModuleGroup* owner) : ChimeraTK::ApplicationModule(owner, "modA", "") {}

    ChimeraTK::VoidInput in1{this, "in1", ""};
    ChimeraTK::ScalarPushInput<unsigned> in2{this, "in2", "", ""};
    ChimeraTK::ArrayPushInput<unsigned> in3{this, "in3", "", 2, ""};

    ChimeraTK::ScalarOutput<unsigned> out1{this, "out1", "", ""};
    ChimeraTK::ScalarOutput<unsigned> out2{this, "out2", "", ""};

    void mainLoop() override {
      auto rag = readAnyGroup();

      ChimeraTK::DataConsistencyGroup dGroup{matchingMode};
      dGroup.add(in1);
      dGroup.add(in2);
      dGroup.add(in3);

      auto diagnostic = [&](ChimeraTK::TransferElementAbstractor& a) {
        return dynamic_cast<const ChimeraTK::DataConsistencyGroupDetail::HistorizedMatcher&>(dGroup.getMatcher())
            .getTargetElements()
            .at(a.getId())
            .lastMatchingIndex;
      };

      ChimeraTK::TransferElementID updatedId;
      while(true) {
        // note, in general it will be a good idea to consider initial values as consistent set; however, here
        // we do not process them
        if(dGroup.update(updatedId)) {
          std::cout << "ModuleA consistent, updated " << dGroup.getElements().at(updatedId).getName();
          if(matchingMode == MatchingMode::historized) {
            std::cout << ", histIndex(in1,in2,in3)=" << diagnostic(in1) << "," << diagnostic(in2) << ","
                      << diagnostic(in3);
          }
          std::cout << std::endl;

          out2 = in2 + 0;
          writeAllDestructively();
        }
        updatedId = rag.readAny();
      }
    }
    MatchingMode matchingMode;
  };

  struct TestFixture {
    struct Server : ChimeraTK::Application {
      Server() : Application("testSuite") {}
      ~Server() override { shutdown(); }

      ModuleA modA{this};
    } testApp;
    ChimeraTK::TestFacility testFacility{testApp};
  };

  /********************************************************************************************************************/

  /*
   * we test that MatchingMode::historized also works with ApplicationCore.
   * Explicit testing, in addition to DeviceAccess tests, makes sence because of MetaDataPropagatingRegisterDecorator.
   */
  BOOST_FIXTURE_TEST_CASE(testHistorizedMatching, TestFixture) {
    std::cout << "testHistorizedMatching" << std::endl;

    testApp.modA.matchingMode = MatchingMode::historized;

    auto in1 = testFacility.getVoid("/modA/in1");
    auto in2 = testFacility.getScalar<unsigned>("/modA/in2");
    auto in3 = testFacility.getArray<unsigned>("/modA/in3");
    auto out1 = testFacility.getScalar<unsigned>("/modA/out1");
    auto out2 = testFacility.getScalar<unsigned>("/modA/out2");

    testFacility.runApplication();

    ChimeraTK::VersionNumber vn;
    in1.write(vn);
    in2 = 10;
    in2.write(vn);

    // provided data not complete yet -> outputs should not be available
    testFacility.stepApplication();
    BOOST_TEST(out1.readLatest() == false);

    // complete provided data and check that output is available
    in3.write(vn);
    testFacility.stepApplication();
    BOOST_TEST(out1.readLatest() == true);
    out2.readLatest();
    BOOST_TEST(out2 == 10);

    // test that historizing actually helps:
    // let VersionNumber provided to in2 overtake the other inputs.
    ChimeraTK::VersionNumber vn2;
    in2 = 11;
    in2.write(vn2);
    ChimeraTK::VersionNumber vn3;
    in2 = 12;
    in2.setDataValidity(ChimeraTK::DataValidity::faulty);
    in2.write(vn3);
    in1.write(vn2);
    in3.write(vn2);
    testFacility.stepApplication();
    BOOST_TEST(out2.readLatest() == true);
    BOOST_TEST(out2 == 11);

    // Test version numbers as seen by consuming modules
    BOOST_TEST(out2.getVersionNumber() == vn2);
    // test whether versionNumber seen "from the inside" is correct
    BOOST_TEST(testApp.modA.out2.getVersionNumber() == vn2);

    // Test data validity as seen by consuming modules
    BOOST_TEST(out2.dataValidity() == ChimeraTK::DataValidity::ok);
    // "from the inside", we should also see correct VersionNumber
    BOOST_TEST(testApp.modA.out2.dataValidity() == ChimeraTK::DataValidity::ok);

    // check that clearing invalid does not cause crash
    in2.setDataValidity(ChimeraTK::DataValidity::faulty);
    in2.write();
    testFacility.stepApplication();
  }

  /********************************************************************************************************************/

  BOOST_FIXTURE_TEST_CASE(testExactMatching, TestFixture) {
    std::cout << "testExactMatching" << std::endl;
    testApp.modA.matchingMode = MatchingMode::exact;

    auto in1 = testFacility.getVoid("/modA/in1");
    auto in2 = testFacility.getScalar<unsigned>("/modA/in2");
    auto in3 = testFacility.getArray<unsigned>("/modA/in3");
    auto out1 = testFacility.getScalar<unsigned>("/modA/out1");
    auto out2 = testFacility.getScalar<unsigned>("/modA/out2");

    testFacility.runApplication();

    // with VersionNumber provided to in2 overtaking the other inputs, data should get lost.
    ChimeraTK::VersionNumber vn10;
    in2 = 20;
    in2.write(vn10);
    ChimeraTK::VersionNumber vn11;
    in2 = 21;
    in2.write(vn11);
    in1.write(vn10);
    in3.write(vn10);
    testFacility.stepApplication();
    BOOST_TEST(out2.readLatest() == false);

    // when other inputs catch up, we should see an update
    in1.write(vn11);
    in3.write(vn11);
    testFacility.stepApplication();
    BOOST_TEST(out2.readLatest() == true);
  }

  /********************************************************************************************************************/

} // namespace Tests::testDataConsistencyAC
