#include <string>
#include <SFML\Audio.hpp>
#include <list>

// Wrapper class for sfml sound

namespace engine
{
class sound_buffer
{
	sf::SoundBuffer buf;
public:
	int load(const std::string path)
	{
		return !buf.loadFromFile(path);
	}
	friend class sound;
};

class sound
{
	sf::Sound s;
public:
	sound(){}
	sound(const sound_buffer& buf)
	{
		set_buffer(buf);
	}
	void set_buffer(const sound_buffer& buf)
	{
		s.setBuffer(buf.buf);
	}
	void play()
	{
		s.play();
	}
	void stop()
	{
		s.stop();
	}
	void pause()
	{
		s.pause();
	}
	void set_pitch(float pitch)
	{
		s.setPitch(pitch);
	}
	void set_loop(bool loop)
	{
		s.setLoop(loop);
	}
	void set_volume(float volume)
	{
		s.setVolume(volume);
	}
	bool is_playing()
	{
		return s.getStatus() == s.Playing;
	}
};

class sound_spawner
{
	std::list<sound> sl;
	void clean_list()
	{
		for (auto i = sl.begin(); i != sl.end(); i++)
		{
			if (!i->is_playing())
			{
				sl.erase(i);
			}
		}
	}
public:
	void spawn(sound_buffer& buf)
	{
		sl.emplace_back(buf);
		sl.back().play();
		clean_list();
	}
	void stop_all()
	{
		for (auto &i : sl)
		{
			i.stop();
		}
		sl.clear();
	}
};

class sound_stream
{
	sf::Music s;
	bool valid;
public:
	sound_stream()
	{
		valid = false;
	}
	int open(const std::string path)
	{
		valid = s.openFromFile(path);
		return !valid;
	}
	void play()
	{
		s.play();
	}
	void stop()
	{
		s.stop();
	}
	void pause()
	{
		s.pause();
	}
	void set_pitch(float pitch)
	{
		s.setPitch(pitch);
	}
	void set_loop(bool loop)
	{
		s.setLoop(loop);
	}
	void set_volume(float volume)
	{
		s.setVolume(volume);
	}
	bool is_playing()
	{
		return s.getStatus() == s.Playing;
	}
	float get_position()
	{
		return s.getPlayingOffset().asSeconds();
	}
	bool is_valid()
	{
		return valid;
	}
};

// Calculate frequency of note from C5
static float note_freq(int halfsteps)
{
	const float a = std::powf(2.f, 1.f / 12);
	return 440 * std::pow(a, halfsteps + 3);
}

class freq_sequence
{
	struct entry
	{
		size_t start, duration;
		float freq, volume;
		int voice;
	};
	std::vector<entry> seq;
public:
	freq_sequence();
	freq_sequence(freq_sequence& a);
	void add(freq_sequence& fs, float start, int voice = 0);
	void add(int note, size_t sample, float duration, float volume, int voice = 0);
	void add(int note, float start, float duration, float volume, int voice = 0);
	void append(int note, float duration, float volume, int voice = 0);
	void append(freq_sequence& fs, int voice = 0);
	freq_sequence snip(size_t s_start, size_t s_duration);
	friend class sample_buffer;
};

class sample_buffer
{
	std::vector<sf::Int16> samples;
public:
	sample_buffer();
	sample_buffer(sample_buffer& m);
	sample_buffer(sample_buffer&& m);
	sample_buffer& operator=(const sample_buffer& r);
	static sample_buffer mix(sample_buffer& buf1, sample_buffer& buf2);
	int mix_with(sample_buffer& buf, int pos);

	enum wave_type
	{
		wave_sine,
		wave_saw,
		wave_triangle,
		wave_noise
	};

	static void generate(sample_buffer& buf, int wave, float f, float v, size_t start = 0, size_t duration = 1);
	static void generate(sample_buffer& buf, int wave, freq_sequence& seq, int voice = 0);
	friend class sample_mix;
};

class sample_mix
{
	struct channel
	{
		freq_sequence* seq;
		int voice, wave;
	};
	std::vector<channel> channels;
	sample_buffer mix;
	sf::SoundBuffer buffer;
	sf::Sound output, output2;
	int c_section;

	void generate_section();

public:

	sample_mix();
	void add_mix(freq_sequence& seq, int wave, int voice);
	void setup();
	void play();
	static void test_song();
};

}