#pragma once

#include <algorithm>
#include <cassert>


#include <boost/program_options.hpp>


namespace po = boost::program_options;

template <typename T>
struct Vec3
{
	T x{};
	T y{};
	T z{};

	inline auto clamp(const Vec3<T>& min, const Vec3<T>& max) -> void
	{
		x = std::clamp(x, min.x, max.x);
		y = std::clamp(y, min.y, max.y);
		z = std::clamp(z, min.z, max.z);
	}
	auto operator<=>(const Vec3<T>& other) const -> auto = default;
};

template <typename T>
inline auto clamp(const Vec3<T>& value, const Vec3<T>& min, const Vec3<T>& max) -> Vec3<T>
{
	Vec3<T> r = value;
	r.clamp(min, max);
	return r;
}

template <typename T>
[[nodiscard]] auto min(const Vec3<T>& v1, const Vec3<T>& v2) -> Vec3<T>
{
	Vec3<T> r;
	r.x = std::min(v1.x, v2.x);
	r.y = std::min(v1.y, v2.y);
	r.z = std::min(v1.z, v2.z);
	return r;
}

template <typename T>
[[nodiscard]] auto max(const Vec3<T>& v1, const Vec3<T>& v2) -> Vec3<T>
{
	Vec3<T> r;
	r.x = std::max(v1.x, v2.x);
	r.y = std::max(v1.y, v2.y);
	r.z = std::max(v1.z, v2.z);
	return r;
}

using Vec3F = Vec3<float>;
using Vec3d = Vec3<double>;
using Vec3I = Vec3<int>;

template <typename T>
struct Box3
{
	Vec3<T> lower{};
	Vec3<T> upper{};

	[[nodiscard]] inline static auto maxBox() -> Box3<T>
	{
		Box3<T> r;
		r.upper.x = std::numeric_limits<T>::max();
		r.upper.y = std::numeric_limits<T>::max();
		r.upper.z = std::numeric_limits<T>::max();

		r.lower.x = std::numeric_limits<T>::min();
		r.lower.y = std::numeric_limits<T>::min();
		r.lower.z = std::numeric_limits<T>::min();
		return r;
	}

	inline auto extend(const Vec3<T>& point) -> void
	{
		lower = min(lower, point);
		upper = max(upper, point);
	}

	inline auto extend(const Box3<T>& box) -> void
	{
		extend(box.lower);
		extend(box.upper);
	}

	inline auto clip(const Box3<T>& clipBox)
	{
		lower = clamp(lower, clipBox.lower, clipBox.upper);
		upper = clamp(upper, clipBox.lower, clipBox.upper);
	}

	[[nodiscard]] inline auto size() const -> Vec3<T>
	{
		Vec3<T> r;
		r.x = std::abs(upper.x - lower.x);
		r.y = std::abs(upper.y - lower.y);
		r.z = std::abs(upper.z - lower.z);
		return r;
	}

	[[nodiscard]] inline auto contains(const Vec3<T>& value) const -> bool
	{
		return value >= lower && value <= upper;
	}

	auto operator<=>(const Box3<T>& other) const -> auto = default;
};

template <typename T>
[[nodiscard]] auto operator*(const Vec3<T>& a, const Vec3<T>& b) -> Vec3<T>
{
	Vec3<T> r;
	r.x = a.x * b.x;
	r.y = a.y * b.y;
	r.z = a.z * b.z;
	return r;
}

template <typename T1, typename T2>
[[nodiscard]] auto operator*(const T2& a, const Vec3<T1>& b) -> std::enable_if_t<std::is_convertible_v<T1,T2>, Vec3<T1>>
{
	Vec3<T1> r;
	r.x = a * b.x;
	r.y = a* b.y;
	r.z = a * b.z;
	return r;
}

template <typename T1, typename T2>
[[nodiscard]] auto operator*(const Vec3<T1>& b, const T2& a) -> std::enable_if_t<std::is_convertible_v<T1,T2>, Vec3<T1>>
{
	return a*b;
}

template <typename T>
[[nodiscard]] auto operator+(const Vec3<T>& a, const Vec3<T>& b) -> Vec3<T>
{
	Vec3<T> r;
	r.x = a.x + b.x;
	r.y = a.y + b.y;
	r.z = a.z + b.z;
	return r;
}

template <typename T>
[[nodiscard]] auto operator-(const Vec3<T>& a, const Vec3<T>& b) -> Vec3<T>
{
	Vec3<T> r;
	r.x = a.x - b.x;
	r.y = a.y - b.y;
	r.z = a.z - b.z;
	return r;
}

template <typename T>
[[nodiscard]] auto clip(const Box3<T>& value, const Box3<T>& clipBox) -> Box3<T>
{
	Box3<T> r(value);
	r.clip(clipBox);
	return r;
}

using Box3F = Box3<float>;
using Box3d = Box3<double>;
using Box3I = Box3<int>;






enum class CuttingStrategy
{
	binaryPartition,
	fitMemoryReq
};

enum class LogLevel
{
	none,
	essential,
	all
};

enum class Filter
{
	max,
	mean
};

static auto operator>>(std::istream& in, Filter& filter) -> std::istream&
{
	std::string token;
	in >> token;

	std::ranges::transform(token, token.begin(), [](unsigned char c) { return std::tolower(c); });

	if (token == "upper")
	{
		filter = Filter::max;
	}
	else if (token == "mean")
	{
		filter = Filter::mean;
	}
	else
	{
		throw po::validation_error{ po::validation_error::kind_t::invalid_option_value };
	}

	return in;
}

static auto operator>>(std::istream& in, LogLevel& level) -> std::istream&
{
	std::string token;
	in >> token;

	std::ranges::transform(token, token.begin(), [](unsigned char c) { return std::tolower(c); });

	if (token == "none")
	{
		level = LogLevel::none;
	}
	else if (token == "essential")
	{
		level = LogLevel::essential;
	}
	else if (token == "all")
	{
		level = LogLevel::all;
	}
	else
	{
		throw po::validation_error{ po::validation_error::kind_t::invalid_option_value };
	}

	return in;
}

static auto operator>>(std::istream& in, CuttingStrategy& strategy) -> std::istream&
{
	std::string token;
	in >> token;

	std::ranges::transform(token, token.begin(), [](unsigned char c) { return std::tolower(c); });

	if (token == "binary_partition")
	{
		strategy = CuttingStrategy::binaryPartition;
	}
	else if (token == "fit_memory_req")
	{
		strategy = CuttingStrategy::fitMemoryReq;
	}
	else
	{
		throw po::validation_error{ po::validation_error::kind_t::invalid_option_value };
	}

	return in;
}

using ClusterId = uint64_t;
