#include "for_each_subset.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <vector>

using testing::UnorderedElementsAre;

namespace cxxtrace_test {
TEST(test_for_each_subset, empty_input)
{
  using v = std::vector<int>;
  auto subsets = std::vector<v>{};
  for_each_subset(
    v{}, [&subsets](const v& subset) -> void { subsets.emplace_back(subset); });
  EXPECT_THAT(subsets, UnorderedElementsAre(v{}));
}

TEST(test_for_each_subset, single_item)
{
  using v = std::vector<int>;
  auto subsets = std::vector<v>{};
  for_each_subset(v{ 42 }, [&subsets](const v& subset) -> void {
    subsets.emplace_back(subset);
  });
  EXPECT_THAT(subsets, UnorderedElementsAre(v{}, v{ 42 }));
}

TEST(test_for_each_subset, two_items)
{
  using v = std::vector<int>;
  auto subsets = std::vector<v>{};
  for_each_subset(v{ 42, 9000 }, [&subsets](const v& subset) -> void {
    subsets.emplace_back(subset);
  });
  EXPECT_THAT(subsets,
              UnorderedElementsAre(v{}, v{ 42 }, v{ 9000 }, v{ 42, 9000 }));
}

TEST(test_for_each_subset, three_items)
{
  using v = std::vector<int>;
  auto subsets = std::vector<v>{};
  for_each_subset(v{ 42, 9000, -1 }, [&subsets](const v& subset) -> void {
    subsets.emplace_back(subset);
  });
  EXPECT_THAT(subsets,
              UnorderedElementsAre(v{},
                                   v{ 42 },
                                   v{ 9000 },
                                   v{ -1 },
                                   v{ 42, 9000 },
                                   v{ 42, -1 },
                                   v{ 9000, -1 },
                                   v{ 42, 9000, -1 }));
}
}
