// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "TestableMode.h"

#include "Application.h"
#include "Utilities.h"

namespace ChimeraTK::detail {

  std::shared_timed_mutex TestableMode::_mutex;
  std::shared_mutex TestableMode::_mutex2;

  /********************************************************************************************************************/

  void TestableMode::enable() {
    setThreadName("TEST THREAD");
    _enabled = true;
    lock("enableTestableMode", false);
  }

  /********************************************************************************************************************/

  bool TestableMode::Lock::tryLockFor(std::chrono::seconds timeout, bool shared) {
    assert(!_ownsLock);
    _isShared = shared;
    if(shared) {
      _ownsLock = _mutex.try_lock_shared_for(timeout);
      if(_ownsLock) {
        _mutex2.lock_shared();
      }
      return _ownsLock;
    }
    _ownsLock = _mutex.try_lock_for(timeout);
    if(_ownsLock) {
      _mutex2.lock();
    }
    return _ownsLock;
  }

  /********************************************************************************************************************/

  void TestableMode::Lock::unlock() {
    assert(_ownsLock);
    _ownsLock = false;
    if(_isShared) {
      _mutex2.unlock_shared();
      _mutex.unlock_shared();
    }
    else {
      _mutex2.unlock();
      _mutex.unlock();
    }
  }

  /********************************************************************************************************************/

  TestableMode::Lock::~Lock() {
    if(_ownsLock) {
      unlock();
    }
  }

  /********************************************************************************************************************/

  TestableMode::Lock& TestableMode::getLockObject() {
    // Note: due to a presumed bug in gcc (still present in gcc 7), the
    // thread_local definition must be in the cc file to prevent seeing different
    // objects in the same thread under some conditions. Another workaround for
    // this problem can be found in commit
    // dc051bfe35ce6c1ed954010559186f63646cf5d4
    thread_local Lock myLock;
    return myLock;
  }

  /********************************************************************************************************************/

  bool TestableMode::testLock() const {
    if(not _enabled) {
      return false;
    }
    return getLockObject().ownsLock();
  }

  /********************************************************************************************************************/

  namespace {
    /// This is a trick to make sure the exception is never caught, not even by the BOOST test framework.
    void terminateTestStalled() {
      struct TestStalled : std::exception {
        [[nodiscard]] const char* what() const noexcept override { return "Test stalled."; }
      };
      try {
        throw TestStalled();
      }
      catch(...) {
        std::terminate();
      }
    }
  } // namespace

  /********************************************************************************************************************/

  void TestableMode::lock(const std::string& name, bool shared) {
    // don't do anything if testable mode is not enabled
    if(not _enabled) {
      return;
    }

    // debug output if enabled (also prevent spamming the same message)
    if(_enableDebug) {                                                  // LCOV_EXCL_LINE (only cout)
      logger(Logger::Severity::debug, "TestableMode")                   // LCOV_EXCL_LINE (only cout)
          << "Application::testableModeLock(): Thread " << threadName() // LCOV_EXCL_LINE (only cout)
          << " tries to obtain lock for " << name;                      // LCOV_EXCL_LINE (only cout)
    } // LCOV_EXCL_LINE (only cout)

    // obtain the lock
    boost::thread::id lastSeen_lastOwner = _lastMutexOwner;
  repeatTryLock:
    auto success = getLockObject().tryLockFor(std::chrono::seconds(30), shared);
    boost::thread::id currentLastOwner = _lastMutexOwner;
    if(!success) {
      if(currentLastOwner != lastSeen_lastOwner) {
        lastSeen_lastOwner = currentLastOwner;
        usleep(10000);
        goto repeatTryLock;
      }
      std::cerr << "testableModeLock(): Thread " << threadName()                         // LCOV_EXCL_LINE
                << " could not obtain lock for at least 30 seconds, presumably because " // LCOV_EXCL_LINE
                << threadName(_lastMutexOwner) << " [" << pthreadId(_lastMutexOwner)     // LCOV_EXCL_LINE
                << "] does not release it."                                              // LCOV_EXCL_LINE
                << std::endl;                                                            // LCOV_EXCL_LINE
      terminateTestStalled();                                                            // LCOV_EXCL_LINE
    } // LCOV_EXCL_LINE

    _lastMutexOwner = boost::this_thread::get_id();

    // debug output if enabled
    if(_enableDebug) {                                       // LCOV_EXCL_LINE (only cout)
      logger(Logger::Severity::debug, "TestableMode")        // LCOV_EXCL_LINE (only cout)
          << "TestableMode::lock(): Thread " << threadName() // LCOV_EXCL_LINE (only cout)
          << " obtained lock successfully for " << name;     // LCOV_EXCL_LINE (only cout)
    } // LCOV_EXCL_LINE (only cout)
  }

  /********************************************************************************************************************/

