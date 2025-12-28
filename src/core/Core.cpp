#include "Core.h"
#include "io/storage/sqlite/StorageHandlerSQLite.h"
#include "network/rpc/responses/GetBlockNumberRpcResponse.h"

#include "ortools/linear_solver/linear_solver.h"
#include "ortools/base/version.h"

namespace {
// RPC method identifier for block number cache integration.
constexpr RpcMethod kBlockNumberRpcMethod = RpcMethod::GetBlockNumber;
} // namespace

Core::Core(
    char* pArgv)
noexcept:

    mCommandDescriptionPtr(pArgv)
{}

Core::~Core()
{}

int Core::run()
{
    if (isatty(STDOUT_FILENO)) {
        std::cout << endl << endl;
        std::cout << "       _____              _________  " << endl;
        std::cout << "___   ___  /____________________  /  " << endl;
        std::cout << "__ | / /  __/  ___/__  __ \\  __  /  " << endl;
        std::cout << "__ |/ // /_ / /__ __  /_/ / /_/ /    " << endl;
        std::cout << "_____/ \\__/ \\___/ _  .___/\\__,_/  " << endl;
        std::cout << "                  /_/                " << endl;
        std::cout << endl << endl;
        std::cout << "\033[1m  vTCP Network node daemon / v0.1.0 \033[0m" << endl;
        std::cout << "\033[1m  https://github.com/vTCP-Foundation\033[0m" << endl;
        std::cout << endl << endl;
    }

    auto initCode = initSubsystems();
    if (initCode != 0) {
        error() << "Core can't be initialised. Process will now be stopped.";
        return initCode;
    }

    writePIDFile();
    updateProcessName();

    try {
        mCommunicator->beginAcceptMessages();
        mCommandsInterface->beginAcceptCommands();
        info() << "Processing started.";
        mIOCtx.run();
        return 0;

    } catch (Exception &e) {
        mLog->logException("Core", e);
        return -1;
    }
}

int Core::initSubsystems()
{
    int initCode;

    initCode = initSettings();

    if (initCode != 0)
        return initCode;

    json conf;
    try {
        // Optimised conf read.
        // (several params at once)
        // For the details - see settings realisation.
        conf = mSettings->loadParsedJSON();

    } catch (std::exception &e) {
        cerr << utc_now() <<" : ERROR\tSETTINGS\t" <<  e.what() << "." << endl;
        return -1;
    }

    vector<SerializedEquivalent>equivalentsOnWhichIAmIsGateway;
    try {
        equivalentsOnWhichIAmIsGateway = mSettings->iAmGateway(&conf);
    } catch (RuntimeError &) {
        // Logger was not initialized yet
        cerr << utc_now() <<" : ERROR\tCORE\tCan't read if node is gateway from the settings" << endl;
        return -1;
    }

    initCode = initLogger();
    if (initCode != 0) {
        return initCode;
    }

    try {
        // Initialize cache after logger is ready for cache diagnostics.
        mBlockNumberCache = make_unique<BlockNumberCache>(*mLog);
    } catch (const std::exception &e) {
        error() << "Failed to initialize block number cache: " << e.what();
        return -1;
    }

    initCode = initORTools();
    if (initCode != 0) {
        return initCode;
    }

    initCode = initStorageHandler(conf);
    if (initCode != 0) {
        return initCode;
    }

    initCode = initResourcesManager();
    if (initCode != 0) {
        return initCode;
    }

    initCode = initCommandsInterface();
    if (initCode != 0) {
        return initCode;
    }

    initCode = initResultsInterface();
    if (initCode != 0) {
        return initCode;
    }

    initCode = initEventsInterfaceManager(conf);
    if (initCode != 0) {
        return initCode;
    }

    initCode = initContractorsManager(conf);
    if (initCode != 0) {
        return initCode;
    }

    initCode = initProvidingHandler(conf);
    if (initCode != 0) {
        return initCode;
    }

    initCode = initTailManager();
    if (initCode != 0) {
        return initCode;
    }

    initCode = initCommunicator(conf);
    if (initCode != 0) {
        return initCode;
    }

    initCode = initKeysStore();
    if (initCode != 0) {
        return initCode;
    }

    initCode = initObservingHandler(conf);
    if (initCode != 0) {
        return initCode;
    }

    initCode = initSubsystemsController();
    if (initCode != 0) {
        return initCode;
    }

    initCode = initTrustLinesInfluenceController();
    if (initCode != 0) {
        return initCode;
    }

    initCode = initEquivalentsSubsystemsRouter(
                   equivalentsOnWhichIAmIsGateway);
    if (initCode != 0) {
        return initCode;
    }

    initCode = initFeaturesManager(conf);
    if (initCode != 0) {
        return initCode;
    }

    initCode = initExchangeRatesManager();
    if (initCode != 0) {
        return initCode;
    }

    initCode = initExchangePathsManager();
    if (initCode != 0) {
        return initCode;
    }

    initCode = initCommissionsManager(conf);
    if (initCode != 0) {
        return initCode;
    }

    initCode = initTransactionsManager(conf);
    if (initCode != 0) {
        return initCode;
    }

    initCode = initObserverRpcCommunicator(conf);
    if (initCode != 0) {
        return initCode;
    }

    initCode = initCompletedPaymentsMonitoringDelayedTask();
    if (initCode != 0) {
        return initCode;
    }

    initCode = initTopologyEventDelayedTask();
    if (initCode != 0) {
        return initCode;
    }

    connectSignalsToSlots();
    return 0;
}

