// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "MetaDataPropagatingRegisterDecorator.h"

#include "Application.h"
#include "EntityOwner.h"

#include <boost/pointer_cast.hpp>

namespace ChimeraTK {

  template<typename T>
  void MetaDataPropagatingRegisterDecorator<T>::doPostRead(TransferType type, bool hasNewData) {
    NDRegisterAccessorDecorator<T, T>::doPostRead(type, hasNewData);

    // update the version number
    if(!_disableVersionNumberPropagation && _target->getAccessModeFlags().has(AccessMode::wait_for_new_data) &&
        type == TransferType::read) {
      _owner->setCurrentVersionNumber(this->getVersionNumber());
    }

    // Check if the data validity flag changed. If yes, propagate this information to the owning module and the application
    if(_dataValidity != _lastValidity) {
      if(_dataValidity == DataValidity::faulty) { // data validity changes to faulty
        _owner->incrementDataFaultCounter();
        // external input in a circular dependency network
        if(_owner->getCircularNetworkHash() && !_isCircularInput) {
          ++(Application::getInstance()._circularNetworkInvalidityCounters[_owner->getCircularNetworkHash()]);
        }
      }
      else { // data validity changed to OK
        _owner->decrementDataFaultCounter();
        // external inpput in a circular dependency network
        if(_owner->getCircularNetworkHash() && !_isCircularInput) {
          --(Application::getInstance()._circularNetworkInvalidityCounters[_owner->getCircularNetworkHash()]);
        }
      }
      _lastValidity = _dataValidity;
    }
  }

  template<typename T>
  void MetaDataPropagatingRegisterDecorator<T>::doPreWrite(TransferType type, VersionNumber versionNumber) {
    // We cannot use NDRegisterAccessorDecorator<T> here because we need a different implementation of setting the
    // target data validity. So we have a complete implemetation here.

    if(_owner->getCircularNetworkHash() && _dataValidity != _lastValidity) {
      // In circular dependency networks an output which actively has DataValidity::faulty set by the user logic is handled
      // as if an external input was invalid -> increase or decrease the network's invalidity counter accordingly
      if(_dataValidity == DataValidity::faulty) { // data validity changes to faulty
        ++(Application::getInstance()._circularNetworkInvalidityCounters[_owner->getCircularNetworkHash()]);
      }
      else {
        --(Application::getInstance()._circularNetworkInvalidityCounters[_owner->getCircularNetworkHash()]);
      }
      _lastValidity = _dataValidity;
    }

    // Now propagate the flag and the data to the target and perform the write
    if(_dataValidity == DataValidity::faulty) { // the application has manualy set the validity to faulty
      _target->setDataValidity(DataValidity::faulty);
    }
    else {
      if(_direction.dir == VariableDirection::feeding && !_disableDataValidityPropagation) {
        _target->setDataValidity(_owner->getDataValidity());
      }
      else {
        // Do not propagate the owner's validity through the return channel. The user can still override this because
        // of the override above
        _target->setDataValidity(DataValidity::ok);
      }
    }

    for(unsigned int i = 0; i < _target->getNumberOfChannels(); ++i) {
      buffer_2D[i].swap(_target->accessChannel(i));
    }
    _target->preWrite(type, versionNumber);
  }

} // namespace ChimeraTK

INSTANTIATE_TEMPLATE_FOR_CHIMERATK_USER_TYPES(ChimeraTK::MetaDataPropagatingRegisterDecorator);