  void TestableMode::unlock(const std::string& name) {
    if(not _enabled) {
      return;
    }
    if(_enableDebug) {                                         // LCOV_EXCL_LINE (only cout)
      logger(Logger::Severity::debug, "TestableMode")          // LCOV_EXCL_LINE (only cout)
          << "TestableMode::unlock(): Thread " << threadName() // LCOV_EXCL_LINE (only cout)
          << " releases lock for " << name;                    // LCOV_EXCL_LINE (only cout)
    } // LCOV_EXCL_LINE (only cout)
    getLockObject().unlock();
  }

  /********************************************************************************************************************/

  void TestableMode::step(bool waitForDeviceInitialisation) {
    // testableMode.counter must be non-zero, otherwise there is no input for the application to process. It is also
    // sufficient if testableMode.deviceInitialisationCounter is non-zero, if waitForDeviceInitialisation == true. In
    // that case we only wait for the device initialisation to be completed.
    if(_counter == 0 && (!waitForDeviceInitialisation || _deviceInitialisationCounter == 0)) {
      throw ChimeraTK::logic_error("Application::stepApplication() called despite no input was provided "
                                   "to the application to process!");
    }
    // let the application run until it has processed all data (i.e. the semaphore
    // counter is 0)
    size_t oldCounter = 0;
    auto t0 = std::chrono::steady_clock::now();
    while(true) {
      if(_enableDebug && (oldCounter != _counter)) { // LCOV_EXCL_LINE (only cout)
        logger(Logger::Severity::debug, "TestableMode")
            << "Application::stepApplication(): testableMode.counter = " << _counter; // LCOV_EXCL_LINE (only cout)
        oldCounter = _counter;                                                        // LCOV_EXCL_LINE (only cout)
      }
      unlock("stepApplication");
      boost::this_thread::yield();
      lock("stepApplication", false);
      if(_counter > 0 || (waitForDeviceInitialisation && _deviceInitialisationCounter > 0)) {
        usleep(1000);

        // If the application does not finish data processing (and hence counters will stay > 0), assume the test
        // is stalled and terminate the test.
        if(std::chrono::steady_clock::now() - t0 > std::chrono::seconds(30)) {
          // print an informative message first, which lists also all variables
          // currently containing unread data.
          std::cerr << "*** Tests are stalled due to data which has been sent but not received.\n";
          std::cerr
              << "    The following variables still contain unread values or had data loss due to a queue overflow:\n";
          for(auto& pair : _variables) {
            const auto& variable = pair.second;
            if(variable.counter > 0) {
              std::cerr << "    - " << variable.name << " [" << variable.processVariable->getId() << "]";
              // check if process variable still has data in the queue
              try {
                if(variable.processVariable->readNonBlocking()) {
                  std::cerr << " (unread data in queue)";
                }
                else {
                  std::cerr << " (data loss)";
                }
              }
              catch(ChimeraTK::logic_error&) {
                // if we receive a logic_error in readNonBlocking() it just means
                // another thread is waiting on a TransferFuture of this variable,
                // and we actually were not allowed to read...
                std::cerr << " (data loss)";
              }
              std::cerr << "\n";
            }
          }
          std::cerr << "(end of list)\n";
          // Check for modules waiting for initial values (prints nothing if there are no such modules)
          Application::getInstance()._circularDependencyDetector.printWaiters(std::cerr);
          // throw a specialised exception to make sure whoever catches it really knows what he does...
          terminateTestStalled();
        }
      }
      else {
        break;
      }
    }
  }

  /********************************************************************************************************************/

  void TestableMode::setThreadName(const std::string& name) {
    std::unique_lock<std::mutex> myLock(_threadNamesMutex);
    _threadNames[boost::this_thread::get_id()] = name;
    _threadPThreadId[boost::this_thread::get_id()] = gettid();
    Utilities::setThreadName(name);
  }

  /********************************************************************************************************************/

  std::string TestableMode::threadName(const boost::thread::id& threadId) {
    std::unique_lock<std::mutex> myLock(_threadNamesMutex);
    if(auto const& it = _threadNames.find(threadId); it != _threadNames.end()) {
      return it->second;
    }

    return "*UNKNOWN_THREAD*";
  }

  /********************************************************************************************************************/

  pid_t TestableMode::pthreadId(const boost::thread::id& threadId) {
    std::unique_lock<std::mutex> myLock(_threadNamesMutex);
    if(auto const& it = _threadPThreadId.find(threadId); it != _threadPThreadId.end()) {
      return it->second;
    }

    return 0;
  }
  /********************************************************************************************************************/

  TestableMode::LastMutexOwner::operator boost::thread::id() {
    std::lock_guard<std::mutex> lk(_mxLastMutexOwner);
    return _lastMutexOwner;
  }

  /********************************************************************************************************************/

  TestableMode::LastMutexOwner& TestableMode::LastMutexOwner::operator=(const boost::thread::id& id) {
    std::lock_guard<std::mutex> lk(_mxLastMutexOwner);
    _lastMutexOwner = id;
    return *this;
  }

  /********************************************************************************************************************/

} // namespace ChimeraTK::detail
