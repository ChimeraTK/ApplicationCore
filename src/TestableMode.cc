// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "TestableMode.h"

#include "Application.h"
namespace ChimeraTK::detail {
  std::timed_mutex TestableMode::mutex;
  /*********************************************************************************************************************/

  void TestableMode::enable() {
    setThreadName("TEST THREAD");
    enabled = true;
    lock("enableTestableMode");
  }

  /*********************************************************************************************************************/

  std::unique_lock<std::timed_mutex>& TestableMode::getLockObject() {
    // Note: due to a presumed bug in gcc (still present in gcc 7), the
    // thread_local definition must be in the cc file to prevent seeing different
    // objects in the same thread under some conditions. Another workaround for
    // this problem can be found in commit
    // dc051bfe35ce6c1ed954010559186f63646cf5d4
    thread_local std::unique_lock<std::timed_mutex> myLock(TestableMode::mutex, std::defer_lock);
    return myLock;
  }
  /*********************************************************************************************************************/

  bool TestableMode::testLock() {
    if(not enabled) return false;
    return getLockObject().owns_lock();
  }

  /*********************************************************************************************************************/

  void TestableMode::lock(const std::string& name) {
    // don't do anything if testable mode is not enabled
    if(not enabled) return;

    // debug output if enabled (also prevent spamming the same message)
    if(enableDebug && repeatingMutexOwner == 0) {                             // LCOV_EXCL_LINE
                                                                              // (only cout)
      std::cout << "Application::testableModeLock(): Thread " << threadName() // LCOV_EXCL_LINE (only cout)
                << " tries to obtain lock for " << name << std::endl;         // LCOV_EXCL_LINE (only cout)
    }                                                                         // LCOV_EXCL_LINE (only cout)

    // if last lock was obtained repeatedly by the same thread, sleep a short time
    // before obtaining the lock to give the other threads a chance to get the
    // lock first
    if(repeatingMutexOwner > 0) usleep(10000);

    // obtain the lock
    auto lastSeen_lastOwner = lastMutexOwner;
  repeatTryLock:
    auto success = getLockObject().try_lock_for(std::chrono::seconds(30));
    if(!success) {
      auto currentLastOwner = lastMutexOwner;
      if(currentLastOwner != lastSeen_lastOwner) {
        lastSeen_lastOwner = currentLastOwner;
        goto repeatTryLock;
      }
      std::cout << "testableModeLock(): Thread " << threadName()                // LCOV_EXCL_LINE
                << " could not obtain lock for 30 seconds, presumably because " // LCOV_EXCL_LINE
                << threadName(lastMutexOwner) << " does not release it."        // LCOV_EXCL_LINE
                << std::endl;                                                   // LCOV_EXCL_LINE

      // throw a specialised exception to make sure whoever catches it really knows what he does...
      throw TestsStalled(); // LCOV_EXCL_LINE
    }                       // LCOV_EXCL_LINE

    // check if the last owner of the mutex was this thread, which may be a hint
    // that no other thread is waiting for the lock
    if(lastMutexOwner == boost::this_thread::get_id()) {
      // debug output if enabled
      if(enableDebug && repeatingMutexOwner == 0) {                         // LCOV_EXCL_LINE
                                                                            // (only cout)
        std::cout << "testableModeLock(): Thread " << threadName()          // LCOV_EXCL_LINE (only cout)
                  << " repeatedly obtained lock successfully for " << name  // LCOV_EXCL_LINE (only cout)
                  << ". Further messages will be suppressed." << std::endl; // LCOV_EXCL_LINE (only cout)
      }                                                                     // LCOV_EXCL_LINE (only cout)

      // increase counter for stall detection
      repeatingMutexOwner++;

      // detect stall: if the same thread got the mutex with no other thread
      // obtaining it in between for one second, we assume no other thread is able
      // to process data at this time. The test should fail in this case
      if(repeatingMutexOwner > 100) {
        // print an informative message first, which lists also all variables
        // currently containing unread data.
        std::cerr << "*** Tests are stalled due to data which has been sent but not received." << std::endl;
        std::cerr << "    The following variables still contain unread values or had data loss due to a queue overflow:"
                  << std::endl;
        for(auto& pair : variables) {
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
            catch(std::logic_error&) {
              // if we receive a logic_error in readNonBlocking() it just means
              // another thread is waiting on a TransferFuture of this variable,
              // and we actually were not allowed to read...
              std::cerr << " (data loss)";
            }
            std::cerr << std::endl;
          }
        }
        std::cerr << "(end of list)" << std::endl;
        // Check for modules waiting for initial values (prints nothing if there are no such modules)
        Application::getInstance().circularDependencyDetector.printWaiters();
        // throw a specialised exception to make sure whoever catches it really knows what he does...
        throw TestsStalled();
      }
    }
    else {
      // last owner of the mutex was different: reset the counter and store the
      // thread id
      repeatingMutexOwner = 0;
      lastMutexOwner = boost::this_thread::get_id();

      // debug output if enabled
      if(enableDebug) {                                                       // LCOV_EXCL_LINE (only cout)
        std::cout << "TestableMode::lock(): Thread " << threadName()          // LCOV_EXCL_LINE (only cout)
                  << " obtained lock successfully for " << name << std::endl; // LCOV_EXCL_LINE (only cout)
      }                                                                       // LCOV_EXCL_LINE (only cout)
    }
  }

  /*********************************************************************************************************************/

  void TestableMode::unlock(const std::string& name) {
    if(not enabled) return;
    if(enableDebug &&
        (not repeatingMutexOwner                                     // LCOV_EXCL_LINE (only cout)
            || lastMutexOwner != boost::this_thread::get_id())) {    // LCOV_EXCL_LINE (only cout)
      std::cout << "TestableMode::unlock(): Thread " << threadName() // LCOV_EXCL_LINE (only cout)
                << " releases lock for " << name << std::endl;       // LCOV_EXCL_LINE (only cout)
    }
    // LCOV_EXCL_LINE (only cout)
    getLockObject().unlock();
  }

  /*********************************************************************************************************************/

  void TestableMode::step(bool waitForDeviceInitialisation) {
    // testableMode.counter must be non-zero, otherwise there is no input for the application to process. It is also
    // sufficient if testableMode.deviceInitialisationCounter is non-zero, if waitForDeviceInitialisation == true. In
    // that case we only wait for the device initialisation to be completed.
    if(counter == 0 && (!waitForDeviceInitialisation || deviceInitialisationCounter == 0)) {
      throw ChimeraTK::logic_error("Application::stepApplication() called despite no input was provided "
                                   "to the application to process!");
    }
    // let the application run until it has processed all data (i.e. the semaphore
    // counter is 0)
    size_t oldCounter = 0;
    while(counter > 0 || (waitForDeviceInitialisation && deviceInitialisationCounter > 0)) {
      if(enableDebug && (oldCounter != counter)) { // LCOV_EXCL_LINE (only cout)
        std::cout << "Application::stepApplication(): testableMode.counter = " << counter
                  << std::endl; // LCOV_EXCL_LINE (only cout)
        oldCounter = counter;   // LCOV_EXCL_LINE (only cout)
      }
      unlock("stepApplication");
      boost::this_thread::yield();
      lock("stepApplication");
    }
  }

  /*********************************************************************************************************************/

  void TestableMode::setThreadName(const std::string& name) {
    std::unique_lock<std::mutex> myLock(threadNamesMutex);
    threadNames[boost::this_thread::get_id()] = name;
  }

  /*********************************************************************************************************************/

  std::string TestableMode::threadName(const boost::thread::id& threadId) {
    std::unique_lock<std::mutex> myLock(threadNamesMutex);
    if(auto const& it = threadNames.find(threadId); it != threadNames.end()) {
      return it->second;
    }

    return "*UNKNOWN_THREAD*";
  }

} // namespace ChimeraTK::detail
