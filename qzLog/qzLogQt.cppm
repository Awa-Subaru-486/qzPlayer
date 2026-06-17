module;

#include <QString>
#include <QSize>
#include <QColor>
#include <QRect>
#include <fmt/format.h>

export module qzLog:qt;

// UTF-16 → UTF-8 零分配直写输出迭代器
export template<>
struct fmt::formatter<QString> : fmt::formatter<std::string> {
    auto format(const QString& s, fmt::format_context& ctx) const {
        auto out = ctx.out();
        const auto* data = s.utf16();
        const int len = s.size();
        for (int i = 0; i < len; ++i) {
            char32_t cp = data[i];
            if (QChar::isHighSurrogate(cp) && i + 1 < len) {
                cp = QChar::surrogateToUcs4(cp, data[++i]);
            }
            if (cp < 0x80) {
                *out++ = static_cast<char>(cp);
            } else if (cp < 0x800) {
                *out++ = static_cast<char>(0xC0 | (cp >> 6));
                *out++ = static_cast<char>(0x80 | (cp & 0x3F));
            } else if (cp < 0x10000) {
                *out++ = static_cast<char>(0xE0 | (cp >> 12));
                *out++ = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                *out++ = static_cast<char>(0x80 | (cp & 0x3F));
            } else {
                *out++ = static_cast<char>(0xF0 | (cp >> 18));
                *out++ = static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
                *out++ = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                *out++ = static_cast<char>(0x80 | (cp & 0x3F));
            }
        }
        return out;
    }
};

export template<>
struct fmt::formatter<QSize> : fmt::formatter<std::string> {
    auto format(const QSize& s, fmt::format_context& ctx) const {
        return fmt::format_to(ctx.out(), "{}x{}", s.width(), s.height());
    }
};

export template<>
struct fmt::formatter<QRect> : fmt::formatter<std::string> {
    auto format(const QRect& s, fmt::format_context& ctx) const {
        return fmt::format_to(ctx.out(), "{}x{}", s.width(), s.height());
    }
};

export template<>
struct fmt::formatter<QRectF> : fmt::formatter<std::string> {
    auto format(const QRectF& s, fmt::format_context& ctx) const {
        return fmt::format_to(ctx.out(), "{}x{}", s.width(), s.height());
    }
};

export template<>
struct fmt::formatter<QLatin1String> : fmt::formatter<std::string_view> {
    auto format(const QLatin1String& s, fmt::format_context& ctx) const {
        return fmt::formatter<std::string_view>::format(
            std::string_view(s.data(), s.size()), ctx);
    }
};

export template<>
struct fmt::formatter<QColor> : fmt::formatter<std::string> {
    auto format(const QColor& c, fmt::format_context& ctx) const {
        if (c.alpha() == 255)
            return fmt::format_to(ctx.out(), "#{:02x}{:02x}{:02x}", c.red(), c.green(), c.blue());
        return fmt::format_to(ctx.out(), "#{:02x}{:02x}{:02x}{:02x}", c.red(), c.green(), c.blue(), c.alpha());
    }
};
