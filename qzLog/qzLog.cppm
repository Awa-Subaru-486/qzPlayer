module;

#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/android_sink.h>
#include <spdlog/async.h>
#include <fmt/format.h>
#include <fmt/chrono.h>
#include <fmt/std.h>

#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <chrono>
#include <sstream>

export module qzLog;

export import :qt;

export namespace qz::Log
{
    // 日志级别
    enum class level : int {
        trace    = 0,
        debug    = 1,
        info     = 2,
        warn     = 3,
        error    = 4,
        critical = 5,
        off      = 6
    };

    // 当前日志级别
    std::atomic<int> g_log_level{static_cast<int>(level::info)};

    // 初始化日志系统
    auto init(const std::string& app_name,
              const std::string& log_dir = "",
              size_t max_files = 5,
              size_t max_file_size_mb = 3) -> void;

    // 关闭日志系统
    auto shutdown() -> void;

    // 刷新日志缓冲
    auto flush() -> void;

    // 获取日志文件路径
    auto get_log_file_path() -> std::string;

    // 运行时修改日志级别
    auto set_level(level lvl) -> void;

    // 内部日志输出
    namespace detail {
        void log_msg(int lvl, std::string_view msg);
    }

    template<typename... Args>
    auto trace(fmt::format_string<Args...> fmt, Args&&... args) -> void {
        if (static_cast<int>(level::trace) >= g_log_level.load(std::memory_order_relaxed))
            detail::log_msg(static_cast<int>(level::trace), fmt::format(fmt, std::forward<Args>(args)...));
    }

    template<typename... Args>
    auto debug(fmt::format_string<Args...> fmt, Args&&... args) -> void {
        if (static_cast<int>(level::debug) >= g_log_level.load(std::memory_order_relaxed))
            detail::log_msg(static_cast<int>(level::debug), fmt::format(fmt, std::forward<Args>(args)...));
    }

    template<typename... Args>
    auto info(fmt::format_string<Args...> fmt, Args&&... args) -> void {
        if (static_cast<int>(level::info) >= g_log_level.load(std::memory_order_relaxed))
            detail::log_msg(static_cast<int>(level::info), fmt::format(fmt, std::forward<Args>(args)...));
    }

    template<typename... Args>
    auto warn(fmt::format_string<Args...> fmt, Args&&... args) -> void {
        if (static_cast<int>(level::warn) >= g_log_level.load(std::memory_order_relaxed))
            detail::log_msg(static_cast<int>(level::warn), fmt::format(fmt, std::forward<Args>(args)...));
    }

    template<typename... Args>
    auto error(fmt::format_string<Args...> fmt, Args&&... args) -> void {
        if (static_cast<int>(level::error) >= g_log_level.load(std::memory_order_relaxed))
            detail::log_msg(static_cast<int>(level::error), fmt::format(fmt, std::forward<Args>(args)...));
    }

    template<typename... Args>
    auto critical(fmt::format_string<Args...> fmt, Args&&... args) -> void {
        if (static_cast<int>(level::critical) >= g_log_level.load(std::memory_order_relaxed))
            detail::log_msg(static_cast<int>(level::critical), fmt::format(fmt, std::forward<Args>(args)...));
    }

    // 日志分类
    class LogCategory {
    public:
        explicit LogCategory(const char* name) : name_(name), enabled_(false) {
            std::lock_guard lock(cat_mutex());
            apply_rules(name);
            categories().push_back(this);
        }
        const char* name() const { return name_; }
        bool is_enabled() const { return enabled_.load(std::memory_order_relaxed); }
        void set_enabled(bool e) { enabled_.store(e, std::memory_order_relaxed); }
    private:
        friend void set_filter_rules(const std::string& rules);
        const char* name_;
        std::atomic<bool> enabled_;

        static auto cat_mutex() -> std::mutex&;
        static auto categories() -> std::vector<LogCategory*>&;
        static auto filter_rules() -> std::vector<std::pair<std::string, bool>>&;
        static auto match_category(const std::string& pattern, const char* name) -> bool;
        void apply_rules(const char* n);
    };

