// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "Logger.h"

#include <iostream>

namespace ChimeraTK {

  /********************************************************************************************************************/

  Logger::~Logger() {
    if(_mainLoopThread.joinable()) {
    retry:
      try {
        _mainLoopThread.interrupt();

        // try joining the thread
        do {
          // it may not suffice to send interrupt() once, as the exception might get
          // overwritten in the queue, thus we repeat this until the thread was
          // joined.
          _messageQueue.push_exception(std::make_exception_ptr(boost::thread_interrupted()));
        } while(!_mainLoopThread.try_join_for(boost::chrono::milliseconds(10)));
      }
      catch(boost::thread_interrupted&) {
        // ignore interruptions at this point
        goto retry;
      }
      catch(std::system_error& e) {
        std::cerr << "std::system_error caught: " << e.what() << std::endl;
        std::terminate();
      }
      catch(...) {
        std::cerr << "unknown exception caught." << std::endl;
        std::terminate();
      }
    }
    assert(!_mainLoopThread.joinable());
  }

  /********************************************************************************************************************/

  Logger::StreamProxy Logger::getStream(Logger::Severity severity, std::string context) {
    return {this, severity, std::move(context)};
  }

  /********************************************************************************************************************/

  Logger::StreamProxy::StreamProxy(Logger* logger, Severity severity, std::string context)
  : std::ostream(severity >= logger->_minSeverity ? &_buf : nullptr), _severity(severity), _context(std::move(context)),
    _logger(*logger) {}

  /********************************************************************************************************************/

  Logger::StreamProxy::~StreamProxy() {
    // Send out the log message
    _logger.log(_severity, std::move(_context), _buf.str());
  }

  /********************************************************************************************************************/

  void Logger::log(Logger::Severity severity, std::string context, std::string message) noexcept {
    try {
      _messageQueue.push({severity, std::move(context), std::move(message)});
    }
    catch(std::system_error& e) {
      std::cerr << "std::system_error caught: " << e.what() << std::endl;
      std::terminate();
    }
  }

  /********************************************************************************************************************/

  std::string Logger::severityToString(Severity severity) {
    switch(severity) {
      case Logger::Severity::trace:
        return "t";
      case Logger::Severity::debug:
        return "d";
      case Logger::Severity::info:
        return "i";
      case Logger::Severity::warning:
        return "W";
      case Logger::Severity::error:
        return "E";
    }
    return "?";
  }

  /********************************************************************************************************************/

  void Logger::mainLoop() {
    while(true) {
      LogMessage message;
      _messageQueue.pop_wait(message);

      auto* stream = &std::cout;
      if(message.severity >= Severity::warning) {
        stream = &std::cerr;
      }

      // Prefix with time stamp, source and severity. This format matches the DOOCS format (except that we added the
      // severity).
      auto t = std::time(nullptr);
      auto tm = *std::localtime(&t);
      std::stringstream prefix;
      prefix.setf(std::ios::left, std::ios::adjustfield);
      prefix << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S") << " " << std::setw(20) << message.context.substr(0, 20) << " ["
             << severityToString(message.severity) << "] ";

      // print text line by line with prefix to stream
      std::stringstream ss(message.text);
      std::string line;
      while(std::getline(ss, line, '\n')) {
        *stream << prefix.str() << line << "\n";
      }
      *stream << std::flush;
    }
  }

  /********************************************************************************************************************/

} // namespace ChimeraTK
