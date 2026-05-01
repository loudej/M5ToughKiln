#ifndef STOPWATCH_H
#define STOPWATCH_H

#include <cstdint>

/// True after every `pred` stays true for `duration_ms` continuously; any `false` resets.
/// Uses `armed_` so `start_ms_ == 0` is never overloaded as “unset”.
class Stopwatch {
public:
    constexpr explicit Stopwatch(uint32_t duration_ms) noexcept : duration_ms_(duration_ms) {}

    /// @param now   e.g. `millis()`
    /// @param first First predicate (at least one required)
    /// @param rest  Additional predicates, all AND-ed with `first`
    /// @return      `true` once elapsed while every predicate stays true
    template<typename... Bools>
    [[nodiscard]] bool operator()(uint32_t now, bool first, Bools... rest) noexcept {
        const bool bad = conj(first, rest...);
        if (!bad) {
            armed_ = false;
            return false;
        }
        if (!armed_) {
            armed_    = true;
            start_ms_ = now;
            return false;
        }
        return (now - start_ms_) >= duration_ms_;
    }

    void reset() noexcept { armed_ = false; }

private:
    static constexpr bool conj() noexcept { return true; }
    template<typename... Tail>
    static constexpr bool conj(bool head, Tail... tail) noexcept {
        return head && conj(tail...);
    }

    const uint32_t duration_ms_;
    bool     armed_    = false;
    uint32_t start_ms_ = 0;
};

#endif // STOPWATCH_H
