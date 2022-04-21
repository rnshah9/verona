// Copyright Microsoft and Project Verona Contributors.
// SPDX-License-Identifier: MIT
#include "args.h"

#include <cpp/when.h>
#include <memory>
#include <test/harness.h>

struct Fork
{
  size_t uses{0};

  void use()
  {
    ++uses;
  }

  ~Fork()
  {
    // Contains the uses == 0 case, due to copies during
    // setup needing to be destroyed.
    // Should look to make example copy free.
    check(((HUNGER * 2) == uses) || (uses == 0));
  }
};

cown_ptr<Fork> get_left(std::vector<cown_ptr<Fork>>& forks, size_t index)
{
  return forks[index];
}

cown_ptr<Fork> get_right(std::vector<cown_ptr<Fork>>& forks, size_t index)
{
  // This code handles tables by splitting the index into the table and then
  // the philosopher index.  It wraps the fork around for the last philosoher
  // on each table.
  size_t table_size = NUM_PHILOSOPHERS / NUM_TABLES;
  size_t table = index / table_size;
  size_t next_fork = (index + 1) % table_size;
  return forks[(table * table_size) + next_fork];
}

struct Philosopher
{
  cown_ptr<Fork> left;
  cown_ptr<Fork> right;
  size_t hunger;

  Philosopher(std::vector<cown_ptr<Fork>>& forks, size_t index)
  : left(get_left(forks, index)), right(get_right(forks, index)), hunger(HUNGER)
  {}

  static void eat(std::unique_ptr<Philosopher> phil)
  {
    if (phil->hunger > 0)
    {
      when(phil->left, phil->right)
        << [phil = std::move(phil)](
             acquired_cown<Fork> f1, acquired_cown<Fork> f2) mutable {
             f1->use();
             f2->use();
             busy_loop(WORK_USEC);
             phil->hunger--;
             eat(std::move(phil));
           };
    }
  }
};

void test_body()
{
  std::vector<cown_ptr<Fork>> forks;

  for (size_t i = 0; i < NUM_PHILOSOPHERS; i++)
  {
    forks.push_back(make_cown<Fork>({}));
  }

  if (OPTIMAL_ORDER)
  {
    for (size_t i = 0; i < NUM_PHILOSOPHERS; i += 2)
    {
      auto phil = std::make_unique<Philosopher>(forks, i);
      Philosopher::eat(std::move(phil));
    }
    for (size_t i = 1; i < NUM_PHILOSOPHERS; i += 2)
    {
      auto phil = std::make_unique<Philosopher>(forks, i);
      Philosopher::eat(std::move(phil));
    }
  }
  else
  {
    for (size_t i = 0; i < NUM_PHILOSOPHERS; i++)
    {
      auto phil = std::make_unique<Philosopher>(forks, i);
      Philosopher::eat(std::move(phil));
    }
  }
}

void test1()
{
  when() << test_body;
}

int verona_main(SystematicTestHarness& harness)
{
  harness.run(test1);

  return 0;
}