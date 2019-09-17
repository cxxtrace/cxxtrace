#include "concurrency_test_runner.h"
#include "cxxtrace_concurrency_test_base.h"
#include <cassert>
#include <condition_variable>
#include <cxxtrace/thread.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

using testing::ElementsAre;

namespace cxxtrace_test {
namespace {
class logging_concurrency_test : public cxxtrace_test::detail::concurrency_test
{
public:
  explicit logging_concurrency_test(int thread_count) noexcept
    : thread_count_{ thread_count }
  {}

  auto set_up() -> void override
  {
    auto guard = std::lock_guard<std::mutex>{ this->mutex_ };
    this->log_ << "set_up\n";
  }

  auto run_thread(int thread_index) -> void override
  {
    auto guard = std::lock_guard<std::mutex>{ this->mutex_ };
    this->log_ << "run_thread " << thread_index << "\n";
  }

  auto tear_down() -> void override
  {
    auto guard = std::lock_guard<std::mutex>{ this->mutex_ };
    this->log_ << "tear_down\n";
  }

  auto test_depth() const noexcept -> concurrency_test_depth override
  {
    return concurrency_test_depth::full;
  }

  auto thread_count() const noexcept -> int override
  {
    return this->thread_count_;
  }

  auto log() const -> std::string
  {
    auto guard = std::lock_guard<std::mutex>{ this->mutex_ };
    return this->log_.str();
  }

private:
  int thread_count_;

