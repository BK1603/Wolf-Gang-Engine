#include <rpg/scene.hpp>
#include <rpg/rpg_config.hpp>

using namespace rpg;

// #########
// scene
// #########

scene::scene()
{
	mTilemap_display.set_depth(defs::TILES_DEPTH);

	mWorld_node.add_child(mTilemap_display);
	mWorld_node.add_child(mPlayer);
	mFocus_player = true;

	mPathfinding_system.set_collision_system(mCollision_system);

	mEntity_manager.set_root_node(mWorld_node);

	mPack = nullptr;
}

scene::~scene()
{
	util::info("Destroying scene");
}

panning_node& scene::get_world_node()
{
	return mWorld_node;
}

collision_system&
scene::get_collision_system()
{
	return mCollision_system;
}

void
scene::clean(bool pFull)
{
	mScript->abort_all();

	mEnd_functions.clear();

	mTilemap_display.clean();
	mTilemap_manipulator.clean();
	mCollision_system.clean();
	mEntity_manager.clean();
	mColored_overlay.clean();
	mSound_FX.stop_all();
	mBackground_music.pause_music();

	focus_player(true);

	mPlayer.clean();

	if (pFull) // Cleanup everything
	{
		mBackground_music.clean();

		// Clear all contexts for recompiling
		for (auto &i : pScript_contexts)
			i.second.clean();
		pScript_contexts.clear();
	}
}

bool scene::load_scene(std::string pName)
{
	assert(mResource_manager != nullptr);
	assert(mScript != nullptr);

	clean();

	mCurrent_scene_name = pName;

	util::info("Loading scene '" + pName + "'");

	if (mPack)
	{
		if (!mLoader.load(defs::DEFAULT_SCENES_PATH.string(), pName, *mPack))
		{
			util::error("Unable to open scene '" + pName + "'");
			return false;
		}
	}
	else
	{
		if (!mLoader.load((defs::DEFAULT_DATA_PATH / defs::DEFAULT_SCENES_PATH).string(), pName))
		{
			util::error("Unable to open scene '" + pName + "'");
			return false;
		}
	}

	auto collision_boxes = mLoader.get_collisionboxes();
	if (collision_boxes)
		mCollision_system.load_collision_boxes(collision_boxes);

	mWorld_node.set_boundary_enable(mLoader.has_boundary());
	mWorld_node.set_boundary(mLoader.get_boundary());

	auto &context = pScript_contexts[mLoader.get_name()];

	// Compile script if not already
	if (!context.is_valid())
	{
		context.set_script_system(*mScript);
		if (mPack)
			context.build_script(mLoader.get_script_path(), *mPack);
		else
			context.build_script(mLoader.get_script_path());
	}
	else
		util::info("Script is already compiled");

	// Setup context if still valid
	if (context.is_valid())
	{
		context.clean_globals();
		mCollision_system.setup_script_defined_triggers(context);
		context.start_all_with_tag("start");
		mEnd_functions = context.get_all_with_tag("door");
	}

	auto tilemap_texture = mResource_manager->get_resource<engine::texture>(engine::resource_type::texture, mLoader.get_tilemap_texture());
	if (!tilemap_texture)
	{
		util::error("Invalid tilemap texture");
		return false;
	}
	mTilemap_display.set_texture(tilemap_texture);

	mTilemap_manipulator.load_tilemap_xml(mLoader.get_tilemap());
	mTilemap_manipulator.update_display(mTilemap_display);

	// Pre-execute so the scene script can setup things before the render.
	mScript->tick();

	update_focus();

	util::info("Cleaning up resources...");
	// Ensure resources are ready and unused stuff is put away
	mResource_manager->ensure_load();
	mResource_manager->unload_unused();
	util::info("Resources ready");

	return true;
}

bool scene::load_scene(std::string pName, std::string pDoor)
{
	if (!load_scene(pName))
		return false;

	auto door = mCollision_system.get_door_entry(pDoor);
	if (!door)
	{
		util::warning("Enable to find door '" + pDoor + "'");
		return false;
	}
	mPlayer.set_direction(character_entity::vector_direction(door->get_offset()));
	mPlayer.set_position(door->calculate_player_position());
	return true;
}

#ifndef LOCKED_RELEASE_MODE
bool scene::create_scene(const std::string & pName)
{
	const auto xml_path = defs::DEFAULT_DATA_PATH / defs::DEFAULT_SCENES_PATH / (pName + ".xml");
	const auto script_path = defs::DEFAULT_DATA_PATH / defs::DEFAULT_SCENES_PATH / (pName + ".as");

	if (engine::fs::exists(xml_path) || engine::fs::exists(script_path))
	{
		util::error("Scene '" + pName + "' already exists");
		return false;
	}

	// Ensure the existance of the directories
	engine::fs::create_directories(xml_path.parent_path());

	// Create xml file
	std::ofstream xml(xml_path.string().c_str());
	xml << defs::MINIMAL_XML_SCENE;
	xml.close();

	// Create script file
	std::ofstream script(script_path.string().c_str());
	script << defs::MINIMAL_SCRIPT_SCENE;
	script.close();

	return true;
}
#endif

