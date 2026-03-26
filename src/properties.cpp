
#include "properties.h"

#include <fstream>
#include <iostream>

namespace trace {

std::string trim(const std::string& value) {
  const std::string whitespace = " \t\n\r";
  const auto first = value.find_first_not_of(whitespace);
  if (first == std::string::npos) {
    return "";
  }

  const auto last = value.find_last_not_of(whitespace);
  return value.substr(first, last - first + 1);
}

PropertyMap loadProperties(const std::string& path) {
  std::ifstream input(path);
  PropertyMap properties;

  if (!input.is_open()) {
    std::cerr << "[WARN] config file not found: " << path << ", using defaults" << std::endl;
    return properties;
  }

  std::string line;
  while (std::getline(input, line)) {
    const std::string cleaned = trim(line);
    if (cleaned.empty()) {
      continue;
    }
    if (cleaned[0] == '#' || cleaned[0] == ';') {
      continue;
    }

    const auto delimiter = cleaned.find('=');
    if (delimiter == std::string::npos) {
      continue;
    }

    const std::string key = trim(cleaned.substr(0, delimiter));
    const std::string value = trim(cleaned.substr(delimiter + 1));
    if (!key.empty()) {
      properties[key] = value;
    }
  }

  return properties;
}

std::string getProperty(
    const PropertyMap& properties,
    const std::string& key,
    const std::string& default_value) {
  const auto iterator = properties.find(key);
  if (iterator == properties.end()) {
    return default_value;
  }
  return iterator->second;
}

}  // namespace trace
  