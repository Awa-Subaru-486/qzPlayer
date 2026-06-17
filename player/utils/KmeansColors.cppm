/**
 * @file KmeansColors.cppm
 * @brief 基于Lab颜色空间的K-means聚类，提取QImage主导颜色
 * @details 异步执行，不阻塞UI线程。自动过滤黑/白范围内的颜色。
 */

module;

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <numeric>
#include <optional>
#include <random>
#include <span>
#include <unordered_map>
#include <vector>

#include <QColor>
#include <QImage>
#include <QtConcurrent>

export module KmeansColors;

export namespace qz
{
    /**
     * @brief 从QImage异步提取主导颜色
     * @param image 输入图像
     * @return QFuture，结果为主导颜色或 std::nullopt（提取失败）
     */
    auto extract_dominant_color(const QImage& image) -> QFuture<std::optional<QColor>>;
}

module:private;

namespace qz
{
    struct Lab
    {
        float l{};
        float a{};
        float b{};

        Lab operator+(const Lab& o) const { return {l + o.l, a + o.a, b + o.b}; }
        Lab operator/(float s) const { return {l / s, a / s, b / s}; }
    };

    static auto srgb_to_linear(float u) -> float
    {
        return (u <= 0.04045f) ? (u / 12.92f) : std::pow((u + 0.055f) / 1.055f, 2.4f);
    }

    static auto linear_to_srgb(float u) -> float
    {
        return (u <= 0.0031308f) ? (12.92f * u) : ((1.055f * std::pow(u, 1.0f / 2.4f)) - 0.055f);
    }

    static auto rgb_to_lab(uint8_t r8, uint8_t g8, uint8_t b8) -> Lab
    {
        float r = srgb_to_linear(r8 / 255.0f);
        float g = srgb_to_linear(g8 / 255.0f);
        float b = srgb_to_linear(b8 / 255.0f);

        float x = ((r * 0.4124564f) + (g * 0.3575761f) + (b * 0.1804375f)) / 0.95047f;
        float y = (r * 0.2126729f) + (g * 0.7151522f) + (b * 0.0721750f);
        float z = ((r * 0.0193339f) + (g * 0.1191920f) + (b * 0.9503041f)) / 1.08883f;

        auto f = [](float t) {
            constexpr float d = 6.0f / 29.0f;
            constexpr float d3 = d * d * d;
            return (t > d3) ? std::cbrt(t) : ((t / (3.0f * d * d)) + (4.0f / 29.0f));
        };

        return {(116.0f * f(y)) - 16.0f, 500.0f * (f(x) - f(y)), 200.0f * (f(y) - f(z))};
    }

    static auto lab_to_rgb(const Lab& lab) -> std::array<uint8_t, 3>
    {
        auto f_inv = [](float t) {
            constexpr float d = 6.0f / 29.0f;
            return (t > d) ? (t * t * t) : (3.0f * d * d * (t - (4.0f / 29.0f)));
        };

        float y = (lab.l + 16.0f) / 116.0f;
        float x = (lab.a / 500.0f) + y;
        float z = y - (lab.b / 200.0f);

        float lx = 0.95047f * f_inv(x);
        float ly = f_inv(y);
        float lz = 1.08883f * f_inv(z);

        float r = (lx * 3.2404542f) + (ly * -1.5371385f) + (lz * -0.4985314f);
        float g = (lx * -0.9692660f) + (ly * 1.8760108f) + (lz * 0.0415560f);
        float b = (lx * 0.0556434f) + (ly * -0.2040259f) + (lz * 1.0572252f);

        return {
            static_cast<uint8_t>(std::round(linear_to_srgb(std::clamp(r, 0.0f, 1.0f)) * 255.0f)),
            static_cast<uint8_t>(std::round(linear_to_srgb(std::clamp(g, 0.0f, 1.0f)) * 255.0f)),
            static_cast<uint8_t>(std::round(linear_to_srgb(std::clamp(b, 0.0f, 1.0f)) * 255.0f))
        };
    }

