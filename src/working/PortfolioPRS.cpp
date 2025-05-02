#include "working/PortfolioPRS.hpp"
#include "painless.hpp"
#include "utils/Logger.hpp"
#include "utils/NumericConstants.hpp"
#include "utils/Parameters.hpp"
#include "utils/System.hpp"
#include "working/SequentialWorker.hpp"

#include "solvers/CDCL/Kissat.hpp"
#include "solvers/CDCL/KissatINCSolver.hpp"
#include "solvers/CDCL/KissatMABSolver.hpp"
#include "solvers/CDCL/MapleCOMSPSSolver.hpp"

#include "containers/ClauseDatabaseBufferPerEntity.hpp"
#include "containers/ClauseDatabasePerSize.hpp"

#include "sharing/GlobalStrategies/GenericGlobalSharing.hpp"
#include "sharing/LocalStrategies/HordeSatSharing.hpp"

#include "sharing/SharingStrategyFactory.hpp"
#include "solvers/SolverFactory.hpp"

#include "preprocessors/PRS-Preprocessors/preprocess.hpp"

#include <thread>

namespace Painless {

PortfolioPRS::PortfolioPRS() {}

PortfolioPRS::~PortfolioPRS()
{
        // Wait for global sharer ending to have the Painless::mpi_winner value
        for (int i = 0; i < sharers.size(); i++) {
                sharers[i]->join();
        }
        Painless::mpiutils::sendModelToRoot();
        if (0 == Painless::mpi_rank && finalResult == SatResult::SAT) {
                this->restoreModelDist(finalModel);
        }

#ifndef NDEBUG
        for (size_t i = 0; i < slaves.size(); i++) {
                delete slaves[i];
        }
#endif
}

void
PortfolioPRS::solve(const std::vector<int>& cube)
{
        LOG0(">> PortfolioPRS");
        std::vector<Painless::ClauseUtils::simpleClause> initClauses;
        std::vector<std::thread> clausesLoad;

        std::vector<std::shared_ptr<SolverCdclInterface>> cdclSolvers;
        std::vector<std::shared_ptr<LocalSearchInterface>> localSolvers;
        std::vector<std::shared_ptr<SharingStrategy>> sharingStrategies;

        int receivedFinalResultBcast = 0;
        unsigned int varCount;
        SatResult res;

        if (!dist) {
                LOGERROR("PortfolioPRS is only available on dist mode for now.");
                globalEnding = true;
                mutexGlobalEnd.lock();
                condGlobalEnd.notify_all();
                mutexGlobalEnd.unlock();
                exit(PERR_NOT_SUPPORTED);
        }

        strategyEnding = false;

        // Mpi rank 0 is the leader, sole executor of PRS preprocessing.
        if (0 == Painless::mpi_rank) {
                /* PRS */
                this->preprocessors.push_back(std::make_shared<Painless::preprocess>(0));
                this->preprocessors.at(0)->loadFormula(Painless::__globalParameters__.filename.c_str());
                for (auto& preproc : preprocessors) {
                        res = preproc->solve({});
                        LOGDEBUG1("PRS returned %d", res);
                        if (20 == static_cast<int>(res)) {
                                LOG0("PRS answered UNSAT");
                                finalResult = SatResult::UNSAT;
                                this->join(this, finalResult, {});
                        } else if (10 == static_cast<int>(res)) {
                                LOG0("PRS answered SAT");
                                finalModel = preproc->getModel();
                                finalResult = SatResult::SAT;
                                this->join(this, finalResult, finalModel);
                        }
                }

                receivedFinalResultBcast = static_cast<int>(finalResult.load());

                if (receivedFinalResultBcast != 0)
                        goto prs_sync;

                // Free some memory
                for (auto& preproc : preprocessors)
                        preproc->releaseMemory();

                auto lastSimplification = preprocessors.back();
                varCount = lastSimplification->getVariablesCount();

                initClauses = std::move(lastSimplification->getSimplifiedFormula());
        }

prs_sync:
        TESTRUNMPI(MPI_Bcast(&receivedFinalResultBcast, 1, MPI_INT, 0, MPI_COMM_WORLD));

        if (receivedFinalResultBcast != 0) {
                Painless::mpi_winner = 0;
                finalResult = static_cast<SatResult>(receivedFinalResultBcast);
                globalEnding = true;
                LOGDEBUG1("[PRS] It is the mpi end: %d", receivedFinalResultBcast);
                mutexGlobalEnd.lock();
                condGlobalEnd.notify_all();
                mutexGlobalEnd.unlock();
                return;
        }

        // Create Groups
        uint quarter = Painless::mpi_world_size / 4;
        uint halfquarter = Painless::mpi_world_size / 8;

        // For better clarity
        sizePerGroup.insert({ PRSGroups::SAT, halfquarter });
        sizePerGroup.insert({ PRSGroups::UNSAT, quarter });
        sizePerGroup.insert({ PRSGroups::MAPLE, halfquarter });
        sizePerGroup.insert({ PRSGroups::LGL, 1 });
        sizePerGroup.insert({ PRSGroups::DEFAULT, Painless::mpi_world_size - (halfquarter * 2 + quarter + 1) });

        this->computeNodeGroup(Painless::mpi_world_size, Painless::mpi_rank);

        std::string portfolio = Painless::__globalParameters__.solver;
        if (portfolio[0] != 'k' && portfolio[0] != 'K' && portfolio[0] != 'I') {
                LOGERROR("%c solver cannot be used as default solver in PortfolioPRS!", portfolio[0]);
                std::abort();
        }

        // Create Solvers & Set Diversfication
        uint threadsPerProc = Painless::__globalParameters__.cpus;
        switch (this->nodeGroup) {
                case PRSGroups::SAT:
                        this->solversPortfolio = portfolio[0];
                        SolverFactory::createSolvers(threadsPerProc, 'd', this->solversPortfolio, cdclSolvers, localSolvers);
                        for (auto& kissat : cdclSolvers) {
                                if (kissat->getSolverType() == SolverCdclType::KISSATINC) {
                                        static_cast<KissatINCSolver*>(kissat.get())->setFamily(Painless::KissatFamily::SAT_STABLE);
                                } else if (kissat->getSolverType() == SolverCdclType::KISSAT) {
                                        static_cast<Kissat*>(kissat.get())->setFamily(Painless::KissatFamily::SAT_STABLE);
                                } else if (kissat->getSolverType() == SolverCdclType::KISSATMAB) {
                                        static_cast<KissatMABSolver*>(kissat.get())->setFamily(Painless::KissatFamily::SAT_STABLE);
                                }
                        }
                        break;
                case PRSGroups::UNSAT:
                        this->solversPortfolio = portfolio[0];
                        SolverFactory::createSolvers(threadsPerProc, 'd', this->solversPortfolio, cdclSolvers, localSolvers);
                        for (auto& kissat : cdclSolvers) {
                                if (kissat->getSolverType() == SolverCdclType::KISSATINC) {
                                        static_cast<KissatINCSolver*>(kissat.get())->setFamily(Painless::KissatFamily::UNSAT_FOCUSED);
                                } else if (kissat->getSolverType() == SolverCdclType::KISSAT) {
                                        static_cast<Kissat*>(kissat.get())->setFamily(Painless::KissatFamily::UNSAT_FOCUSED);
                                } else if (kissat->getSolverType() == SolverCdclType::KISSATMAB) {
                                        static_cast<KissatMABSolver*>(kissat.get())->setFamily(Painless::KissatFamily::UNSAT_FOCUSED);
                                }
                        }
                        break;
                case PRSGroups::DEFAULT:
                        this->solversPortfolio = portfolio[0];
                        SolverFactory::createSolvers(threadsPerProc, 'd', this->solversPortfolio, cdclSolvers, localSolvers);
                        for (auto& kissat : cdclSolvers) {
                                if (kissat->getSolverType() == SolverCdclType::KISSATINC) {
                                        static_cast<KissatINCSolver*>(kissat.get())->setFamily(Painless::KissatFamily::MIXED_SWITCH);
                                } else if (kissat->getSolverType() == SolverCdclType::KISSAT) {
                                        static_cast<Kissat*>(kissat.get())->setFamily(Painless::KissatFamily::MIXED_SWITCH);
                                } else if (kissat->getSolverType() == SolverCdclType::KISSATMAB) {
                                        static_cast<KissatMABSolver*>(kissat.get())->setFamily(Painless::KissatFamily::MIXED_SWITCH);
                                }
                        }
                        break;
                case PRSGroups::MAPLE:
                        this->solversPortfolio = portfolio[1];
                        SolverFactory::createSolvers(threadsPerProc, 'd', this->solversPortfolio, cdclSolvers, localSolvers);
                        break;
                case PRSGroups::LGL:
                        this->solversPortfolio = "l";
                        SolverFactory::createSolvers(threadsPerProc, 'd', this->solversPortfolio, cdclSolvers, localSolvers);
                        break;
        }

        // Diversify

        LOG0("I am in group %d with portfolio '%s'", static_cast<int>(nodeGroup), solversPortfolio.c_str());
        // auto generalIDScaler = [rank = Painless::mpi_rank, size = Painless::mpi_world_size](unsigned id) { return rank * size + id; };
        // auto typeIDScaler = [rank = rankInMyGroup, size = sizePerGroup.at(nodeGroup)](unsigned id) {
        auto generalIDScaler = [rank = Painless::mpi_rank,
                                                        size = Painless::__globalParameters__.cpus](const std::shared_ptr<SolverInterface>& solver) {
                return rank * size + solver->getSolverId();
        };
        auto typeIDScaler = [rank = rankInMyGroup,
                                                 size = Painless::__globalParameters__.cpus](const std::shared_ptr<SolverInterface>& solver) {
                return rank * size + solver->getSolverTypeId();
        };

        SolverFactory::diversification(cdclSolvers, localSolvers, generalIDScaler, typeIDScaler);

        // Send Formula from leader to workers

        Painless::mpiutils::sendFormula(initClauses, &varCount, 0);

        // Load Formula in Solvers

        for (auto cdcl : cdclSolvers) {
                clausesLoad.emplace_back(&LocalSearchInterface::addInitialClauses, cdcl, std::ref(initClauses), varCount);
        }
        for (auto local : localSolvers) {
                clausesLoad.emplace_back(&LocalSearchInterface::addInitialClauses, local, std::ref(initClauses), varCount);
        }

        // Wait for clauses init
        for (auto& worker : clausesLoad)
                worker.join();

        clausesLoad.clear();

        LOG1("All solvers loaded the clauses");

        // Launch Solvers

        for (auto cdcl : cdclSolvers) {
                this->addSlave(new SequentialWorker(cdcl));
        }
        for (auto local : localSolvers) {
                this->addSlave(new SequentialWorker(local));
        }

        for (size_t i = 0; i < slaves.size(); i++) {
                slaves[i]->solve(cube);
        }

        /* Sharing */
        /* ------- */

        uint maxClauseSizeLocal = 80;
        uint maxClauseSizeGlobal = 60;

        /* Local HordeSat with a database limited to clause of size 80 */
        std::shared_ptr<SharingStrategy> localStrategy =
                std::make_shared<HordeSatSharing>(std::make_shared<Painless::ClauseDatabaseBufferPerEntity>(maxClauseSizeLocal),
                                                                                  Painless::__globalParameters__.sharedLiteralsPerProducer,
                                                                                  Painless::__globalParameters__.hordeInitialLbdLimit,
                                                                                  Painless::__globalParameters__.hordeInitRound);

        sharingStrategies.push_back(localStrategy);

        /* Global Sharing Strategy with genericSharing */
        /* Left neighbor is my producer, my right one is my consumer */
        std::shared_ptr<GlobalSharingStrategy> globalStrategy =
                std::make_shared<GenericGlobalSharing>(std::make_shared<Painless::ClauseDatabasePerSize>(maxClauseSizeGlobal),
                                                                                           std::vector<int>{ left_neighbor },
                                                                                           std::vector<int>{ right_neighbor },
                                                                                           Painless::__globalParameters__.globalSharedLiterals);

        sharingStrategies.push_back(globalStrategy);

        /* Init MPI for the Global Sharing Strategy */
        globalStrategy->initMpiVariables();

        /*
         * Producers exports their clauses to local Strategy which in turn exports to global Strategy
         * Producers clause production is managed by the local strategy
         * Received clauses from other nodes are directly exported to consumers
         * Received clauses from previous node are transfered to next node, thus globalStrategy is its own client
         */
        localStrategy->addClient(globalStrategy);
        globalStrategy->addClient(globalStrategy);

        /* Connect all solvers as consumers and producers of local strategy */
        for (auto& solver : cdclSolvers) {
                localStrategy->addClient(solver);
                localStrategy->addProducer(solver);             // This initializes the needed data structures for heuristics ...
                localStrategy->connectProducer(solver); // This adds the strategy to the client list of solver
                globalStrategy->addClient(solver);              // All solvers are clients of the global strategy
        }

        /* Launch sharers */
        SharingStrategyFactory::launchSharers(sharingStrategies, this->sharers);

        initClauses.clear();
}

void
PortfolioPRS::computeNodeGroup(int worldSize, int myRank)
{
        // Validate input
        if (myRank < 0 || myRank >= worldSize) {
                throw std::out_of_range("Invalid rank");
        }

        int previousGroupsSizes = 0;

        // SAT group
        if (myRank < sizePerGroup[PRSGroups::SAT]) {
                this->nodeGroup = PRSGroups::SAT;
                goto neighbors;
        }
        previousGroupsSizes += sizePerGroup[PRSGroups::SAT];

        // UNSAT group
        if (myRank < previousGroupsSizes + sizePerGroup[PRSGroups::UNSAT]) {
                this->nodeGroup = PRSGroups::UNSAT;
                goto neighbors;
        }
        previousGroupsSizes += sizePerGroup[PRSGroups::UNSAT];

        // MAPLE group
        if (myRank < previousGroupsSizes + sizePerGroup[PRSGroups::MAPLE]) {
                this->nodeGroup = PRSGroups::MAPLE;
                goto neighbors;
        }
        previousGroupsSizes += sizePerGroup[PRSGroups::MAPLE];

        // LGL group
        if (myRank < previousGroupsSizes + sizePerGroup[PRSGroups::LGL]) {
                this->nodeGroup = PRSGroups::LGL;
                goto neighbors;
        }
        previousGroupsSizes += sizePerGroup[PRSGroups::LGL];

        // DEFAULT group
        this->nodeGroup = PRSGroups::DEFAULT;

neighbors:
        unsigned myGroupSize = sizePerGroup[nodeGroup];
        rankInMyGroup = myRank - previousGroupsSizes;

        // Neighbors Compute:
        left_neighbor = previousGroupsSizes + (rankInMyGroup - 1 + myGroupSize) % myGroupSize;
        right_neighbor = previousGroupsSizes + (rankInMyGroup + 1) % myGroupSize;
}

void
PortfolioPRS::restoreModelDist(std::vector<int>& model)
{
        assert(model.size() > 0);
        for (auto it = preprocessors.rbegin(); it != preprocessors.rend(); ++it) {
                LOGDEBUG1("preprocessor %u", std::distance(it, preprocessors.rend()) - 1);
                (*it)->restoreModel(model);
        }
}

void
PortfolioPRS::join(WorkingStrategy* strat, SatResult res, const std::vector<int>& model)
{
        if (res == SatResult::UNKNOWN || strategyEnding)
                return;

        strategyEnding = true;

        setSolverInterrupt();

        if (parent == NULL) { // If it is the top strategy
                finalResult = res;
                globalEnding = true;

                if (res == SatResult::SAT) {
                        finalModel = model;
                }
                if (strat != this) {
                        SequentialWorker* winner = (SequentialWorker*)strat;
                        winner->solver->printWinningLog();
                } else {
                        LOGSTAT("The winner is of type: PRS-PRE");
                }

                mutexGlobalEnd.lock();
                condGlobalEnd.notify_all();
                mutexGlobalEnd.unlock();
                LOGDEBUG1("Broadcasted the end");
        } else { // Else forward the information to the parent strategy
                parent->join(this, res, model);
        }
}

void
PortfolioPRS::setSolverInterrupt()
{
        for (size_t i = 0; i < slaves.size(); i++) {
                slaves[i]->setSolverInterrupt();
        }
}

void
PortfolioPRS::unsetSolverInterrupt()
{
        for (size_t i = 0; i < slaves.size(); i++) {
                slaves[i]->unsetSolverInterrupt();
        }
}

void
PortfolioPRS::waitInterrupt()
{
        for (size_t i = 0; i < slaves.size(); i++) {
                slaves[i]->waitInterrupt();
        }
}

} // namespace Painless
