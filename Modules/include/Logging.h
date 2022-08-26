/*!
 * \author Klaus Zenker (HZDR)
 * \date 03.04.2018
 * \page loggingdoc Logging module and Logger
 * \section loggingintro Introduction to the logging mechanism
 * The logging provided here requires to add the LoggingModule to your
 * Application.
 * The module introduces the following input variables:
 * - targetStream: Allows to choose where messages send to the logging module
 * end up:
 *   - 0: cout/cerr+logfile
 *   - 1: logfile
 *   - 2: cout/cerr
 *   - 3: controlsystem only
 *   - 4: nowhere
 * - logFile: Give the logfile name. If the file is not empty logging messages
 *   will be appended. If you choose targetStream 0 or 1 and don't set a logFile
 *   the Logging module simply skips the file writing.
 * - logLevel: Choose a certain logging level of the Module. Messages send to
 *   the Logging module also include a logging level.
 *   The Logging module compares both levels and decides if a message is
 *   dropped (e.g. message level is DEBUG and Module level is ERROR) or broadcasted.
 * - maxTailLength: The number of messages published by the Logging module (see
 *   logTail), i.e. to the control system. If set to 0 the number of messages defaults to 20.
 *   This length has no influence on the targetStreams, that receive all
 *   messages (depending on the logLevel). The logLevel also applies to messages
 *   that are published by the Logging module via the logTail
 *
 * Available logging levels are:
 *  - DEBUG
 *  - INFO
 *  - WARNING
 *  - ERROR
 *  - SILENT
 *
 *  The only variable that is published by the Logging module is the logTail. It
 *  contains the list of latest messages.
 *  Messages are separated by a newline character. The number of messages
 *  published in the logTail is set via the
 *  input variable tailLength. Other than that, messages are written to
 *  cout/cerr and/or a log file as explained above.
 *
 *  A Logger class is used to send messages to the LoggingModule.
 *  The foreseen way of using the Logger is to add a Logger to a module that
 *  should send log messages.
 *  The Logger adds two variables that will be available in the control system:
 *  - alias: It can be set at runtime and will be used as prefix in messages of that particular
 *  Logger. If it is set empty the name of the owning module is used.
 *  - message: This is the message send to the LoggingModule. It includes the severity encoded
 *  as number in the first character of the string followed by the message.
 *
 *
 *  The LoggingModule will take care of finding all Loggers.
 *  Therefore, the LoggingModule needs to be constructed last - after all ApplicationModules
 *  using a Logger are constructed. It will look in its owner for all accessors with tags that match the
 *  tags given to the LoggingModule. Like that one can use tags in the Logger to assign senders for the LoggingModule
 *  and in addition the LoggingModule can be placed inside a ModuleGroup to consider only accessors in that
 *  group when setting up the connections.
 *
 *  The following example shows how to integrate the Logging module and the
 Logging into an application (myApp) and one module sending
 *  messages (TestModule).
 *  \code
 *  sruct TestModule: public ChimeraTK::ApplicationModule{
 *  // use default tag of the Logger -> "Logging"
 *  boost::shared_ptr<Logger> logger{new Logger(this)};
 *  ...
 *  };
 *  struct myApp : public ChimeraTK::Application{
 *
 *
 *  TestModule { this, "test", "" };
 *
 *  // needs to be added after all modules that use a Logger!
 *  // Default tag of the LoggingModule is used -> "Logging"
 *  LoggingModule log { this, "LoggingModule", "LoggingModule test" };
 *  ...
 *  };
 *
 *  void TestModule::mainLoop{
 *    logger.sendMessage("Test",LogLevel::DEBUG);
 *    ...
 *  }
 *  \endcode
 *
 *  A message always looks like this:
 *  LogLevel::LoggingModuleName/SendingModuleName TimeString -> message\n
 *  In the example given above the message could look like:
 *  \code
 *  DEBUG::LogggingModule/test 2018-Apr-12 14:03:07.947949 -> Test
 *  \endcode
 *  \remark Instead of adding a Logger to every module that should feed the
 Logging module, one could also consider using only one Logger object.
 *  This is not thread safe and would not work for multiple modules trying to
 send messages via the Logger object to the Logging module at the same time.
 *
 *  \attention If sendMessage is called multiple times in a sequence some messages might get lost.
 *  This is because of the internal buffer used by ChimeraTK, that has a size of 3. If the LoggingModule
 *  is not done processing a message, the internal buffer is full and a new message arrives it is dropped.

 */

#ifndef MODULES_LOGGING_H_
#define MODULES_LOGGING_H_

#include "ApplicationCore.h"

#include <ChimeraTK/RegisterPath.h>

#include <fstream>
#include <map>
#include <queue>

namespace ctk = ChimeraTK;

namespace logging {

  /** Define available logging levels. INTERNAL is used to
   * indicate an already published message  */
  enum LogLevel { DEBUG, INFO, WARNING, ERROR, SILENT, INTERNAL };

  /** Define stream operator to use the LogLevel in streams, e.g. std::cout */
  std::ostream& operator<<(std::ostream& os, const logging::LogLevel& level);

  /** Construct a sting containing the current time. */
  std::string getTime();

  /**
   * \brief Class used to send messages in a convenient way to the LoggingModule.
   *
   * In principle this class only adds two output variables and provides a simple
   * method to fill these variables. They are supposed to be connected to the
   * LoggingModule via LoggingModule::addSource. If sendMessage is used before
   * chimeraTK process variables are initialized an internal buffer is used to
   * store those messages. Once the process variables are initialized the messages
   * from the buffer are send. \attention This only happens once a message is send
   * after chimeraTK process variables are initialized! In other words if no
   * message is send in the mainLoop messages from defineConnections will never be
   * shown.
   */
  class Logger : ctk::VariableGroup {
   private:
    std::queue<std::string> msg_buffer;

