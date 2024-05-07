// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <ChimeraTK/cppext/future_queue.hpp>

#include <boost/thread.hpp>

#include <sstream>

namespace ChimeraTK {

  /********************************************************************************************************************/

  class Logger {
   public:
    /**
     * Obtain global instance of Logger singleton
     */
    static Logger& getInstance();

    /**
     * Severity levels used by the Logger. Note: there is no "fatal" severity, since the logger system does not work
     * if the application is terminated immediately after sending a message to the log. Fatal errors shall be printed
     * directly to std::cerr before terminating the application.
     */
    enum class Severity { trace, debug, info, warning, error };

    /**
     * Set the minimum severity level to be passed to the logger. By default, the minimum severity is set to
     * Severity::info, so that trace and debug messages will not be processed. This will also prevent (to a certain
     * extend) that the message text is composed at sender-side, which improves performance.
     */
    void setMinSeverity(Severity minSeverity) { _minSeverity = minSeverity; }

    /**
     * Proxy for output stream, handed out to the log sources by the Logger::Module.
     */
    class StreamProxy : public std::ostream {
     public:
      StreamProxy(Logger* logger, Severity severity, std::string context);
      ~StreamProxy() override;

     private:
      std::stringbuf _buf;
      Severity _severity;
      std::string _context;
      Logger& _logger;
    };

    /**
     * Return an output stream object for the given severity. Writing to the stream object will compose the log message
     * locally, if the given severity is above the configured minimum severity (cf. setMinSeverity()). When the stream
     * object is destroyed (i.e. as it goes out of scope) the message will be sent to the logging thread for further
     * processing. The stream object shall not live long, typically each log line will use its own stream object, unless
     * multiple lines shall be sent and printed consistently, in which case "\n" or std::endl can be written to the
     * stream. It is not necessary to terminate the line ("\n" or std::endl) manually before destrying the stream
     * object, since the
     *
     * If the given severity is below the minimum severity, the stream will be in a failed state and hence writing to
     * the stream will be a no-op (like std::ostream constructed from a nullptr). If data written to the stream is
     * expensive to obtain, it is recommended to check StreamProxy::good() before computing the data.
     *
     * The given context string will be used to identify the source of the log information.
     *
     * Note: Consider using the convenience function ChimeraTK::logger() instead of calling this member function
     * directly.
     */
    StreamProxy getStream(Severity severity, std::string context);

    ~Logger();

   private:
    Logger() = default;

    /**
     * Return a shared_ptr for the Logger instance. This will be used by the Application class to make sure the Logger
     * lives at least as long as the Application. We return a reference to a singleton instance of the shared_ptr, so
     * we do not have to increase/decrease the reference counter each time we use the Logger.
     */
    static std::shared_ptr<Logger>& getSharedPtr();

    /**
     * Log the given message with the given severity. The logging happens asynchronously to this function call, but
     * subsequent calles from the same thread to this function will preserve the order of log messages.
     *
     * This function is thread safe and may be called concurrently from multiple threads.
     */
    void log(Severity severity, std::string context, std::string message) noexcept;

    // The mainLoop() is executed in a dedicated thread waits for incoming log messages and prints them
    void mainLoop();

    // Convert Severity to string
    static std::string severityToString(Severity severity);

    // Struct with all arguments of log(), to be placed on to the _messageQueue
    struct LogMessage {
      Severity severity{Severity::info};
      std::string context;
      std::string text;
    };

    // This queue is filled in log() and read in mainLoop()
    cppext::future_queue<LogMessage> _messageQueue{10};

    // Minimum severity to be sent to the queue. This allows filtering lower severity messages at sender side, even
    // before the message text has been (fully) composed.
    std::atomic<Severity> _minSeverity{Severity::info};

    // Thread executing mainLoop().
    // Note: The thread must be started only after all other data members have been initialised, so this line must
    // come last (or the thread must be only started in the constructor body).
    boost::thread _mainLoopThread{[this] { mainLoop(); }};

    friend class Application;
  };

  /********************************************************************************************************************/

  /**
   * Convenience function to obtain the logger stream
   */
  inline Logger::StreamProxy logger(Logger::Severity severity, std::string context) {
    return Logger::getInstance().getStream(severity, std::move(context));
  }

  /********************************************************************************************************************/
  /********************************************************************************************************************/

  inline Logger& Logger::getInstance() {
    return *getSharedPtr();
  }

  /********************************************************************************************************************/

  inline std::shared_ptr<Logger>& Logger::getSharedPtr() {
    static std::shared_ptr<Logger> instance(new Logger());
    return instance;
  }

  /********************************************************************************************************************/

} // namespace ChimeraTK
