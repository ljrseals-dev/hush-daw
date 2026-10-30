#pragma once
// Fixed-size sliding window with O(1) mean/variance. No allocation. (Replaces upstream's std::deque history.)
#include <array>
#include <cstddef>

namespace dawchecker::dsp {

template <std::size_t N>
class RunningWindow
{
    static_assert(N > 0, "window must be non-empty");
public:
    void clear() noexcept { buf_.fill(0.0); head_ = count_ = since_ = 0; sum_ = sumsq_ = 0.0; }

    void push(double x) noexcept
    {
        if (count_ == N) { const double old = buf_[head_]; sum_ -= old; sumsq_ -= old * old; }
        else ++count_;
        buf_[head_] = x;
        sum_ += x; sumsq_ += x * x;
        head_ = (head_ + 1) % N;
        if (++since_ >= N) { recompute(); since_ = 0; }   // bounds floating-point drift
    }

    std::size_t count() const noexcept { return count_; }
    bool full() const noexcept { return count_ == N; }
    double mean() const noexcept { return count_ ? sum_ / static_cast<double>(count_) : 0.0; }
    double variance() const noexcept
    {
        if (!count_) return 0.0;
        const double m = mean();
        const double v = sumsq_ / static_cast<double>(count_) - m * m;
        return v > 0.0 ? v : 0.0;
    }

private:
    void recompute() noexcept
    {
        sum_ = sumsq_ = 0.0;
        const std::size_t start = (count_ == N) ? head_ : 0;
        for (std::size_t i = 0; i < count_; ++i) { const double v = buf_[(start + i) % N]; sum_ += v; sumsq_ += v * v; }
    }
    std::array<double, N> buf_{};
    std::size_t head_ = 0, count_ = 0, since_ = 0;
    double sum_ = 0.0, sumsq_ = 0.0;
};

} // namespace dawchecker::dsp
