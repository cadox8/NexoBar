#include <boost/convert.hpp>
#include <boost/convert/spirit.hpp>
#include <optional>

using _string = std::string;
using _string_view = std::string_view;

#define CONVERT_FUNC(src, dst) std::optional<dst> src ## _2_ ## dst (src d_){  \
auto tmp = boost::convert<dst>(d_, boost::cnv::spirit());\
return tmp ? *tmp : std::optional<dst>();\
}

#define CONVERT_TYPE(type) \
	CONVERT_FUNC(_string_view, type) \
	CONVERT_FUNC(type, _string)

namespace ccc {
    CONVERT_TYPE(int32_t);
    CONVERT_TYPE(uint32_t);
    CONVERT_TYPE(int64_t);
    CONVERT_TYPE(uint64_t);
    CONVERT_TYPE(double);
}
