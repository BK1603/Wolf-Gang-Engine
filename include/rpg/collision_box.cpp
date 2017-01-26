#include "collision_box.hpp"

using namespace rpg;

wall_group::wall_group()
{
	mIs_enabled = true;
	mFunction = nullptr;
}

void wall_group::set_function(script_function & pFunction)
{
	mFunction = &pFunction;
}

bool wall_group::call_function()
{
	if (mFunction)
	{
		return mFunction->call();
	}
	return false;
}

void wall_group::set_name(const std::string & pName)
{
	mName = pName;
}

const std::string & wall_group::get_name() const
{
	return mName;
}

void wall_group::set_enabled(bool pEnabled) 
{
	mIs_enabled = pEnabled;
}

bool wall_group::is_enabled()const
{
	return mIs_enabled;
}

bool trigger::call_function()
{
	if (!mWall_group.expired())
		return std::shared_ptr<wall_group>(mWall_group)->call_function();
	return false;
}


void collision_box_container::clean()
{
	mWall_groups.clear();
	mBoxes.clear();
}

std::shared_ptr<wall_group> collision_box_container::get_group(const std::string& pName)
{
	for (auto& i : mWall_groups)
		if (i->get_name() == pName)
			return i;
	return{};
}

std::shared_ptr<wall_group> collision_box_container::create_group(const std::string & pName)
{
	for (auto& i : mWall_groups)
		if (i->get_name() == pName)
			return i;
	std::shared_ptr<wall_group> new_wall_group(new wall_group);
	mWall_groups.push_back(new_wall_group);
	new_wall_group->set_name(pName);
	return new_wall_group;
}

std::shared_ptr<collision_box> collision_box_container::add_wall()
{
	std::shared_ptr<collision_box> box(new collision_box);
	mBoxes.push_back(box);
	return box;
}

std::shared_ptr<trigger> collision_box_container::add_trigger()
{
	std::shared_ptr<trigger> box(new trigger);
	mBoxes.push_back(box);
	return box;
}

std::shared_ptr<button> collision_box_container::add_button()
{
	std::shared_ptr<button> box(new button);
	mBoxes.push_back(box);
	return box;
}

std::shared_ptr<door> collision_box_container::add_door()
{
	std::shared_ptr<door> box(new door);
	mBoxes.push_back(box);
	return box;
}

std::shared_ptr<collision_box> rpg::collision_box_container::add_collision_box(collision_box::type pType)
{
	switch (pType)
	{
	case collision_box::type::wall:
		return add_wall();
	case collision_box::type::trigger:
		return add_trigger();
	case collision_box::type::button:
		return add_button();
	case collision_box::type::door:
		return add_door();
	}
	return{}; // Should never reach this point
}

std::vector<std::shared_ptr<collision_box>> collision_box_container::collision(engine::frect pRect)
{
	std::vector<std::shared_ptr<collision_box>> hits;
	for (auto& i : mBoxes)
		if (i->is_enabled()
			&& i->get_region().is_intersect(pRect))
			hits.push_back(i);
	return hits;
}

std::vector<std::shared_ptr<collision_box>> collision_box_container::collision(engine::fvector pPoint)
{
	std::vector<std::shared_ptr<collision_box>> hits;
	for (auto& i : mBoxes)
		if (i->is_enabled()
			&& i->get_region().is_intersect(pPoint))
			hits.push_back(i);
	return hits;
}

std::vector<std::shared_ptr<collision_box>> collision_box_container::collision(collision_box::type pType, engine::frect pRect)
{
	std::vector<std::shared_ptr<collision_box>> hits;
	for (auto& i : mBoxes)
		if (i->is_enabled()
			&& i->get_type() == pType
			&& i->get_region().is_intersect(pRect))
			hits.push_back(i);
	return hits;
}

std::vector<std::shared_ptr<collision_box>> collision_box_container::collision(collision_box::type pType, engine::fvector pPoint)
{
	std::vector<std::shared_ptr<collision_box>> hits;
	for (auto& i : mBoxes)
		if (i->is_enabled()
			&& i->get_type() == pType
			&& i->get_region().is_intersect(pPoint))
			hits.push_back(i);
	return hits;
}

bool collision_box_container::load_xml(tinyxml2::XMLElement * pEle)
{
	clean();
	auto ele_box = pEle->FirstChildElement();
	while (ele_box)
	{
		const std::string type = ele_box->Name();

		std::shared_ptr<collision_box> box;

		// Construct collision box based on type
		if (type == "wall")
		{
			box = add_wall();
		}
		else if (type == "trigger")
		{
			box = add_trigger();
		}
		else if (type == "button")
		{
			box = add_button();
		}
		else if (type == "door")
		{
			auto door = add_door();
			door->name        = util::safe_string(ele_box->Attribute("name"));
			door->destination = util::safe_string(ele_box->Attribute("destination"));
			door->scene_path  = util::safe_string(ele_box->Attribute("scene"));
			door->offset.x    = ele_box->FloatAttribute("offsetx");
			door->offset.y    = ele_box->FloatAttribute("offsety");
			box = door;
		}
		else
		{
			util::error("Unknown collision box type '" + type + "'");
			ele_box = ele_box->NextSiblingElement();
			continue;
		}

		// Set the wall group (if any)
		const std::string group_name = util::safe_string(ele_box->Attribute("group"));
		if (!group_name.empty())
			box->set_wall_group(create_group(group_name));

		// Get rectangle region
		engine::frect rect;
		rect.x = ele_box->FloatAttribute("x");
		rect.y = ele_box->FloatAttribute("y");
		rect.w = ele_box->FloatAttribute("w");
		rect.h = ele_box->FloatAttribute("h");
		box->set_region(rect);

		ele_box = ele_box->NextSiblingElement();
	}
	return false;
}

bool collision_box_container::generate_xml(tinyxml2::XMLDocument & pDocument, tinyxml2::XMLElement * pEle) const
{
	pEle->DeleteChildren();
	for (auto& i : mBoxes)
	{
		std::string type_name;
		switch (i->get_type())
		{
		case collision_box::type::wall:
			type_name = "wall";
			break;
		case collision_box::type::trigger:
			type_name = "trigger";
			break;
		case collision_box::type::button:
			type_name = "button";
			break;
		case collision_box::type::door:
			type_name = "door";
			break;
		}

		auto ele_box = pDocument.NewElement(type_name.c_str());
		pEle->InsertEndChild(ele_box);
		i->generate_xml_attibutes(ele_box);
	}
	return false;
}

bool collision_box_container::remove_box(std::shared_ptr<collision_box> pBox)
{
	for (size_t i = 0; i < mBoxes.size(); i++)
		if (mBoxes[i] == pBox)
		{
			mBoxes.erase(mBoxes.begin() + i);
			return true;
		}
	return false;
}

bool collision_box_container::remove_box(size_t pIndex)
{
	mBoxes.erase(mBoxes.begin() + pIndex);
	return true;
}

const std::vector<std::shared_ptr<collision_box>>& collision_box_container::get_boxes() const
{
	return mBoxes;
}

size_t collision_box_container::get_count() const
{
	return mBoxes.size();
}

void door::generate_xml_attibutes(tinyxml2::XMLElement * pEle) const
{
	generate_basic_attributes(pEle);
	pEle->SetAttribute("name", name.c_str());
	pEle->SetAttribute("destination", destination.c_str());
	pEle->SetAttribute("scene", scene_path.c_str());
	pEle->SetAttribute("offsetx", offset.x);
	pEle->SetAttribute("offsety", offset.y);
}
