#ifndef RPG_HPP
#define RPG_HPP

#include "renderer.hpp"
#include "utility.hpp"
#include "rpg_managers.hpp"
#include "rpg_config.hpp"
#include "your_soul.hpp"
#include "tinyxml2\tinyxml2.h"

#include <set>
#include <list>
#include <string>
#include <array>
#include <functional>

#include <angelscript.h>
#include <angelscript/add_on/scriptbuilder/scriptbuilder.h>

namespace rpg{

class panning_node :
	public engine::node
{
	engine::fvector boundary, viewport;
public:
	void set_boundary(engine::fvector a);
	void set_viewport(engine::fvector a);
	void set_focus(engine::fvector pos);
};

template<typename T>
class node_list :
	public engine::node
{
	std::list<T> items;
public:

	void clear()
	{
		items.clear();
	}

	template<class... T_ARG>
	auto add_item(T_ARG&&... arg)
	{
		items.emplace_back(std::forward<T_ARG>(arg)...);
		add_child(items.back());
		return items.back();
	}

	auto add_item()
	{
		items.emplace_back();
		add_child(items.back());
		return items.back();
	}

	auto begin() { return items.begin(); }
	auto end()   { return items.end();   }
	auto back()  { return items.back();  }
};

class flag_container
{
	std::set<std::string> flags;
public:
	bool set_flag(const std::string& name);
	bool unset_flag(const std::string& name);
	bool has_flag(const std::string& name);
};

class controls
{
	std::array<bool, 7> c_controls;
public:
	enum class control
	{
		activate,
		left,
		right,
		up,
		down,
		select_next,
		select_previous
	};
	controls();
	void trigger(control c);
	bool is_triggered(control c);
	void reset();
};

class entity :
	public engine::render_client,
	public engine::node,
	public   util::named
{
	struct entity_animation :
		public util::named
	{
		engine::animation anim;
		int type;
	};

	engine::animation_node node;
	std::list<entity_animation> animations;
	entity_animation *c_animation;

	bool dynamic_depth;
public:
	enum e_type
	{
		movement,
		constant,
		speech,
		user
	};
	entity();
	void play_withtype(e_type type);
	void stop_withtype(e_type type);
	void tick_withtype(e_type type);
	bool set_animation(std::string name);
	int draw(engine::renderer &_r);
	util::error load_entity(std::string path, texture_manager& tm);
	void set_dynamic_depth(bool a);
protected:
	util::error load_animations(tinyxml2::XMLElement* e, texture_manager& tm);
	util::error load_xml_animation(tinyxml2::XMLElement* ele, engine::animation &anim, texture_manager& tm);
};

class character :
	public entity
{
	std::string cyclegroup;
	std::string cycle;
	float move_speed;
public:

	enum struct e_cycle
	{
		default,
		left,
		right,
		up,
		down,
		idle
	};

	character();
	void set_cycle_group(std::string name);
	void set_cycle(std::string name);
	void set_cycle(e_cycle type);

	void  set_speed(float f);
	float get_speed();
};

class narrative_dialog :
	public engine::render_client
{
	engine::sprite_node box;
	engine::sprite_node cursor;
	engine::text_node   text;
	engine::font        font;
	engine::clock       timer;

	bool        revealing;
	size_t      c_char;
	std::string full_text;
	float       interval;

	util::error load_box(tinyxml2::XMLElement* e, texture_manager& tm);
	util::error load_font(tinyxml2::XMLElement* e);

public:
	enum class position
	{
		top,
		bottom
	};

	narrative_dialog();

	void set_box_position(position pos);

	bool is_revealing();

	void reveal_text(const std::string& str, bool append = false);
	void instant_text(std::string str, bool append = false);

	void show_box();
	void hide_box();
	bool is_box_open();

	void set_interval(float ms);

	util::error load_narrative(tinyxml2::XMLElement* e, texture_manager& tm);

	int draw(engine::renderer &r);

protected:
	void refresh_renderer(engine::renderer& r);
};

class collision_system
{
public:
	struct collision_box
		: engine::frect
	{
		std::string invalid_on_flag;
		std::string spawn_flag;
		bool valid;
		collision_box() : valid(true){}
	};

