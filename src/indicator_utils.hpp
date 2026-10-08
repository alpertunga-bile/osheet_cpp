#ifndef OSHEET_CPP_INDICATOR_UTILS_HPP_
#define OSHEET_CPP_INDICATOR_UTILS_HPP_

#include "indicators/progress_bar.hpp"
#include "indicators/progress_spinner.hpp"
#include <cstdint>

namespace osheet {

struct Spinner
{
  enum class Status : uint8_t
  {
    STARTED = 0,
    IN_PROGRESS,
    SUCCESS_FINISHED,
    FAILED_FINISHED
  };

  auto init(const char* text) -> void;
  auto set_values(const char* text, Spinner::Status status) -> void;
  auto tick(int loop_count = 10, int64_t ms_sleep = 40) -> void;

  Spinner::Status             current_status;
  indicators::ProgressSpinner spinner;
};

struct ProgressBar
{
  enum class Status : uint8_t
  {
    STARTED = 0,
    IN_PROGRESS,
    SUCCESS_FINISHED,
    FAILED_FINISHED
  };

  auto init(const char* text, uint16_t total_count) -> void;
  auto set_values(const char* text, uint16_t value, ProgressBar::Status status)
    -> void;

  uint16_t                total_count;
  ProgressBar::Status     current_status;
  indicators::ProgressBar pbar;
};

}

#endif