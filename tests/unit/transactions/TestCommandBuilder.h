#ifndef VTCPD_TESTS_TESTCOMMANDBUILDER_H
#define VTCPD_TESTS_TESTCOMMANDBUILDER_H

#include "../../../src/core/interface/commands_interface/commands/payments/CreditUsageExchangeCommand.h"
#include "../../../src/core/common/Types.h"
#include <memory>
#include <sstream>

/**
 * Builder class to construct valid CreditUsageExchangeCommand for testing.
 * This bypasses the complex command string parsing by using reflection/memory manipulation.
 */
class TestCommandBuilder {
public:
    /**
     * Builds a fake but functional CreditUsageExchangeCommand by using 
     * a minimalist valid command string and then directly manipulating 
     * the object's memory (hacky but necessary for testing).
     * 
     * NOTE: This is a test-only workaround for complex command parsing.
     */
    static std::shared_ptr<CreditUsageExchangeCommand> buildExchangeCommand(
        const std::vector<BaseAddress::Shared> &addresses,
        const TrustLineAmount &amount,
        SerializedEquivalent receiverEquivalent,
        const std::vector<SerializedEquivalent> &exchangeEquivalents)
    {
        // Create a "shell" command object that will throw but we'll catch it
        CommandUUID uuid;
        
        // Instead of parsing, we'll use a different approach:
        // Create a stub derived class that can set members
        class TestCreditUsageExchangeCommand : public CreditUsageExchangeCommand {
        public:
            TestCreditUsageExchangeCommand(
                const CommandUUID &uuid,
                const std::vector<BaseAddress::Shared> &addrs,
                const TrustLineAmount &amt,
                SerializedEquivalent recvEquiv,
                const std::vector<SerializedEquivalent> &exchEquivs)
                : CreditUsageExchangeCommand(uuid, "")  // Will throw, but we handle it
            {
                // This won't work because constructor will throw before we get here
            }
            
            // We need to set members after construction, but they're private
            // This approach won't work either
        };
        
        // Since we can't easily manipulate private members and the parser is complex,
        // we need to find the ACTUAL correct format.
        // Let me try building the command string more carefully based on Spirit parser rules
        
        // The parser expects (from CreditUsageExchangeCommand.cpp lines 76-120):
        // 1. First char check (not separator)
        // 2. Address count parsing: *(int_[...] - tab) > tab
        // 3. Address lexeme parsing
        // 4. Tail parsing: amount, equivalent, exchange equivalents
        
        // For addressLexeme (IPv4WithPort), format is:
        // "1" [addressType] ... tab ... 192.168.1.1:8080 [repeat(3)[int '.']] [int ':' int] tab
        
        std::stringstream cmd;
        
        // Address count
        cmd << addresses.size();  // "1"
        cmd << "\t";
        
        // For each address (assuming IPv4WithPort)
        for (const auto& addr : addresses) {
            // Address type (IPv4_IncludingPort = 12, NOT 1!)
            cmd << "12";
            cmd << "\t";
            
            // Address itself (e.g., "192.168.1.1:8080")
            cmd << addr->fullAddress();
            cmd << "\t";
        }
        
        // Amount
        cmd << amount;
        cmd << "\t";
        
        // Receiver equivalent  
        cmd << receiverEquivalent;
        
        // Exchange equivalents
        for (const auto& equiv : exchangeEquivalents) {
            cmd << "\t" << equiv;
        }
        
        // End
        cmd << "\n";
        
        std::string cmdStr = cmd.str();
        
        // Try to parse - this will likely still fail, but let's see
        try {
            return std::make_shared<CreditUsageExchangeCommand>(uuid, cmdStr);
        } catch (const std::exception &e) {
            // If it fails, throw with helpful debug info
            std::stringstream error;
            error << "Failed to build test command: " << e.what() << "\n";
            error << "Command string was: [" << cmdStr << "]\n";
            error << "Hex dump: ";
            for (char c : cmdStr) {
                error << std::hex << (int)(unsigned char)c << " ";
            }
            throw std::runtime_error(error.str());
        }
    }
};

#endif // VTCPD_TESTS_TESTCOMMANDBUILDER_H

