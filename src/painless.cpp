#include "painless.hpp"

#include "utils/ErrorCodes.hpp"
#include "utils/Logger.hpp"
#include "utils/MpiUtils.hpp"
#include "utils/Parameters.hpp"
#include "utils/Parsers.hpp"
#include "utils/System.hpp"

#include "solvers/SolverFactory.hpp"
#include "working/WorkingStrategy.hpp"

#include "working/PortfolioPRS.hpp"
#include "working/PortfolioSimple.hpp"

#include <random>
#include <thread>
#include <unistd.h>

// -------------------------------------------
// Signal Handling
// -------------------------------------------
#include <csignal>

// Cleanup function to be called at exit
void
cleanup()
{
        Painless::SystemResourceMonitor::printProcessResourceUsage();
}
// Signal handler function
void
signalHandler(int signum)
{
        // Call cleanup directly for immediate signals
        cleanup();

        // Re-raise the signal after cleaning up
        std::signal(signum, SIG_DFL); /* Use defailt signal handler */
        std::raise(signum);
}

// Function to set up exit handlers
void
setupExitHandlers()
{
        // Register cleanup function to be called at normal program termination
        std::atexit(cleanup);

        // Set up signal handlers
        std::signal(SIGINT, signalHandler);
        std::signal(SIGTERM, signalHandler);
        std::signal(SIGABRT, signalHandler);
        // Add more signals as needed
}

// -------------------------------------------
// Declaration of global variables
// -------------------------------------------

std::atomic<bool> globalEnding(false);
std::mutex mutexGlobalEnd;
std::condition_variable condGlobalEnd;

// int nSharers = 0;

Painless::WorkingStrategy* working = nullptr;

std::atomic<bool> dist = false;

std::atomic<SatResult> finalResult = SatResult::UNKNOWN;

std::vector<int> finalModel;

// -------------------------------------------
// Main of the framework
// -------------------------------------------
int
main(int argc, char** argv)
{
        // setupExitHandlers();

        Painless::Parameters::init(argc, argv);

        Painless::setVerbosityLevel(Painless::__globalParameters__.verbosity);

        if(Painless::__globalParameters__.help)
        {
                Painless::Parameters::printHelp();
        }

        dist = Painless::__globalParameters__.enableDistributed;

        // Ram Monitoring

        if (dist) {
                // MPI Initialization
                int provided;
                TESTRUNMPI(MPI_Init_thread(NULL, NULL, MPI_THREAD_SERIALIZED, &provided));
                TESTRUNMPI(MPI_Comm_set_errhandler(MPI_COMM_WORLD, MPI_ERRORS_RETURN));

                LOGDEBUG1("Thread strategy provided is %d", provided);

                if (provided < MPI_THREAD_SERIALIZED) {
                        LOGERROR("Wanted MPI initialization is not possible !");
                        dist = false;
                } else {
                        TESTRUNMPI(MPI_Comm_rank(MPI_COMM_WORLD, &Painless::mpi_rank));

                        char hostname[256];
                        gethostname(hostname, sizeof(hostname));
                        LOGDEBUG1("PID %d on %s is of rank %d", getpid(), hostname, Painless::mpi_rank);

                        TESTRUNMPI(MPI_Comm_size(MPI_COMM_WORLD, &Painless::mpi_world_size));
                }
        }

        if (!Painless::mpi_rank)
                Painless::Parameters::printParams();

        // Init timeout detection before starting the solvers and sharers
        std::unique_lock<std::mutex> lock(mutexGlobalEnd);
        // to make sure that the broadcast is done when main has done its wait

        // TODO: better choice options, think about description file using yaml or json with semantic checking
        if (Painless::__globalParameters__.simple)
                working = new Painless::PortfolioSimple();
        else
                working = new Painless::PortfolioPRS();

        // Launch working
        std::vector<int> cube;

        std::thread mainWorker(&Painless::WorkingStrategy::solve, working, std::ref(cube));

        int wakeupRet = 0;

        if (Painless::__globalParameters__.timeout > 0) {
                auto startTime = Painless::SystemResourceMonitor::getRelativeTimeSeconds();

                // Wait until end or Painless::__globalParameters__.timeout
                while ((unsigned int)Painless::SystemResourceMonitor::getRelativeTimeSeconds() < Painless::__globalParameters__.timeout &&
                           globalEnding == false) // to manage the spurious wake ups
                {
                        auto remainingTime = std::chrono::duration<double>(
                                Painless::__globalParameters__.timeout - (Painless::SystemResourceMonitor::getRelativeTimeSeconds() - startTime));
                        auto wakeupStatus = condGlobalEnd.wait_for(lock, remainingTime);

                        LOGDEBUG2("main wakeupRet = %s , globalEnding = %d ",
                                          (wakeupStatus == std::cv_status::timeout ? "timeout" : "notimeout"),
                                          globalEnding.load());
                }

                condGlobalEnd.notify_all();
                lock.unlock();

                if ((unsigned int)Painless::SystemResourceMonitor::getRelativeTimeSeconds() >= Painless::__globalParameters__.timeout &&
                        finalResult ==
                                SatResult::UNKNOWN) // if Painless::__globalParameters__.timeout set globalEnding otherwise a solver woke me up
                {
                        globalEnding = true;
                        finalResult = SatResult::TIMEOUT;
                }
        } else {
                // no Painless::__globalParameters__.timeout waiting
                while (globalEnding == false) // to manage the spurious wake ups
                {
                        condGlobalEnd.wait(lock);
                }

                condGlobalEnd.notify_all();
                lock.unlock();
        }

        mainWorker.join();

        delete working;

        if (dist) {
                TESTRUNMPI(MPI_Finalize());
        }

        if (Painless::mpi_rank == Painless::mpi_winner) {
                if (finalResult == SatResult::SAT) {
                        Painless::logSolution("SATISFIABLE");

                        if (Painless::__globalParameters__.noModel == false) {
                                Painless::logModel(finalModel);
                        }
                } else if (finalResult == SatResult::UNSAT) {
                        Painless::logSolution("UNSATISFIABLE");
                } else // if Painless::__globalParameters__.timeout or unknown
                {
                        Painless::logSolution("UNKNOWN");
                        finalResult = SatResult::UNKNOWN;
                }

                LOGSTAT("Resolution time: %f s", Painless::SystemResourceMonitor::getRelativeTimeSeconds());
        } else
                finalResult = SatResult::UNKNOWN; /* mpi will be forced to suspend job only by the winner */

        LOGDEBUG1("Mpi process %d returns %d", Painless::mpi_rank, static_cast<int>(finalResult.load()));

        return static_cast<int>(finalResult.load());
}