bool scene::reload_scene()
{
	if (mCurrent_scene_name.empty())
	{
		util::error("No scene to reload");
		return 1;
	}

	clean(true);

	return load_scene(mCurrent_scene_name);
}

const std::string& scene::get_path()
{
	return mLoader.get_name();
}

const std::string& scene::get_name()
{
	return mLoader.get_name();
}

void scene::load_script_interface(script_system& pScript)
{
	mEntity_manager.load_script_interface(pScript);
	mBackground_music.load_script_interface(pScript);
	mColored_overlay.load_script_interface(pScript);
	mPathfinding_system.load_script_interface(pScript);
	mCollision_system.load_script_interface(pScript);

	pScript.add_function("void set_tile(const string &in, vec, int, int)", AS::asMETHOD(scene, script_set_tile), this);
	pScript.add_function("void remove_tile(vec, int)", AS::asMETHOD(scene, script_remove_tile), this);

	pScript.add_function("int _spawn_sound(const string&in, float, float)", AS::asMETHOD(scene, script_spawn_sound), this);
	pScript.add_function("void _stop_all()", AS::asMETHOD(engine::sound_spawner, stop_all), &mSound_FX);

	pScript.add_function("entity get_player()", AS::asMETHOD(scene, script_get_player), this);
	pScript.add_function("void _set_player_locked(bool)", AS::asMETHOD(player_character, set_locked), &mPlayer);
	pScript.add_function("bool _get_player_locked()", AS::asMETHOD(player_character, is_locked), &mPlayer);

	pScript.add_function("void _set_focus(vec)", AS::asMETHOD(scene, script_set_focus), this);
	pScript.add_function("vec _get_focus()", AS::asMETHOD(scene, script_get_focus), this);
	pScript.add_function("void _focus_player(bool)", AS::asMETHOD(scene, focus_player), this);

	pScript.add_function("vec get_boundary_position()", AS::asMETHOD(scene, script_get_boundary_position), this);
	pScript.add_function("vec get_boundary_size()", AS::asMETHOD(scene, script_get_boundary_size), this);
	pScript.add_function("void get_boundary_position(vec)", AS::asMETHOD(scene, script_set_boundary_position), this);
	pScript.add_function("void get_boundary_size(vec)", AS::asMETHOD(scene, script_set_boundary_size), this);
	pScript.add_function("void set_boundary_enable(bool)", AS::asMETHOD(panning_node, set_boundary_enable), this);

	pScript.add_function("vec get_display_size()", AS::asMETHOD(scene, script_get_display_size), this);

	pScript.add_function("const string& get_scene_name()", AS::asMETHOD(scene, get_name), this);

	mScript = &pScript;
}

#ifndef LOCKED_RELEASE_MODE
void scene::load_terminal_interface(engine::terminal_system & pTerminal)
{
	mTerminal_cmd_group = std::make_shared<engine::terminal_command_group>();

	mTerminal_cmd_group->set_root_command("scene");

	mTerminal_cmd_group->add_command("reload",
		[&](const std::vector<engine::terminal_argument>& pArgs)->bool
	{
		return reload_scene();
	}, "- Reload the scene");

	mTerminal_cmd_group->add_command("load",
		[&](const engine::terminal_arglist& pArgs)->bool
	{
		if (pArgs.size() <= 0)
		{
			util::error("Not enough arguments");
			util::info("scene load <scene_name>");
			return false;
		}

		return load_scene(pArgs[0]);
	}, "<Scene Name> - Load a scene by name");

	mTerminal_cmd_group->add_command("create",
		[&](const engine::terminal_arglist& pArgs)->bool
	{
		if (pArgs.size() <= 0)
		{
			util::error("Not enough arguments");
			util::info("scene create <scene_name>");
			return false;
		}

		if (!create_scene(pArgs[0]))
			return false;

		return load_scene(pArgs[0]);
	}, "<Scene Name> - Create a new scene");

	pTerminal.add_group(mTerminal_cmd_group);
}
#endif

void scene::set_resource_manager(engine::resource_manager& pResource_manager)
{
	mResource_manager = &pResource_manager;
	mEntity_manager.set_resource_manager(pResource_manager);
}

