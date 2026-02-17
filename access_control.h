#ifndef ACCESS_CONTROL_H
#define ACCESS_CONTROL_H

#include <arpa/inet.h>
#include <string>
#include <unordered_set>

// 简单的 IPv4 访问控制：支持白名单/黑名单（精确 IP 匹配）。
// 规则：
//  1) 如果 IP 在黑名单中 -> 拒绝
//  2) 如果白名单非空 -> 仅允许白名单中的 IP
//  3) 否则 -> 允许
//
// 配置文件格式（每行一个 IPv4，支持 # 注释与空行）：
//   127.0.0.1
//   192.168.1.10
//   # comment

class AccessControl {
public:
    AccessControl();

    // 从文件加载白名单/黑名单。返回成功加载的条目数；
    // 若文件不存在返回 0；若解析/读取出错返回 -1。
    int load_whitelist(const std::string& path);
    int load_blacklist(const std::string& path);

    // 判断是否允许该地址访问
    bool allow(const sockaddr_in& addr) const;

    // 用于日志打印的 IP 字符串
    static std::string ip_to_string(const sockaddr_in& addr);

    // 查询当前规则是否启用（白/黑名单任一非空即启用）
    bool enabled() const;

    // 用于输出当前加载条目数量
    size_t whitelist_size() const { return m_whitelist.size(); }
    size_t blacklist_size() const { return m_blacklist.size(); }

private:
    static int load_list_file(const std::string& path, std::unordered_set<uint32_t>& out);

private:
    std::unordered_set<uint32_t> m_whitelist; // network byte order
    std::unordered_set<uint32_t> m_blacklist; // network byte order
};

#endif
