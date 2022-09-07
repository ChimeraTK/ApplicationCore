// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "VariableNetworkNode.h"

#include <ChimeraTK/ControlSystemAdapter/BidirectionalProcessArray.h>
#include <ChimeraTK/TransferElement.h>

#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>

#include <atomic>
#include <cstddef>
#include <map>
#include <mutex>

namespace ChimeraTK::detail {
  struct TestableMode {
    /** Special exception class which will be thrown if tests with the testable
     * mode are stalled. Normally this exception should never be caught. The only
     * reason for catching it might be a situation where the expected behaviour of
     * an app is to do nothing and one wants to test this. Note that the stall
     * condition only appears after a certain timeout, thus tests relying on this
     * will take time.
     *
     *  This exception must not be based on a generic exception class to prevent
     * catching it unintentionally. */
    class TestsStalled {};

    /** Flag if noisy debug output is enabled for the testable mode */
    bool enableDebug{false};

    /** Semaphore counter used in testable mode to check if application code is
     * finished executing. This value may only be accessed while holding the
     * testableMode.mutex. */
    size_t counter{0};

    /** Flag if connections should be made in testable mode (i.e. the
     * TestableModeAccessorDecorator is put around all push-type input accessors
     * etc.). */
    bool enabled{false};

    /** Semaphore counter used in testable mode to check if device initialisation is finished executing. This value may
     *  only be accessed while holding mutex. This counter is a separate counter from
     *  counter so stepApplication() can be controlled whether to obey this counter. */
    size_t deviceInitialisationCounter{0};

    /** Lock the testable mode mutex for the current thread. Internally, a
     * thread-local std::unique_lock<std::mutex> will be created and re-used in
     * subsequent calls within the same thread to this function and to
     *  testableModeUnlock().
     *
     *  This function should generally not be used in user code. */
    void lock(const std::string& name);

    /** Unlock the testable mode mutex for the current thread. See also
     * testableModeLock().
     *
     *  Initially the lock will not be owned by the current thread, so the first
     * call to this function will throw an exception (see
     * std::unique_lock::unlock()), unless testableModeLock() has been called
     * first.
     *
     *  This function should generally not be used in user code. */
    void unlock(const std::string& name);

    /** Test if the testable mode mutex is locked by the current thread.
     *
     *  This function should generally not be used in user code. */
    bool testLock();

    [[nodiscard]] bool canStep() const { return counter != 0; }

    void step(bool waitForDeviceInitialisation);

    /**
     * Set string holding the name of the current thread or the specified thread ID. This is used e.g. for
     * debugging output of the testable mode.
     */
    void setThreadName(const std::string& name);

    /**
     * Enable testable mode
     */
    void enable();

    enum class DecoratorType { READ, WRITE };

    /** Return a fresh variable ID which can be assigned to a sender/receiver
     * pair. The ID will always be non-zero. */
    static size_t getNextVariableId() {
      static size_t nextId{0};
      return ++nextId;
    }

    template<typename T>
    boost::shared_ptr<NDRegisterAccessor<T>> decorate(boost::shared_ptr<NDRegisterAccessor<T>> other,
        DecoratorType direction, const std::string& name = {}, size_t varId = 0);

    template<typename T>
    using AccessorPair = std::pair<boost::shared_ptr<NDRegisterAccessor<T>>, boost::shared_ptr<NDRegisterAccessor<T>>>;

    template<typename T>
    AccessorPair<T> decorate(
        AccessorPair<T> other, const VariableNetworkNode& producer, const VariableNetworkNode& consumer);

    /** Decorator of the NDRegisterAccessor which facilitates tests of the
     * application */
    template<typename UserType>
    class AccessorDecorator : public ChimeraTK::NDRegisterAccessorDecorator<UserType> {
     public:
      AccessorDecorator(detail::TestableMode& testableMode,
          boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>> accessor, bool handleRead, bool handleWrite,
          size_t variableIdRead, size_t variableIdWrite);

