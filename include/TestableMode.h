// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "Logger.h"
#include "VariableNetworkNode.h"

#include <ChimeraTK/ControlSystemAdapter/BidirectionalProcessArray.h>
#include <ChimeraTK/TransferElement.h>

#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>

#include <atomic>
#include <cstddef>
#include <map>
#include <shared_mutex>

namespace ChimeraTK {
  class ConnectionMaker;
  class DeviceManager;
  class TriggerFanOut;
  class TestFacility;
} /* namespace ChimeraTK */

namespace ChimeraTK::detail {
  struct TestableMode {
    /**
     * Lock the testable mode mutex for the current thread. Internally, a thread-local std::unique_lock<std::mutex> will
     * be created and re-used in subsequent calls within the same thread to this function and to testableModeUnlock().
     *
     * The shared parameter determines whether the mutex is locked shared or unique. The test always needs to acquire
     * a unique lock while the tested application can acquire a shared lock (so multiple application threads can run in
     * parallel).
     *
     * This function should generally not be used in user code.
     */
    void lock(const std::string& name, bool shared);

    /**
     * Unlock the testable mode mutex for the current thread. See also testableModeLock().
     *
     * Initially the lock will not be owned by the current thread, so the first call to this function will throw an
     * exception (see std::unique_lock::unlock()), unless testableModeLock() has been called first.
     *
     * This function should generally not be used in user code.
     */
    void unlock(const std::string& name);

    /**
     * Test if the testable mode mutex is locked by the current thread.
     *
     * This function should generally not be used in user code.
     */
    [[nodiscard]] bool testLock() const;

    /**
     * Check whether set() can be called or it would throw due to no data in the queues.
     */
    [[nodiscard]] bool canStep() const { return _counter != 0; }

    /**
     * Let the application modules run until all data in the queues have been processed. Will throw if no data is
     * available in tue queues to be processed.
     */
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

    /**
     * Enable noisy debugging output for testable mode
     */
    void setEnableDebug(bool enable = true) { _enableDebug = enable; }

    /**
     * Enable debugging output for decorating the accessor
     */
    void setEnableDebugDecorating(bool enable = true) { _debugDecorating = enable; }

    /**
     * Check whether testable mode has been enabled
     */
    [[nodiscard]] bool isEnabled() const { return _enabled; }

    /**
     * Return a fresh variable ID which can be assigned to a sender/receiver pair. The ID will always be non-zero.
     */
    static size_t getNextVariableId() {
      static size_t nextId{0};
      return ++nextId;
    }

    enum class DecoratorType { READ, WRITE };

    template<typename T>
    boost::shared_ptr<NDRegisterAccessor<T>> decorate(boost::shared_ptr<NDRegisterAccessor<T>> other,
        DecoratorType direction, const std::string& name = {}, size_t varId = 0);

    template<typename T>
    using AccessorPair = std::pair<boost::shared_ptr<NDRegisterAccessor<T>>, boost::shared_ptr<NDRegisterAccessor<T>>>;

    template<typename T>
    AccessorPair<T> decorate(
        AccessorPair<T> other, const VariableNetworkNode& producer, const VariableNetworkNode& consumer);

    /**
     * Decorator of the NDRegisterAccessor which facilitates tests of the application
     */
    template<typename UserType>
    class AccessorDecorator : public ChimeraTK::NDRegisterAccessorDecorator<UserType> {
     public:
      AccessorDecorator(detail::TestableMode& testableMode,
          boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>> accessor, bool handleRead, bool handleWrite,
          size_t variableIdRead, size_t variableIdWrite);

      // FIXME: https://redmine.msktools.desy.de/issues/12242
      // NOLINTNEXTLINE(google-default-arguments)
      bool doWriteTransfer(ChimeraTK::VersionNumber versionNumber = {}) override;

      // FIXME: https://redmine.msktools.desy.de/issues/12242
      // NOLINTNEXTLINE(google-default-arguments)
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

