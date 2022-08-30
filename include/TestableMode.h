// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <ChimeraTK/TransferElement.h>

#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>

#include <atomic>
#include <cstddef>
#include <map>
#include <mutex>

namespace ChimeraTK::detail {
  struct TestableMode {
    struct VariableDescriptor {
      /** name of the variable, used to print sensible information. */
      std::string name;

      /** The Process variable that has been decorated with a TestableModeDecorator */
      boost::shared_ptr<TransferElement> processVariable;

      /** Whether the update mode is UpdateMode::poll (so we do not use the decorator) */
      bool isPollMode{false};

      /** Like the global counter but broken out for each variable.
       * This is not actually used as a semaphore counter but only in case of a
       * detected stall (see repeatingMutexOwner) to print a list of
       * variables which still contain unread values
       */
      size_t counter{0};
    };

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

    /**
     * Map of unique IDs to a VariableDescriptor holding information about Variables
     * that are being tracked as part of testable mode
     */
    std::map<size_t, VariableDescriptor> variables;

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

   private:
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
} // namespace ChimeraTK::detail