      bool doWriteTransfer(ChimeraTK::VersionNumber versionNumber = {}) override;

      bool doWriteTransferDestructively(ChimeraTK::VersionNumber versionNumber = {}) override;

      void doReadTransferSynchronously() override { _target->readTransfer(); }

      /** Release the testableModeLock */
      void releaseLock();

      void doPreRead(TransferType type) override;

      /** Obtain the testableModeLock if not owned yet, and decrement the counter.
       */
      void obtainLockAndDecrementCounter(bool hasNewData);

      /** Obtain the testableModeLock if not owned yet, decrement the counter, and
       * release the lock again. */
      void decrementCounter();

      void doPostRead(TransferType type, bool hasNewData) override;

     protected:
      using ChimeraTK::NDRegisterAccessor<UserType>::buffer_2D;
      using ChimeraTK::NDRegisterAccessorDecorator<UserType>::_target;

      bool _handleRead, _handleWrite;
      size_t _variableIdRead, _variableIdWrite;
      TestableMode& _testableMode;

      bool accountForWriteOperation(const std::function<bool(void)>& writeOperation);
    };

   private:
    struct VariableDescriptor {
      /** name of the variable, used to print sensible information. */
      std::string name;

      /** The Process variable that has been decorated with a TestableModeDecorator */
      boost::shared_ptr<TransferElement> processVariable;

      /** Like the global counter but broken out for each variable.
       * This is not actually used as a semaphore counter but only in case of a
       * detected stall (see repeatingMutexOwner) to print a list of
       * variables which still contain unread values
       */
      size_t counter{0};
    };

    /** Mutex used in testable mode to take control over the application threads.
     * Use only through the lock object obtained through
     * getLockObject().
     *
     *  This member is static, since it should survive destroying an application
     * instance and creating a new one. Otherwise getLockObject()
     * would not work, since it relies on thread_local instances which have to be
     * static. The static storage duration presents no problem in either case,
     * since there can only be one single instance of Application at a time (see
     * ApplicationBase constructor). */
    static std::timed_mutex mutex;

    /**
     * Map of unique IDs to a VariableDescriptor holding information about Variables
     * that are being tracked as part of testable mode
     */
    std::map<size_t, VariableDescriptor> variables;

    /** Last thread which successfully obtained the lock for the testable mode.
     * This is used to prevent spamming repeating messages if the same thread
     * acquires and releases the lock in a loop without another thread
     *  activating in between. */
    boost::thread::id lastMutexOwner;

    /** Counter how often the same thread has acquired the testable mode mutex in
     * a row without another thread owning it in between. This is an indicator for
     * the test being stalled due to data send through a process
     *  variable but not read by the receiver. */
    std::atomic<size_t> repeatingMutexOwner{false};

    /** Obtain the lock object for the testable mode lock for the current thread.
     * The returned object has thread_local storage duration and must only be used
     * inside the current thread. Initially (i.e. after the first call in one
     * particular thread) the lock will not be owned by the returned object, so it
     * is important to catch the corresponding exception when calling
     * std::unique_lock::unlock(). */
    static std::unique_lock<std::timed_mutex>& getLockObject();

    /** Map of thread names */
    std::map<boost::thread::id, std::string> threadNames;

    /// Mutex for accessing threadNames
    std::mutex threadNamesMutex;

    /**
     * Get string holding the name of the current thread or the specified thread ID. This is used e.g. for debugging
     * output of the testable mode. Will return "*UNKNOWN_THREAD*" if the name for the given ID has not yet been set.
     */
    std::string threadName(const boost::thread::id& threadId = boost::this_thread::get_id());
  };

  /********************************************************************************************************************/
  /********************************************************************************************************************/
  /***** Inline implementations for TestableMode **/
  /********************************************************************************************************************/
  /********************************************************************************************************************/

