#include "rw_lock.h"

#include <gtest/gtest.h>
#include <chrono>  // NOLINT
#include <thread>  // NOLINT
#include <vector>
#include <cstring>

namespace f = unimem;

void f1(f::RWLock *lock) {
  lock->RDLock();
  lock->UNLock();
}

TEST(RWLOCK, read_read) {
  f::RWLock lock;
  lock.RDLock();
  std::thread t1(f1, &lock);
  std::thread t2(f1, &lock);
  t1.join();
  t2.join();
  lock.UNLock();
}

void f2(f::RWLock *lock, std::vector<int> *result) {
  lock->RDLock();
  ASSERT_EQ(result->size(), 0UL);
  lock->UNLock();
}

void f3(f::RWLock *lock, std::vector<int> *result) {
  lock->WRLock();
  result->push_back(1);
  lock->UNLock();
}

TEST(RWLOCK, read_write) {
  f::RWLock lock;
  std::vector<int> result;

  lock.RDLock();
  std::thread t1(f2, &lock, &result);
  t1.join();
  std::thread t2(f3, &lock, &result);
  std::this_thread::sleep_for(std::chrono::seconds(1));
  ASSERT_EQ(result.size(), 0UL);
  lock.UNLock();
  t2.join();
  ASSERT_EQ(result.size(), 1UL);
}

void f4(f::RWLock *lock, std::vector<int> *result) {
  lock->RDLock();
  ASSERT_EQ(result->size(), 1UL);
  lock->UNLock();
}

TEST(RWLOCK, write_read) {
  f::RWLock lock;
  std::vector<int> result;

  lock.WRLock();
  std::thread t1(f4, &lock, &result);
  std::this_thread::sleep_for(std::chrono::seconds(1));
  result.push_back(1);
  lock.UNLock();
  t1.join();
}



int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
