#include <librdkafka/rdkafka.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::vector<std::string> split_features(const std::string &value) {
    std::vector<std::string> items;
    std::stringstream ss(value);
    std::string item;

    while (std::getline(ss, item, ',')) {
        item.erase(
            std::remove_if(item.begin(), item.end(), [](unsigned char ch) {
                return std::isspace(ch) != 0;
            }),
            item.end());
        if (!item.empty()) {
            items.push_back(item);
        }
    }

    return items;
}

bool has_feature(const std::vector<std::string> &features, const std::string &required) {
    return std::find(features.begin(), features.end(), required) != features.end();
}

void print_usage(const char *argv0) {
    std::cout
        << "Usage: " << argv0 << " [--require feature1,feature2,...]\n"
        << "\n"
        << "Examples:\n"
        << "  " << argv0 << "\n"
        << "  " << argv0 << " --require ssl,sasl\n";
}

}  // namespace

int main(int argc, char **argv) {
    std::vector<std::string> required_features;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            return 0;
        }

        if (arg == "--require") {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for --require\n";
                print_usage(argv[0]);
                return 2;
            }
            required_features = split_features(argv[++i]);
            continue;
        }

        std::cerr << "Unknown argument: " << arg << "\n";
        print_usage(argv[0]);
        return 2;
    }

    std::cout << "=== librdkafka Probe ===\n";
    std::cout << "librdkafka version: " << rd_kafka_version_str()
              << " (" << rd_kafka_version() << ")\n";

    rd_kafka_conf_t *conf = rd_kafka_conf_new();
    if (!conf) {
        std::cerr << "Failed to create rd_kafka_conf\n";
        return 3;
    }

    char features_buffer[4096];
    std::size_t features_size = sizeof(features_buffer);
    const auto get_result = rd_kafka_conf_get(
        conf, "builtin.features", features_buffer, &features_size);

    std::string builtin_features;
    if (get_result == RD_KAFKA_CONF_OK) {
        builtin_features = features_buffer;
    } else {
        builtin_features = "<unavailable>";
    }

    std::cout << "builtin.features: " << builtin_features << "\n";

    char errstr[512];
    rd_kafka_t *rk = rd_kafka_new(RD_KAFKA_PRODUCER, conf, errstr, sizeof(errstr));
    if (!rk) {
        std::cerr << "rd_kafka_new failed: " << errstr << "\n";
        return 4;
    }

    std::cout << "rd_kafka_new: ok\n";

    const auto available_features = split_features(builtin_features);
    bool missing_required = false;
    for (const auto &feature : required_features) {
        const bool present = has_feature(available_features, feature);
        std::cout << "require[" << feature << "]: " << (present ? "ok" : "missing") << "\n";
        if (!present) {
            missing_required = true;
        }
    }

    rd_kafka_destroy(rk);

    if (missing_required) {
        return 5;
    }

    std::cout << "probe result: ok\n";
    return 0;
}