      [[nodiscard]] boost::shared_ptr<NDRegisterAccessor<UserType>> decorateDeepInside(
          [[maybe_unused]] std::function<boost::shared_ptr<NDRegisterAccessor<UserType>>(
              const boost::shared_ptr<NDRegisterAccessor<UserType>>&)> factory) override {
        // By returning nullptr, we forbid that DataConsistencyDecorator is put inside of this decorator.
        // It would mess up our data updates counting scheme.
        return {};
      }

     protected:
      using ChimeraTK::NDRegisterAccessor<UserType>::buffer_2D;
      using ChimeraTK::NDRegisterAccessorDecorator<UserType>::_target;

      bool _handleRead, _handleWrite;
      size_t _variableIdRead, _variableIdWrite;
      TestableMode& _testableMode;

      bool accountForWriteOperation(const std::function<bool(void)>& writeOperation);
    };

   private:
    friend class ChimeraTK::DeviceManager;
    friend class ChimeraTK::TriggerFanOut;
    friend class ChimeraTK::TestFacility;

    /**
     * Flag if noisy debug output is enabled for the testable mode
     */
    bool _enableDebug{false};

    /**
     * Semaphore counter used in testable mode to check if application code is finished executing. This value may only
     * be accessed while holding the testableMode.mutex.
     */
    std::atomic<size_t> _counter{0};

    /**
     * Flag if connections should be made in testable mode (i.e. the TestableModeAccessorDecorator is put around all
     * push-type input accessors etc.).
     */
    bool _enabled{false};

    /**
     * Semaphore counter used in testable mode to check if device initialisation is finished executing. This value may
     * only be accessed while holding mutex. This counter is a separate counter from counter so stepApplication() can be
     * controlled whether to obey this counter.
     */
    std::atomic<size_t> _deviceInitialisationCounter{0};

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
      std::atomic<size_t> counter{0};
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
    static std::shared_timed_mutex _mutex;

    /**
     * This additional mutex is only there to eliminate a false positive in TSAN:
     * https://github.com/llvm/llvm-project/issues/142370
     *
     * Theis additional mutex is not "timed" so TSAN properly recognises it. It will be only locked after the timed
     * _mutex has been locked successfully, so that it will always succeed in locking immediately (without blocking).
     * This should add minimal overhead while fixing the TSAN false positive.
     *
     * Using a boost::shared_mutex instead had severe performance issues.
     */
    static std::shared_mutex _mutex2;

    /**
     * Map of unique IDs to a VariableDescriptor holding information about Variables
     * that are being tracked as part of testable mode
     */
    std::map<size_t, VariableDescriptor> _variables;

    /**
     * Last thread which successfully obtained the lock for the testable mode. This is used to prevent spamming
     * repeating messages if the same thread acquires and releases the lock in a loop without another thread activating
     * in between.
     *
     * @todo: After moving away from Ubuntu 20.04, we can replace this construction with an
     *        std::atomic<boost::thread::id>.
     */
    class LastMutexOwner {
     public:
      LastMutexOwner& operator=(const boost::thread::id& id);
      // NOLINTNEXTLINE(google-explicit-constructor)
      operator boost::thread::id();

     private:
      boost::thread::id _lastMutexOwner;
      std::mutex _mxLastMutexOwner;
    } _lastMutexOwner;

    // forward declaration
    class Lock;

    /** Obtain the lock object for the testable mode lock for the current thread.
     * The returned object has thread_local storage duration and must only be used
     * inside the current thread. Initially (i.e. after the first call in one
     * particular thread) the lock will not be owned by the returned object, so it
     * is important to catch the corresponding exception when calling
     * std::unique_lock::unlock(). */
    static Lock& getLockObject();

    /**
     * Class to manage locking/unlocking the _mutex within a thread. It allows a timed try-lock on the mutex with either
     * shared or exclusive access. The unlock() function will use the appropriate unlock function matching the last lock
     * type (shared/exclusive). It will also keep track of the current lock ownership.
     *
     * To obtain such object, use getLockObject().
     */
    class Lock {
     public:
      [[nodiscard]] bool tryLockFor(std::chrono::seconds timeout, bool shared);
      void unlock();
      [[nodiscard]] bool ownsLock() const { return _ownsLock; }
      ~Lock();

