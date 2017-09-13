#ifndef ENGINE_AUDIO_HPP
#define ENGINE_AUDIO_HPP

#include <engine/resource.hpp>

#include <string>
#include <SFML/Audio.hpp>
#include <list>
#include <memory>
#include <engine/resource_pack.hpp>

// Wrapper class for sfml sound

namespace engine
{

class sound_file :
	public resource
{
public:
	static const size_t streaming_threshold = 1000000;
	bool requires_streaming() const;

	bool load();
	bool unload();

	void set_filepath(const std::string& pPath);

private:
	std::string mSound_source;
	sf::SoundBuffer mSFML_buffer;

	bool mRequires_streaming;

	friend class new_sound;
};

class new_sound
{
public:
	new_sound()
	{
		mReady = false;
	}

	void set_sound_resource(std::shared_ptr<sound_file> pResource);

	void play();
	void stop();
	void pause();
	void set_pitch(float pPitch);
	float get_pitch() const;
	void set_loop(bool pLoop);
	float get_loop() const;
	void set_volume(float pVolume);
	float get_volume() const;
	bool is_playing() const;
	float get_duration() const;
	float get_playoffset() const;
	void set_playoffset(float pSeconds);

private:
	std::shared_ptr<sound_file> mSource;

	bool mReady;

	sf::Sound mSFML_streamless_sound;

	struct sfml_stream_ : public sf::InputStream
	{
		engine::pack_stream stream;
		virtual sf::Int64 read(void* pData, sf::Int64 pSize);
		virtual sf::Int64 seek(sf::Int64 pPosition);
		virtual sf::Int64 tell();
		virtual sf::Int64 getSize();
	} mSfml_stream;

	sf::Music mSFML_stream_sound;
};

class sound_buffer :
	public resource
{
public:
	void set_sound_source(const std::string& pFilepath);
	bool load();
	bool unload();

private:
	std::string mSound_source;
	std::unique_ptr<sf::SoundBuffer> mSFML_buffer;

	friend class sound;
};

class sound
{
public:
	sound() {}
	sound(std::shared_ptr<sound_buffer> buf);
	void set_buffer(std::shared_ptr<sound_buffer> buf);
	void play();
	void stop();
	void pause();
	void set_pitch(float pitch);
	void set_loop(bool loop);
	void set_volume(float volume);
	bool is_playing();

private:
	std::shared_ptr<sound_buffer> mSound_buffer;
	sf::Sound s;
};

/// A pool for sound objects
class sound_spawner
{
	std::list<sound> mSounds;
	engine::sound* get_new_sound_object();
public:
	void spawn(std::shared_ptr<sound_buffer> pBuffer, float pVolume = 100, float pPitch = 1);
	void stop_all();
};

class sound_stream
{
public:
	sound_stream();

	bool open(const std::string& path);
	bool open(const std::string& path, const pack_stream_factory& mPack);
	void play();
	void stop();
	void pause();
	void set_pitch(float pitch);
	void set_loop(bool loop);
	void set_volume(float volume);
	float get_volume();
	bool is_playing();
	float get_position();
	void set_position(float pSeconds);

	float get_duration();

	bool is_valid();

private:

	struct sfml_stream_ : public sf::InputStream
	{
		engine::pack_stream stream;
		virtual sf::Int64 read(void* pData, sf::Int64 pSize);
		virtual sf::Int64 seek(sf::Int64 pPosition);
		virtual sf::Int64 tell();
		virtual sf::Int64 getSize();
	} sfml_stream;

	sf::Music mSFML_music;
	bool valid;

};


}

#endif // !ENGINE_AUDIO_HPP
