#pragma once

#include <cstdio>
#include <utility>
#include <vector>
#include <optional>

template<typename T>
struct Range {
  T lower; 
  T upper;

  struct Iterator {
    T value;

    T operator*() const {
      return value;
    }

    Iterator operator++() {
      value += 1;
      return *this;
    }

    bool operator!=(Iterator b) {
      return value != b.value;
    }
  };

  Range(T upper)
    : lower(0), upper(upper)
  {
  }

  Range(T lower, T upper)
    : lower(lower), upper(upper)
  {
  }

  Iterator begin() const { return Iterator { lower }; }
  Iterator end() const { return Iterator { upper }; }
};

std::optional<std::vector<uint8_t>> load_binary(const char* path);