  mutable std::mutex mutex_{};
  std::ostringstream log_{};
};
}

TEST(test_concurrency_test_runner,
     single_threaded_run_executes_set_up_run_thread_and_tear_down)
{
  auto test = logging_concurrency_test{ 1 };
  auto runner = concurrency_test_runner{ &test };
  runner.start_workers();
  runner.run_once();
  runner.stop_workers();
  EXPECT_EQ(test.log(), "set_up\nrun_thread 0\ntear_down\n");
}

TEST(test_concurrency_test_runner,
     multiple_runs_have_separate_setups_and_teardowns)
{
  auto test = logging_concurrency_test{ 1 };
  auto runner = concurrency_test_runner{ &test };
  runner.start_workers();
  runner.run_once();
  runner.run_once();
  runner.run_once();
  runner.stop_workers();
  EXPECT_EQ(test.log(),
            "set_up\nrun_thread 0\ntear_down\n"
            "set_up\nrun_thread 0\ntear_down\n"
            "set_up\nrun_thread 0\ntear_down\n");
}

TEST(test_concurrency_test_runner, test_threads_execute_concurrently)
{
  class synchronizing_concurrency_test
    : public cxxtrace_test::detail::concurrency_test
  {
  public:
    auto set_up() -> void override { this->state_ = state::initial; }

    auto run_thread(int thread_index) -> void override
    {
      switch (thread_index) {
        case 0: {
          auto done = false;
          auto lock = std::unique_lock<std::mutex>{ this->mutex_ };
          while (!done) {
            switch (this->state_) {
              case state::initial:
                this->log_ << "thread 0 started\n";
                this->state_ = state::thread_0_started;
                this->state_changed_.notify_all();
                break;

              case state::thread_0_started:
                this->state_changed_.wait(lock, [&] {
                  return this->state_ != state::thread_0_started;
                });
                break;

              case state::thread_1_acknowledged:
                this->log_ << "thread 0 saw acknowledgement\n";
                done = true;
                break;
            }
          }
        }

        case 1: {
          auto done = false;
          auto lock = std::unique_lock<std::mutex>{ this->mutex_ };
          while (!done) {
            switch (this->state_) {
              case state::initial:
                this->state_changed_.wait(
                  lock, [&] { return this->state_ != state::initial; });
                break;

              case state::thread_0_started:
                this->log_ << "thread 1 acknowledged\n";
                this->state_ = state::thread_1_acknowledged;
                this->state_changed_.notify_all();
                [[fallthrough]];

              case state::thread_1_acknowledged:
                done = true;
                break;
            }
          }
        }
      }
    }

    auto tear_down() -> void override {}

    auto test_depth() const noexcept -> concurrency_test_depth override
    {
      return concurrency_test_depth::full;
    }

    auto thread_count() const noexcept -> int override { return 2; }

    auto log() const -> std::string
    {
      auto guard = std::lock_guard<std::mutex>{ this->mutex_ };
      return this->log_.str();
    }

  private:
    enum class state
    {
      initial,
      thread_0_started,
      thread_1_acknowledged,
    };

    mutable std::mutex mutex_{};
    std::condition_variable state_changed_;
    state state_;
    std::ostringstream log_{};
  };

  auto test = synchronizing_concurrency_test{};
  auto runner = concurrency_test_runner{ &test };
  runner.start_workers();
  runner.run_once();
  runner.stop_workers();
  EXPECT_EQ(test.log(),
            "thread 0 started\n"
            "thread 1 acknowledged\n"
            "thread 0 saw acknowledgement\n");
}

TEST(test_concurrency_test_runner, failure_setting_up_skips_run_and_teardown)
{
  class test_concurrency_test : public cxxtrace_test::detail::concurrency_test
  {
  public:
    auto set_up() -> void override
    {
      this->log_ << "set_up\n";
      this->runner->add_failure("set_up failure");
    }

    auto run_thread(int thread_index) -> void override
    {
      this->log_ << "run_thread " << thread_index << "\n";
    }

    auto tear_down() -> void override { this->log_ << "tear_down\n"; }

    auto test_depth() const noexcept -> concurrency_test_depth override
    {
      return concurrency_test_depth::full;
    }

    auto thread_count() const noexcept -> int override { return 1; }

    auto log() const -> std::string { return this->log_.str(); }

    concurrency_test_runner* runner{ nullptr };

  private:
    std::ostringstream log_{};
  };

  auto test = test_concurrency_test{};
  auto runner = concurrency_test_runner{ &test };
  test.runner = &runner;
  runner.start_workers();
  runner.run_once();
  runner.stop_workers();
  EXPECT_EQ(test.log(), "set_up\n");
  EXPECT_THAT(runner.failures(), ElementsAre("set_up failure"));
}

TEST(test_concurrency_test_runner, failure_in_test_runs_teardown)
{
  class test_concurrency_test : public cxxtrace_test::detail::concurrency_test
  {
  public:
    auto set_up() -> void override { this->log_ << "set_up\n"; }

    auto run_thread(int thread_index) -> void override
    {
      this->log_ << "run_thread " << thread_index << "\n";
      this->runner->add_failure("run_thread failure");
    }

    auto tear_down() -> void override { this->log_ << "tear_down\n"; }

    auto test_depth() const noexcept -> concurrency_test_depth override
    {
      return concurrency_test_depth::full;
    }

    auto thread_count() const noexcept -> int override { return 1; }

    auto log() const -> std::string { return this->log_.str(); }

    concurrency_test_runner* runner{ nullptr };

  private:
    std::ostringstream log_{};
  };

  auto test = test_concurrency_test{};
  auto runner = concurrency_test_runner{ &test };
  test.runner = &runner;
  runner.start_workers();
  runner.run_once();
  runner.stop_workers();
  EXPECT_EQ(test.log(), "set_up\nrun_thread 0\ntear_down\n");
  EXPECT_THAT(runner.failures(), ElementsAre("run_thread failure"));
}

TEST(test_concurrency_test_runner, run_count_includes_successes_and_failures)
{
  class test_concurrency_test : public cxxtrace_test::detail::concurrency_test
  {
  public:
    auto set_up() -> void override
    {
      if (this->set_up_should_fail) {
        this->runner->add_failure("set_up failure");
      }
    }

    auto run_thread([[maybe_unused]] int thread_index) -> void override
    {
      if (this->run_thread_should_fail) {
        this->runner->add_failure("run_thread failure");
      }
    }

    auto tear_down() -> void override
    {
      if (this->tear_down_should_fail) {
        this->runner->add_failure("tear_down failure");
      }
    }

    auto test_depth() const noexcept -> concurrency_test_depth override
    {
      return concurrency_test_depth::full;
    }

    auto thread_count() const noexcept -> int override { return 1; }

    bool set_up_should_fail{ false };
    bool run_thread_should_fail{ false };
    bool tear_down_should_fail{ false };
    concurrency_test_runner* runner{ nullptr };
  };

  auto test = test_concurrency_test{};
  auto runner = concurrency_test_runner{ &test };
  test.runner = &runner;
  runner.start_workers();

  runner.run_once();

  test.set_up_should_fail = true;
  runner.run_once();
  test.set_up_should_fail = false;

  test.run_thread_should_fail = true;
  runner.run_once();
  test.run_thread_should_fail = false;

  test.tear_down_should_fail = true;
  runner.run_once();
  test.tear_down_should_fail = false;

  runner.stop_workers();
  EXPECT_EQ(runner.run_count(), 4);
}

TEST(test_concurrency_test_runner, resetting_workers_creates_new_threads)
{
  class test_concurrency_test : public cxxtrace_test::detail::concurrency_test
  {
  public:
    auto set_up() -> void override {}

    auto run_thread([[maybe_unused]] int thread_index) -> void override
    {
      this->run_thread_id = cxxtrace::get_current_thread_id();
    }

    auto tear_down() -> void override {}

    auto test_depth() const noexcept -> concurrency_test_depth override
    {
      return concurrency_test_depth::full;
    }

    auto thread_count() const noexcept -> int override { return 1; }

    cxxtrace::thread_id run_thread_id{};
  };

  auto test = test_concurrency_test{};
  auto runner = concurrency_test_runner{ &test };

  runner.start_workers();
  runner.run_once();
  auto run_1_thread_id = test.run_thread_id;
  runner.stop_workers();

  runner.start_workers();
  runner.run_once();
  auto run_2_thread_id = test.run_thread_id;
  runner.stop_workers();

  EXPECT_NE(run_2_thread_id, run_1_thread_id);
}
}