int Core::initSettings()
{
    try {
        mSettings = make_unique<Settings>();
        return 0;

    } catch (const std::exception &e) {
        // Logger was not initialized yet
        cerr << "\033[31m" << utc_now() <<" : ERR  \t[CORE]\t" <<  e.what() << "." << "\033[0m" << endl;
        return -1;
    }
}

int Core::initLogger()
{
    try {
        mLog = make_unique<Logger>();
        return 0;

    } catch (...) {
        cerr << utc_now() <<" : FATAL\t[CORE]\tLogger cannot be initialized." << endl;
        return -1;
    }
}

int Core::initORTools()
{
    try {
        // Check OR-Tools availability by creating a simple solver
        std::unique_ptr<operations_research::MPSolver> testSolver(
            operations_research::MPSolver::CreateSolver("GLOP"));
        
        if (!testSolver) {
            error() << "OR-Tools GLOP solver is not available";
            return -1;
        }

        // Get OR-Tools version and check minimum requirements
        int major = operations_research::OrToolsMajorVersion();
        int minor = operations_research::OrToolsMinorVersion();
        
        info() << "OR-Tools version: " << major << "." << minor;
        
        // Require OR-Tools version >= 9.0 as per PRD specification
        if (major < 9) {
            error() << "OR-Tools version " << major << "." << minor 
                   << " is insufficient. Minimum version 9.0 required.";
            return -1;
        }
        
        info() << "OR-Tools integration verified successfully";
        return 0;

    } catch (const std::exception &e) {
        error() << "OR-Tools initialization failed: " << e.what();
        return -1;
    } catch (...) {
        error() << "OR-Tools initialization failed with unknown error";
        return -1;
    }
}

int Core::initTailManager()
{
    try {
        mTailManager = make_unique<TailManager>(
                           mIOCtx,
                           *mLog);

        info() << "Tail manager is successfully initialised";
        return 0;

    } catch (const std::exception &e) {
        mLog->logException("Core", e);
        return -1;
    }
}

int Core::initCommunicator(
    const json &conf)
{
    try {
        auto interface = mSettings->interface(&conf);
        DatabaseConfiguration dbConfig = mSettings->databaseConfiguration(&conf);
        
        mCommunicator = make_unique<Communicator>(
                            mIOCtx,
                            interface.first,
                            interface.second,
                            mContractorsManager.get(),
                            mTailManager.get(),
                            mProvidingHandler.get(),
                            dbConfig,
                            *mLog);

        string providerType = (dbConfig.providerType == DatabaseProviderType::SQLite) ? "SQLite" : "PostgreSQL";
        info() << "Network communicator (" << providerType << ") is successfully initialised";
        return 0;

    } catch (const std::exception &e) {
        error() << "Failed to initialize communicator: " << e.what();
        mLog->logException("Core", e);
        return -1;
    }
}

int Core::initObservingHandler(
    const json &conf)
{
    try {
        mObservingHandler = make_unique<ObservingHandler>(
                                mSettings->observers(&conf),
                                mIOCtx,
                                mStorageHandler.get(),
                                mKeysStore.get(),
                                mResourcesManager.get(),
                                *mLog);

        info() << "Observing handler is successfully initialised";
        return 0;

    } catch (const std::exception &e) {
        mLog->logException("Core", e);
        return -1;
    }
}

int Core::initObserverRpcCommunicator(
    const json &conf)
{
    try {
        IPv4WithPortAddress::Shared observerAddress = nullptr;
        const auto observers = mSettings->observers(&conf);
        for (const auto &observer : observers) {
            if (observer.first != "ipv4") {
                continue;
            }
            try {
                observerAddress = make_shared<IPv4WithPortAddress>(
                    observer.second);
                break;
            } catch (const exception &e) {
                warning() << "initObserverRpcCommunicator: invalid observer address "
                          << observer.second << ". Details: " << e.what();
            }
        }

        if (observerAddress == nullptr) {
            warning() << "initObserverRpcCommunicator: no valid IPv4 observer address configured";
        }

        mObserverRpcCommunicator = make_unique<ObserverRpcCommunicator>(
            mIOCtx,
            observerAddress,
            *mLog);
        info() << "Observer RPC communicator is successfully initialised";
        return 0;

    } catch (const std::exception &e) {
        mLog->logException("Core", e);
        return -1;
    }
}

int Core::initResultsInterface()
{
    try {
        mResultsInterface = make_unique<ResultsInterface>(
                                *mLog);
        info() << "Results interface is successfully initialised";
        return 0;

    } catch (const std::exception &e) {
        mLog->logException("Core", e);
        return -1;
    }
}

