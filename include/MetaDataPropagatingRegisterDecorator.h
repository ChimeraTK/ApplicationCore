// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "Flags.h"

#include <ChimeraTK/NDRegisterAccessorDecorator.h>

namespace ChimeraTK {

  /********************************************************************************************************************/

  // we can only declare the classes here but not use them/include the header to avoid a circular dependency
  class EntityOwner;
  class VariableNetworkNode;

  /********************************************************************************************************************/

  /**
   *  A mix-in helper class so you can set the flags without knowing the user data type.
   */
  class MetaDataPropagationFlagProvider {
   public:
    [[nodiscard]] DataValidity getLastValidity() const { return _lastValidity; }

   protected:
    /**
     *  Flag whether this is decorating a circular input
     */
    bool _isCircularInput{false};

    /**
     *  Value of validity flag from last read or write operation.
     *  This is atomic to allow the InvalidityTracer module to access this information.
     */
    std::atomic<DataValidity> _lastValidity{DataValidity::ok};

    // The VariableNetworkNode needs access to _isCircularInput. It cannot be set at construction time because the
    // network is not complete yet and isCircularInput is not know at that moment.
    friend class VariableNetworkNode;
  };

  /********************************************************************************************************************/

  /**
   *  NDRegisterAccessorDecorator which propagates meta data attached to input process variables through the owning
   *  ApplicationModule. It will set the current version number of the owning ApplicationModule in postRead. At the
   *  same time it will also propagate the DataValidity flag to/from the owning module.
   */
  template<typename T>
  class MetaDataPropagatingRegisterDecorator : public NDRegisterAccessorDecorator<T, T>,
                                               public MetaDataPropagationFlagProvider {
   public:
    MetaDataPropagatingRegisterDecorator(
        const boost::shared_ptr<NDRegisterAccessor<T>>& target, EntityOwner* owner, VariableDirection direction)
    : NDRegisterAccessorDecorator<T, T>(target), _owner(owner), _direction(direction) {}

    void doPreRead(TransferType type) override { NDRegisterAccessorDecorator<T, T>::doPreRead(type); }

    void doPostRead(TransferType type, bool hasNewData) override;
    void doPreWrite(TransferType type, VersionNumber versionNumber) override;

    [[nodiscard]] DataValidity getTargetValidity() const { return _target->dataValidity(); }

    void disableDataValidityPropagation() { _disableDataValidityPropagation = true; }

   protected:
    EntityOwner* _owner;
    VariableDirection _direction;
    bool _disableDataValidityPropagation = false;

    using TransferElement::_dataValidity;
    using NDRegisterAccessorDecorator<T>::_target;
    using NDRegisterAccessorDecorator<T>::buffer_2D;
    using MetaDataPropagationFlagProvider::_isCircularInput;
  };

  /********************************************************************************************************************/

  DECLARE_TEMPLATE_FOR_CHIMERATK_USER_TYPES(MetaDataPropagatingRegisterDecorator);

  /********************************************************************************************************************/

} /* namespace ChimeraTK */