    // 设置分类过滤规则，格式: "name=true;name_1=true"
    // 用法: static LogCategory cat("name"); cat_info(cat, "msg={}", val);
    auto set_filter_rules(const std::string& rules) -> void;

    template<typename... Args>
    auto cat_trace(const LogCategory& cat, fmt::format_string<Args...> fmt, Args&&... args) -> void {
        if (cat.is_enabled() && static_cast<int>(level::trace) >= g_log_level.load(std::memory_order_relaxed))
            detail::log_msg(static_cast<int>(level::trace), fmt::format("[{}] {}", cat.name(), fmt::format(fmt, std::forward<Args>(args)...)));
    }

    template<typename... Args>
    auto cat_debug(const LogCategory& cat, fmt::format_string<Args...> fmt, Args&&... args) -> void {
        if (cat.is_enabled() && static_cast<int>(level::debug) >= g_log_level.load(std::memory_order_relaxed))
            detail::log_msg(static_cast<int>(level::debug), fmt::format("[{}] {}", cat.name(), fmt::format(fmt, std::forward<Args>(args)...)));
    }

    template<typename... Args>
    auto cat_info(const LogCategory& cat, fmt::format_string<Args...> fmt, Args&&... args) -> void {
        if (cat.is_enabled() && static_cast<int>(level::info) >= g_log_level.load(std::memory_order_relaxed))
            detail::log_msg(static_cast<int>(level::info), fmt::format("[{}] {}", cat.name(), fmt::format(fmt, std::forward<Args>(args)...)));
    }

    template<typename... Args>
    auto cat_warn(const LogCategory& cat, fmt::format_string<Args...> fmt, Args&&... args) -> void {
        if (cat.is_enabled() && static_cast<int>(level::warn) >= g_log_level.load(std::memory_order_relaxed))
            detail::log_msg(static_cast<int>(level::warn), fmt::format("[{}] {}", cat.name(), fmt::format(fmt, std::forward<Args>(args)...)));
    }

    template<typename... Args>
    auto cat_error(const LogCategory& cat, fmt::format_string<Args...> fmt, Args&&... args) -> void {
        if (cat.is_enabled() && static_cast<int>(level::error) >= g_log_level.load(std::memory_order_relaxed))
            detail::log_msg(static_cast<int>(level::error), fmt::format("[{}] {}", cat.name(), fmt::format(fmt, std::forward<Args>(args)...)));
    }

    template<typename... Args>
    auto cat_critical(const LogCategory& cat, fmt::format_string<Args...> fmt, Args&&... args) -> void {
        if (cat.is_enabled() && static_cast<int>(level::critical) >= g_log_level.load(std::memory_order_relaxed))
            detail::log_msg(static_cast<int>(level::critical), fmt::format("[{}] {}", cat.name(), fmt::format(fmt, std::forward<Args>(args)...)));
    }
}

namespace qz::Log::detail
{
    void log_msg(int lvl, std::string_view msg) {
        auto* logger = spdlog::default_logger_raw();
        if (logger) {
            logger->log(static_cast<spdlog::level::level_enum>(lvl), msg);
        }
    }
}

namespace qz::Log
{
    auto init(const std::string& app_name,
              const std::string& log_dir,
              size_t max_files,
              size_t max_file_size_mb) -> void
    {
        std::string log_filepath = log_dir.empty()
            ? (app_name + ".log")
            : (log_dir + "/" + app_name + ".log");

        spdlog::init_thread_pool(8192, 1);

#ifdef __ANDROID__
        auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            log_filepath, max_file_size_mb * 1024 * 1024, max_files);
        auto android_sink = std::make_shared<spdlog::sinks::android_sink_mt>(app_name.c_str());
        std::vector<spdlog::sink_ptr> sinks{file_sink, android_sink};
        auto logger = std::make_shared<spdlog::async_logger>(
            app_name, sinks.begin(), sinks.end(),
            spdlog::thread_pool(), spdlog::async_overflow_policy::block);
#else
        auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            log_filepath, max_file_size_mb * 1024 * 1024, max_files);
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        std::vector<spdlog::sink_ptr> sinks{console_sink, file_sink};
        auto logger = std::make_shared<spdlog::async_logger>(
            app_name, sinks.begin(), sinks.end(),
            spdlog::thread_pool(), spdlog::async_overflow_policy::block);
