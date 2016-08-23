#include "rpg.hpp"
#include "rpg_managers.hpp"

using namespace rpg;

int
texture_manager::load_settings(tinyxml2::XMLElement* pEle)
{
	assert(pEle != nullptr);

	using namespace tinyxml2;

	XMLElement* ele_entry = pEle->FirstChildElement();
	while (ele_entry)
	{
		texture_entry &nentry = mTextures[ele_entry->Name()];
		nentry.path = ele_entry->Attribute("path");

		// the optional atlas path
		const char* att_atlas = ele_entry->Attribute("atlas");
		if (att_atlas)
		{
			nentry.atlas = att_atlas;
			nentry.has_atlas = true;
		}
		else
			nentry.has_atlas = false;

		nentry.is_loaded = false;
		ele_entry = ele_entry->NextSiblingElement();
	}
	return 0;
}

engine::texture*
texture_manager::get_texture(const std::string& pName)
{
	auto& iter = mTextures.find(pName);
	if (iter == mTextures.end())
	{
		util::error("Texture with name '" + pName + "' Does not exist.\n");
		return nullptr;
	}

	auto &entry = iter->second;
	if (!entry.is_loaded)
	{
		entry.texture.load_texture(entry.path);
		if (entry.has_atlas)
			entry.texture.load_atlas_xml(entry.atlas);
		entry.is_loaded = true;
	}
	return &entry.texture;
}

std::vector<std::string>
texture_manager::construct_list()
{
	std::vector<std::string> retval;
	retval.reserve(mTextures.size());
	for (auto& i : mTextures)
		retval.push_back(i.first);
	return std::move(retval);
}

util::error
sound_manager::load_sounds(tinyxml2::XMLElement* pEle_root)
{
	auto ele_sound = pEle_root->FirstChildElement();
	while (ele_sound)
	{
		std::string name = ele_sound->Name();

		if (auto att_path = ele_sound->Attribute("path"))
			mBuffers[name].load(att_path);
		else
			util::error("Please provide a path to sound");

		ele_sound = ele_sound->NextSiblingElement();
	}

	return 0;
}

int
sound_manager::spawn_sound(const std::string& name)
{
	auto &buffer = mBuffers.find(name);
	if (buffer == mBuffers.end())
		return 1;
	mSounds.spawn(buffer->second);
	return 0;
}

void sound_manager::stop_all()
{
	mSounds.stop_all();
}
