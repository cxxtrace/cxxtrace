#include "concurrency_test_runner.h"
#include "cxxtrace_concurrency_test_base.h"
#include <algorithm>
#include <cassert>
#include <condition_variable>
#include <cxxtrace/string.h>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace cxxtrace_test {
class concurrency_test_runner::impl
{
public:
  explicit impl(detail::concurrency_test* test)
    : test_{ test }
  {}

  auto start_workers() -> void
  {
    assert(this->workers_.empty());
    assert(!this->exit_workers_);
    assert(this->current_round_ == round::tick);

    auto thread_count = this->test_->thread_count();
    assert(thread_count > 0);
    this->workers_.resize(thread_count);
    for (auto i = 0; i < thread_count; ++i) {
      this->workers_[i].thread =
        std::thread{ [i, this] { this->run_worker_thread(i); } };
    }
  }

  auto run_once() -> void
  {
    {
      auto guard = std::lock_guard<std::mutex>{ this->mutex_ };
      this->failures_.clear();
    }

    this->run_count_ += 1;
    this->test_->set_up();

    {
      auto guard = std::lock_guard<std::mutex>{ this->mutex_ };
      if (!this->failures_.empty()) {
        return;
      }
    }

    {
      auto guard = std::unique_lock<std::mutex>{ this->mutex_ };
      this->current_round_ = !this->current_round_;
      this->start_next_round_.notify_all();

      this->worker_finished_round_.wait(guard, [&] {
        return std::all_of(this->workers_.begin(),
                           this->workers_.end(),
                           [&](const worker& worker) {
                             return worker.finished_round ==
                                    this->current_round_;
                           });
      });
    }

    this->test_->tear_down();
  }

  auto stop_workers() -> void
  {
    assert(!this->workers_.empty());

    {
      auto guard = std::lock_guard<std::mutex>{ this->mutex_ };
      assert(!this->exit_workers_);
      this->exit_workers_ = true;
      this->start_next_round_.notify_all();
    }
    for (auto& worker : this->workers_) {
      worker.thread.join();
    }
    this->workers_.clear();
    this->exit_workers_ = false;
    this->current_round_ = round::tick;
  }

  auto run_count() -> long { return this->run_count_; }

  auto add_failure(cxxtrace::czstring message) -> void
  {
    auto guard = std::lock_guard<std::mutex>{ this->mutex_ };
    this->failures_.emplace_back(message);
  }

  auto failures() const -> std::vector<std::string>
  {
    auto guard = std::lock_guard<std::mutex>{ this->mutex_ };
    return this->failures_;
  }

private:
  enum class round
  {
    tick,
    tock,
  };

  friend auto operator!(round x) -> round
  {
    switch (x) {
      case round::tick:
        return round::tock;
      case round::tock:
        return round::tick;
    }
  }

  struct worker
  {
    std::thread thread;
    round finished_round{ round::tick };
  };

  auto run_worker_thread(int thread_index) noexcept -> void
  {
    auto& worker = this->workers_[thread_index];
    for (;;) {
      auto active_round = round{};
      {
        auto guard = std::unique_lock<std::mutex>{ this->mutex_ };
        auto finished_round = worker.finished_round;
        this->start_next_round_.wait(guard, [&] {
          return this->current_round_ != finished_round || this->exit_workers_;
        });
        if (this->exit_workers_) {
          return;
        }
        active_round = this->current_round_;
      }

      this->test_->run_thread(thread_index);

      {
        auto guard = std::unique_lock<std::mutex>{ this->mutex_ };
        worker.finished_round = active_round;
        this->worker_finished_round_.notify_one();
      }
    }
  }

  detail::concurrency_test* test_;

  std::vector<worker> workers_{};
  long run_count_{ 0 };

  // mutex_ protects worker::finished_round (in workers_), exit_workers_,
  // current_round_, and failures_.
  mutable std::mutex mutex_;

  round current_round_{ round::tick };
  bool exit_workers_{ false };
  std::vector<std::string> failures_;

  // Signal after changing current_round_ or exit_workers_.
  std::condition_variable start_next_round_{};

  // Signal after changing worker::finished_round (in workers_).
  std::condition_variable worker_finished_round_{};
};

concurrency_test_runner::concurrency_test_runner(detail::concurrency_test* test)
  : impl_{ std::make_unique<impl>(test) }
{}

concurrency_test_runner::~concurrency_test_runner() = default;

auto
concurrency_test_runner::start_workers() -> void
{
  return this->impl_->start_workers();
}

auto
concurrency_test_runner::run_once() -> void
{
  return this->impl_->run_once();
}

auto
concurrency_test_runner::stop_workers() -> void
{
  return this->impl_->stop_workers();
}

auto
concurrency_test_runner::run_count() -> long
{
  return this->impl_->run_count();
}

auto
concurrency_test_runner::add_failure(cxxtrace::czstring message) -> void
{
  return this->impl_->add_failure(message);
}

auto
concurrency_test_runner::failures() const -> std::vector<std::string>
{
  return this->impl_->failures();
}
}
