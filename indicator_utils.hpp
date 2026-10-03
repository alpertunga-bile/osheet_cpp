#ifndef OSHEET_CPP_INDICATOR_UTILS_HPP_
#define OSHEET_CPP_INDICATOR_UTILS_HPP_

#include "indicators/progress_spinner.hpp"
#include <cstdint>

enum class SpinnerStatus : uint8_t
{
  STARTED = 0,
  IN_PROGRESS,
  SUCCESS_FINISHED,
  FAILED_FINISHED
};

namespace osheet {

auto
create_spinner(indicators::ProgressSpinner& spinner, const char* text) -> void;

auto
set_spinner_values(indicators::ProgressSpinner& spinner,
                   const char*                  text,
                   SpinnerStatus                status) -> void;

auto
iter_spinner(indicators::ProgressSpinner& spinner,
             int                          loop_count = 10,
             int64_t                      ms_sleep   = 40) -> void;

}

#endif