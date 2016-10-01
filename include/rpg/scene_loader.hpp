#ifndef RPG_SCENE_LOADER_HPP
#define RPG_SCENE_LOADER_HPP

#include "../../tinyxml2/tinyxml2.h"
#include "../../tinyxml2/xmlshortcuts.hpp"

#include <engine/vector.hpp>
#include <engine/rect.hpp>

#include <string>
#include <vector>


namespace rpg {

class scene_loader
{
public:
	scene_loader();

	int load(const std::string& pPath);

	const engine::fvector& get_boundary();
	const std::string& get_name();
	const std::string& get_script_path();
	const std::string& get_tilemap_texture();
	const std::string& get_scene_path();

	std::vector<engine::frect> construct_wall_list();

	tinyxml2::XMLElement* get_collisionboxes();
	tinyxml2::XMLElement* get_tilemap();

	tinyxml2::XMLDocument& get_document();

private:
	tinyxml2::XMLDocument mXml_Document;
	std::string mScript_path;
	std::string mScene_name;
	std::string mTilemap_texture;
	std::string mScene_path;
	engine::fvector mBoundary;
	tinyxml2::XMLElement* mEle_collisionboxes;
	tinyxml2::XMLElement* mEle_map;
};

}

#endif