#endif

#ifdef NDEBUG
        logger->set_level(spdlog::level::info);
        g_log_level.store(static_cast<int>(level::info), std::memory_order_relaxed);
        logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [thread:%t] %v");
#else
        logger->set_level(spdlog::level::debug);
        g_log_level.store(static_cast<int>(level::debug), std::memory_order_relaxed);
        logger->set_pattern("[%^%l%$] %v");
#endif

        logger->flush_on(spdlog::level::err);
        spdlog::flush_every(std::chrono::seconds(3));
        spdlog::set_default_logger(logger);
        spdlog::info("Logger initialized: {}", log_filepath);
    }

    auto shutdown() -> void
    {
        auto* logger = spdlog::default_logger_raw();
        if (logger) {
            logger->flush();
        }
        spdlog::shutdown();
    }

    auto flush() -> void
    {
        auto* logger = spdlog::default_logger_raw();
        if (logger) {
            logger->flush();
        }
    }

    auto get_log_file_path() -> std::string
    {
        auto l = spdlog::default_logger();
        if (!l || l->sinks().empty()) return "";
        for (auto& sink : l->sinks()) {
            auto* rotating = dynamic_cast<spdlog::sinks::rotating_file_sink_mt*>(sink.get());
            if (rotating) return rotating->filename();
        }
        return "";
    }

    auto set_level(level lvl) -> void
    {
        g_log_level.store(static_cast<int>(lvl), std::memory_order_relaxed);
        auto* logger = spdlog::default_logger_raw();
        if (logger) logger->set_level(static_cast<spdlog::level::level_enum>(lvl));
    }

    auto LogCategory::cat_mutex() -> std::mutex& {
        static std::mutex m;
        return m;
    }

    auto LogCategory::categories() -> std::vector<LogCategory*>& {
        static std::vector<LogCategory*> cats;
        return cats;
    }

    auto LogCategory::filter_rules() -> std::vector<std::pair<std::string, bool>>& {
        static std::vector<std::pair<std::string, bool>> rules;
        return rules;
    }

    auto LogCategory::match_category(const std::string& pattern, const char* name) -> bool {
        if (pattern.size() >= 2 && pattern.compare(pattern.size() - 2, 2, ".*") == 0) {
            std::string prefix = pattern.substr(0, pattern.size() - 1);
            return std::string(name).compare(0, prefix.size(), prefix) == 0;
        }
        return pattern == name;
    }

    void LogCategory::apply_rules(const char* n) {
        for (auto& [pattern, enabled] : filter_rules()) {
            if (match_category(pattern, n)) enabled_ = enabled;
        }
    }

    auto set_filter_rules(const std::string& rules) -> void
    {
        std::lock_guard<std::mutex> lock(LogCategory::cat_mutex());
        auto& fr = LogCategory::filter_rules();
        fr.clear();

        std::istringstream iss(rules);
        std::string rule;
        while (std::getline(iss, rule, ';')) {
            size_t start = rule.find_first_not_of(" \t");
            size_t end = rule.find_last_not_of(" \t");
            if (start == std::string::npos) continue;
            rule = rule.substr(start, end - start + 1);
            auto pos = rule.find('=');
            if (pos == std::string::npos) continue;
            std::string pattern = rule.substr(0, pos);
            std::string value = rule.substr(pos + 1);
            bool enabled = (value == "true" || value == "1");
            fr.emplace_back(pattern, enabled);
        }

        for (auto* cat : LogCategory::categories()) {
            cat->set_enabled(false);
            for (auto& [pattern, enabled] : fr) {
                if (LogCategory::match_category(pattern, cat->name())) {
                    cat->set_enabled(enabled);
                }
            }
        }
    }
}
