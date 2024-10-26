export module base;

import <cstdio>;
import <utility>;
import <vector>;
import <optional>;

export template<typename T>
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

export std::optional<std::vector<uint8_t>> load_binary(const char* path) {
  FILE* file;
  if (fopen_s(&file, path, "rb")) {
    return std::nullopt;
  }

  fseek(file, 0, SEEK_END);
  size_t len = ftell(file);
  rewind(file);

  std::vector<uint8_t> buf(len);
  fread(buf.data(), len, 1, file);

  fclose(file);

  return buf;
}