int Core::initEventsInterfaceManager(
    const json &conf)
{
    auto eventsConf = mSettings->events(&conf);
    vector<pair<string, bool>> filesToBlock;
    vector<pair<string, SerializedEventType>> filesToEvents;
    try {
        if (eventsConf == nullptr) {
            info() << "There are no events in config";
        } else {
            for (const auto &eventConf : eventsConf) {
                filesToBlock.emplace_back(
                    eventConf.at("file").get<string>(),
                    eventConf.at("blocked").get<bool>());
                for (const auto &eventTypesConf : eventConf.at("events")) {
                    filesToEvents.emplace_back(
                        eventConf.at("file").get<string>(),
                        (SerializedEventType)eventTypesConf);
                }
            }
        }

        mEventsInterfaceManager = make_unique<EventsInterfaceManager>(
                                      filesToEvents,
                                      filesToBlock,
                                      *mLog);
        info() << "Events interface manager is successfully initialised";
        return 0;
    } catch (const std::exception &e) {
        mLog->logException("Core", e);
        return -1;
    }
}

int Core::initEquivalentsSubsystemsRouter(
    vector<SerializedEquivalent> equivalentIAmGateway)
{
    try {
        mEquivalentsSubsystemsRouter = make_unique<EquivalentsSubsystemsRouter>(
                                           mStorageHandler.get(),
                                           mKeysStore.get(),
                                           mContractorsManager.get(),
                                           mEventsInterfaceManager.get(),
                                           mIOCtx,
                                           equivalentIAmGateway,
                                           *mLog);
        info() << "EquivalentsSubsystemsRouter is successfully initialised";
        return 0;
    } catch (const std::exception &e) {
        mLog->logException("Core", e);
        return -1;
    }

}

int Core::initResourcesManager()
{
    try {
        mResourcesManager = make_unique<ResourcesManager>();
        info() << "Resources manager is successfully initialized";
        return 0;
    } catch (const std::exception &e) {
        mLog->logException("Core", e);
        return -1;
    }
}

int Core::initTransactionsManager(
    const json &conf)
{
    auto cyclesClearingConf = mSettings->cyclesClearing(&conf);
    try {
        CyclesRunningParameters cyclesRunningParameters;
        if (cyclesClearingConf != nullptr) {
            cyclesRunningParameters = CyclesRunningParameters(
                                          cyclesClearingConf.at("three_nodes_enabled").get<bool>(),
                                          cyclesClearingConf.at("four_nodes_enabled").get<bool>(),
                                          cyclesClearingConf.at("five_nodes_enabled").get<bool>(),
                                          cyclesClearingConf.at("five_nodes_interval_sec").get<uint32_t>(),
                                          cyclesClearingConf.at("six_nodes_enabled").get<bool>(),
                                          cyclesClearingConf.at("six_nodes_interval_sec").get<uint32_t>(),
                                          cyclesClearingConf.at("routing_tables_interval_days").get<uint16_t>());
        }

        mTransactionsManager = make_unique<TransactionsManager>(
                                   mIOCtx,
                                   mContractorsManager.get(),
                                   mEquivalentsSubsystemsRouter.get(),
                                   mResourcesManager.get(),
                                   mResultsInterface.get(),
                                   mStorageHandler.get(),
                                   mKeysStore.get(),
                                   mFeaturesManager.get(),
                                   mEventsInterfaceManager.get(),
                                   mTailManager.get(),
                                   cyclesRunningParameters,
                                   *mLog,
                                   mSubsystemsController.get(),
                                   mTrustLinesInfluenceController.get(),
                                   mExchangeRatesManager.get(),
                                   mExchangePathsManager.get(),
                                   mCommissionsManager.get(),
                                   mObservingHandler.get(),
                                   mSettings->hopsCount(&conf));
        info() << "Transactions handler is successfully initialised";
        return 0;

    } catch (const std::exception &e) {
        mLog->logException("Core", e);
        return -1;
    }
}

int Core::initCommandsInterface()
{
    try {
        mCommandsInterface = make_unique<CommandsInterface>(
                                 mIOCtx,
                                 *mLog);
        info() << "Commands interface is successfully initialised";
        return 0;

    } catch (const std::exception &e) {
        mLog->logException("Core", e);
        return -1;
    }
}

