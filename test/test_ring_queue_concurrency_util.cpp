#include "test_ring_queue_concurrency_util.h"
#include <array>
#include <cxxtrace/detail/mpsc_ring_queue.h>
#include <experimental/memory_resource>
#include <experimental/vector>
#include <gmock/gmock.h>

using testing::UnorderedElementsAre;

namespace cxxtrace_test {
class test_queue_push_outcomes : public testing::Test
{
protected:
  template<class T>
  using vector = std::experimental::pmr::vector<T>;
  using push_result = queue_push_operations<4>::push_result;

  static inline std::experimental::pmr::memory_resource* memory =
    std::experimental::pmr::new_delete_resource();

  template<auto Capacity>
  static auto possible_queue_push_outcomes(
    const queue_push_operations<Capacity>& results) -> auto
  {
    return results.possible_outcomes(memory);
  }
};

TEST_F(test_queue_push_outcomes, two_successful_pushes_push_in_any_order)
{
  auto operations = queue_push_operations<4>{
    /*initial_push_size=*/0,
    /*producer_push_sizes=*/{ 1, 1 },
    /*producer_push_results=*/{ push_result::pushed, push_result::pushed },
  };
  auto outcomes = possible_queue_push_outcomes(operations);
  auto producer_1 = 100;
  auto producer_2 = 101;
  EXPECT_THAT(outcomes,
              UnorderedElementsAre(vector<int>{ producer_1, producer_2 },
                                   vector<int>{ producer_2, producer_1 }));
}

TEST_F(test_queue_push_outcomes, successful_pushes_come_after_initial_push)
{
  auto operations = queue_push_operations<4>{
    /*initial_push_size=*/2,
    /*producer_push_sizes=*/{ 1, 1 },
    /*producer_push_results=*/{ push_result::pushed, push_result::pushed },
  };
  auto outcomes = possible_queue_push_outcomes(operations);
  auto initial_a = 100;
  auto initial_b = 101;
  auto producer_1 = 102;
  auto producer_2 = 103;
  EXPECT_THAT(outcomes,
              UnorderedElementsAre(
                vector<int>{ initial_a, initial_b, producer_1, producer_2 },
                vector<int>{ initial_a, initial_b, producer_2, producer_1 }));
}

TEST_F(test_queue_push_outcomes, three_successful_pushes_push_in_any_order)
{
  auto operations = queue_push_operations<4>{
    /*initial_push_size=*/0,
    /*producer_push_sizes=*/{ 1, 1, 1 },
    /*producer_push_results=*/
    { push_result::pushed, push_result::pushed, push_result::pushed }
  };
  auto outcomes = possible_queue_push_outcomes(operations);
  auto producer_1 = 100;
  auto producer_2 = 101;
  auto producer_3 = 102;
  EXPECT_THAT(
    outcomes,
    UnorderedElementsAre(vector<int>{ producer_1, producer_2, producer_3 },
                         vector<int>{ producer_1, producer_3, producer_2 },
                         vector<int>{ producer_2, producer_1, producer_3 },
                         vector<int>{ producer_2, producer_3, producer_1 },
                         vector<int>{ producer_3, producer_1, producer_2 },
                         vector<int>{ producer_3, producer_2, producer_1 }));
}

TEST_F(test_queue_push_outcomes, two_successful_pushes_are_atomic)
{
  auto operations = queue_push_operations<4>{
    /*initial_push_size=*/0,
    /*producer_push_sizes=*/{ 2, 2 },
    /*producer_push_results=*/{ push_result::pushed, push_result::pushed }
  };
  auto outcomes = possible_queue_push_outcomes(operations);
  auto producer_1_a = 100;
  auto producer_1_b = 101;
  auto producer_2_a = 102;
  auto producer_2_b = 103;
  EXPECT_THAT(
    outcomes,
    UnorderedElementsAre(
      // Producer 1, then producer 2.
      vector<int>{ producer_1_a, producer_1_b, producer_2_a, producer_2_b },
      // Producer 2, then producer 1.
      vector<int>{ producer_2_a, producer_2_b, producer_1_a, producer_1_b }));
}

TEST_F(test_queue_push_outcomes, failed_pushes_leave_initial_push_unchanged)
{
  auto initial_push_size = 2;
  auto producer_1_push_size = 1;
  auto producer_2_push_size = 1;

  auto initial_a = 100;
  auto initial_b = 101;
  auto producer_1 = 102;
  auto producer_2 = 103;

  {
    auto operations = queue_push_operations<4>{
      /*initial_push_size=*/initial_push_size,
      /*producer_push_sizes=*/{ producer_1_push_size, producer_2_push_size },
      /*producer_push_results=*/
      { push_result::skipped, push_result::skipped },
    };
    auto outcomes = possible_queue_push_outcomes(operations);
    EXPECT_THAT(outcomes,
                UnorderedElementsAre(vector<int>{ initial_a, initial_b }));
  }

  {
    auto operations = queue_push_operations<4>{
      /*initial_push_size=*/initial_push_size,
      /*producer_push_sizes=*/{ producer_1_push_size, producer_2_push_size },
      /*producer_push_results=*/
      { push_result::skipped, push_result::pushed },
    };
    auto outcomes = possible_queue_push_outcomes(operations);
    EXPECT_THAT(
      outcomes,
      UnorderedElementsAre(vector<int>{ initial_a, initial_b, producer_2 }));
  }

  {
    auto operations = queue_push_operations<4>{
      /*initial_push_size=*/initial_push_size,
      /*producer_push_sizes=*/{ producer_1_push_size, producer_2_push_size },
      /*producer_push_results=*/
      { push_result::pushed, push_result::skipped },
    };
    auto outcomes = possible_queue_push_outcomes(operations);
    EXPECT_THAT(
      outcomes,
      UnorderedElementsAre(vector<int>{ initial_a, initial_b, producer_1 }));
  }
}

TEST_F(test_queue_push_outcomes,
       interrupted_pushes_leave_initial_push_unchanged)
{
  auto initial_push_size = 2;
  auto producer_1_push_size = 1;
  auto producer_2_push_size = 1;

  auto initial_a = 100;
  auto initial_b = 101;
  auto producer_1 = 102;
  auto producer_2 = 103;

  {
    auto operations = queue_push_operations<4>{
      /*initial_push_size=*/initial_push_size,
      /*producer_push_sizes=*/{ producer_1_push_size, producer_2_push_size },
      /*producer_push_results=*/
      { push_result::interrupted, push_result::interrupted },
    };
    auto outcomes = possible_queue_push_outcomes(operations);
    EXPECT_THAT(outcomes,
                UnorderedElementsAre(vector<int>{ initial_a, initial_b }));
  }

  {
    auto operations = queue_push_operations<4>{
      /*initial_push_size=*/initial_push_size,
      /*producer_push_sizes=*/{ producer_1_push_size, producer_2_push_size },
      /*producer_push_results=*/
      { push_result::interrupted, push_result::pushed },
    };
    auto outcomes = possible_queue_push_outcomes(operations);
    EXPECT_THAT(
      outcomes,
      UnorderedElementsAre(vector<int>{ initial_a, initial_b, producer_2 }));
  }

  {
    auto operations = queue_push_operations<4>{
      /*initial_push_size=*/initial_push_size,
      /*producer_push_sizes=*/{ producer_1_push_size, producer_2_push_size },
      /*producer_push_results=*/
      { push_result::pushed, push_result::interrupted },
    };
    auto outcomes = possible_queue_push_outcomes(operations);
    EXPECT_THAT(
      outcomes,
      UnorderedElementsAre(vector<int>{ initial_a, initial_b, producer_1 }));
  }
}

TEST_F(test_queue_push_outcomes, independently_overflowing_pushes_overflow)
{
  auto operations = queue_push_operations<4>{
    /*initial_push_size=*/3,
    /*producer_push_sizes=*/{ 2, 2 },
    /*producer_push_results=*/{ push_result::pushed, push_result::pushed },
  };
  auto outcomes = possible_queue_push_outcomes(operations);
  auto producer_1_a = 103;
  auto producer_1_b = 104;
  auto producer_2_a = 105;
  auto producer_2_b = 106;
  EXPECT_THAT(
    outcomes,
    UnorderedElementsAre(
      // Producer 1, then producer 2.
      vector<int>{ producer_1_a, producer_1_b, producer_2_a, producer_2_b },
      // Producer 2, then producer 1.
      vector<int>{ producer_2_a, producer_2_b, producer_1_a, producer_1_b }));
}

TEST_F(test_queue_push_outcomes, mutually_overflowing_pushes_overflow)
{
  auto operations = queue_push_operations<4>{
    /*initial_push_size=*/0,
    /*producer_push_sizes=*/{ 3, 3 },
    /*producer_push_results=*/{ push_result::pushed, push_result::pushed },
  };
  auto outcomes = possible_queue_push_outcomes(operations);
  auto producer_1_a = 100;
  auto producer_1_b = 101;
  auto producer_1_c = 102;
  auto producer_2_a = 103;
  auto producer_2_b = 104;
  auto producer_2_c = 105;
  EXPECT_THAT(
    outcomes,
    UnorderedElementsAre(
      // Producer 1, then producer 2.
      vector<int>{ producer_1_c, producer_2_a, producer_2_b, producer_2_c },
      // Producer 2, then producer 1.
      vector<int>{ producer_2_c, producer_1_a, producer_1_b, producer_1_c }));
}

TEST_F(test_queue_push_outcomes,
       failed_mutually_overflowing_pushes_preserve_pushed_items)
{
  auto initial_push_size = 0;
  auto producer_1_push_size = 3;
  auto producer_2_push_size = 3;

  auto producer_1_a = 100;
  auto producer_1_b = 101;
  auto producer_1_c = 102;
  auto producer_2_a = 103;
  auto producer_2_b = 104;
  auto producer_2_c = 105;

  {
    auto operations = queue_push_operations<4>{
      /*initial_push_size=*/initial_push_size,
      /*producer_push_sizes=*/{ producer_1_push_size, producer_2_push_size },
      /*producer_push_results=*/
      { push_result::pushed, push_result::skipped },
    };
    auto outcomes = possible_queue_push_outcomes(operations);
    EXPECT_THAT(outcomes,
                UnorderedElementsAre(
                  vector<int>{ producer_1_a, producer_1_b, producer_1_c }));
  }

  {
    auto operations = queue_push_operations<4>{
      /*initial_push_size=*/initial_push_size,
      /*producer_push_sizes=*/{ producer_1_push_size, producer_2_push_size },
      /*producer_push_results=*/
      { push_result::skipped, push_result::pushed },
    };
    auto outcomes = possible_queue_push_outcomes(operations);
    EXPECT_THAT(outcomes,
                UnorderedElementsAre(
                  vector<int>{ producer_2_a, producer_2_b, producer_2_c }));
  }
}

TEST_F(test_queue_push_outcomes,
       interrupted_mutually_overflowing_pushes_may_overwrite_pushed_items)
{
  auto initial_push_size = 0;
  auto producer_1_push_size = 3;
  auto producer_2_push_size = 3;

  auto producer_1_a = 100;
  auto producer_1_b = 101;
  auto producer_1_c = 102;
  auto producer_2_a = 103;
  auto producer_2_b = 104;
  auto producer_2_c = 105;

  {
    auto operations = queue_push_operations<4>{
      /*initial_push_size=*/initial_push_size,
      /*producer_push_sizes=*/{ producer_1_push_size, producer_2_push_size },
      /*producer_push_results=*/
      { push_result::pushed, push_result::interrupted },
    };
    auto outcomes = possible_queue_push_outcomes(operations);
    EXPECT_THAT(outcomes,
                UnorderedElementsAre(
                  // Either:
                  // * Producer 1 only. Producer 2's push was interrupted early
                  //   (either before or after producer 1).
                  // * Producer 2 (interrupted; overwrote), then producer 1.
                  vector<int>{ producer_1_a, producer_1_b, producer_1_c },
                  // Producer 1, then producer 2 (interrupted; overwrote).
                  vector<int>{ producer_1_c }));
  }

  {
    auto operations = queue_push_operations<4>{
      /*initial_push_size=*/initial_push_size,
      /*producer_push_sizes=*/{ producer_1_push_size, producer_2_push_size },
      /*producer_push_results=*/
      { push_result::interrupted, push_result::pushed },
    };
    auto outcomes = possible_queue_push_outcomes(operations);
    EXPECT_THAT(outcomes,
                UnorderedElementsAre(
                  // Either:
                  // * Producer 1 only. Producer 2's push was interrupted early
                  //   (either before or after producer 1).
                  // * Producer 2 (interrupted; overwrote), then producer 1.
                  vector<int>{ producer_2_a, producer_2_b, producer_2_c },
                  // Producer 1, then producer 2 (interrupted; overwrote).
                  vector<int>{ producer_2_c }));
  }
}

TEST_F(test_queue_push_outcomes,
       failed_overflowing_push_leaves_initial_push_unchanged)
{
  auto operations = queue_push_operations<4>{
    /*initial_push_size=*/2,
    /*producer_push_sizes=*/{ 1, 3 },
    /*producer_push_results=*/
    { push_result::pushed, push_result::skipped },
  };
  auto outcomes = possible_queue_push_outcomes(operations);
  auto initial_a = 100;
  auto initial_b = 101;
  auto producer_1 = 102;
  EXPECT_THAT(
    outcomes,
    UnorderedElementsAre(vector<int>{ initial_a, initial_b, producer_1 }));
}

TEST_F(test_queue_push_outcomes,
       interrupted_overflowing_push_possibly_overwrites_initial_push)
{
  {
    auto operations = queue_push_operations<4>{
      /*initial_push_size=*/2,
      /*producer_push_sizes=*/{ 1, 3 },
      /*producer_push_results=*/
      { push_result::pushed, push_result::interrupted },
    };
    auto outcomes = possible_queue_push_outcomes(operations);
    auto initial_a = 100;
    auto initial_b = 101;
    auto producer_1 = 102;
    EXPECT_THAT(outcomes,
                UnorderedElementsAre(
                  // Producer 1 only. Producer 2's push was interrupted early
                  // (either before or after producer 1).
                  vector<int>{ initial_a, initial_b, producer_1 },
                  // Producer 1, then producer 2 (interrupted; overwrote).
                  vector<int>{ producer_1 },
                  // Producer 2 (interrupted; overwrote), then producer 1.
                  vector<int>{ initial_b, producer_1 }));
  }

  {
    auto operations = queue_push_operations<4>{
      /*initial_push_size=*/2,
      /*producer_push_sizes=*/{ 1, 2 },
      /*producer_push_results=*/
      { push_result::pushed, push_result::interrupted },
    };
    auto outcomes = possible_queue_push_outcomes(operations);
    auto initial_a = 100;
    auto initial_b = 101;
    auto producer_1 = 102;
    EXPECT_THAT(outcomes,
                UnorderedElementsAre(
                  // Producer 1 only. Producer 2's push was interrupted early
                  // (either before or after producer 1).
                  vector<int>{ initial_a, initial_b, producer_1 },
                  // Either:
                  // * Producer 1, then producer 2 (interrupted; overwrote).
                  // * Producer 2 (interrupted; overwrote), then producer 1.
                  vector<int>{ initial_b, producer_1 }));
  }
}
}