   public:
    /**
     * \brief Default constructor: Allows late initialization of modules (e.g.
     * when creating arrays of modules).
     *
     *  This constructor also has to be here to mitigate a bug in gcc. It is
     * needed to allow constructor inheritance of modules owning other modules.
     * This constructor will not actually be called then. See this bug report:
     * https://gcc.gnu.org/bugzilla/show_bug.cgi?id=67054 */
    Logger() {}

    /**
     * \brief Constructor to be used.
     *
     * \param module The owning module that is using the Logger. It will appear as
     * sender in the LoggingModule, which is receiving messages from the Logger.
     * \param name Name used to initialise the VariableGroup.
     * \param description Description used to initialise the VariableGroup.
     * \param tag A tag that is used to identify the Logger by the LoggingModule.
     */
    Logger(ctk::Module* module, const std::string& name = "Logging",
        const std::string& description = "VariableGroup added by the Logger", const std::string& tag = "Logging");
    /** Message to be send to the logging module */
    ctk::ScalarOutput<std::string> message;

    /** Alias that is used instead of the module name when printing messages. */
    ctk::ScalarPollInput<std::string> alias;

    /**
     * \brief Send a message, which means to update the message and messageLevel
     * member variables.
     *
     */
    void sendMessage(const std::string& msg, const logging::LogLevel& level);

    void prepare() override;
  };

  /**
   * \brief Module used to handle logging messages.
   *
   * A ChimeraTK module is producing messages, that are send to the LoggingModule
   * via the \c message variable. The message is then put into the logfile ring
   * buffer and published in the \c LogFileTail. In addition the message can be
   * put to an ostream. Available streams are:
   * - file stream
   * - cout/cerr
   *
   * You can control which stream is used by setting the targetStream varibale:
   * 0: cout/cerr and logfile
   * 1: logfile
   * 2: cout/cerr
   * 3: none
   *
   * The logfile is given by the client using the logFile variable.
   *
   * \attention The LoggingModule should be added last to the Application. Doing so
   * all logging messages added by Logger object will be collected and connected to
   * the LoggingModule.
   */
  class LoggingModule : public ctk::ApplicationModule {
   private:
    ctk::VariableNetworkNode getAccessorPair(const ctk::RegisterPath& namePrefix);

    /** Map of VariableGroups required to build the hierarchies. The key it the
     * full path name. */
    std::map<std::string, ctk::VariableGroup> groupMap;

    /** Create VariableGroups from the full path of the module */
    ctk::RegisterPath prepareHierarchy(const ctk::RegisterPath& namePrefix);

    struct MessageSource {
      struct Data : ctk::HierarchyModifyingGroup {
        using ctk::HierarchyModifyingGroup::HierarchyModifyingGroup;
        ctk::ScalarPushInput<std::string> msg{this, "message", "", "", {"_logging_internal"}};
        ctk::ScalarPollInput<std::string> alias{this, "alias", "", "", {"_logging_internal"}};
      };

      Data data;
      std::string sendingModule;
      MessageSource(const ChimeraTK::RegisterPath& path, Module* module)
      : data(module, (std::string)path, ""), sendingModule(((std::string)path).substr(1)) {}

      bool operator==(const MessageSource& other) { return other.sendingModule == sendingModule; }
      bool operator==(const std::string& name) { return name == sendingModule; }
    };
    /** List of senders. */
    std::vector<MessageSource> sources;

    /** Map key is the transfer id of the ScalarPushInput variable pointed to */
    std::map<ChimeraTK::TransferElementID, MessageSource*> id_list;

    /** Number of messages stored in the tail */
    size_t messageCounter{0};

    /** Broadcast message to cout/cerr and log file
     * \param msg The mesage
     * \param isError If true cerr is used. Else cout is used.
     */
    void broadcastMessage(std::string msg, const bool& isError = false);

   public:
    LoggingModule(ctk::EntityOwner* owner, const std::string& name, const std::string& description,
        ctk::HierarchyModifier hierarchyModifier = ctk::HierarchyModifier::none,
        const std::unordered_set<std::string>& tags = {"Logging"});

    LoggingModule() {}

    ctk::ScalarPollInput<uint> targetStream{this, "targetStream", "",
        "Set the tagret stream: 0 (cout/cerr+logfile), 1 (logfile), 2 "
        "(cout/cerr), 3 (Controls System only), 4 (nowhere)",
        {"CS", getName()}};

    ctk::ScalarPollInput<std::string> logFile{this, "logFile", "",
        "Name of the external logfile. If empty messages are pushed to "
        "cout/cerr",
        {"CS", getName()}};

    ctk::ScalarPollInput<uint> tailLength{this, "maxTailLength", "",
        "Maximum number of messages to be shown in the logging stream tail. 0 is treated as 20.", {"CS", getName()}};

    ctk::ScalarPollInput<uint> logLevel{
        this, "logLevel", "", "Current log level used for messages.", {"CS", getName()}};

    ctk::ScalarOutput<std::string> logTail{
        this, "logTail", "", "Tail of the logging stream.", {"CS", "PROCESS", getName()}};

    std::unique_ptr<std::ofstream> file; ///< Log file where to write log messages

    size_t getNumberOfModules() { return sources.size(); }

    /**
     * Application core main loop.
     */
    void mainLoop() override;

    void terminate() override;

    void findTagAndAppendToModule(ctk::VirtualModule& virtualParent, const std::string& tag,
        bool eliminateAllHierarchies, bool eliminateFirstHierarchy, bool negate,
        ctk::VirtualModule& root) const override;
  };

} // namespace logging

#endif /* MODULES_LOGGING_H_ */
