#ifndef TIME_HPP
#define TIME_HPP

#include <chrono>

namespace engine
{
typedef float time_t;

// Universal time converter for clock
class utime
{
	time_t t;
public:


	// Seconds
	time_t s() const
	{
		return t;
	}

	// Seconds as integer
	int s_i() const
	{
		return (int)t;
	}

	// Microseconds
	time_t ms() const
	{
		return t * 1000;
	}

	// Microseconds as integer
	int ms_i() const
	{
		return (int)(t * 1000);
	}

	// Nanoseconds
	time_t ns() const
	{
		return t*1000*1000;
	}

	utime(time_t A)
	{
		t = A;
	}
};

class clock
{
	bool play;
	std::chrono::time_point<std::chrono::high_resolution_clock>
		start_point,
		pause_point;
public:
	clock()
	{
		play = true;
		start_point = std::chrono::high_resolution_clock::now();
	}
	utime get_elapse() const
	{
		std::chrono::time_point<std::chrono::high_resolution_clock> end_point = std::chrono::high_resolution_clock::now();
		std::chrono::duration<time_t> elapsed_seconds = end_point - start_point;
		return elapsed_seconds.count();
	}
	void start()
	{
		if (!play)
			start_point += std::chrono::high_resolution_clock::now() - pause_point;
		play = true;
	}
	void pause()
	{
		play = false;
		pause_point = std::chrono::high_resolution_clock::now();
	}
	utime restart()
	{
		utime elapse_time = get_elapse();
		start_point = std::chrono::high_resolution_clock::now();
		return elapse_time;
	}
};

class timer
{
public:
	void start(float pSeconds)
	{
		if (pSeconds <= 0)
			return;
		mSeconds = pSeconds;
		mStart_point = std::chrono::high_resolution_clock::now();
	}

	bool is_reached() const
	{
		std::chrono::duration<time_t> time = std::chrono::high_resolution_clock::now() - mStart_point;
		return time.count() >= mSeconds;
	}

private:
	std::chrono::time_point<std::chrono::high_resolution_clock>
		mStart_point;
	time_t mSeconds;
};

class counter_clock
{
public:
	void start()
	{
		mStart_point = std::chrono::high_resolution_clock::now();
	}

	void set_interval(float pInterval)
	{
		if (pInterval <= 0)
			throw "Bad time";
		mInterval = pInterval;
	}

	size_t get_count() const
	{
		std::chrono::duration<time_t> time = std::chrono::high_resolution_clock::now() - mStart_point;
		return static_cast<size_t>(std::floor(time.count() / mInterval));
	}
private:
	std::chrono::time_point<std::chrono::high_resolution_clock>
		mStart_point;
	time_t mInterval;
};

class frame_clock
{
	clock mFps_clock;
	clock mDelta_clock;
	size_t mFrames;
	float mFps;
	float mInterval;
	float mDelta;
public:
	frame_clock(float _interval = 1)
	{
		mFps = 0;
		mFrames = 0;
		mInterval = _interval;
	}

	void set_interval(float seconds)
	{
		mInterval = seconds;
	}
	
	float get_delta() const
	{
		return mDelta;
	}

	float get_fps() const
	{
		return mFps;
	}

	void update()
	{
		++mFrames;
		auto time = mFps_clock.get_elapse().s();
		if (time >= mInterval)
		{
			mFps = mFrames / time;
			mFrames = 0;
			mFps_clock.restart();
		}

		mDelta = mDelta_clock.get_elapse().s();
		mDelta_clock.restart();
	}
};

}

#endif