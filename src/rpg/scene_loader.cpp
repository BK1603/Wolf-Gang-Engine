#include <rpg/scene_loader.hpp>
#include <rpg/rpg_config.hpp>

#include <engine/utility.hpp>

using namespace rpg;

scene_loader::scene_loader()
{
	mEle_collisionboxes = nullptr;
	mEle_map = nullptr;
}

bool scene_loader::load(const std::string & pName)
{
	mXml_Document.Clear();

	mScene_path = defs::DEFAULT_SCENES_PATH / (pName + ".xml");
	mScript_path = defs::DEFAULT_SCENES_PATH / (pName + ".as");
	mScene_name = pName;

	if (mXml_Document.LoadFile(mScene_path.string().c_str()))
	{
		util::error("Unable to open scene. Please check path.");
		return 1;
	}

	fix();

	auto ele_root = mXml_Document.FirstChildElement("scene");
	if (!ele_root)
	{
		util::error("Unable to get root element 'scene'.");
		return 1;
	}

	// Get collision boxes
	mEle_collisionboxes = ele_root->FirstChildElement("collisionboxes");
	if (mEle_collisionboxes)
		construct_wall_list();

	// Get boundary
	if (auto ele_boundary = ele_root->FirstChildElement("boundary"))
	{
		engine::frect boundary;
		boundary.x = ele_boundary->FloatAttribute("x");
		boundary.y = ele_boundary->FloatAttribute("y");
		boundary.w = ele_boundary->FloatAttribute("w");
		boundary.h = ele_boundary->FloatAttribute("h");
		mBoundary = boundary * 32.f;
		mHas_boundary = true;
	}
	else
	{
		mHas_boundary = false;
	}

	// Get tilemap
	if (mEle_map = ele_root->FirstChildElement("map"))
	{
		// Load tilemap texture
		if (auto ele_texture = mEle_map->FirstChildElement("texture"))
			mTilemap_texture = ele_texture->GetText();
		else
		{
			util::error("Tilemap texture is not defined");
			return 1;
		}

	}

	return 0;
}

void scene_loader::clean()
{
	mXml_Document.Clear();
	mScript_path.clear();
	mScene_name.clear();
	mTilemap_texture.clear();
	mScene_path.clear();
	mBoundary = engine::frect();
	mHas_boundary = false;
	mEle_collisionboxes = nullptr;
	mEle_map = nullptr;
	mWalls.clear();
}

void scene_loader::fix()
{
	auto ele_scene = mXml_Document.FirstChildElement("scene");
	if (!ele_scene)
	{
		ele_scene = mXml_Document.NewElement("scene");
		mXml_Document.InsertFirstChild(ele_scene);
	}

	auto ele_map = ele_scene->FirstChildElement("map");
	if (!ele_map)
	{
		ele_map = mXml_Document.NewElement("map");
		ele_scene->InsertFirstChild(ele_map);
	}

	auto ele_texture = ele_map->FirstChildElement("texture");
	if (!ele_texture)
		ele_map->InsertFirstChild(mXml_Document.NewElement("texture"));

	auto ele_collisionboxes = ele_scene->FirstChildElement("collisionboxes");
	if (!ele_collisionboxes)
		ele_scene->InsertFirstChild(mXml_Document.NewElement("collisionboxes"));
}

bool scene_loader::has_boundary() const
{
	return mHas_boundary;
}

const engine::frect& scene_loader::get_boundary() const
{
	return mBoundary;
}

const std::string& scene_loader::get_name()
{
	return mScene_name;
}

std::string scene_loader::get_script_path() const
{
	return mScript_path.string();
}

std::string scene_loader::get_tilemap_texture() const
{
	return mTilemap_texture.string();
}

std::string rpg::scene_loader::get_scene_path() const
{
	return mScene_path.string();
}

const std::vector<engine::frect>& rpg::scene_loader::get_walls() const
{
	return mWalls;
}

void scene_loader::construct_wall_list()
{
	assert(mEle_collisionboxes != 0);

	auto ele_wall = mEle_collisionboxes->FirstChildElement("wall");
	while (ele_wall)
	{
		mWalls.push_back(util::shortcuts::rect_float_att(ele_wall));
		ele_wall = ele_wall->NextSiblingElement("wall");
	}
}

util::optional_pointer<tinyxml2::XMLElement> scene_loader::get_collisionboxes()
{
	return mEle_collisionboxes;
}

util::optional_pointer<tinyxml2::XMLElement> scene_loader::get_tilemap()
{
	return mEle_map;
}

tinyxml2::XMLDocument& rpg::scene_loader::get_document()
{
	return mXml_Document;
}
