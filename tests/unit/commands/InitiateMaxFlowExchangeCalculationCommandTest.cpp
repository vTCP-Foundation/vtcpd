#include <gtest/gtest.h>
#include <boost/uuid/uuid_generators.hpp>
#include "../../../src/core/interface/commands_interface/commands/max_flow_calculation/InitiateMaxFlowExchangeCalculationCommand.h"
#include "../../../src/core/common/exceptions/ValueError.h"

class InitiateMaxFlowExchangeCalculationCommandTest : public ::testing::Test {
protected:
    void SetUp() override {
        uuid = boost::uuids::random_generator()();
    }

    string buildCommandString(const vector<SerializedEquivalent>& exchangeEquivalents) {
        // Format: 1\t12\t127.0.0.1:2001\t1\t2\t3\t4\t5
        // Note: addressType "12" = IPv4_IncludingPort
        stringstream ss;
        ss << "1" << kTokensSeparator;  // contractorsCount
        ss << "12" << kTokensSeparator << "127.0.0.1:2001" << kTokensSeparator;  // addressType + address
        ss << "1";  // equivalent (receiver equivalent)
        
        for (const auto& exchEquiv : exchangeEquivalents) {
            ss << kTokensSeparator << exchEquiv;
        }
        
        ss << '\n';
        return ss.str();
    }

    CommandUUID uuid;
    const char kTokensSeparator = '\t';
};

TEST_F(InitiateMaxFlowExchangeCalculationCommandTest, testValidExchangeEquivalents_5Elements_Success) {
    vector<SerializedEquivalent> exchangeEquivalents = {2, 3, 4, 5, 6};
    string commandStr = buildCommandString(exchangeEquivalents);
    
    EXPECT_NO_THROW({
        InitiateMaxFlowExchangeCalculationCommand command(uuid, commandStr);
        EXPECT_EQ(command.exchangeEquivalents().size(), 5);
    });
}

TEST_F(InitiateMaxFlowExchangeCalculationCommandTest, testValidExchangeEquivalents_1Element_Success) {
    vector<SerializedEquivalent> exchangeEquivalents = {2};
    string commandStr = buildCommandString(exchangeEquivalents);
    
    EXPECT_NO_THROW({
        InitiateMaxFlowExchangeCalculationCommand command(uuid, commandStr);
        EXPECT_EQ(command.exchangeEquivalents().size(), 1);
    });
}

TEST_F(InitiateMaxFlowExchangeCalculationCommandTest, testValidExchangeEquivalents_EmptyVector_Success) {
    vector<SerializedEquivalent> exchangeEquivalents = {};
    string commandStr = buildCommandString(exchangeEquivalents);
    
    EXPECT_NO_THROW({
        InitiateMaxFlowExchangeCalculationCommand command(uuid, commandStr);
        EXPECT_EQ(command.exchangeEquivalents().size(), 0);
    });
}

TEST_F(InitiateMaxFlowExchangeCalculationCommandTest, testInvalidExchangeEquivalents_6Elements_ThrowsValueError) {
    vector<SerializedEquivalent> exchangeEquivalents = {2, 3, 4, 5, 6, 7};
    string commandStr = buildCommandString(exchangeEquivalents);
    
    EXPECT_THROW({
        InitiateMaxFlowExchangeCalculationCommand command(uuid, commandStr);
    }, ValueError);
}

TEST_F(InitiateMaxFlowExchangeCalculationCommandTest, testAccessors_ValidData_ReturnsCorrectValues) {
    vector<SerializedEquivalent> exchangeEquivalents = {2, 3, 4};
    string commandStr = buildCommandString(exchangeEquivalents);
    
    InitiateMaxFlowExchangeCalculationCommand command(uuid, commandStr);
    
    EXPECT_EQ(command.equivalent(), 1);
    EXPECT_EQ(command.contractorAddresses().size(), 1);
    EXPECT_EQ(command.exchangeEquivalents(), exchangeEquivalents);
    EXPECT_EQ(command.identifier(), "GET:contractors/transactions/max/exchange");
}