     private:
      Lock() = default;

      bool _ownsLock{false};
      bool _isShared{false}; // only meaningful when _ownsLock = true

      friend Lock& TestableMode::getLockObject();
    };

    /** Map of thread names */
    std::map<boost::thread::id, std::string> _threadNames;

    /** Map of pthread ids. Use only for debugging purpuses. */
    std::map<boost::thread::id, pid_t> _threadPThreadId;

    /// Mutex for accessing threadNames
    std::mutex _threadNamesMutex;

    /**
     * Get string holding the name of the current thread or the specified thread ID. This is used e.g. for debugging
     * output of the testable mode. Will return "*UNKNOWN_THREAD*" if the name for the given ID has not yet been set.
     */
    std::string threadName(const boost::thread::id& threadId = boost::this_thread::get_id());

    /**
     * Get the pthread id for the given thread. Use only for debugging purposes!
     *
     * Background: Debuggers (like VS code) print this ID in the callstack, which makes it easier to find the right
     * thread.
     *
     * Returns 0 if thread ID is not known.
     */
    pid_t pthreadId(const boost::thread::id& threadId = boost::this_thread::get_id());

    bool _debugDecorating{false};
    friend class ChimeraTK::ConnectionMaker;
  };

  /********************************************************************************************************************/
  /********************************************************************************************************************/
  /***** Inline implementations for TestableMode **/
  /********************************************************************************************************************/
  /********************************************************************************************************************/

  template<typename T>
  TestableMode::AccessorPair<T> TestableMode::decorate(
      AccessorPair<T> other, const VariableNetworkNode& producer, const VariableNetworkNode& consumer) {
    if(not _enabled) {
      return other;
    }

    if(_debugDecorating) {
      logger(Logger::Severity::debug, "TestableMode")
          << "      Decorating pair " << producer.getQualifiedName() << "[" << other.first->getId() << "] -> "
          << consumer.getQualifiedName() << "[" << other.second->getId() << "]";
    }

    // create variable IDs
    size_t varId = detail::TestableMode::getNextVariableId();
    size_t varIdReturn;
    AccessorPair<T> result;

    if(producer.getDirection().withReturn) {
      varIdReturn = detail::TestableMode::getNextVariableId();
    }

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
    auto& variable = _variables.at(varId);
    variable.name = "Internal:" + producer.getQualifiedName();
    if(consumer.getType() != NodeType::invalid) {
      variable.name += "->" + consumer.getQualifiedName();
    }
    if(producer.getDirection().withReturn) {
      auto& returnVariable = _variables.at(varIdReturn);
      returnVariable.name = variable.name + " (return)";
    }

    return result;
  }

  /********************************************************************************************************************/