int Core::initStorageHandler(
    const json &conf)
{
    try {
        // Get database configuration from Settings
        DatabaseConfiguration dbConfig = mSettings->databaseConfiguration(&conf);
        
        // Create storage handler through factory
        mStorageHandler = StorageProviderFactory::createStorageHandler(
            dbConfig,
            "storagedb",
            *mLog);
        
        string providerType = (dbConfig.providerType == DatabaseProviderType::SQLite) ? "SQLite" : "PostgreSQL";
        info() << "Storage handler (" << providerType << ") is successfully initialised";
        return 0;
        
    } catch (const std::exception &e) {
        error() << "Failed to initialize storage handler: " << e.what();
        cerr << "\033[31m" << utc_now() << " : ERR  \t[CORE]\t" 
             << "Database configuration error: " << e.what() << "\033[0m" << endl;
        cerr << "\033[31m" << "Check your database_config in conf.json. Expected formats:" << "\033[0m" << endl;
        cerr << "\033[31m" << "  SQLite: \"database_config\": \"sqlite3:///path/to/directory\"" << "\033[0m" << endl;
        cerr << "\033[31m" << "  PostgreSQL: \"database_config\": \"postgresql://user:pass@host:port/database\"" << "\033[0m" << endl;
        cerr << "\033[31m" << "  PostgreSQL (without DB): \"database_config\": \"postgresql://user:pass@host:port/\"" << "\033[0m" << endl;
        mLog->logException("Core", e);
        return -1;
    }
}

int Core::initContractorsManager(
    const json &conf)
{
    try {
        mContractorsManager = make_unique<ContractorsManager>(
                                  mSettings->addresses(&conf),
                                  mStorageHandler.get(),
                                  *mLog);
        info() << "Contractors manager is successfully initialised";
        return 0;
    } catch (const std::exception &e) {
        mLog->logException("Core", e);
        return -1;
    }
}

int Core::initProvidingHandler(
    const json &conf)
{
    auto providersConf = mSettings->providers(&conf);
    try {
        vector<Provider::Shared> providers;
        uint32_t addressUpdatingPeriod = 0;
        uint32_t cachedAddressTTLSeconds = 0;
        if (providersConf == nullptr) {
            info() << "There are no providers in config";
        } else {
            addressUpdatingPeriod = providersConf.at("address_updating_period_sec").get<uint32_t>();
            cachedAddressTTLSeconds = providersConf.at("cached_addresses_ttl_sec").get<uint32_t>();
            for (const auto &providerConf : providersConf.at("providers")) {
                vector<pair<string, string>> providerAddressesStr;
                for (const auto &providerAddressConf : providerConf.at("addresses")) {
                    providerAddressesStr.emplace_back(
                        providerAddressConf.at("type").get<string>(),
                        providerAddressConf.at("address").get<string>());
                }
                providers.push_back(
                    make_shared<Provider>(
                        providerConf.at("name").get<string>(),
                        providerConf.at("key").get<string>(),
                        providerConf.at("participant_id").get<ProviderParticipantID>(),
                        providerAddressesStr));
            }
        }

        mProvidingHandler = make_unique<ProvidingHandler>(
                                providers,
                                addressUpdatingPeriod,
                                cachedAddressTTLSeconds,
                                mIOCtx,
                                mContractorsManager->selfContractor(),
                                *mLog);
        info() << "Providing handler is successfully initialised";
        return 0;
    } catch (const std::exception &e) {
        mLog->logException("Core", e);
        return -1;
    }
}

int Core::initSubsystemsController()
{
    try {
        mSubsystemsController = make_unique<SubsystemsController>(
                                    *mLog);
        info() << "Subsystems controller is successfully initialized";
        return 0;
    } catch (const std::exception &e) {
        mLog->logException("Core", e);
        return -1;
    }
}

int Core::initTrustLinesInfluenceController()
{
    try {
        mTrustLinesInfluenceController = make_unique<TrustLinesInfluenceController>(
                                             *mLog);
        info() << "Trust Lines Influence controller is successfully initialized";
        return 0;
    } catch (const std::exception &e) {
        mLog->logException("Core", e);
        return -1;
    }
}

int Core::initKeysStore()
{
    try {
        mKeysStore = make_unique<Keystore>(
                         *mLog);
        // Initialize and ensure at least one payment key exists
        auto ioTrx = mStorageHandler->beginTransaction();
        mKeysStore->init(ioTrx);
        info() << "Keys store is successfully initialized";
        return 0;
    } catch (const std::exception &e) {
        mLog->logException("Core", e);
        return -1;
    }
}

int Core::initTopologyEventDelayedTask()
{
    try {
        mTopologyEventDelayedTask = make_unique<TopologyEventDelayedTask>(
                                        mIOCtx,
                                        mEquivalentsSubsystemsRouter.get(),
                                        *mLog);
        info() << "Topology Event Delayed Task is successfully initialized";
        return 0;
    } catch (const std::exception &e) {
        mLog->logException("Core", e);
        return -1;
    }
}

// Initializes the delayed task that emits monitoring signals for completed payments.
int Core::initCompletedPaymentsMonitoringDelayedTask()
{
    try {
        mCompletedPaymentsMonitoringDelayedTask = make_unique<CompletedPaymentsMonitoringDelayedTask>(
            mIOCtx,
            *mLog);
        info() << "Completed Payments Monitoring Delayed Task is successfully initialized";
        return 0;
    } catch (const std::exception &e) {
        mLog->logException("Core", e);
        return -1;
    }
}

