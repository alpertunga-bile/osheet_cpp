#include "indicator_utils.hpp"

#include "indicators/cursor_control.hpp"

#include <format>

namespace osheet {

auto
Spinner::init(const char* text) -> void
{
  set_values(text, Spinner::Status::STARTED);
}

auto
Spinner::set_values(const char* text, Spinner::Status status) -> void
{
  spinner.set_option(indicators::option::PostfixText{ text });
  spinner.set_option(indicators::option::ShowPercentage{ false });

  switch (status) {
    case Spinner::Status::IN_PROGRESS:
    case Spinner::Status::STARTED:
      spinner.set_option(
        indicators::option::ForegroundColor{ indicators::Color::yellow });
      break;
    case Spinner::Status::SUCCESS_FINISHED:
      spinner.set_option(
        indicators::option::ForegroundColor{ indicators::Color::green });
      spinner.set_option(indicators::option::PrefixText{ "✔" });
      spinner.set_option(indicators::option::ShowSpinner{ false });
      spinner.mark_as_completed();
      break;
    case Spinner::Status::FAILED_FINISHED:
      spinner.set_option(
        indicators::option::ForegroundColor{ indicators::Color::red });
      spinner.set_option(indicators::option::PrefixText{ "✖" });
      spinner.set_option(indicators::option::ShowSpinner{ false });
      spinner.mark_as_completed();
      break;
    default:
      break;
  }
}

auto
Spinner::tick(int loop_count, int64_t ms_sleep) -> void
{
  for (int i = 0; i < loop_count; ++i) {
    spinner.tick();
    std::this_thread::sleep_for(std::chrono::milliseconds(ms_sleep));
  }
}

auto
ProgressBar::init(const char* text, uint16_t total_count) -> void
{
  indicators::show_console_cursor(false);

  pbar.set_option(indicators::option::BarWidth{ 50 });
  pbar.set_option(indicators::option::Start{ "[" });
  pbar.set_option(indicators::option::Fill{ "■" });
  pbar.set_option(indicators::option::Lead{ "■" });
  pbar.set_option(indicators::option::Remainder{ "-" });
  pbar.set_option(indicators::option::End{ " ]" });
  pbar.set_option(indicators::option::PostfixText("Extracting tiles"));
  pbar.set_option(
    indicators::option::ForegroundColor{ indicators::Color::cyan });
  pbar.set_option(indicators::option::ShowPercentage{ true });
  pbar.set_option(indicators::option::FontStyles{
    std::vector<indicators::FontStyle>{ indicators::FontStyle::bold } });

  set_values(text, 0, ProgressBar::Status::STARTED);
  this->total_count = total_count;
}

auto
ProgressBar::set_values(const char*         text,
                        uint16_t            value,
                        ProgressBar::Status status) -> void
{
  pbar.set_option(indicators::option::PostfixText{ text });

  switch (status) {
    case ProgressBar::Status::IN_PROGRESS:
      pbar.set_option(
        indicators::option::ForegroundColor{ indicators::Color::yellow });
      pbar.set_option(indicators::option::PostfixText{
        std::format("{} {}/{}", text, value, this->total_count) });

      pbar.set_progress(static_cast<float>(value) /
                        static_cast<float>(this->total_count) * 100.0f);

      break;
    case ProgressBar::Status::SUCCESS_FINISHED:
      pbar.set_option(
        indicators::option::ForegroundColor{ indicators::Color::green });
      pbar.set_option(
        indicators::option::PostfixText{ std::format("✔ {}", text) });

      pbar.set_progress(100);

      indicators::show_console_cursor(true);
      break;
    case ProgressBar::Status::FAILED_FINISHED:
      pbar.set_option(
        indicators::option::ForegroundColor{ indicators::Color::red });
      pbar.set_option(
        indicators::option::PrefixText{ std::format("✖ {}", text) });

      pbar.set_progress(100);

      indicators::show_console_cursor(true);
      break;
    default:
      break;
  }
}
}