  template<typename T>
  TestableMode::AccessorPair<T> TestableMode::decorate(
      AccessorPair<T> other, const VariableNetworkNode& producer, const VariableNetworkNode& consumer) {
    if(not enabled) return other;

    // create variable IDs
    size_t varId = detail::TestableMode::getNextVariableId();
    size_t varIdReturn;
    AccessorPair<T> result;

    if(producer.getDirection().withReturn) varIdReturn = detail::TestableMode::getNextVariableId();

    // decorate the process variable if testable mode is enabled and mode is push-type
    if(!producer.getDirection().withReturn) {
      result.first = boost::make_shared<AccessorDecorator<T>>(*this, other.first, false, true, varId, varId);
      result.second = boost::make_shared<AccessorDecorator<T>>(*this, other.second, true, false, varId, varId);
    }
    else {
      result.first = boost::make_shared<AccessorDecorator<T>>(*this, other.first, true, true, varIdReturn, varId);
      result.second = boost::make_shared<AccessorDecorator<T>>(*this, other.second, true, true, varId, varIdReturn);
    }

    // put the decorators into the list
    auto& variable = variables[varId];
    variable.name = "Internal:" + producer.getQualifiedName();
    if(consumer.getType() != NodeType::invalid) {
      variable.name += "->" + consumer.getQualifiedName();
    }
    if(producer.getDirection().withReturn) {
      auto& returnVariable = variables[varIdReturn];
      returnVariable.name = variable.name + " (return)";
    }

    return result;
  }

  /********************************************************************************************************************/

  template<typename T>
  boost::shared_ptr<NDRegisterAccessor<T>> TestableMode::decorate(
      boost::shared_ptr<NDRegisterAccessor<T>> other, DecoratorType direction, const std::string& name, size_t varId) {
    if(not enabled) {
      return other;
    }

    if(varId == 0) {
      varId = detail::TestableMode::getNextVariableId();
    }

    auto pvarDec = boost::make_shared<AccessorDecorator<T>>(
        *this, other, direction == DecoratorType::READ, direction == DecoratorType::WRITE, varId, varId);
    if(not name.empty()) {
      variables[varId].name = name;
    }

    return pvarDec;
  }

  /********************************************************************************************************************/
  /********************************************************************************************************************/
  /***** Implementations for TestableMode::AccessorDecorator **/
  /********************************************************************************************************************/
  /********************************************************************************************************************/

  template<typename UserType>
  TestableMode::AccessorDecorator<UserType>::AccessorDecorator(detail::TestableMode& testableMode,
      boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>> accessor, bool handleRead, bool handleWrite,
      size_t variableIdRead, size_t variableIdWrite)
  : ChimeraTK::NDRegisterAccessorDecorator<UserType>(accessor), _handleRead(handleRead), _handleWrite(handleWrite),
    _variableIdRead(variableIdRead), _variableIdWrite(variableIdWrite), _testableMode(testableMode) {
    assert(_variableIdRead != 0);
    assert(_variableIdWrite != 0);

    // if receiving end, register for testable mode (stall detection)
    if(this->isReadable() && handleRead) {
      _testableMode.variables[_variableIdRead].processVariable = accessor;
      assert(accessor->getAccessModeFlags().has(AccessMode::wait_for_new_data));
    }

    // if this decorating a bidirectional process variable, set the
    // valueRejectCallback
    auto bidir = boost::dynamic_pointer_cast<BidirectionalProcessArray<UserType>>(accessor);
    if(bidir) {
      bidir->setValueRejectCallback([this] { decrementCounter(); });
    }
    else {
      assert(!(handleRead && handleWrite));
    }
  }

  /********************************************************************************************************************/

  template<typename UserType>
  bool TestableMode::AccessorDecorator<UserType>::doWriteTransfer(ChimeraTK::VersionNumber versionNumber) {
    if(!_handleWrite) return _target->writeTransfer(versionNumber);
    return accountForWriteOperation([this, versionNumber]() { return _target->writeTransfer(versionNumber); });
  }