int Core::initFeaturesManager(
    const json &conf)
{
    try {
        mFeaturesManager = make_unique<FeaturesManager>(
                               mIOCtx,
                               mSettings->equivalentsRegistryAddress(&conf),
                               mContractorsManager->selfContractor()->outputString(),
                               mStorageHandler.get(),
                               *mLog);
        info() << "Features Manager is successfully initialized";
        return 0;
    } catch (const std::exception &e) {
        mLog->logException("Core", e);
        return -1;
    }
}

int Core::initExchangeRatesManager()
{
    try {
        mExchangeRatesManager = make_unique<ExchangeRatesManager>(
                                    mIOCtx,
                                    *mLog);
        info() << "Exchange Rates Manager is successfully initialized";
        return 0;
    } catch (const std::exception &e) {
        mLog->logException("Core", e);
        return -1;
    }
}

int Core::initExchangePathsManager()
{
    try {
        mExchangePathsManager = make_unique<ExchangePathsManager>(
                                    mIOCtx,
                                    mEquivalentsSubsystemsRouter.get(),
                                    mExchangeRatesManager.get(),
                                    mContractorsManager.get(),
                                    *mLog);
        info() << "Exchange Paths Manager is successfully initialized";
        return 0;
    } catch (const std::exception &e) {
        mLog->logException("Core", e);
        return -1;
    }
}

int Core::initCommissionsManager(
    const json &conf)
{
    try {
        auto commissionsData = mSettings->commissions(&conf);
        mCommissionsManager = make_unique<CommissionsManager>(
                                  *mLog,
                                  commissionsData);
        info() << "Commissions Manager is successfully initialized";
        return 0;
    } catch (const std::exception &e) {
        mLog->logException("Core", e);
        return -1;
    }
}

void Core::connectCommandsInterfaceSignals ()
{
    mCommandsInterface->commandReceivedSignal.connect(
        boost::bind(
            &Core::onCommandReceivedSlot,
            this,
            _1,
            _2));
}

void Core::connectCommunicatorSignals()
{
    //communicator's signal to transactions manager slot
    mCommunicator->signalMessageReceived.connect(
        boost::bind(
            &Core::onMessageReceivedSlot,
            this,
            _1));

    mCommunicator->signalClearTopologyCache.connect(
        boost::bind(
            &Core::onClearTopologyCacheSlot,
            this,
            _1,
            _2));

    //transactions manager's to communicator slot
    mTransactionsManager->transactionOutgoingMessageReadySignal.connect(
        boost::bind(
            &Core::onMessageSendSlot,
            this,
            _1,
            _2));

    mTransactionsManager->transactionOutgoingMessageToAddressReadySignal.connect(
        boost::bind(
            &Core::onMessageSendToAddressSlot,
            this,
            _1,
            _2));

    mTransactionsManager->transactionOutgoingMessageWithCachingReadySignal.connect(
        boost::bind(
            &Core::onMessageSendWithCachingSlot,
            this,
            _1,
            _2,
            _3,
            _4));

    mTransactionsManager->processConfirmationMessageSignal.connect(
        boost::bind(
            &Core::onProcessConfirmationMessageSlot,
            this,
            _1));

    mTransactionsManager->processPongMessageSignal.connect(
        boost::bind(
            &Core::onProcessPongMessageSlot,
            this,
            _1));

    for (const auto &contractorID : mEquivalentsSubsystemsRouter->contractorsShouldBePinged()) {
        mCommunicator->enqueueContractorWithPostponedSending(
            contractorID);
    }
    mEquivalentsSubsystemsRouter->clearContractorsShouldBePinged();

    mFeaturesManager->sendAddressesSignal.connect(
        boost::bind(
            &Core::onSendOwnAddressesSlot,
            this));
}

void Core::connectObservingSignals()
{
    mTransactionsManager->observingClaimSignal.connect(
        boost::bind(
            &Core::onClaimSendToObserverSlot,
            this,
            _1));

    mTransactionsManager->observingTransactionCommittedSignal.connect(
        boost::bind(
            &Core::onAddTransactionToObservingCheckingSlot,
            this,
            _1,
            _2));

    mObservingHandler->mParticipantsVotesSignal.connect(
        boost::bind(
            &Core::onObservingParticipantsVotesSlot,
            this,
            _1,
            _2,
            _3));

    mObservingHandler->mRejectTransactionSignal.connect(
        boost::bind(
            &Core::onObservingTransactionRejectSlot,
            this,
            _1,
            _2));

    mObservingHandler->mUncertainTransactionSignal.connect(
        boost::bind(
            &Core::onObservingTransactionUncertainSlot,
            this,
            _1,
            _2));

    mObservingHandler->mCancelTransactionSignal.connect(
        boost::bind(
            &Core::onObservingTransactionCancelingSlot,
            this,
            _1,
            _2));

    mObservingHandler->mAllowPaymentTransactionsSignal.connect(
        boost::bind(
            &Core::onObservingAllowPaymentTransactionsSlot,
            this,
            _1));
}