	struct trigger : public collision_box
	{
		std::string event;
	};

	struct door : public collision_box
	{
		std::string name;
		std::string scene_path;
		std::string destination;
	};

	collision_box* wall_collision(const engine::frect& r);
	door*          door_collision(const engine::fvector& r);
	trigger*       trigger_collision(const engine::fvector& pos);
	trigger*       button_collision(const engine::fvector& pos);

	engine::fvector get_door_entry(std::string name);

	void validate_collisionbox(collision_box& cb, flag_container& flags);
	void validate_all(flag_container& flags);

	void add_wall(engine::frect r);
	void clear();
	util::error load_collision_boxes(tinyxml2::XMLElement* e, flag_container& flags);

private:
	std::list<collision_box> walls;
	std::list<door> doors;
	std::list<trigger> triggers;
	std::list<trigger> buttons;
};

class tilemap :
	public engine::render_client,
	public engine::node
{
	engine::tile_node node;
	util::error load_tilemap(tinyxml2::XMLElement* e, collision_system& collision, size_t layer);
public:
	tilemap();
	void set_texture(engine::texture& t);
	util::error load_scene_tilemap(tinyxml2::XMLElement* e, collision_system& collision);
	void clear();
	int draw(engine::renderer &_r);
};

class player_character :
	public character
{
public:
	enum class direction
	{
		other,
		up,
		down,
		left,
		right
	};

	player_character();
	void set_locked(bool l);
	bool is_locked();
	void movement(controls &c, collision_system& collision, float delta);
	engine::fvector get_activation_point(float distance = 16);

private:
	bool locked;
	direction facing_direction;
};



class scene :
	public engine::render_proxy,
	public engine::node
{
	tilemap tilemap;
	collision_system collision;

	node_list<character> characters;
	node_list<entity> entities;

public:
	scene();
	collision_system& get_collision_system();
	character* find_character(std::string name);
	entity* find_entity(std::string name);

	void clean_scene();
	util::error load_entities(tinyxml2::XMLElement* e, texture_manager& tm);
	util::error load_characters(tinyxml2::XMLElement* e, texture_manager& tm);
	util::error load_scene(std::string path, flag_container& flags, texture_manager& tm);

protected:
	void refresh_renderer(engine::renderer& _r);
};

class angelscript
{
	typedef void(angelscript::loopfunc_t)();
	std::function<loopfunc_t> loop_function;
	template<typename... T_args>
	void registerloop(void(angelscript::*func)(T_args...), T_args&&... args)
	{
		loop_function = std::bind(func, this, std::forward<T_args>(args)...);
	}

	void tick_wait();

	asIScriptEngine* as_engine;
	void message_callback(const asSMessageInfo * msg);

	asIScriptContext *ctx;
	asIScriptModule *scene_module;

	void dprint(std::string &msg);

	void cmd_say(std::string &message);
	void cmd_wait(float seconds);
	void cmd_keywait();

	void cmd_yield();

public:
	angelscript();
	~angelscript();
	util::error load_scene_script(std::string path);
	void add_function(const char* decl, const asSFuncPtr & ptr, void* instance);
	void add_function(const char* decl, const asSFuncPtr & ptr);
	void call_event_function(std::string name);
	void wait();
	int tick();
};

class game :
	public engine::render_proxy
{
	scene game_scene;
	player_character player;
	texture_manager  textures;
	flag_container   flags;
	narrative_dialog narrative;
	panning_node     root_node;
	engine::clock    frameclock;
	angelscript      scripting;
	controls         c_controls;

	void player_scene_interact(controls& con);

	void load_script_functions();

public:
	game();
	scene& get_scene();
	util::error load_game(std::string path);
	void tick(controls& con);

protected:
	void refresh_renderer(engine::renderer& r);
};

}

#endif