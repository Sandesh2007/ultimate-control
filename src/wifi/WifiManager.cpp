#include "WifiManager.hpp"
#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <memory>
#include <array>
#include <algorithm>
#include <ctime>

namespace Wifi {

class WifiManager::Impl {
public:
    Impl() : wifi_enabled_(check_wifi_enabled()) {}
    ~Impl() = default;

    void scan_networks() {
        if (!wifi_enabled_) {
            networks_.clear();
            if (update_callback_) {
                update_callback_(networks_);
            }
            return;
        }

        networks_.clear();

        std::string cmd = "nmcli -t -f IN-USE,SSID,SIGNAL,SECURITY device wifi list";
        std::array<char, 4096> buffer;
        std::string result;

        FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe) {
            std::cerr << "Failed to run nmcli\n";
            return;
        }
        while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
            result += buffer.data();
        }
        pclose(pipe);

        size_t pos = 0;
        while ((pos = result.find('\n')) != std::string::npos) {
            std::string line = result.substr(0, pos);
            result.erase(0, pos + 1);

            auto tokens = split(line, ':');
            if (tokens.size() >= 4) {
                Network net;
                net.connected = (tokens[0] == "*");
                net.ssid = tokens[1];
                net.bssid = ""; // We don't have BSSID in this command
                try {
                    // Parse the signal strength from nmcli
                    int signal = std::stoi(tokens[2]);

                    // Store the signal strength
                    net.signal_strength = signal;
                } catch (...) {
                    net.signal_strength = 0;
                }
                net.secured = (tokens[3] != "--" && !tokens[3].empty());
                networks_.push_back(net);
            }
        }

        if (update_callback_) {
            update_callback_(networks_);
        }
    }

    void connect(const std::string& ssid, const std::string& password, const std::string& security_type) {
        // Check if we're already connected to this network
        bool already_connected = false;
        for (const auto& net : networks_) {
            if (net.ssid == ssid && net.connected) {
                already_connected = true;
                break;
            }
        }

        if (already_connected) {
            std::cout << "Already connected to " << ssid << std::endl;
            return;
        }

        std::cout << "Connecting to WiFi network: " << ssid << "..." << std::endl;

        // For secured networks, directly create a proper connection profile
        if (!password.empty() && !security_type.empty()) {
            // Get the WiFi interface name
            std::string wifi_interface = get_wifi_interface();
            if (wifi_interface.empty()) {
                std::cerr << "Error: No WiFi interface found" << std::endl;
                return;
            }

            // Create a connection name based on SSID
            std::string conn_name = ssid;

            // First, delete any existing connection with the same name to avoid conflicts
            std::string delete_cmd = "nmcli con delete \"" + conn_name + "\" 2>/dev/null || true";
            std::system(delete_cmd.c_str());

            // Create a connection profile with the correct security settings
            std::string create_cmd = "nmcli con add type wifi con-name \"" + conn_name + "\" ifname " + wifi_interface +
                                    " ssid \"" + ssid + "\" && " +
                                    "nmcli con modify \"" + conn_name + "\" wifi-sec.key-mgmt " + security_type + " && " +
                                    "nmcli con modify \"" + conn_name + "\" wifi-sec.psk \"" + password + "\" && " +
                                    "nmcli con up \"" + conn_name + "\"";

            int result = std::system(create_cmd.c_str());

            if (result == 0) {
                std::cout << "Successfully connected to " << ssid << std::endl;
            } else {
                std::cerr << "Failed to connect to " << ssid << std::endl;
            }
        } else {
            // For open networks, use the simple approach
            std::string cmd = "nmcli dev wifi connect \"" + ssid + "\"";
            if (!password.empty()) {
                cmd += " password \"" + password + "\"";
            }

            int result = std::system(cmd.c_str());

            if (result == 0) {
                std::cout << "Successfully connected to " << ssid << std::endl;
            } else {
                std::cerr << "Failed to connect to " << ssid << std::endl;
            }
        }

        scan_networks();
    }

    void disconnect() {
        // Get the current active connection
        std::string wifi_interface = get_wifi_interface();
        if (wifi_interface.empty()) {
            std::cerr << "Error: No WiFi interface found" << std::endl;
            return;
        }

        // Disconnect the current WiFi connection
        std::string cmd = "nmcli device disconnect " + wifi_interface;
        std::cout << "Disconnecting from WiFi..." << std::endl;
        std::system(cmd.c_str());

        // Scan networks after disconnecting
        scan_networks();
    }

    void forget_network(const std::string& ssid) {
        std::cout << "Forgetting network: " << ssid << std::endl;

        // Delete the connection with the same name as the SSID
        std::string delete_cmd = "nmcli connection delete \"" + ssid + "\" 2>/dev/null || true";
        std::system(delete_cmd.c_str());

        // Also clean up any temp connections that might have been created
        std::string cleanup_cmd = "nmcli -t -f NAME connection show | grep \"temp-conn-\" | xargs -r -n1 nmcli connection delete 2>/dev/null || true";
        std::system(cleanup_cmd.c_str());

        std::cout << "Network forgotten: " << ssid << std::endl;
        scan_networks();
    }

    void enable_wifi() {
        std::string cmd = "nmcli radio wifi on";
        int ret = std::system(cmd.c_str());
        if (ret == 0) {
            wifi_enabled_ = true;
            if (state_callback_) {
                state_callback_(wifi_enabled_);
            }
            // Scan networks after enabling WiFi
            scan_networks();
        }
    }

    void disable_wifi() {
        std::string cmd = "nmcli radio wifi off";
        int ret = std::system(cmd.c_str());
        if (ret == 0) {
            wifi_enabled_ = false;
            if (state_callback_) {
                state_callback_(wifi_enabled_);
            }
            // Clear networks after disabling WiFi
            networks_.clear();
            if (update_callback_) {
                update_callback_(networks_);
            }
        }
    }

    bool is_wifi_enabled() const {
        return wifi_enabled_;
    }

    bool check_wifi_enabled() {
        std::string cmd = "nmcli radio wifi";
        std::array<char, 128> buffer;
        std::string result;

        FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe) return false;
        if (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
            result = buffer.data();
        }
        pclose(pipe);

        // Trim whitespace
        result.erase(0, result.find_first_not_of(" \t\n\r"));
        result.erase(result.find_last_not_of(" \t\n\r") + 1);

        return result == "enabled";
    }

    void set_update_callback(WifiManager::UpdateCallback cb) {
        update_callback_ = cb;
    }

    void set_state_callback(WifiManager::StateCallback cb) {
        state_callback_ = cb;
    }