  template<typename T>
  boost::shared_ptr<NDRegisterAccessor<T>> TestableMode::decorate(
      boost::shared_ptr<NDRegisterAccessor<T>> other, DecoratorType direction, const std::string& name, size_t varId) {
    if(not _enabled) {
      return other;
    }

    if(_debugDecorating) {
      logger(Logger::Severity::debug, "TestableMode")
          << "      Decorating single " << (direction == DecoratorType::READ ? "consumer " : "feeder ") << name << "["
          << other->getId() << "]";
    }

    if(varId == 0) {
      varId = detail::TestableMode::getNextVariableId();
    }

    _variables[varId].processVariable = other;
    if(not name.empty()) {
      _variables.at(varId).name = name;
    }

    auto pvarDec = boost::make_shared<AccessorDecorator<T>>(
        *this, other, direction == DecoratorType::READ, direction == DecoratorType::WRITE, varId, varId);

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
      _testableMode._variables[_variableIdRead].processVariable = accessor;
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
  // FIXME: https://redmine.msktools.desy.de/issues/12242
  // NOLINTNEXTLINE(google-default-arguments)
  bool TestableMode::AccessorDecorator<UserType>::doWriteTransfer(ChimeraTK::VersionNumber versionNumber) {
    if(!_handleWrite) {
      return _target->writeTransfer(versionNumber);
    }
    return accountForWriteOperation([this, versionNumber]() { return _target->writeTransfer(versionNumber); });
  }

  /********************************************************************************************************************/

  template<typename UserType>
  // FIXME: https://redmine.msktools.desy.de/issues/12242
  // NOLINTNEXTLINE(google-default-arguments)
  bool TestableMode::AccessorDecorator<UserType>::doWriteTransferDestructively(ChimeraTK::VersionNumber versionNumber) {
    if(!_handleWrite) {
      return _target->writeTransferDestructively(versionNumber);
    }

    return accountForWriteOperation(
        [this, versionNumber]() { return _target->writeTransferDestructively(versionNumber); });
  }

  /********************************************************************************************************************/

  template<typename UserType>
  void TestableMode::AccessorDecorator<UserType>::releaseLock() {
    if(_testableMode.testLock()) {
      _testableMode.unlock("doReadTransfer " + this->getName());
    }
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
    if(!_testableMode.testLock()) {
      _testableMode.lock("doReadTransfer " + this->getName(), true);
    }
    if(!hasNewData) {
      return;
    }
    auto& variable = _testableMode._variables.at(_variableIdRead);
    if(variable.counter > 0) {
      assert(_testableMode._counter > 0);
      --_testableMode._counter;
      --variable.counter;
      if(_testableMode._enableDebug) {
        logger(Logger::Severity::debug, "TestableMode")
            << "TestableModeAccessorDecorator[name='" << this->getName() << "', id=" << _variableIdRead
            << "]: testableMode.counter decreased, now at value " << _testableMode._counter << " / "
            << variable.counter;
      }
    }
    else {
      if(_testableMode._enableDebug) {
        logger(Logger::Severity::debug, "TestableMode")
            << "TestableModeAccessorDecorator[name='" << this->getName() << "', id=" << _variableIdRead
            << "]: testableMode.counter NOT decreased, was already at value " << _testableMode._counter << " / "
            << variable.counter << "\n"
            << variable.name;
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
    if(_handleRead) {
      obtainLockAndDecrementCounter(hasNewData);
    }
    ChimeraTK::NDRegisterAccessorDecorator<UserType>::doPostRead(type, hasNewData);
  }

  template<typename UserType>
  bool TestableMode::AccessorDecorator<UserType>::accountForWriteOperation(
      const std::function<bool(void)>& writeOperation) {
    bool dataLost = false;
    if(!_testableMode.testLock()) {
      // may happen if first write in thread is done before first blocking read
      _testableMode.lock("write " + this->getName(), true);
    }

    // Increment counters before write(), since another thread might react to the value on the queue already and
    // try to do something with the counter( e.g. decrement it conditionally, see obtainLockAndDecrementCounter()).
    _testableMode._variables.at(_variableIdWrite).counter++;
    _testableMode._counter++;

    dataLost = writeOperation();

    if(dataLost) {
      // if data has been lost, decrement counter again since we never actually put data onto the queue.
      _testableMode._variables.at(_variableIdWrite).counter--;
      _testableMode._counter--;
    }

    if(_testableMode._enableDebug) {
      if(!dataLost) {
        logger(Logger::Severity::debug, "TestableMode")
            << "TestableModeAccessorDecorator::write[name='" << this->getName() << "', id=" << _variableIdWrite
            << "]: testableMode.counter increased, now at value " << _testableMode._counter;
      }
      else {
        logger(Logger::Severity::debug, "TestableMode")
            << "TestableModeAccessorDecorator::write[name='" << this->getName() << "', id=" << _variableIdWrite
            << "]: testableMode.counter not increased due to lost data";
      }
    }
    return dataLost;
  }

  /********************************************************************************************************************/

} // namespace ChimeraTK::detail
