#ifndef VTCPD_COMMANDSRECEIVER_H
#define VTCPD_COMMANDSRECEIVER_H

#include "../../BaseFIFOInterface.h"

#include "../../../logger/Logger.h"

#include "../commands/ErrorUserCommand.h"
#include "../commands/trust_line_channels/InitChannelCommand.h"
#include "../commands/trust_line_channels/SetChannelContractorAddressesCommand.h"
#include "../commands/trust_line_channels/SetChannelContractorCryptoKeyCommand.h"
#include "../commands/trust_line_channels/RegenerateChannelCryptoKeyCommand.h"
#include "../commands/trust_lines/InitTrustLineCommand.h"
#include "../commands/trust_lines/SetOutgoingTrustLineCommand.h"
#include "../commands/trust_lines/CloseIncomingTrustLineCommand.h"
#include "../commands/trust_lines/ShareKeysCommand.h"
#include "../commands/trust_lines/RemoveTrustLineCommand.h"
#include "../commands/trust_lines/ResetTrustLineCommand.h"
#include "../commands/payments/CreditUsageCommand.h"
#include "../commands/max_flow_calculation/InitiateMaxFlowCalculationCommand.h"
#include "../commands/max_flow_calculation/InitiateMaxFlowCalculationFullyCommand.h"
#include "../commands/total_balances/TotalBalancesCommand.h"
#include "../commands/history/HistoryPaymentsCommand.h"
#include "../commands/history/HistoryPaymentsAllEquivalentsCommand.h"
#include "../commands/history/HistoryAdditionalPaymentsCommand.h"
#include "../commands/history/HistoryTrustLinesCommand.h"
#include "../commands/history/HistoryWithContractorCommand.h"
#include "../commands/trust_lines_list/GetFirstLevelContractorsCommand.h"
#include "../commands/trust_lines_list/GetTrustLinesCommand.h"
#include "../commands/trust_lines_list/GetTrustLineByAddressCommand.h"
#include "../commands/trust_lines_list/GetTrustLineByIDCommand.h"
#include "../commands/trust_lines_list/EquivalentListCommand.h"
#include "../commands/trust_line_channels/ContractorListCommand.h"
#include "../commands/trust_line_channels/GetChannelInfoCommand.h"
#include "../commands/trust_line_channels/GetChannelInfoByAddressesCommand.h"
#include "../commands/subsystems_controller/SubsystemsInfluenceCommand.h"
#include "../commands/subsystems_controller/TrustLinesInfluenceCommand.h"
#include "../commands/transactions/PaymentTransactionByCommandUUIDCommand.h"
#include "../commands/general/RemoveOutdatedCryptoDataCommand.h"

#include "../../../common/exceptions/IOError.h"
#include "../../../common/exceptions/ValueError.h"
#include "../../../common/exceptions/MemoryError.h"
#include "../../../common/exceptions/RuntimeError.h"

#include <boost/signals2.hpp>
#include <boost/bind/bind.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/lexical_cast.hpp>

#include <string>
#include <memory>

#ifndef TESTS__TRUSTLINES
#include "../../../common/Types.h"
#include "../../../common/time/TimeUtils.h"
#endif


using namespace std;
using namespace boost::uuids;
namespace signals = boost::signals2;
using namespace boost::placeholders;

/**
 * User commands are transmitted via text protocol.
 * CommandsParser is used for parsing received user input
 * and deserializing them into commands instances.
 *
 *
 * Note: shares almost the same logic as "MessagesParser"
 * (see IncomingMessagesHandler.h for details).
 *
 * todo: hsc: review this class
 */
class CommandsParser
{
    friend class CommandsParserTests;

public:
    CommandsParser(
        Logger &log);

    void appendReadData(
        as::streambuf *buffer,
        const size_t receivedBytesCount);

    pair<bool, BaseUserCommand::Shared> processReceivedCommands();

private:
    inline pair<bool, BaseUserCommand::Shared> tryDeserializeCommand();

    inline pair<bool, BaseUserCommand::Shared> tryParseCommand(
        const CommandUUID &uuid,
        const string &identifier,
        const string &buffer);

    inline pair<bool, BaseUserCommand::Shared> commandError(
        const CommandUUID &uuid,
        const string &identifier,
        const string &str);

    void cutBufferUpToNextCommand();

public:
    static const size_t kUUIDHexRepresentationSize = 36;
    static const size_t kMinCommandSize = kUUIDHexRepresentationSize + 2;
    static const size_t kAverageCommandIdentifierLength = 15;

protected:

    template <typename CommandType, typename... Args>
    inline pair<bool, BaseUserCommand::Shared> newCommand(
        const CommandUUID &uuid,
        const string &buffer) const
    {
        return make_pair(
                   true,
                   static_pointer_cast<BaseUserCommand>(
                       make_shared<CommandType>(
                           uuid,
                           buffer)));
    }

    static string logHeader()
    noexcept;

    LoggerStream error() const
    noexcept;

private:
    string mBuffer;
    Logger &mLog;
};


// todo: refactor, use signals

/**
 * User commands are transmitted via named pipe (FIFO on Linux).
 * This class is used to asynchronously receive them, parse,
 * and transfer for the further execution.
 */
class CommandsInterface: public BaseFIFOInterface
{
public:
    signals::signal<void(bool, BaseUserCommand::Shared)> commandReceivedSignal;

public:
    explicit CommandsInterface(
        as::io_context &ioCtx,
        Logger &logger);

    ~CommandsInterface();

    void beginAcceptCommands();

protected:
    void asyncReceiveNextCommand();

    void handleReceivedInfo(
        const boost::system::error_code &error,
        const size_t bytesTransferred);

    void handleTimeout(
        const boost::system::error_code &error);

    virtual const char* FIFOName() const;

    static string logHeader()
    noexcept;

    LoggerStream error() const
    noexcept;

public:
    static const constexpr char* kFIFOName = "commands.fifo";
    static const constexpr unsigned int kPermissionsMask = 0755;

protected:
    static const constexpr size_t kCommandBufferSize = 1024;

    as::io_context &mIOCtx;
    Logger &mLog;

    as::streambuf mCommandBuffer;

    unique_ptr<as::posix::stream_descriptor> mFIFOStreamDescriptor;
    unique_ptr<as::steady_timer> mReadTimeoutTimer;
    unique_ptr<CommandsParser> mCommandsParser;
};

#endif //VTCPD_COMMANDSRECEIVER_H