// Wires RPC signals between TransactionsManager and ObserverRpcCommunicator.
void Core::connectObserverRpcSignals()
{
    if (mObserverRpcCommunicator == nullptr || mTransactionsManager == nullptr) {
        warning() << "connectObserverRpcSignals: RPC components are not initialised";
        return;
    }

    mTransactionsManager->rpcRequestSignal.connect(
        boost::bind(
            &Core::onRpcRequestSlot,
            this,
            _1));

    mObserverRpcCommunicator->rpcResponseSignal.connect(
        boost::bind(
            &Core::onRpcResponseSlot,
            this,
            _1));
}

void Core::connectResourcesManagerSignals()
{
    mResourcesManager->requestPathsResourcesSignal.connect(
        boost::bind(
            &Core::onPathsResourceRequestedSlot,
            this,
            _1,
            _2,
            _3));

    mResourcesManager->requestExchangePathsResourceSignal.connect(
        boost::bind(
            &Core::onExchangePathsResourceRequestedSlot,
            this,
            _1,
            _2,
            _3,
            _4));

    mResourcesManager->requestObservingBlockNumberSignal.connect(
        boost::bind(
            &Core::onObservingBlockNumberRequestSlot,
            this,
            _1));

    mResourcesManager->attachResourceSignal.connect(
        boost::bind(
            &Core::onResourceCollectedSlot,
            this,
            _1));

    info() << "RequestExchangePathsResourceSignal connected";
}
void Core::connectSignalsToSlots()
{
    connectCommandsInterfaceSignals();
    connectCommunicatorSignals();
    connectResourcesManagerSignals();
    connectObservingSignals();
    connectObserverRpcSignals();

    // Hook delayed monitoring signal into the transactions manager.
    mTransactionsManager->subscribeForCompletedPaymentsMonitoringSignal(
        mCompletedPaymentsMonitoringDelayedTask->monitoringSignal);
}

void Core::onCommandReceivedSlot (
    bool success,
    BaseUserCommand::Shared command)
{
    if (command and success and command->identifier() == SubsystemsInfluenceCommand::identifier()) {
        // In case if network toggle command was received -
        // there is no reason to transfer it's processing to the transactions manager:
        // this command only enables or disables network for the node,
        // and this may be simply done by filtering several slots in the core.
        auto subsystemsInfluenceCommand = static_pointer_cast<SubsystemsInfluenceCommand>(command);
        mSubsystemsController->setFlags(
            subsystemsInfluenceCommand->flags());
#ifdef TESTS
        mSubsystemsController->setForbiddenNodeAddress(
            subsystemsInfluenceCommand->forbiddenNodeAddress());
        mSubsystemsController->setForbiddenAmount(
            subsystemsInfluenceCommand->forbiddenAmount());
        // set node as gateway
        if ((subsystemsInfluenceCommand->flags() & 0x80000000000) != 0) {
            info() << "from now I am gateway";
            mEquivalentsSubsystemsRouter->setMeAsGateway();
        }
#endif
        info() << "SubsystemsInfluenceCommand processed";
        return;
    }

#ifdef TESTS
    if (command and success and command->identifier() == TrustLinesInfluenceCommand::identifier()) {
        auto trustLinesInfluenceCommand = static_pointer_cast<TrustLinesInfluenceCommand>(command);
        mTrustLinesInfluenceController->setFlags(trustLinesInfluenceCommand->flags());
        mTrustLinesInfluenceController->setFirstParameter(
            trustLinesInfluenceCommand->firstParameter());
        mTrustLinesInfluenceController->setSecondParameter(
            trustLinesInfluenceCommand->secondParameter());
        mTrustLinesInfluenceController->setThirdParameter(
            trustLinesInfluenceCommand->thirdParameter());
        return;
    }
#endif

    try {
        mTransactionsManager->processCommand(success, command);

    } catch(exception &e) {
        mLog->logException("Core", e);
    }
}

void Core::onMessageReceivedSlot(
    Message::Shared message)
{
#ifdef TESTS
    if (not mSubsystemsController->isNetworkOn()) {
        // Ignore incoming message in case if network was disabled.
        debug() << "Ignore process incoming message";
        return;
    }
#endif

#ifdef TESTS
    if (mTrustLinesInfluenceController->checkReceivedMessage(message->typeID())) {
        // Ignore incoming message of forbidden type
        debug() << "Ignore processing incoming message of forbidden type " << message->typeID();
        return;
    }
#endif

    try {
        mTransactionsManager->processMessage(message);

    } catch(exception &e) {
        mLog->logException("Core", e);
    }
}

void Core::onClearTopologyCacheSlot(
    const SerializedEquivalent equivalent,
    BaseAddress::Shared nodeAddress)
{
    try {
        mEquivalentsSubsystemsRouter->topologyCacheManager(equivalent)->removeCache(nodeAddress);
    } catch (NotFoundError &e) {
        error() << "There are no topologyCacheManager for onClearTopologyCacheSlot "
                   "with equivalent " << equivalent << " Details are: " << e.what();
    }
}

