#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstdlib>
#include <array>
#include <sys/ioctl.h>
#include <unistd.h>
#include <unordered_map>
#include <sstream>
#include <iomanip>

std::unordered_map<std::string, std::string> read_config(const std::string& filename) {
    std::unordered_map<std::string, std::string> config;
    std::ifstream file(filename);
    if (!file.is_open()) {
        return config;
    }

    std::string line;
    while (std::getline(file, line)) {
        auto key_start = line.find('"');
        if (key_start == std::string::npos) continue;
        auto key_end = line.find('"', key_start + 1);
        auto colon = line.find(':', key_end);
        auto value_start = line.find('"', colon);
        auto value_end = line.find('"', value_start + 1);

        std::string key = line.substr(key_start + 1, key_end - key_start - 1);
        std::string value = line.substr(value_start + 1, value_end - value_start - 1);
        config[key] = value;
    }
    return config;
}

std::string hex_to_ansi(const std::string& hex) {
    if (hex.size() != 8 || hex[0] != '#') return ""; 

    int r, g, b;
    std::istringstream(hex.substr(1,2)) >> std::hex >> r;
    std::istringstream(hex.substr(3,2)) >> std::hex >> g;
    std::istringstream(hex.substr(5,2)) >> std::hex >> b;

    std::ostringstream ansi;
    ansi << "\033[38;2;" << r << ";" << g << ";" << b << "m";
    return ansi.str();
}

std::string color_code(const std::string& color) {
    if (color.empty()) return "\033[37m";

    if (color[0] == '#') {
        std::string ansi = hex_to_ansi(color);
        return ansi.empty() ? "\033[37m" : ansi;
    }

    if (color == "black")   return "\033[30m";
    if (color == "red")     return "\033[31m";
    if (color == "green")   return "\033[32m";
    if (color == "yellow")  return "\033[33m";
    if (color == "blue")    return "\033[34m";
    if (color == "magenta") return "\033[35m";
    if (color == "cyan")    return "\033[36m";
    if (color == "white")   return "\033[37m";

    return "\033[37m"; // fallback branco
}

std::string get_color_or_default(const std::unordered_map<std::string, std::string>& config, const std::string& key) {
    auto it = config.find(key);
    if (it != config.end()) {
        return color_code(it->second);
    }
    return "\033[37m"; 
}

std::string run_command(const char* cmd) {
    std::array<char, 128> buffer{};
    std::string result;
    FILE* pipe = popen(cmd, "r");
    if (!pipe) return "N/A";
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }
    pclose(pipe);
    result.erase(result.find_last_not_of("\n") + 1);
    return result;
}

std::string get_cpu() {
    std::ifstream cpuinfo("/proc/cpuinfo");
    std::string line;
    while (std::getline(cpuinfo, line)) {
        if (line.find("model name") != std::string::npos) {
            return line.substr(line.find(":") + 2);
        }
    }
    return "Unknown CPU";
}

std::string get_memory() {
    std::ifstream meminfo("/proc/meminfo");
    std::string line;
    while (std::getline(meminfo, line)) {
        if (line.find("MemTotal") != std::string::npos) {
            std::istringstream iss(line);
            std::string label, value, unit;
            iss >> label >> value >> unit;
            int mb = std::stoi(value) / 1024;
            return std::to_string(mb) + " MB";
        }
    }
    return "Unknown RAM";
}

std::string get_os() {
    std::ifstream osrelease("/etc/os-release");
    std::string line;
    while (std::getline(osrelease, line)) {
        if (line.find("PRETTY_NAME=") == 0) {
            std::string name = line.substr(13, line.size() - 14);
            return name;
        }
    }
    return "Unknown OS";
}

std::string get_shell() {
    const char* shell = getenv("SHELL");
    return shell ? std::string(shell) : "Unknown Shell";
}

std::string get_gpu() {
    std::string lspci = run_command("lspci | grep -i vga | cut -d ':' -f3");
    return !lspci.empty() ? lspci : "Unknown GPU";
}

std::string get_wm() {
    const char* xdg = getenv("XDG_CURRENT_DESKTOP");
    const char* wm = getenv("DESKTOP_SESSION");
    if (xdg) return std::string(xdg);
    if (wm) return std::string(wm);
    return "Unknown WM";
}

int get_terminal_width() {
    struct winsize w{};
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    return (w.ws_col > 0) ? w.ws_col : 80;
}
int main(int argc, char* argv[]) {
    std::string theme_file = "config.json"; // padrão
    for (int i = 1; i < argc - 1; ++i) {
        if (std::string(argv[i]) == "--theme") {
            theme_file = argv[i + 1];
        }
    }

    auto config = read_config(theme_file);

    std::cout << get_color_or_default(config, "os_color")
              << (config.count("os_icon") ? config["os_icon"] : "OS:") << " \033[0m"
              << get_os() << "\n";

    std::cout << get_color_or_default(config, "cpu_color")
              << (config.count("cpu_icon") ? config["cpu_icon"] : "CPU:") << " \033[0m"
              << get_cpu() << "\n";

    std::cout << get_color_or_default(config, "ram_color")
              << (config.count("ram_icon") ? config["ram_icon"] : "RAM:") << " \033[0m"
              << get_memory() << "\n";

    std::cout << get_color_or_default(config, "shell_color")
              << (config.count("shell_icon") ? config["shell_icon"] : "Shell:") << " \033[0m"
              << get_shell() << "\n";

    std::cout << get_color_or_default(config, "wm_color")
              << (config.count("wm_icon") ? config["wm_icon"] : "WM:") << " \033[0m"
              << get_wm() << "\n";

    std::cout << get_color_or_default(config, "gpu_color")
              << (config.count("gpu_icon") ? config["gpu_icon"] : "GPU:") << " \033[0m"
              << get_gpu() << "\n";

    return 0;
}

