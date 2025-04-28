#include "painless.hpp"

#include "sharing/LocalStrategies/HordeSatSharing.hpp"
#include "sharing/LocalStrategies/SimpleSharing.hpp"

#include "sharing/GlobalStrategies/AllGatherSharing.hpp"
#include "sharing/GlobalStrategies/GenericGlobalSharing.hpp"
#include "sharing/GlobalStrategies/MallobSharing.hpp"

#include "containers/ClauseDatabaseBufferPerEntity.hpp"
#include "containers/ClauseDatabaseMallob.hpp"
#include "containers/ClauseDatabasePerSize.hpp"

#include "SharingStrategyFactory.hpp"

int SharingStrategyFactory::selectedLocal = 0;
int SharingStrategyFactory::selectedGlobal = 0;

void
SharingStrategyFactory::instantiateLocalStrategies(int strategyNumber,
                                                                                                   std::vector<std::shared_ptr<SharingStrategy>>& localStrategies,
                                                                                                   std::vector<std::shared_ptr<SolverCdclInterface>>& cdclSolvers)
{
        std::vector<std::shared_ptr<SharingEntity>> allEntities;
        allEntities.insert(allEntities.end(), cdclSolvers.begin(), cdclSolvers.end());

        if (!allEntities.size()) {
                LOGWARN("No SharingEntity, SharingStrategy %d will not be instantiated", strategyNumber);
                return;
        }

        if (strategyNumber == 0) {
                std::random_device dev;
                std::mt19937 rng(dev());
                std::uniform_int_distribution<std::mt19937::result_type> distShr(1, LOCAL_SHARING_STRATEGY_COUNT);
                strategyNumber = distShr(rng);
        }

        uint maxClauseSize = Painless::__globalParameters__.maxClauseSize;

        // TODO: decide weither or not to make a unique_ptr inside Sharer
        switch (strategyNumber) {
                case 1:
                        LOG0("LSTRAT>> HordeSatSharing(1Grp, per entity buffer)");
                        localStrategies.emplace_back(
                                new HordeSatSharing(std::make_shared<ClauseDatabaseBufferPerEntity>(maxClauseSize),
                                                                        Painless::__globalParameters__.sharedLiteralsPerProducer,
                                                                        Painless::__globalParameters__.hordeInitialLbdLimit,
                                                                        Painless::__globalParameters__.hordeInitRound,
                                                                        allEntities,
                                                                        allEntities));
                        localStrategies.back()->connectConstructorProducers();
                        break;
                case 2:
                        LOG0("LSTRAT>> HordeSatSharing(1Grp, common database)");
                        localStrategies.emplace_back(new HordeSatSharing(std::make_shared<Painless::ClauseDatabasePerSize>(maxClauseSize),
                                                                                                                         Painless::__globalParameters__.sharedLiteralsPerProducer,
                                                                                                                         Painless::__globalParameters__.hordeInitialLbdLimit,
                                                                                                                         Painless::__globalParameters__.hordeInitRound,
                                                                                                                         allEntities,
                                                                                                                         allEntities));
                        localStrategies.back()->connectConstructorProducers();
                        break;
                        break;
                case 3:
                        if (cdclSolvers.size() <= 2) {
                                LOGERROR("Please select another sharing strategy other than 2 or 3 if you want to have %d solvers.",
                                                 cdclSolvers.size());
                                LOGERROR("If you used -dist option, the strategies may not work");
                                exit(PWARN_LSTRAT_CPU_COUNT);
                        }

                        LOG0("LSTRAT>> HordeSatSharing(2Grp of producers)");

                        localStrategies.emplace_back(
                                new HordeSatSharing(std::make_shared<Painless::ClauseDatabasePerSize>(maxClauseSize),
                                                                        Painless::__globalParameters__.sharedLiteralsPerProducer,
                                                                        Painless::__globalParameters__.hordeInitialLbdLimit,
                                                                        Painless::__globalParameters__.hordeInitRound,
                                                                        { allEntities.begin(), allEntities.begin() + allEntities.size() / 2 },
                                                                        { allEntities.begin(), allEntities.end() }));
                        localStrategies.back()->connectConstructorProducers();
                        localStrategies.emplace_back(
                                new HordeSatSharing(std::make_shared<Painless::ClauseDatabasePerSize>(maxClauseSize),
                                                                        Painless::__globalParameters__.sharedLiteralsPerProducer,
                                                                        Painless::__globalParameters__.hordeInitialLbdLimit,
                                                                        Painless::__globalParameters__.hordeInitRound,
                                                                        { allEntities.begin() + allEntities.size() / 2, allEntities.end() },
                                                                        { allEntities.begin(), allEntities.end() }));
                        localStrategies.back()->connectConstructorProducers();
                        break;
                case 4:
                        LOG0("LSTRAT>> SimpleSharing");

                        localStrategies.emplace_back(new SimpleSharing(std::make_shared<Painless::ClauseDatabasePerSize>(maxClauseSize),
                                                                                                                   Painless::__globalParameters__.simpleShareLimit,
                                                                                                                   Painless::__globalParameters__.sharedLiteralsPerProducer,
                                                                                                                   allEntities,
                                                                                                                   allEntities));
                        localStrategies.back()->connectConstructorProducers();
                        break;
                case 5:
                        LOG0("LSTRAT>> HordeSatSharing(1Grp, MallobDB)");
                        localStrategies.emplace_back(
                                new HordeSatSharing(std::make_shared<ClauseDatabaseMallob>(maxClauseSize, 2, 100'000, 1),
                                                                        Painless::__globalParameters__.sharedLiteralsPerProducer,
                                                                        Painless::__globalParameters__.hordeInitialLbdLimit,
                                                                        Painless::__globalParameters__.hordeInitRound,
                                                                        allEntities,
                                                                        allEntities));
                        localStrategies.back()->connectConstructorProducers();
                        break;
                default:
                        LOGERROR("The sharing strategy number chosen isn't correct, use a value between 1 and %d",
                                         LOCAL_SHARING_STRATEGY_COUNT);
                        break;
        }

        SharingStrategyFactory::selectedLocal = strategyNumber;
}

void
SharingStrategyFactory::instantiateGlobalStrategies(
        int strategyNumber,
        std::vector<std::shared_ptr<GlobalSharingStrategy>>& globalStrategies)
{
        int right_neighbor = (Painless::mpi_rank - 1 + Painless::mpi_world_size) % Painless::mpi_world_size;
        int left_neighbor = (Painless::mpi_rank + 1) % Painless::mpi_world_size;
        std::vector<int> subscriptions;
        std::vector<int> subscribers;

        std::shared_ptr<Painless::ClauseDatabase> simpleDB;

        switch (strategyNumber) {
                case 0:
                        LOGWARN("For now, gshr-strat at 0 is AllGatherSharing, a future default one is in dev");
                case 1:
                        LOG0("GSTRAT>> AllGatherSharing");
                        simpleDB = std::make_shared<Painless::ClauseDatabasePerSize>(Painless::__globalParameters__.maxClauseSize);
                        globalStrategies.emplace_back(new AllGatherSharing(simpleDB, Painless::__globalParameters__.globalSharedLiterals));
                        break;
                case 2:
                        LOG0("GSTRAT>> MallobSharing");
                        simpleDB = std::make_shared<ClauseDatabaseMallob>(
                                Painless::__globalParameters__.maxClauseSize, 2, Painless::__globalParameters__.globalSharedLiterals * 10, 1);
                        globalStrategies.emplace_back(new MallobSharing(simpleDB,
                                                                                                                        Painless::__globalParameters__.globalSharedLiterals,
                                                                                                                        Painless::__globalParameters__.mallobMaxBufferSize,
                                                                                                                        Painless::__globalParameters__.mallobLBDLimit,
                                                                                                                        Painless::__globalParameters__.mallobSizeLimit,
                                                                                                                        Painless::__globalParameters__.mallobSharingsPerSecond,
                                                                                                                        Painless::__globalParameters__.mallobMaxCompensation,
                                                                                                                        Painless::__globalParameters__.mallobResharePeriod));
                        break;
                case 3:
                        LOG0("GSTRAT>> GenericGlobalSharing As RingSharing");
                        simpleDB = std::make_shared<Painless::ClauseDatabasePerSize>(Painless::__globalParameters__.maxClauseSize);
                        subscriptions.push_back(right_neighbor);
                        subscriptions.push_back(left_neighbor);
                        subscribers.push_back(right_neighbor);
                        subscribers.push_back(left_neighbor);

                        globalStrategies.emplace_back(new GenericGlobalSharing(
                                simpleDB, subscriptions, subscribers, Painless::__globalParameters__.globalSharedLiterals));
                        break;
                default:
                        LOGERROR("Option gshr-lit=%d is not defined",strategyNumber);
                        std::abort();
                        break;
        }

        /*Since bootstraping, loop is not optimzed*/
        for (unsigned int i = 0; i < globalStrategies.size() && dist; i++) {
                if (!globalStrategies.at(i)->initMpiVariables()) {
                        LOGERROR("The global sharing strategy %d wasn't able to initalize its MPI variables", i);
                        TESTRUNMPI(MPI_Finalize());
                        dist = false;
                        globalStrategies.erase(globalStrategies.begin() + i);
                }
        }

        SharingStrategyFactory::selectedGlobal = strategyNumber;
}

void
SharingStrategyFactory::launchSharers(std::vector<std::shared_ptr<SharingStrategy>>& sharingStrategies,
                                                                          std::vector<std::unique_ptr<Sharer>>& sharers)
{
        if (Painless::__globalParameters__.oneSharer) {
                sharers.emplace_back(new Sharer(0, sharingStrategies));
        } else {
                for (unsigned int i = 0; i < sharingStrategies.size(); i++) {
                        sharers.emplace_back(new Sharer(i, sharingStrategies[i]));
                }
        }
}

void
SharingStrategyFactory::addEntitiesToLocal(std::vector<std::shared_ptr<SharingStrategy>>& localStrategies,
                                                                                   std::vector<std::shared_ptr<SolverCdclInterface>>& newSolvers)
{
        switch (SharingStrategyFactory::selectedLocal) {
                case 1:
                case 4:
                case 5:
                        LOG0("UPDATE>> 1Grp");
                        for (auto newSolver : newSolvers) {
                                localStrategies[0]->addClient(newSolver);
                                localStrategies[0]->addProducer(newSolver);
                                localStrategies[0]->connectProducer(newSolver);
                        }
                        break;
                case 2:
                case 3:
                        LOG0("UPDATE>> 2Grp of producers");

                        for (unsigned int i = 0; i < newSolvers.size() / 2; i++) {
                                localStrategies[0]->addClient(newSolvers[i]);
                                localStrategies[0]->addProducer(newSolvers[i]);
                                localStrategies[0]->connectProducer(newSolvers[i]);

                                localStrategies[1]->addClient(newSolvers[i]);
                        }

                        for (unsigned int i = newSolvers.size() / 2; i < newSolvers.size(); i++) {
                                localStrategies[0]->addClient(newSolvers[i]);

                                localStrategies[1]->addProducer(newSolvers[i]);
                                localStrategies[1]->connectProducer(newSolvers[i]);
                                localStrategies[1]->addClient(newSolvers[i]);
                        }
                        break;
                default:
                        LOGWARN("The sharing strategy number chosen isn't correct, use a value between 1 and %d",
                                        LOCAL_SHARING_STRATEGY_COUNT);
                        break;
        }
}
