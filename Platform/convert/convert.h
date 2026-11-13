#pragma once
#include <cstdint>
#include <optional>
#include <string>

#define CONVERT_FUNC(src, dst) std::optional<dst> src ## _2_ ## dst (src d);
#define CONVERT_FUNC_PUBLIC(src, dst) CONVERT_FUNC(src, dst) \
	template<> inline std::optional<dst> convert(src d_) { return src ## _2_ ## dst(d_);}

using _string = std::string;
using _string_view = std::string_view;

#define CONVERT_TYPE_PUBLIC(type) \
	CONVERT_FUNC_PUBLIC(_string_view, type) \
	CONVERT_FUNC_PUBLIC(type, _string)

namespace nexobar {
    template<typename T, typename D>
    std::optional<T> convert(D d) {
        static_assert(!std::is_same_v<T, T>, "Not use this function. Use boost::convert directly");
        return std::nullopt;
    }

    template<typename T>
    inline std::optional<T> convert(const std::string& s) {
        return nexobar::convert<T>(std::string_view(s));
    }

    template<typename T>
    inline std::optional<T> convert(const char* s) {
        return nexobar::convert<T>(std::string_view(s));
    }

    //////////////////////////////////////////////////////
    //////////////////////////////////////////////////////
    CONVERT_TYPE_PUBLIC(int32_t);
    CONVERT_TYPE_PUBLIC(uint32_t);
    CONVERT_TYPE_PUBLIC(int64_t);
    CONVERT_TYPE_PUBLIC(uint64_t);
    CONVERT_TYPE_PUBLIC(double);
}
