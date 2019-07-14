#include <cassert>
#include <cxxtrace/detail/sample.h>
#include <cxxtrace/snapshot.h>
#include <vector>

namespace cxxtrace {
samples_snapshot::samples_snapshot(const samples_snapshot&) noexcept(false) =
  default;
samples_snapshot::samples_snapshot(samples_snapshot&&) noexcept = default;
samples_snapshot&
samples_snapshot::operator=(const samples_snapshot&) noexcept(false) = default;
samples_snapshot&
samples_snapshot::operator=(samples_snapshot&&) noexcept = default;
samples_snapshot::~samples_snapshot() noexcept = default;

auto
samples_snapshot::at(size_type index) noexcept(false) -> sample_ref
{
  return sample_ref{ &this->samples.at(index) };
}

auto
samples_snapshot::size() const noexcept -> size_type
{
  return size_type{ this->samples.size() };
}

samples_snapshot::samples_snapshot(
  std::vector<detail::snapshot_sample> samples) noexcept
  : samples{ std::move(samples) }
{}

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
