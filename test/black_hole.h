#include <benchmark/benchmark.h>
#include <random>

namespace cxxtrace_test {
// Motivation: Simulate "other code" in microbenchmarks. See Aleksey Shipilёv's
// fantistic Nanotrusting the Nanotime article [1].
//
// Prior art: BlackHole from Oracle's JMH benchmarking framework [2].
//
// [1] https://shipilev.net/blog/2014/nanotrusting-nanotime/
// [2]
// https://hg.openjdk.java.net/code-tools/jmh/file/99d7b73cf1e3/jmh-core/src/main/java/org/openjdk/jmh/infra/Blackhole.java
//
class black_hole
{
public:
  // Waste time by making the Arithmetic Logic Unit perform some work.
  auto alu(int multiplier) noexcept -> void
  {
    // We want to store intermediate state in registers, not in memory. Avoid
    // mutating this->alu_state in our loop; work on a copy.
    auto alu_state = this->alu_state;
    for (auto i = 0; i < multiplier; ++i) {
      alu_state();
    }
    benchmark::DoNotOptimize(alu_state);
    this->alu_state = alu_state;
  }

private:
  using base_engine = std::minstd_rand;
  // Avoid branches in libc++'s std::linear_congruential_engine by choosing a
  // 64-bit result type.
  // TODO(strager): Remplement the algorithm ourselves (or any other algorithm)
  // so the generated code is portable between compilers and standard library
  // implementations.
  std::linear_congruential_engine<std::uint64_t,
                                  base_engine::multiplier,
                                  base_engine::increment,
                                  base_engine::modulus>
    alu_state;
};
}