void Core::onMessageSendSlot(
    Message::Shared message,
    const ContractorID contractorID)
{
#ifdef TESTS
    if (not mSubsystemsController->isNetworkOn()) {
        // Ignore outgoing message in case if network was disabled.
        debug() << "Ignore send message";
        return;
    }
#endif

    try {
        mCommunicator->sendMessage(
            message,
            contractorID);

    } catch (exception &e) {
        mLog->logException("Core", e);
    }
}

void Core::onMessageSendToAddressSlot(
    Message::Shared message,
    BaseAddress::Shared address)
{
#ifdef TESTS
    if (not mSubsystemsController->isNetworkOn()) {
        // Ignore outgoing message in case if network was disabled.
        debug() << "Ignore send message";
        return;
    }
#endif

    try {
        mCommunicator->sendMessage(
            message,
            address);

    } catch (exception &e) {
        mLog->logException("Core", e);
    }
}

void Core::onMessageSendWithCachingSlot(
    TransactionMessage::Shared message,
    ContractorID contractorID,
    Message::MessageType incomingMessageTypeFilter,
    uint32_t cacheTimeLiving)
{
#ifdef TESTS
    if (not mSubsystemsController->isNetworkOn()) {
        // Ignore outgoing message in case if network was disabled.
        debug() << "Ignore send message";
        return;
    }
#endif

    try {
        mCommunicator->sendMessageWithCacheSaving(
            message,
            contractorID,
            incomingMessageTypeFilter,
            cacheTimeLiving);

    } catch (exception &e) {
        mLog->logException("Core", e);
    }
}

void Core::onClaimSendToObserverSlot(
    ObservingClaimAppendRequestMessage::Shared message)
{
    mObservingHandler->sendClaimRequestToObservers(message);
}

void Core::onAddTransactionToObservingCheckingSlot(
    const TransactionUUID& transactionUUID,
    BlockNumber maxBlockNumberForClaiming)
{
    mObservingHandler->addTransactionForChecking(
        transactionUUID,
        maxBlockNumberForClaiming);
}

void Core::onObservingParticipantsVotesSlot(
    const TransactionUUID &transactionUUID,
    BlockNumber maximalClaimingBlockNumber,
    map<PaymentNodeID, sphincs::Signature::Shared> participantsSignatures)
{
    mTransactionsManager->launchPaymentTransactionAfterGettingObservingSignatures(
        transactionUUID,
        maximalClaimingBlockNumber,
        participantsSignatures);
}

void Core::onObservingTransactionRejectSlot(
    const TransactionUUID &transactionUUID,
    BlockNumber maximalClaimingBlockNumber)
{
    mTransactionsManager->launchPaymentTransactionForObservingRejecting(
        transactionUUID,
        maximalClaimingBlockNumber);
}

void Core::onObservingTransactionUncertainSlot(
    const TransactionUUID &transactionUUID,
    BlockNumber maximalClaimingBlockNumber)
{
    mTransactionsManager->launchPaymentTransactionForObservingUncertainStage(
        transactionUUID,
        maximalClaimingBlockNumber);
}

void Core::onObservingTransactionCancelingSlot(
    const TransactionUUID &transactionUUID,
    BlockNumber maximalClaimingBlockNumber)
{
    mTransactionsManager->launchCancelingPaymentTransaction(
        transactionUUID,
        maximalClaimingBlockNumber);
}

void Core::onObservingAllowPaymentTransactionsSlot(
    bool allowPaymentTransactions)
{
    mTransactionsManager->allowPaymentTransactionsDueToObserving(
        allowPaymentTransactions);
}

void Core::onPathsResourceRequestedSlot(
    const TransactionUUID &transactionUUID,
    BaseAddress::Shared destinationNodeAddress,
    const SerializedEquivalent equivalent)
{
    try {
        mTransactionsManager->launchFindPathByMaxFlowTransaction(
            transactionUUID,
            destinationNodeAddress,
            equivalent);

    } catch (exception &e) {
        mLog->logException("Core", e);
    }
}

void Core::onExchangePathsResourceRequestedSlot(
    const TransactionUUID &transactionUUID,
    BaseAddress::Shared contractorAddress,
    const vector<SerializedEquivalent> &exchangeEquivalents,
    const SerializedEquivalent receiverEquivalent)
{
    info() << "Exchange paths requested for transaction " << transactionUUID
           << ", contractor " << contractorAddress->fullAddress()
           << ", " << exchangeEquivalents.size() << " exchange equivalent(s)"
           << ", receiver equivalent " << receiverEquivalent;

    try {
        mTransactionsManager->launchFindPathsByMaxFlowExchangeTransaction(
            transactionUUID,
            contractorAddress,
            exchangeEquivalents,
            receiverEquivalent);

    } catch (exception &e) {
        mLog->logException("Core", e);
    }

    debug() << "Scheduled FindPathsByMaxFlowExchangeTransaction for exchange paths";
}

void Core::onObservingBlockNumberRequestSlot(
    const TransactionUUID &transactionUUID)
{
    mObservingHandler->requestActualBlockNumber(
        transactionUUID);
}