    static auto lab_diff_sq(const Lab& c1, const Lab& c2) -> float
    {
        float dl = c1.l - c2.l;
        float da = c1.a - c2.a;
        float db = c1.b - c2.b;
        return (dl * dl) + (da * da) + (db * db);
    }

    static auto is_near_black_or_white(const Lab& lab) -> bool
    {
        float chroma = (lab.a * lab.a) + (lab.b * lab.b);
        bool low_chroma = chroma < 300.0f;
        return (lab.l < 30.0f && low_chroma) || (lab.l > 80.0f && low_chroma);
    }

    struct CentroidData
    {
        Lab centroid;
        float percentage{};
    };

    static auto image_to_lab(const QImage& image) -> std::vector<Lab>
    {
        constexpr int max_sample = 200;
        QImage sampled = image;
        if (image.width() > max_sample || image.height() > max_sample)
        {
            sampled = image.scaled(max_sample, max_sample, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
        sampled = sampled.convertedTo(QImage::Format_RGBA8888);

        const int w = sampled.width();
        const int h = sampled.height();

        std::unordered_map<uint32_t, Lab> cache;
        cache.reserve(4096);
        std::vector<Lab> pixels;
        pixels.reserve(w * h);

        for (int y = 0; y < h; ++y)
        {
            const auto* scan = sampled.constScanLine(y);
            for (int x = 0; x < w; ++x)
            {
                int i = x * 4;
                uint8_t r = scan[i], g = scan[i + 1], b = scan[i + 2], a = scan[i + 3];
                if (a == 0) continue;

                uint32_t key = (static_cast<uint32_t>(r) << 16) |
                               (static_cast<uint32_t>(g) << 8) |
                               static_cast<uint32_t>(b);
                if (auto it = cache.find(key); it != cache.end())
                    pixels.push_back(it->second);
                else
                {
                    auto lab = rgb_to_lab(r, g, b);
                    cache[key] = lab;
                    pixels.push_back(lab);
                }
            }
        }
        return pixels;
    }

    static auto init_plus_plus(size_t k, std::mt19937& rng,
                               std::span<const Lab> buf,
                               std::vector<Lab>& centroids) -> void
    {
        if (k == 0 || buf.empty()) return;
        const size_t n = buf.size();
        centroids.push_back(buf[std::uniform_int_distribution<size_t>(0, n - 1)(rng)]);

        std::vector<float> min_dist(n);
        for (size_t c = 1; c < k; ++c)
        {
            std::transform(buf.begin(), buf.end(), min_dist.begin(),
                           [&centroids](const Lab& p) {
                               float md = std::numeric_limits<float>::max();
                               for (const auto& ct : centroids) md = std::min(md, lab_diff_sq(p, ct));
                               return md;
                           });

            float sum = std::reduce(min_dist.begin(), min_dist.end(), 0.0f);
            if (!std::isnormal(sum)) return;

            float inv = 1.0f / sum;
            std::vector<float> w(n);
            std::transform(min_dist.begin(), min_dist.end(), w.begin(), [inv](float d) { return d * inv; });

            std::discrete_distribution<size_t> sampler(w.begin(), w.end());
            centroids.push_back(buf[sampler(rng)]);
        }
    }

    static auto assign_centroids(std::span<const Lab> buf, std::span<const Lab> centroids,
                                 std::vector<uint8_t>& indices) -> void
    {
        const size_t k = centroids.size();
        indices.resize(buf.size());
        std::transform(buf.begin(), buf.end(), indices.begin(),
                       [k, &centroids](const Lab& p) -> uint8_t {
                           uint8_t best = 0;
                           float md = std::numeric_limits<float>::max();
                           for (size_t i = 0; i < k; ++i)
                           {
                               float d = lab_diff_sq(p, centroids[i]);
                               if (d < md) { md = d; best = static_cast<uint8_t>(i); }
                           }
                           return best;
                       });
    }

    static auto recompute_centroids(std::mt19937& rng, std::span<const Lab> buf,
                                    std::vector<Lab>& centroids,
                                    std::span<const uint8_t> indices) -> void
    {
        const size_t k = centroids.size();
        const size_t n = buf.size();
        std::vector<float> sl(k, 0), sa(k, 0), sb(k, 0);
        std::vector<uint64_t> cnt(k, 0);

        for (size_t i = 0; i < n; ++i)
        {
            auto idx = indices[i];
            sl[idx] += buf[i].l; sa[idx] += buf[i].a; sb[idx] += buf[i].b;
            cnt[idx]++;
        }

        std::uniform_real_distribution<float> ld(0.0f, 100.0f), ad(-128.0f, 127.0f);
        for (size_t i = 0; i < k; ++i)
        {
            if (cnt[i] > 0) centroids[i] = {sl[i] / cnt[i], sa[i] / cnt[i], sb[i] / cnt[i]};
            else centroids[i] = {ld(rng), ad(rng), ad(rng)};
        }
    }

    static auto convergence_score(std::span<const Lab> cur, std::span<const Lab> old) -> float
    {
        auto r = std::transform_reduce(
            cur.begin(), cur.end(), old.begin(),
            std::array<float, 3>{0, 0, 0},
            [](const std::array<float, 3>& a, const std::array<float, 3>& b) {
                return std::array<float, 3>{a[0] + b[0], a[1] + b[1], a[2] + b[2]};
            },
            [](const Lab& c, const Lab& o) {
                return std::array<float, 3>{c.l - o.l, c.a - o.a, c.b - o.b};
            });
        return (r[0] * r[0]) + (r[1] * r[1]) + (r[2] * r[2]);
    }

    static auto run_kmeans(std::vector<Lab>& pixels) -> std::vector<CentroidData>
    {
        constexpr size_t k = 8;
        constexpr size_t max_iter = 20;
        constexpr float converge = 5.0f;
        constexpr size_t runs = 3;

        float best_score = std::numeric_limits<float>::max();
        std::vector<Lab> best_centroids;
        std::vector<uint8_t> best_indices;

        for (size_t run = 0; run < runs; ++run)
        {
            std::mt19937 rng(static_cast<std::mt19937::result_type>(run));
            std::vector<Lab> centroids;
            centroids.reserve(k);
            init_plus_plus(k, rng, pixels, centroids);

            std::vector<uint8_t> indices;
            indices.reserve(pixels.size());
            auto old = centroids;

            for (size_t iter = 0; iter < max_iter; ++iter)
            {
                assign_centroids(pixels, centroids, indices);
                recompute_centroids(rng, pixels, centroids, indices);
                if (convergence_score(centroids, old) <= converge) break;
                old = centroids;
            }

            assign_centroids(pixels, centroids, indices);
            float score = convergence_score(centroids, old);
            if (score < best_score)
            {
                best_score = score;
                best_centroids = std::move(centroids);
                best_indices = std::move(indices);
            }
        }

        // 按占比排序
        const size_t kk = best_centroids.size();
        std::vector<uint64_t> counts(kk, 0);
        for (uint8_t idx : best_indices) counts[idx]++;

        auto len = static_cast<float>(best_indices.size());
        std::vector<CentroidData> result;
        result.reserve(kk);
        for (size_t i = 0; i < kk; ++i)
            if (counts[i] > 0)
                result.push_back({best_centroids[i], static_cast<float>(counts[i]) / len});

        std::ranges::sort(result, std::greater{}, &CentroidData::percentage);
        return result;
    }

    static auto pick_accent(const std::vector<CentroidData>& colors) -> std::optional<QColor>
    {
        for (const auto& [centroid, percentage] : colors)
        {
            if (!is_near_black_or_white(centroid))
            {
                auto [r, g, b] = lab_to_rgb(centroid);
                return QColor(r, g, b);
            }
        }
        return std::nullopt;
    }

    auto extract_dominant_color(const QImage& image) -> QFuture<std::optional<QColor>>
    {
        return QtConcurrent::run([img = image]() -> std::optional<QColor> {
            if (img.isNull()) return std::nullopt;
            auto pixels = image_to_lab(img);
            if (pixels.empty()) return std::nullopt;
            auto colors = run_kmeans(pixels);
            return pick_accent(colors);
        });
    }
}