  /********************************************************************************************************************/

  template<typename UserType>
  bool TestableMode::AccessorDecorator<UserType>::doWriteTransferDestructively(ChimeraTK::VersionNumber versionNumber) {
    if(!_handleWrite) return _target->writeTransferDestructively(versionNumber);

    return accountForWriteOperation(
        [this, versionNumber]() { return _target->writeTransferDestructively(versionNumber); });
  }

  /********************************************************************************************************************/

  template<typename UserType>
  void TestableMode::AccessorDecorator<UserType>::releaseLock() {
    if(_testableMode.testLock()) _testableMode.unlock("doReadTransfer " + this->getName());
  }

  /********************************************************************************************************************/

  template<typename UserType>
  void TestableMode::AccessorDecorator<UserType>::doPreRead(TransferType type) {
    _target->preRead(type);

    // Blocking reads have to release the lock so the data transport can happen
    if(_handleRead && type == TransferType::read &&
        TransferElement::_accessModeFlags.has(AccessMode::wait_for_new_data)) {
      releaseLock();
    }
  }

  /********************************************************************************************************************/

  template<typename UserType>
  void TestableMode::AccessorDecorator<UserType>::obtainLockAndDecrementCounter(bool hasNewData) {
    if(!_testableMode.testLock()) _testableMode.lock("doReadTransfer " + this->getName());
    if(!hasNewData) return;
    auto& variable = _testableMode.variables[_variableIdRead];
    if(variable.counter > 0) {
      assert(_testableMode.counter > 0);
      --_testableMode.counter;
      --variable.counter;
      if(_testableMode.enableDebug) {
        std::cout << "TestableModeAccessorDecorator[name='" << this->getName() << "', id=" << _variableIdRead
                  << "]: testableMode.counter decreased, now at value " << _testableMode.counter << " / "
                  << variable.counter << std::endl;
      }
    }
    else {
      if(_testableMode.enableDebug) {
        std::cout << "TestableModeAccessorDecorator[name='" << this->getName() << "', id=" << _variableIdRead
                  << "]: testableMode.counter NOT decreased, was already at value " << _testableMode.counter << " / "
                  << variable.counter << std::endl;
        std::cout << variable.name << std::endl;
      }
    }
  }

  /********************************************************************************************************************/

  template<typename UserType>
  void TestableMode::AccessorDecorator<UserType>::decrementCounter() {
    obtainLockAndDecrementCounter(true);
    releaseLock();
  }

  /********************************************************************************************************************/

  template<typename UserType>
  void TestableMode::AccessorDecorator<UserType>::doPostRead(TransferType type, bool hasNewData) {
    if(_handleRead) obtainLockAndDecrementCounter(hasNewData);
    ChimeraTK::NDRegisterAccessorDecorator<UserType>::doPostRead(type, hasNewData);
  }

  template<typename UserType>
  bool TestableMode::AccessorDecorator<UserType>::accountForWriteOperation(
      const std::function<bool(void)>& writeOperation) {
    bool dataLost = false;
    if(!_testableMode.testLock()) {
      // may happen if first write in thread is done before first blocking read
      _testableMode.lock("write " + this->getName());
    }

    dataLost = writeOperation();

    if(!dataLost) {
      ++_testableMode.counter;
      ++_testableMode.variables[_variableIdWrite].counter;
      if(_testableMode.enableDebug) {
        std::cout << "TestableModeAccessorDecorator::write[name='" << this->getName() << "', id=" << _variableIdWrite
                  << "]: testableMode.counter increased, now at value " << _testableMode.counter << std::endl;
      }
    }
    else {
      if(_testableMode.enableDebug) {
        std::cout << "TestableModeAccessorDecorator::write[name='" << this->getName() << "', id=" << _variableIdWrite
                  << "]: testableMode.counter not increased due to lost data" << std::endl;
      }
    }
    return dataLost;
  }

  /********************************************************************************************************************/

} // namespace ChimeraTK::detail
