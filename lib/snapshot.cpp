#include <algorithm>
#include <cassert>
#include <cxxtrace/detail/snapshot_sample.h>
#include <cxxtrace/detail/thread.h>
#include <cxxtrace/snapshot.h>
#include <cxxtrace/thread.h>
#include <iterator>
#include <string>
#include <vector>

namespace cxxtrace {
samples_snapshot::samples_snapshot(
  std::vector<detail::snapshot_sample> samples,
  detail::thread_name_set thread_names) noexcept
  : samples{ std::move(samples) }
  , thread_names{ std::move(thread_names) }
{}

samples_snapshot::samples_snapshot(const samples_snapshot&) noexcept(false) =
  default;
samples_snapshot::samples_snapshot(samples_snapshot&&) noexcept = default;
samples_snapshot&
samples_snapshot::operator=(const samples_snapshot&) noexcept(false) = default;
samples_snapshot&
samples_snapshot::operator=(samples_snapshot&&) noexcept(false) = default;
samples_snapshot::~samples_snapshot() noexcept = default;

auto
samples_snapshot::at(size_type index) const noexcept(false) -> sample_ref
{
  return sample_ref{ &this->samples.at(index) };
}

auto
samples_snapshot::size() const noexcept -> size_type
{
  return size_type{ this->samples.size() };
}

auto
samples_snapshot::thread_name(thread_id thread) const noexcept -> czstring
{
  return this->thread_names.name_of_thread_by_id(thread);
}

auto
samples_snapshot::thread_ids() const noexcept(false) -> std::vector<thread_id>
{
  auto ids = std::vector<thread_id>{};
  std::transform(this->thread_names.names.begin(),
                 this->thread_names.names.end(),
                 std::back_inserter(ids),
                 [](const std::pair<const thread_id, std::string>& entry) {
                   return entry.first;
                 });
  return ids;
}

auto
sample_ref::category() const noexcept -> czstring
{
  return this->sample->category;
}

auto
sample_ref::kind() const noexcept -> sample_kind
{
  return this->sample->kind;
}

auto
sample_ref::name() const noexcept -> czstring
{
  return this->sample->name;
}

auto
sample_ref::thread_id() const noexcept -> cxxtrace::thread_id
{
  return this->sample->thread_id;
}

auto
sample_ref::timestamp() const -> time_point
{
  return this->sample->timestamp;
}

sample_ref::sample_ref(const detail::snapshot_sample* sample) noexcept
  : sample{ sample }
{}
}
