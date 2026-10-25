// ai.hpp - 引擎内 AI 助手模块：原生支持豆包/千问等 OpenAI 兼容接口
// 自写 HTTPS 客户端 + JSON，不依赖第三方游戏引擎。
#pragma once
#include <string>

namespace dbg {

// 一个 AI 服务预设：地址 + 模型自动填好，用户只需填密钥
struct AIProvider {
    std::string name;     // 显示名
    std::string baseUrl;  // OpenAI 兼容的 base URL（/chat/completions 在其下）
    std::string model;    // 默认模型名
};

// 豆包（字节火山方舟 Ark，OpenAI 兼容）
inline AIProvider aiDoubao() {
    return AIProvider{"豆包 Doubao", "https://ark.cn-beijing.volces.com/api/v3", "doubao-seed-1-6-250615"};
}
// 千问（阿里 DashScope 兼容模式）
inline AIProvider aiQwen() {
    return AIProvider{"千问 Qwen", "https://dashscope.aliyuncs.com/compatible-mode/v1", "qwen-plus"};
}

// 调用聊天接口：baseUrl 下 POST /chat/completions，Bearer 密钥，返回助手回复文本。
// 成功返回 true 并把回复写到 out；失败返回 false 并把错误写到 err。
bool aiChat(const AIProvider& p, const std::string& apiKey,
            const std::string& system, const std::string& user,
            std::string& out, std::string& err);

} // namespace dbg