private:
    std::vector<Network> networks_;
    WifiManager::UpdateCallback update_callback_;
    WifiManager::StateCallback state_callback_;
    bool wifi_enabled_;

    std::string get_wifi_interface() {
        std::string cmd = "nmcli device status | grep wifi | grep -v p2p | awk '{print $1}'";
        std::array<char, 128> buffer;
        std::string result;

        FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe) return "";
        if (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
            result = buffer.data();
        }
        pclose(pipe);

        // Trim whitespace
        result.erase(0, result.find_first_not_of(" \t\n\r"));
        result.erase(result.find_last_not_of(" \t\n\r") + 1);

        return result;
    }

    static std::vector<std::string> split(const std::string& s, char delimiter) {
        std::vector<std::string> tokens;
        std::string token;
        for (char c : s) {
            if (c == delimiter) {
                tokens.push_back(token);
                token.clear();
            } else {
                token += c;
            }
        }
        tokens.push_back(token);
        return tokens;
    }
};

WifiManager::WifiManager() : impl_(std::make_unique<Impl>()) {}

WifiManager::~WifiManager() = default;

void WifiManager::scan_networks() {
    impl_->scan_networks();
}

void WifiManager::connect(const std::string& ssid, const std::string& password, const std::string& security_type) {
    impl_->connect(ssid, password, security_type);
}

void WifiManager::disconnect() {
    impl_->disconnect();
}

void WifiManager::forget_network(const std::string& ssid) {
    impl_->forget_network(ssid);
}

void WifiManager::enable_wifi() {
    impl_->enable_wifi();
}

void WifiManager::disable_wifi() {
    impl_->disable_wifi();
}

bool WifiManager::is_wifi_enabled() const {
    return impl_->is_wifi_enabled();
}

void WifiManager::set_update_callback(UpdateCallback cb) {
    impl_->set_update_callback(cb);
}

void WifiManager::set_state_callback(StateCallback cb) {
    impl_->set_state_callback(cb);
}

} // namespace Wifi