bool scene::load_settings(const game_settings_loader& pSettings)
{
	util::info("Loading scene system...");

	mWorld_node.set_unit(pSettings.get_unit_pixels());

	mWorld_node.set_viewport(pSettings.get_screen_size());

	auto texture = mResource_manager->get_resource<engine::texture>(engine::resource_type::texture, pSettings.get_player_texture());
	if (!texture)
	{
		util::error("Could not load texture '" + pSettings.get_player_texture() + "' for player character");
		return false;
	}
	mPlayer.mSprite.set_texture(texture);
	mPlayer.set_cycle(character_entity::cycle::def);

	mBackground_music.set_root_directory(pSettings.get_music_path());

	util::info("Scene loaded");

	return true;
}

player_character& scene::get_player()
{
	return mPlayer;
}

void scene::tick(engine::controls &pControls)
{
	assert(get_renderer() != nullptr);
	mPlayer.movement(pControls, mCollision_system, get_renderer()->get_delta());
	update_focus();
	update_collision_interaction(pControls);
}

void scene::focus_player(bool pFocus)
{
	mFocus_player = pFocus;
}

void scene::set_resource_pack(engine::pack_stream_factory* pPack)
{
	mPack = pPack;
	mBackground_music.set_resource_pack(pPack);
}

void scene::script_set_focus(engine::fvector pPosition)
{
	mFocus_player = false;
	mWorld_node.set_focus(pPosition);
}

engine::fvector scene::script_get_focus()
{
	return mWorld_node.get_focus();
}

entity_reference scene::script_get_player()
{
	return mPlayer;
}

engine::fvector scene::script_get_boundary_position()
{
	return mWorld_node.get_boundary().get_offset();
}

engine::fvector scene::script_get_boundary_size()
{
	return mWorld_node.get_boundary().get_size();
}

void scene::script_set_boundary_size(engine::fvector pSize)
{
	mWorld_node.set_boundary(mWorld_node.get_boundary().set_size(pSize));
}

void scene::script_spawn_sound(const std::string & pName, float pVolume, float pPitch)
{
	auto sound = mResource_manager->get_resource<engine::sound_buffer>(engine::resource_type::sound, pName);
	if (!sound)
	{
		util::error("Could not spawn sound '" + pName + "'");
		return;
	}

	mSound_FX.spawn(sound, pVolume, pPitch);
}

engine::fvector scene::script_get_display_size()
{
	assert(get_renderer() != nullptr);
	return get_renderer()->get_target_size();
}

void scene::script_set_boundary_position(engine::fvector pPosition)
{
	mWorld_node.set_boundary(mWorld_node.get_boundary().set_offset(pPosition));
}

void scene::script_set_tile(const std::string& pAtlas, engine::fvector pPosition
	, int pLayer, int pRotation)
{
	mTilemap_manipulator.set_tile(pPosition, pLayer, pAtlas, pRotation);
	mTilemap_manipulator.update_display(mTilemap_display);
}

void scene::script_remove_tile(engine::fvector pPosition, int pLayer)
{
	mTilemap_manipulator.remove_tile(pPosition, pLayer);
	mTilemap_manipulator.update_display(mTilemap_display);
}

void scene::refresh_renderer(engine::renderer& pR)
{
	mWorld_node.set_viewport(pR.get_target_size());
	pR.add_object(mTilemap_display);
	pR.add_object(mPlayer);
	mColored_overlay.set_renderer(pR);
	mEntity_manager.set_renderer(pR);
}

void scene::update_focus()
{
	if (mFocus_player)
		mWorld_node.set_focus(mPlayer.get_position(mWorld_node));
}

void scene::update_collision_interaction(engine::controls & pControls)
{
	collision_box_container& container = mCollision_system.get_container();

	const auto collision_box = mPlayer.get_collision_box();

	// Check collision with triggers
	{
		auto triggers = container.collision(collision_box::type::trigger, collision_box);
		for (auto& i : triggers)
			std::dynamic_pointer_cast<trigger>(i)->call_function();
	}

	// Check collision with doors
	{
		const auto hit = container.first_collision(collision_box::type::door, collision_box);
		if (hit)
		{
			const auto hit_door = std::dynamic_pointer_cast<door>(hit);
			if (mEnd_functions.empty()) // No end functions to call
			{
				load_scene(hit_door->get_scene(), hit_door->get_destination());
			}
			else if (!mEnd_functions[0]->is_running())
			{
				mScript->abort_all();
				for (auto& i : mEnd_functions)
				{
					if (i->call())
					{
						i->set_arg(0, (void*)&hit_door->get_scene());
						i->set_arg(1, (void*)&hit_door->get_destination());
					}
				}
			}
		}
	}

	// Check collision with buttons on when "activate" is triggered
	if (pControls.is_triggered("activate"))
	{
		auto buttons = container.collision(collision_box::type::button, mPlayer.get_activation_point());
		for (auto& i : buttons)
			std::dynamic_pointer_cast<button>(i)->call_function();
	}
}

