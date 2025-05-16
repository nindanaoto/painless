#include "painless.hpp"
#include "working/WorkingStrategy.hpp"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <vector>

// -------------------------------------------
// Declaration of global variables
// -------------------------------------------

std::atomic<bool> globalEnding(false);
std::mutex mutexGlobalEnd;
std::condition_variable condGlobalEnd;

// int nSharers = 0;

WorkingStrategy* working = NULL;

std::atomic<bool> dist = false;

std::atomic<SatResult> finalResult = SatResult::UNKNOWN;

std::vector<int> finalModel;