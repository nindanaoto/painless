#pragma once

#include "solvers/SolverInterface.hpp"
#include "working/WorkingStrategy.hpp"

#include "utils/ErrorCodes.hpp"
#include "utils/Logger.hpp"
#include "utils/MpiUtils.hpp"
#include "utils/Parameters.hpp"
#include "utils/Parsers.hpp"
#include "utils/System.hpp"

#include "solvers/SolverFactory.hpp"

#include "working/PortfolioPRS.hpp"
#include "working/PortfolioSimple.hpp"


#include <atomic>
#include <condition_variable>
#include <mutex>
#include <vector>
#include <thread>

/// Is it the end of the search
extern std::atomic<bool> globalEnding;

/// @brief  Mutex for timeout cond
extern std::mutex mutexGlobalEnd;

/// @brief Cond to wait on timeout or wakeup on globalEnding
extern std::condition_variable condGlobalEnd;

/// Working strategy
extern WorkingStrategy* working;

/// Final result
extern std::atomic<SatResult> finalResult;

/// Model for SAT instances
extern std::vector<int> finalModel;

/// To check if painless is using distributed mode
extern std::atomic<bool> dist;