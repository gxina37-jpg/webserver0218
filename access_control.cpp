#include "access_control.h"
#include <iostream>
#include <cstring>
#include <fstream>
#include <algorithm>
#include <cctype>

AccessControl::AccessControl() {}

static inline std::string trim_copy(std::string s) {
    // left trim
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));
    // right trim
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), s.end());
    return s;
}

int AccessControl::load_list_file(const std::string& path, std::unordered_set<uint32_t>& out) {
    std::ifstream in(path);
    if (!in.is_open()) {
        // 文件不存在：当作未启用，不算错误
        return 0;
    }

    int loaded = 0;
    std::string line;
    while (std::getline(in, line)) {
        line = trim_copy(line);
        if (line.empty()) continue;
        if (line[0] == '#') continue;

        // 支持行尾注释："1.2.3.4  # comment"
        size_t hash_pos = line.find('#');
        if (hash_pos != std::string::npos) {
            line = trim_copy(line.substr(0, hash_pos));
            if (line.empty()) continue;
        }

        in_addr addr{};
        if (inet_pton(AF_INET, line.c_str(), &addr) != 1) {
            // 解析失败：忽略该行（不终止）
            continue;
        }
        std::cout << "加载IP地址" << std::endl;
        std::cout << addr.s_addr << std::endl;
        out.insert(addr.s_addr);
        loaded++;
    }
    return loaded;
}

int AccessControl::load_whitelist(const std::string& path) {
    return load_list_file(path, m_whitelist);
}

int AccessControl::load_blacklist(const std::string& path) {
    return load_list_file(path, m_blacklist);
}

bool AccessControl::allow(const sockaddr_in& addr) const {
    uint32_t ip = addr.sin_addr.s_addr;
    // 黑名单优先
    for (auto i : m_blacklist) {
        std::cout << i << std::endl;
    }
    std::cout << "IP地址" << "\n";
    char buf[INET_ADDRSTRLEN] = {0};
    inet_ntop(AF_INET, &ip, buf, sizeof(buf));
    std::cout << buf << std::endl;
    std::cout << ip << std::endl;
    if (!m_blacklist.empty() && m_blacklist.find(ip) != m_blacklist.end()) {
        return false;
    }
    // 白名单非空则严格限制
    if (!m_whitelist.empty()) {
        return m_whitelist.find(ip) != m_whitelist.end();
    }
    return true;
}

std::string AccessControl::ip_to_string(const sockaddr_in& addr) {
    char buf[INET_ADDRSTRLEN] = {0};
    inet_ntop(AF_INET, &addr.sin_addr, buf, sizeof(buf));
    return std::string(buf);
}

bool AccessControl::enabled() const {
    return !m_whitelist.empty() || !m_blacklist.empty();
}
