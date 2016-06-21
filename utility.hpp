#ifndef UTILITY_UTILITY_HPP
#define UTILITY_UTILITY_HPP

#include <string>
#include <iostream>
#include <memory>


namespace utility
{

// Allows one class to dominate and hides a hidden item that
// can be retrived for shady stuff.
template<typename T1, typename T2_S>
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

// Gives error handling in the form a return.
// Simply provide an error message and it will print the message
// if the error is not handled by handle_error().
// Ex: return "Failed to do thing!";
// Error codes can be provided as well.
// To specify no error, simply return 0.
// The main purpose was to rid of all the std::cout and make
// error handling simply one liners.
class error
{
	std::shared_ptr<bool> err;
	std::string error_str;
	int error_code;
public:
	error()
	{
		err.reset(new bool(false));
		error_code = 0;
	}
	error(const error& A)
	{
		err = A.err;
		error_str = A.error_str;
		error_code = A.error_code;
	}
	error(const std::string& message)
	{
		err.reset(new bool(true));
		error_str = message;
	}
	error(int code)
	{
		error_code = code;
		err.reset(new bool(code != 0));
	}

	~error()
	{
		if (*err && err.unique())
			std::cout << "Error : " << error_str << " : \n";
	}

	// Specify not to print message.
	// Returns whether or not it had an error.
	bool handle_error()
	{
		bool prev = *err;
		*err = false;
		return prev;
	}

	const std::string& get_message()
	{
	    return error_str;
	}
	
	int get_error_code()
	{
		return error_code;
	}

	error& operator=(const error& A)
	{
		err = A.err;
		error_str = A.error_str;
		error_code = A.error_code;
		return *this;
	}
	
	bool has_error()
	{
	    return *err;
	}

	explicit operator bool()
	{
		return *err;
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