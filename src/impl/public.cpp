#include "public.hpp"
#include <chrono>
int64_t getCurrentTimestamp()
{
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}