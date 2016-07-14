#ifndef UTILITY_UTILITY_HPP
#define UTILITY_UTILITY_HPP

#include <string>
#include <iostream>
#include <memory>
#include <list>


namespace utility
{

template<typename T>
T clamp(T v, T min, T max)
{
	if (v < min) return min;
	if (v > max) return max;
	return v;
}

template<typename T1,typename T2>
class template_map
{
	std::map<T1, T2> map_keys;
public:

	void set_key(T1 k)
	{
		map_keys[k];
	}

	void set_key(T1 k, T2 def)
	{
		map_keys[k] = def;
	}

	bool remove_key(T1 key)
	{
		if (map_keys.find(key) == map_keys.end())
			return false;
		map_keys.erase(key);
		return true;
	}

	void copy_map(std::map<T1, T2>& m)
	{
		m = map_keys;
	}
};

template<typename T1, typename T2>
class static_map
{
	std::map<T1, T2> map;
public:
	static_map(){}
	
	static_map(const template_map& temp)
	{
		temp.setup_map(map);
	}

	void set_template(const template_map& temp)
	{
		map.clear();
		temp.setup_map(map);
	}

	static_map& operator = (const template_map& temp)
	{
		map.clear();
		temp.setup_map(map);
		return *this;
	}

	bool is_exist(const T1& key)
	{
		return map.find(key) != map.end();
	}

	T2* get(const T1& key)
	{
		if (!is_exist(key))
			return nullptr;
		return &map[key];
	}

	T2* operator [](const T1& key)
	{
		return get(key);
	}
};

// Allows one class to dominate and hides a hidden item that
// can be retrieved for shady stuff.
// Why have such a strangely shady class? Convienence, of course!
template<class T1, typename T2_S>
class shadow_pair
	: public T1
{
	T2_S shadow;
public:
	template<class T1, typename T2_S>
	friend T2_S& get_shadow(shadow_pair<T1, T2_S>& A);
};

// Get reference of shadow in the shadow pair.
template<class T1, typename T2_S>
static T2_S& get_shadow(shadow_pair<T1, T2_S>& A)
{
	return A.shadow;
}

// Meant for arrays
template<typename T>
T pingpong_value(T v, T end)
{
	assert(end != 0);
	return ((v / end) % 2) ? end - (v%end) : (v%end);
}

// Iterate through sequences with features like pingponging and looping
template<typename T>
class seq_tracker
{
	T counter;
	T proc;
	T start, end;
	int type;

	void calculate_counter()
	{
		if (type == LINEAR_LOOP)
			proc = (counter % (end - start)) + start;
		if (type == LINEAR_CLAMP)
			proc = clamp(counter, start, end - 1);
		if (type == PING_PONG)
		{
			proc = counter%end;
			if ((counter / end) % 2)
				proc = end - proc;
		}
	}

public:

	enum count_type
	{
		LINEAR_LOOP,
		LINEAR_CLAMP,
		PING_PONG
	};

	seq_tracker()
	{
		counter = 0;
		proc = 0;
		type = LINEAR_LOOP;
	}

	operator T()
	{
		return proc;
	}

	void set_count(T n)
	{
		counter = n;
		calculate_counter();
	}

	T get_count()
	{
		return proc;
	}

	void set_type(int a)
	{
		type = a;
		calculate_counter();
	}

	void set_start(T n)
	{
		start = n;
	}

	void set_end(T n)
	{
		end = n;
	}

	// Only useful with type=LINEAR_CLAMP
	bool is_finished()
	{
		return proc == end;
	}

	T step(T amount)
	{
		counter += amount;
		calculate_counter();
		return proc;
	}
};

// Gives error handling in the form a return.
// Simply provide an error message and it will print the message
// if the error is not handled by handle_error().
// Ex: return "Failed to do thing!";
// Error codes can be provided as well.
// To specify no error, simply return error::NOERROR or return 0.
// The main purpose was to rid of all the std::cout and make
// error handling simply one liners.
class error
{
	struct handler{
		bool unhandled; 
		std::string message;
		int code;
	};
	std::shared_ptr<handler> err;
public:

	static const int NOERROR = 0, ERROR = 1;

	error()
	{
		err.reset(new handler);
		err->code = NOERROR;
		err->unhandled = false;
	}
	error(const error& A)
	{
		err = A.err;
	}
	error(const std::string& message)
	{
		err.reset(new handler);
		err->unhandled = true;
		err->message = message;
		err->code = ERROR;
	}
	error(int code)
	{
		err.reset(new handler);
		err->unhandled = (code != NOERROR);
		err->code = code;
	}

	~error()
	{
		if (err->unhandled && err.unique())
		{
			if (err->message.empty())
				std::cout << "Error Code :" << err->code << "\n";
			else
				std::cout << "Error : " << err->message << "\n";
		}
	}

	error& handle_error()
	{
		err->unhandled = false;
		return *this;
	}

	const std::string& get_message()
	{
	    return err->message;
	}
	
	int get_error_code()
	{
		return err->code;
	}

	error& operator=(const error& A)
	{
		err = A.err;
		return *this;
	}
	
	bool has_error()
	{
		return err->code != NOERROR;
	}

	bool is_handled()
	{
		return !err->unhandled;
	}

	explicit operator bool()
	{
		return err->code != NOERROR;
	}
};

// Allows you to return an item or return an error.
// Takes normal return values as "return value;".
// To return an error, simply:
// return utility::error("Failed to do thing!");
template<typename T>
class ret
{
	T item;
	error err;
	int has_return;
public:

	ret()
	{
		has_return = false;
	}
	
    template<typename T1>
	ret(const T1& A)
	{
		item = A;
		has_return = true;
	}
	
	ret(const error& A)
	{
		err = A;
		has_return = false;
	}
	
	ret& operator=(const ret& A)
	{
		item = A.item;
		err = A.err;
		has_return = A.has_return;
		return *this;
	}

	ret(const ret& A)
	{
		*this = A;
	}

	T& get_return()
	{
		return item;
	}
	
	error& get_error()
	{
	    return err;
	}
	
	// Check for no error
	explicit operator bool()
	{
		return !err.has_error() && has_return;
	}
};

template<typename T>
static T& get_return(ret<T> r)
{
	return r.get_return();
}

}

#endif