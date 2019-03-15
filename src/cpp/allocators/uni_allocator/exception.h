#pragma once

#include <string>

class DebugAssertionFail : public std::exception {
  virtual const char* what() const throw() {
    // TODO: add stack trace here.
    return "Debug mode assertion failed";
  }
};

class MemoryCorruptedException : public std::exception {
  virtual const char* what() const throw() {
    return "Memory Corrupted Exception";
  }
};
