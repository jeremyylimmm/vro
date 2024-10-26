#include "base.h"

std::optional<std::vector<uint8_t>> load_binary(const char* path) {
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