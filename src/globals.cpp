#include "painless.hpp"
#include "working/WorkingStrategy.hpp"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <vector>

namespace Painless {

// Global variables definitions
std::mutex mutexGlobalEnd;
std::condition_variable condGlobalEnd;
std::atomic<bool> globalEnding = false;

WorkingStrategy* working = nullptr;

std::atomic<bool> dist = false;

std::atomic<Painless::SatResult> finalResult = Painless::SatResult::UNKNOWN;

std::vector<int> finalModel;

} // namespace Painless
