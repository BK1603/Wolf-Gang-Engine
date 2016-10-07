#ifndef UTILITY_UTILITY_HPP
#define UTILITY_UTILITY_HPP

#include <string>
#include <iostream>
#include <memory>
#include <list>
#include <cassert>
#include <type_traits>

namespace util
{

class tracked_owner
{
public:
	tracked_owner()
	{
		mIs_valid.reset(new bool);
		*mIs_valid = true;
	}

	~tracked_owner()
	{
		*mIs_valid = false;
	}

	template<typename T>
	friend class tracking_ptr;

private:
	std::shared_ptr<bool> mIs_valid;
};

template<typename T>
class tracking_ptr
{
public:
	tracking_ptr() {}
	~tracking_ptr() {}
	
	tracking_ptr(T& a)
	{
		set(a);
	}

	tracking_ptr(const tracking_ptr& a)
	{
		set(a);
	}

	tracking_ptr& operator=(const tracking_ptr& r)
	{
		set(r);
		return *this;
	}

	tracking_ptr& operator=(T& r)
	{
		set(r);
		return *this;
	}

	void set(T& r)
	{
		assert(r.mIs_valid != nullptr);
		mIs_valid = r.mIs_valid;
		mPointer = &r;
	}

	void set(const tracking_ptr& r)
	{
		if (!r.is_valid())
			return;
		mIs_valid = r.mIs_valid;
		mPointer = r.mPointer;
	}

	void reset()
	{
		mIs_valid.reset();
	}

	bool is_valid() const
	{
		return mIs_valid != nullptr && *mIs_valid != false;
	}

	T* get() const
	{
		assert(mIs_valid != nullptr);
		assert(*mIs_valid != false);
		return mPointer;
	}

	T& operator*() const
	{
		return *get();
	}

	T* operator->() const
	{
		return get();
	}

	operator bool() const
	{
		return is_valid();
	}

private:
	std::shared_ptr<bool> mIs_valid;
	T* mPointer;
};


template<typename T>
static T floor_align(T v, T scale)
{
	return std::floor(v / scale)*scale;
}

class nocopy {
public:
	nocopy(){}
private:
	nocopy(const nocopy&) = delete;
	nocopy& operator=(const nocopy&) = delete;
};

static std::string safe_string(const char* str)
{
	if (str == nullptr)
		return std::string(); // Empty string
	return str;
}


template<typename T>
static T to_numeral(const std::string& str, size_t *i = nullptr)
{
	static_assert(std::is_arithmetic<T>::value, "Requires arithmetic type");
	return 0;
}

template<>
static char to_numeral<char>(const std::string& str, size_t *i)
{
	return (char)std::stoi(str, i);
}

template<>
static int to_numeral<int>(const std::string& str, size_t *i)
{
	return std::stoi(str, i);
}

template<>
static float to_numeral<float>(const std::string& str, size_t *i)
{
	return std::stof(str, i);
}

template<>
static double to_numeral<double>(const std::string& str, size_t *i)
{
	return std::stod(str, i);
}


template<typename T>
static T to_numeral(const std::string& str, std::string::iterator& iter)
{
	size_t i = 0;
	T val = to_numeral<T>(std::string(iter, str.end()), &i);
	iter += i;
	return val;
}

template<typename T>
static T to_numeral(const std::string& str, std::string::const_iterator& iter)
{
	size_t i = 0;
	T val = to_numeral<T>(std::string(iter, str.end()), &i);
	iter += i;
	return val;
}

class named
{
	std::string name;
public:
	void set_name(const std::string& str)
	{
		name = str;
	}
	const std::string& get_name()
	{
		return name;
	}
};

template<typename T>
static inline T clamp(T v, T min, T max)
{
	if (v < min) return min;
	if (v > max) return max;
	return v;
}


// Pingpong array
template<typename T>
T pingpong_index(T v, T end)
{
	assert(end != 0);
	return ((v / end) % 2) ? end - (v%end) : (v%end);
}

static void error(const std::string& pMessage)
{
	std::cout << "Error : " << pMessage << "\n";
}


}

#endif