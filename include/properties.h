
#pragma once

#include <string>
#include <unordered_map>

namespace trace {

using PropertyMap = std::unordered_map<std::string, std::string>;

PropertyMap loadProperties(const std::string& path);
std::string trim(const std::string& value);
std::string getProperty(
    const PropertyMap& properties,
    const std::string& key,
    const std::string& default_value);

}  // namespace trace
  