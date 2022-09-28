#pragma once

#include <string>
#include <memory>
#include <unordered_map>

template<typename T>
class Asset_Manager {
private:
    std::unordered_map<std::string, std::shared_ptr<T>> assets;

public:
    const T &get(const std::string &name) {
        if (assets.find(name) == assets.end()) {
            assets[name] = std::make_shared<T>();

            try {
                assets[name]->load(name);
            }
            catch (std::exception e) {
                throw e;
            }
        }

        return *assets[name];
    }

    bool exists(const std::string &name) const {
        return assets.find(name) != assets.end();
    }

    void remove(const std::string &name) {
        assert(exists(name));

        assets.erase(assets.find(name));
    }
};