void Core::onResourceCollectedSlot(
    BaseResource::Shared resource)
{
    try {
        mTransactionsManager->attachResourceToTransaction(
            resource);

    } catch (exception &e) {
        mLog->logException("Core", e);
    }
}

void Core::onProcessConfirmationMessageSlot(
    ConfirmationMessage::Shared confirmationMessage)
{
    mCommunicator->processConfirmationMessage(
        confirmationMessage);
}

void Core::onProcessPongMessageSlot(
    ContractorID contractorID)
{
    mCommunicator->processPongMessage(
        contractorID);
}

// Forwards RPC requests emitted by transactions to the observer communicator,
// serving cached block number responses when available.
void Core::onRpcRequestSlot(
    RpcRequest::Shared request)
{
    if (request == nullptr) {
        warning() << "onRpcRequestSlot: empty RPC request";
        return;
    }

#ifdef TESTS
    if (not mSubsystemsController->isNetworkOn()) {
        debug() << "Ignore RPC request send while network is disabled";
        return;
    }
#endif

    // Serve cached block number response before using the observer.
    if (request->method() == kBlockNumberRpcMethod) {
        if (mBlockNumberCache != nullptr && !mBlockNumberCache->needsRefresh()) {
            info() << "GetBlockNumber cache hit for transaction "
                   << request->transactionUUID();
            auto response = make_shared<GetBlockNumberRpcResponse>(
                request->transactionUUID(),
                RpcResponseStatus::Success,
                mBlockNumberCache->getCachedBlockNumber());
            try {
                mTransactionsManager->onRpcResponseReceived(response);
            } catch (const exception &e) {
                mLog->logException("Core", e);
            }
            return;
        }
        debug() << "GetBlockNumber cache miss, forwarding to observer";
    }

    if (mObserverRpcCommunicator == nullptr) {
        warning() << "onRpcRequestSlot: RPC communicator is not initialised";
        return;
    }

    try {
        mObserverRpcCommunicator->sendRequest(request);

    } catch (const exception &e) {
        mLog->logException("Core", e);
    }
}

// Delivers observer RPC responses back to the transactions manager,
// updating the block number cache on successful responses.
void Core::onRpcResponseSlot(
    RpcResponse::Shared response)
{
    if (mTransactionsManager == nullptr) {
        warning() << "onRpcResponseSlot: transactions manager is not initialised";
        return;
    }

    try {
        // Refresh block number cache from successful observer response.
        if (response->method() == kBlockNumberRpcMethod && response->isSuccess()) {
            auto blockResponse = dynamic_pointer_cast<GetBlockNumberRpcResponse>(response);
            if (blockResponse != nullptr && mBlockNumberCache != nullptr) {
                mBlockNumberCache->update(blockResponse->blockNumber());
                info() << "BlockNumber cache updated with block "
                       << blockResponse->blockNumber();
            }
        }
        mTransactionsManager->onRpcResponseReceived(
            response);

    } catch (const exception &e) {
        mLog->logException("Core", e);
    }
}

void Core::onSendOwnAddressesSlot()
{
    mTransactionsManager->launchUpdateChannelAddressesInitiatorTransaction();
}

void Core::writePIDFile()
{
    try {
        std::ofstream pidFile("process.pid");
        pidFile << ::getpid() << std::endl;
        pidFile.close();

    } catch (std::exception &e) {
        error() << "Can't write/update pid file. Error message is: " << e.what();
    }
}

void Core::updateProcessName()
{
    const string kProcessName(string("vtcpd:") + mContractorsManager->selfContractor()->mainAddress()->fullAddress());
    prctl(PR_SET_NAME, kProcessName.c_str());
    // Avoid corrupting memory by overwriting argv[0] with a longer string.
    // On Linux, argv/environ are stored in a contiguous buffer of fixed size
    // determined by the original command line. Writing past that size leads
    // to memory corruption and hard-to-diagnose crashes (e.g., invalid vtable).
    //
    // Safely limit the overwrite to the original argv[0] length and zero-fill
    // the tail so that /proc/self/cmdline shows the new title without overflow.
    if (mCommandDescriptionPtr != nullptr) {
        size_t maxlen = ::strlen(mCommandDescriptionPtr);
        if (maxlen > 0) {
            // Clear existing bytes to avoid leaking previous content
            ::memset(mCommandDescriptionPtr, 0, maxlen);
            // Copy at most maxlen-1 to keep NUL terminator within bounds
            ::strncpy(mCommandDescriptionPtr, kProcessName.c_str(), maxlen - 1);
        }
    }
}

string Core::logHeader()
noexcept
{
    return "CORE";
}

LoggerStream Core::error() const
noexcept
{
    return mLog->error(logHeader());
}

LoggerStream Core::warning() const
noexcept
{
    return mLog->warning(logHeader());
}

LoggerStream Core::info() const
noexcept
{
    return mLog->info(logHeader());
}

LoggerStream Core::debug() const
noexcept
{
    return mLog->debug(logHeader());
}
