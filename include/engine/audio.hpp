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

	friend class sound;
};

// This sound classs automatically chooses between loading the sound file
// to memory in its entirety or streaming it.
class sound
{
public:
	sound();

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

class sound_spawner
{
public:
	void spawn(std::shared_ptr<sound_file> pBuffer, float pVolume = 1, float pPitch = 1);
	void stop_all();

private:
	engine::sound * get_new_sound_object();
	std::list<sound> mSounds;
};


}

#endif // !ENGINE_AUDIO_HPP
