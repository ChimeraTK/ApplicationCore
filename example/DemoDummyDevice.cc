// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#include <ChimeraTK/BackendFactory.h>
#include <ChimeraTK/DeviceAccessVersion.h>
#include <ChimeraTK/DummyBackend.h>

class DemoDummy : public ChimeraTK::DummyBackend {
 public:
  DemoDummy(std::string mapFileName) : DummyBackend(mapFileName) {}

  static boost::shared_ptr<DeviceBackend> createInstance(
      std::string /* address */, std::map<std::string, std::string> parameters) {
    return boost::shared_ptr<DeviceBackend>(new DemoDummy(parameters["map"]));
  }

  void read(uint64_t bar, uint64_t address, int32_t* data, size_t sizeInBytes) override {
    // if probeSignal register is read, fill it first
    if(bar == 2) {
      assert(address == 0);
      assert(sizeInBytes == 65536);

      // build average of feed forward and setpoint tables
      for(int i = 0; i < 65536; ++i) {
        _barContents[2][i] = (_barContents[0][i] + _barContents[1][i]) / 2;
      }
    }

    // perform the original read
    DummyBackend::read(bar, address, data, sizeInBytes);
  }

  /** Class to register the backend type with the factory. */
  class BackendRegisterer {
   public:
    BackendRegisterer();
  };
  static BackendRegisterer backendRegisterer;
};

DemoDummy::BackendRegisterer DemoDummy::backendRegisterer;

DemoDummy::BackendRegisterer::BackendRegisterer() {
  std::cout << "DemoDummy::BackendRegisterer: registering backend type DemoDummy" << std::endl;
  ChimeraTK::BackendFactory::getInstance().registerBackendType("DemoDummy", &DemoDummy::createInstance);
}
