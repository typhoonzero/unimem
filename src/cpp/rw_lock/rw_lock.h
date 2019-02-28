/* Copyright (c) 2018 PaddlePaddle Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License. */

#pragma once

#include <pthread.h>
#include <string>
#include <exception>

namespace unimem {

class RWLockException: public std::exception {
 public:
  RWLockException(const std::string& msg) : msg_(msg) {}

  virtual const char* what() const throw() {
    return msg_.c_str();
  }
 private:
  std::string msg_;
};


struct RWLock {
  RWLock() { pthread_rwlock_init(&lock_, nullptr); }

  ~RWLock() { pthread_rwlock_destroy(&lock_); }

  inline void RDLock() {
    if (pthread_rwlock_rdlock(&lock_) != 0) {
      throw RWLockException("aquire readlock failed.");
    }
                      
  }

  inline void WRLock() {
    if (pthread_rwlock_wrlock(&lock_) != 0) {
      throw RWLockException("aquire writelock failed.");
    }
  }

  inline void UNLock() {
    if (pthread_rwlock_unlock(&lock_) != 0) {
      throw RWLockException("unlock failed.");
    }
  }

 private:
  pthread_rwlock_t lock_;
};

class AutoWRLock {
 public:
  explicit AutoWRLock(RWLock* rw_lock) : lock_(rw_lock) { Lock(); }

  ~AutoWRLock() { UnLock(); }

 private:
  inline void Lock() { lock_->WRLock(); }

  inline void UnLock() { lock_->UNLock(); }

 private:
  RWLock* lock_;
};

class AutoRDLock {
 public:
  explicit AutoRDLock(RWLock* rw_lock) : lock_(rw_lock) { Lock(); }

  ~AutoRDLock() { UnLock(); }

 private:
  inline void Lock() { lock_->RDLock(); }

  inline void UnLock() { lock_->UNLock(); }

 private:
  RWLock* lock_;
};

}  // namespace unimem
