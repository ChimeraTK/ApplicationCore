// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "TestFacility.h"

namespace ChimeraTK {

  /********************************************************************************************************************/

  TestFacility::TestFacility(bool enableTestableMode) : TestFacility(Application::getInstance(), enableTestableMode) {}

  /********************************************************************************************************************/
  TestFacility::TestFacility(Application& application, bool enableTestableMode) : _app(application) {
    auto pvManagers = createPVManager();
    _pvManager = pvManagers.first;
    _app.setPVManager(pvManagers.second);
    if(enableTestableMode) {
      _app.enableTestableMode();
    }
    _app.initialise();
  }

  /********************************************************************************************************************/

  void TestFacility::runApplication() const {
    _app._testFacilityRunApplicationCalled = true;

    // send default values for all control system variables
    for(auto& pv : _pvManager->getAllProcessVariables()) {
      callForType(pv->getValueType(), [&pv, this](auto arg) {
        using T = decltype(arg);

        // Applies only to writeable variables.
        // @todo FIXME It should also NOT apply for application-to-controlsystem variables with a return channel,
        // despite being writeable here!
        if(!pv->isWriteable()) {
          return;
        }

        // Safety check against incorrect usage
        if(pv->getVersionNumber() != VersionNumber(nullptr)) {
          throw ChimeraTK::logic_error("The variable '" + pv->getName() +
              "' has been written before TestFacility::runApplication() was called. Instead use "
              "TestFacility::setScalarDefault() resp. setArrayDefault() to set initial values.");
        }

        // Get the PV accessor
        auto pv_casted = boost::dynamic_pointer_cast<NDRegisterAccessor<T>>(pv);

        // If default value has been stored, copy the default value to the PV.
        if constexpr(!std::is_same<T, ChimeraTK::Void>::value) {
          auto table = boost::fusion::at_key<T>(_defaults.table);
          if(table.find(pv->getName()) != table.end()) {
            /// Since pv_casted is the undecorated PV (lacking the TestableModeAccessorDecorator), we need to copy the
            /// value also to the decorator. We still have to write through the undecorated PV, otherwise the tests are
            /// stalled. @todo It is not understood why this happens!
            /// Decorated accessors are stored in different maps for scalars are arrays...
            if(pv_casted->getNumberOfSamples() == 1) { // scalar
              auto accessor = this->getScalar<T>(pv->getName());
              accessor = table.at(pv->getName())[0];
            }
            else { // array
              auto accessor = this->getArray<T>(pv->getName());
              accessor = table.at(pv->getName());
            }
            // copy value also to undecorated PV
            pv_casted->accessChannel(0) = table.at(pv->getName());
          }
        }

        // Write the initial value. This must be done even if no default value has been stored, since it is expected
        // by the application.
        pv_casted->write();
      });
    }
    // start the application
    _app.run();
    // set thread name
    Application::registerThread("TestThread");
    // make sure all initial values have been propagated when in testable mode
    if(_app.getTestableMode()._enabled) {
      // call stepApplication() only in testable mode and only if the queues are not empty
      if(_app.getTestableMode()._counter != 0 || _app.getTestableMode()._deviceInitialisationCounter != 0) {
        stepApplication();
      }
    }

    // receive all initial values for the control system variables
    if(_app.getTestableMode()._enabled) {
      for(auto& pv : _pvManager->getAllProcessVariables()) {
        if(!pv->isReadable()) {
          continue;
        }
        callForTypeNoVoid(pv->getValueType(), [&](auto t) {
          using UserType = decltype(t);
          this->getArray<UserType>(pv->getName()).readNonBlocking();
        });
      }
    }
  }

  /********************************************************************************************************************/

  bool TestFacility::canStepApplication() const {
    return _app.getTestableMode().canStep();
  }

  /********************************************************************************************************************/

  void TestFacility::stepApplication(bool waitForDeviceInitialisation) const {
    _app.getTestableMode().step(waitForDeviceInitialisation);
  }

  /********************************************************************************************************************/

  ChimeraTK::VoidRegisterAccessor TestFacility::getVoid(const ChimeraTK::RegisterPath& name) const {
    return getAccessor<ChimeraTK::Void>(name);
  }

  /********************************************************************************************************************/

} // namespace ChimeraTK
