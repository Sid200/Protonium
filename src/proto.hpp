#pragma once
#include <string>
#include <string_view>

#include "includes/Expressions.hpp"

class RuntimeError;

class Proto {
private:
    bool m_hitError = false;
    bool m_hitRuntimeError = false;
    Proto() = default;
public:
    static Proto& getInstance();
    Proto(const Proto&) = delete;
    void operator=(const Proto&) = delete;
    void run(std::string src, bool allowExpr = false);
    void runFile(std::string_view path);

    void setErr(bool val);
    void setRuntimeError(bool val);
    bool hadError() const;
    bool hadRuntimeError() const;
    void error(std::size_t line, std::string_view msg, std::string snippet="");
    void runtimeError(const RuntimeError& error);

    void warn(std::size_t line, const std::string& warning);
};