#include "rpg.hpp"

using namespace rpg;

game::game()
{
	renderer = nullptr;
	lock_mc_movement = false;
	root.set_viewport_size({ 320, 256 });
}

void 
game::set_text_default(engine::text_node& n)
{
	n.set_color(255, 255, 255);
	n.set_size(32);
	n.set_scale(0.5);
}

entity*
game::find_entity(std::string name)
{
	for (auto &i : entities)
	{
		if (i.get_name() == name)
			return &i;
	}
	utility::error("Character '" + name + "' does not exist.");
	return nullptr;
}

void
game::clear_entities()
{
	for (auto &i = entities.begin(); i != entities.end(); i++)
	{
		if (!utility::get_shadow(*i))
			entities.erase(i);
	}
}

engine::node&
game::get_root()
{
	return root;
}

scene*
game::find_scene(const std::string name)
{
	for (auto &i : scenes)
	{
		if (i.name == name)
			return &i;
	}
	return nullptr;
}

void
game::clean_scene()
{
	tile_system.ground.clear_all();
	tile_system.misc.clear_all();
	close_narrative_box();
}

void
game::switch_scene(scene* nscene)
{
	c_scene = nscene;
}

utility::error
game::load_scene(std::string path)
{
	clean_scene();
	using namespace tinyxml2;

	bool scene_exists = false;

	XMLDocument doc;
	if (doc.LoadFile(path.c_str()))
		return "Could not load scene file at '" + path + "'";

	XMLElement* main_e = doc.FirstChildElement("scene");
	if (!main_e) return "Please add root node 'scene'. <scene>...</scene>";

	// Get name and check if scene is already loaded
	if (auto name = main_e->Attribute("name"))
	{
		if (auto existing = find_scene(name))
		{
			switch_scene(existing);
			scene_exists = true;
		}
		else{
			scenes.emplace_back();
			c_scene = &scenes.back();
			c_scene->name = name;
		}
	}
	else
		return "Error: Please provide scene name.\n";

	// Load collision boxes
	// Only loads once so the collision boxes that
	// have been used are preserved
	if (!scene_exists)
	{
		if (auto thing = main_e->FirstChildElement("collisionboxes"))
			c_scene->parse_collisionbox_xml(thing);
	}

	// Set Boundary
	if (auto b = main_e->FirstChildElement("boundary"))
		root.set_bounds_size(
	{
		b->FloatAttribute("w") * 32,
		b->FloatAttribute("h") * 32
	});

	// Parse events
	if (!c_scene->events.size())
		c_scene->parse_events_xml(main_e);

	// Trigger start event (optional)
	auto start_event = find_event("_start_");
	if (start_event)
		tracker.queue_event(start_event);

	if (auto entit = main_e->FirstChildElement("entities"))
	{
		clear_entities();
		if (load_entities_list(entit))
			return "Failed to load entities for scene";
	}
	
	// Set main character of scene
	if (auto mc = main_e->FirstChildElement("maincharacter"))
	{
		auto name = mc->GetText();
		if (!name) return "Name of entity for main character not specified.";
		set_maincharacter(name);
	}
	else return "Please specify main character by <maincharacter>...</maincharacter>";

	// Load tilemap
	if (auto enviro_tex = main_e->FirstChildElement("tilemap_texture"))
		if (auto name = enviro_tex->GetText())
		{
			auto tex = tm.get_texture(name);
			if (!tex) return 1;
			tile_system.ground.set_texture(*tex);
		}
	tile_system.ground.clear_all();
	XMLElement *tm_e = main_e->FirstChildElement("tilemap");
	while (tm_e)
	{
		if (tm_e->BoolAttribute("set"))
			load_tilemap_individual(tm_e, tm_e->IntAttribute("layer"));
		else
			load_tilemap(tm_e, tm_e->IntAttribute("layer"));
		tm_e = tm_e->NextSiblingElement("tilemap");
	}
	return 0;
}

interpretor::job_list*
game::find_event(std::string name)
{
	// Find event
	for (auto& i : c_scene->events)
	{
		if (i.name == name)
		{
			return &i.jobs;
		}
	}
	return nullptr;
}

void
game::trigger_control(control_type key)
{
	control[key] = true;
}

void
game::reset_control()
{
	for (int i = 0; i < CONTROL_COUNT; i++)
	{
		control[i] = false;
	}
}

bool
game::has_flag(std::string name)
{
	for (auto &i : flags)
	{
		if (i == name)
			return true;
	}
	return false;
}

void
game::open_narrative_box()
{
	narrative.box .set_visible(true);
	narrative.text.set_visible(true);
	lock_mc_movement = true;
	narrative.expression.set_visible(true);
}

void
game::close_narrative_box()
{
	narrative.box       .set_visible(false);
	narrative.cursor    .set_visible(false);
	narrative.text      .set_visible(false);
	narrative.expression.set_visible(false);
	narrative.expression.bind_client(nullptr);
	narrative.expression.set_visible(false);
	lock_mc_movement = false;
}

bool
game::check_event_collisionbox(int type, engine::fvector pos)
{
	for (auto &i : c_scene->collisionboxes)
	{
		if (i.type == type &&
			pos.x >= i.pos.x &&
			pos.y >= i.pos.y &&
			pos.x <= i.pos.x + i.size.x &&
			pos.y <= i.pos.y + i.size.y)
		{
			// Skip if flag does not exist
			if (!i.if_flag.empty() &&
				flags.find(i.if_flag) == flags.end())
				continue;

			// Skip if flag exist, otherwise create the flag and continue
			if (!i.bind_flag.empty() &&
				!flags.insert(i.bind_flag).second)
				continue;

			if (i.name.empty())
				tracker.call_event(&i.inline_event);
			else
			{
				auto nevent = find_event(i.name);
				if (nevent)
					tracker.call_event(nevent);
			}
			return true;
		}
	}
	return false;
}

bool
game::check_wall_collisionbox(engine::fvector pos, engine::fvector size)
{
	for (auto &i : c_scene->collisionboxes)
	{
		if (i.type == i.WALL &&
			pos.x + size.x >= i.pos.x &&
			pos.y + size.y >= i.pos.y &&
			pos.x <= i.pos.x + i.size.x &&
			pos.y <= i.pos.y + i.size.y)
			return true;
	}
	return false;
}

bool
game::is_mc_moving()
{
	if (control[control_type::LEFT])  return true;
	if (control[control_type::RIGHT]) return true;
	if (control[control_type::UP])    return true;
	if (control[control_type::DOWN])  return true;
	return false;
}

int
game::mc_movement()
{
	using namespace engine;
	if (!main_character || lock_mc_movement) return 1;
	float delta = frame_clock.get_elapse().s()*100;
	
	fvector mc_pos = main_character->get_relative_position();

	if (control[control_type::LEFT])
	{
		main_character->set_cycle(entity::LEFT);
		if (!check_wall_collisionbox(mc_pos - fvector(10, 8), { 10, 8 }))
			main_character->move_left(delta);
	}
	if (control[control_type::RIGHT])
	{
		main_character->set_cycle(entity::RIGHT);
		if (!check_wall_collisionbox(mc_pos - fvector(0, 8), { 10, 8 }))
			main_character->move_right(delta);
	}
	if (control[control_type::UP])
	{
		main_character->set_cycle(entity::UP);
		if (!check_wall_collisionbox(mc_pos - fvector(8, 16), { 16, 16 }))
			main_character->move_up(delta);
	}
	if (control[control_type::DOWN])
	{
		main_character->set_cycle(entity::DOWN);
		if (!check_wall_collisionbox(mc_pos - fvector(8, 0), { 16, 4 }))
			main_character->move_down(delta);
	}
	
	if (is_mc_moving() && !lock_mc_movement)
		check_event_collisionbox(scene::collisionbox::TOUCH_EVENT, mc_pos);
	else if (main_character->get_animation())
		main_character->get_animation()->restart();
	return 0;
}

int
game::tick_interpretor()
{
	// This is the insane interpretor for xml scenes :D
	do
	{
		using namespace rpg::interpretor;

		job_entry* job = tracker.get_job();

		if (!job)
		{
			close_narrative_box();
			return 1;
		}

		switch (job->op)
		{
		case job_op::SAY:
		{
			JOB_say* j = (JOB_say*)job;

			if (tracker.is_start())
			{
				// Setup expression
				// Moves narrative text to make room for expression
				if (narrative.speaker)
				{
					if (!j->expression.empty())
					{
						auto expr = expressions.find(j->expression);
						narrative.expression.bind_client(&expr->second);
						expr->second.start();
						narrative.text.set_relative_position({ 64, 5 });
					}

					if (auto expr = narrative.expression.get_client())
						expr->start();
					else
						narrative.text.set_relative_position({ 10, 5 });

					narrative.speaker->animation_start(entity::SPEECH);
				}

				// Setup narrative box
				if (!narrative.box.is_visible())
					open_narrative_box();

				j->c_char = 0;
				
				if (!j->append)
					narrative.text.set_text("");
			}

			// Instant revealing of text
			if (j->char_interval < 0)
			{
				sound.FX_dialog_click.play();
				if (j->append)
					narrative.text.set_text(narrative.text.get_text() + j->text);
				else
					narrative.text.set_text(j->text);
				tracker.next_job();
				break;
			}

			// Reveal text one by one
			if (j->clock.get_elapse().ms() >= j->char_interval)
			{
				if (j->c_char % DIALOG_FX_INTERVAL == 0) sound.FX_dialog_click.play(); // Play FX
				int amnt = 1 + (j->text[j->c_char] == ' ' ? 1 : 0); // Jump spaces (smoother to look at)
				narrative.text.append_text(j->text.substr(j->c_char, amnt));
				j->c_char += amnt;
				j->clock.restart();
			}

			tracker.wait_job();

			// Text Reveal has finished
			if (j->c_char >= j->text.size())
			{
				tracker.next_job();
				if (auto expr = narrative.expression.get_client())
					expr->stop();
				if (narrative.speaker)
					narrative.speaker->animation_stop(entity::SPEECH);
			}
			break;
		}
		case job_op::WAIT:
		{
			JOB_wait* j = (JOB_wait*)job;
			if (tracker.is_start())
				j->clock.restart();
			tracker.wait_job();

			if (j->clock.get_elapse().ms() >= j->ms)
				tracker.next_job();
			break;
		}
		case job_op::WAITFORKEY:
		{
			if (tracker.is_start())
				narrative.cursor.set_visible(true);
			tracker.wait_job();
			if (control[control_type::ACTIVATE])
			{
				narrative.cursor.set_visible(false);
				tracker.next_job();
			}
			break;
		}
		case job_op::HIDEBOX:
		{
			close_narrative_box();
			tracker.next_job();
			break;
		}
		case job_op::SELECTION:
		{
			JOB_selection* j = (JOB_selection*)job;
			if (tracker.is_start())
			{
				narrative.option1.set_text("*" + j->opt1 + "   " + j->opt2);
				narrative.option1.set_visible(true);
				narrative.option2.set_visible(true);
			}

			// Select left option
			if (control[control_type::SELECT_PREV])
			{
				narrative.option1.set_text("*" + j->opt1 + "   " + j->opt2);
				j->sel = 0;
			}

			// Select right option
			if (control[control_type::SELECT_NEXT])
			{
				narrative.option1.set_text(" " + j->opt1 + "  *" + j->opt2);
				j->sel = 1;
			}

			// Select option
			if (control[control_type::ACTIVATE])
			{
				narrative.option1.set_visible(false);
				narrative.option2.set_visible(false);
				auto nevent = find_event(j->event[j->sel]);
				if (nevent)
					tracker.call_event(nevent);
				else
					utility::error("Event '" + j->event[j->sel] + "' not found");
			}
			else
				tracker.wait_job();
			break;
		}
		case job_op::ENTITY_CURRENT:
		{
			JOB_entity_current* j = (JOB_entity_current*)job;
			tracker.next_job();

			if (j->name.empty())
			{
				narrative.speaker = nullptr;
				break;
			}

			entity* nentity = find_entity(j->name);
			if (!nentity) return 1;
			if (narrative.speaker) narrative.expression.set_visible(false);
			narrative.speaker = nentity;
			break;
		}
		case job_op::ENTITY_MOVE:
		{
			JOB_entity_move* j = (JOB_entity_move*)job;
			if (!narrative.speaker)
			{
				tracker.next_job();
				break;
			}
			if (j->set)
			{
				narrative.speaker->set_relative_position(j->move * 32);
				tracker.next_job();
				break;
			}

			if (tracker.is_start())
				j->clock.restart();

			j->calculate();

			// todo

			tracker.next_job();
			break;
		}
		case job_op::ENTITY_SETCYCLEGROUP:
		{
			if (narrative.speaker)
			{
				JOB_entity_setcyclegroup* j = (JOB_entity_setcyclegroup*)job;
				narrative.speaker->set_cycle_group(j->group_name);
			}
			tracker.next_job();
			break;
		}
		case job_op::ENTITY_SETANIMATION:
		{
			if (narrative.speaker)
			{
				JOB_entity_setanimation* j = (JOB_entity_setanimation*)job;
				narrative.speaker->set_cycle_animation(j->name, entity::MISC);
				narrative.speaker->set_cycle(entity::MISC);
			}
			tracker.next_job();
			break;
		}
		case job_op::ENTITY_ANIMATIONSTART:
		{
			if (narrative.speaker)
			{
				JOB_entity_animationstart* j = (JOB_entity_animationstart*)job;
				narrative.speaker->animation_start(entity::USER_TRIGGERED, j->loop);
			}
			tracker.next_job();
			break;
		}
		case job_op::ENTITY_ANIMATIONSTOP:
		{
			if (narrative.speaker)
			{
				narrative.speaker->animation_stop(entity::USER_TRIGGERED);
			}
			tracker.next_job();
			break;
		}
		case job_op::ENTITY_SETDIRECTION:
		{
			if (narrative.speaker)
			{
				JOB_entity_setdirection* j = (JOB_entity_setdirection*)job;
				narrative.speaker->set_cycle(j->direction);
			}
			tracker.next_job();
			break;
		}
		case job_op::FLAG_SET:
		{
			JOB_flag_set* j = (JOB_flag_set*)job;
			flags.insert(j->name);
			tracker.next_job();
			break;
		}
		case job_op::FLAG_UNSET:
		{
			JOB_flag_unset* j = (JOB_flag_unset*)job;
			auto & c = flags.find(j->name);
			if (c != flags.end())
				flags.erase(c);
			tracker.next_job();
			break;
		}
		case job_op::FLAG_IF:
		{
			JOB_flag_if* j = (JOB_flag_if*)job;
			if (flags.find(j->name) != flags.end()) // Check flag existance
			{
				if (j->inline_event.size())
					tracker.call_event(&j->inline_event); // Trigger inline event
				else if (auto nevent = find_event(j->event)) // Trigger external event
					tracker.call_event(nevent);
			}
			else
				tracker.next_job();
			break;
		}
		case job_op::FLAG_EXITIF:
		{
			JOB_flag_exitif* j = (JOB_flag_exitif*)job;
			if (has_flag(j->name))
				tracker.cancel_event();
			else
				tracker.next_job();
			break;
		}
		case job_op::FLAG_ONCE:
		{
			JOB_flag_once* j = (JOB_flag_once*)job;
			if (has_flag(j->name))
				tracker.cancel_event();
			else
			{
				flags.insert(j->name);
				tracker.next_job();
			}
			break;
		}
		case job_op::MUSIC_SET:
		{
			JOB_music_set* j = (JOB_music_set*)job;
			if (j->path != utility::get_shadow(sound.bg_music))
			{
				if (sound.bg_music.is_playing())
					sound.bg_music.stop();
				sound.bg_music.open(j->path);
				sound.bg_music.play();
				sound.bg_music.set_loop(j->loop);
				utility::get_shadow(sound.bg_music) = j->path;
			}
			tracker.next_job();
			break;
		}
		case job_op::MUSIC_STOP:
		{
			sound.bg_music.stop();
			tracker.next_job();
			break;
		}
		case job_op::MUSIC_PAUSE:
		{
			sound.bg_music.pause();
			tracker.next_job();
			break;
		}
		case job_op::MUSIC_PLAY:
		{
			sound.bg_music.play();
			tracker.next_job();
			break;
		}
		case job_op::SCENE_LOAD:
		{
			JOB_scene_load* j = (JOB_scene_load*)job;
			load_scene(j->path);
			tracker.next_job();
			break;
		}
		case job_op::TILE_REPLACE:
		{
			JOB_tile_replace* j = (JOB_tile_replace*)job;
			engine::ivector pos;
			tile_system.ground.set_layer(j->layer);
			for (pos.x = j->pos1.x; pos.x < j->pos2.x; pos.x++)
			{
				for (pos.y = j->pos1.y; pos.y < j->pos2.y; pos.y++)
				{
					tile_system.ground.set_tile(pos, j->name, j->rot);
				}
			}
			tracker.next_job();
			break;
		}
		case job_op::FX_FADEIN:
		{
			JOB_fx_fade* j = (JOB_fx_fade*)job;
			if (tracker.is_start())
			{
				lock_mc_movement = true;
				j->clock.restart();
			}
			engine::color c = graphic_fx.fade_overlap.get_color();
			engine::time_t t = j->clock.get_elapse().s();
			if (t < FADE_DURATION && c.a > 0)
			{
				c.a = 255 * (1 - (t / FADE_DURATION));
				graphic_fx.fade_overlap.set_color(c);
				tracker.wait_job();
			}
			else
			{
				graphic_fx.fade_overlap.set_color((c.a = 0, c));
				lock_mc_movement = false;
				tracker.next_job();
			}
			break;
		}
		case job_op::FX_FADEOUT:
		{
			JOB_fx_fade* j = (JOB_fx_fade*)job;
			if (tracker.is_start())
			{
				lock_mc_movement = true;
				j->clock.restart();
			}
			engine::color c = graphic_fx.fade_overlap.get_color();
			engine::time_t t = j->clock.get_elapse().s();
			if (t < FADE_DURATION && c.a < 255)
			{
				c.a = 255 * (t / FADE_DURATION);
				graphic_fx.fade_overlap.set_color(c);
				tracker.wait_job();
			}
			else
			{
				graphic_fx.fade_overlap.set_color((c.a = 255, c));
				tracker.next_job();
				lock_mc_movement = false;
			}
			break;
		}

		default:
		{
			std::cout << "Error: Unsupported opcode has been requested (Means bad!). '" << job->op << "'\n";
			tracker.next_job();
			break; 
		}
		}
	} while (tracker.is_start());
	return 0;
}

int
game::tick(engine::renderer& _r)
{
	mc_movement();

	if (control[ACTIVATE] && !lock_mc_movement)
		check_event_collisionbox(scene::collisionbox::BUTTON, main_character->get_activate_point());

	tick_interpretor();

	// Update pan
	root.update_origin(main_character->get_relative_position() - engine::fvector(0, 16));

	frame_clock.restart();
	reset_control();
	return 0;
}

utility::error
game::load_textures(std::string path)
{
	if (!renderer) return 1;
	tm.load_settings(path);
	
	{ // Narrative Box
		engine::texture *narrativebox_tx = tm.get_texture("NarrativeBox");
		if (!narrativebox_tx) return "Failed to load Narrative Box";
		narrative.box.set_texture(*narrativebox_tx,"NarrativeBox");
		narrative.box.set_position({ 32, 150 });
		narrative.box.set_visible(false);
		narrative.box.set_depth(NARRATIVE_BOX_DEPTH);
		renderer->add_client(&narrative.box);
	}
	{ // Narrative Cursor (the arrow thing)
		engine::texture *cursor_tx = tm.get_texture("NarrativeBox");
		if (!cursor_tx) return "Failed to load Narrative cursor";
		narrative.cursor.set_texture(*cursor_tx, "SelectCursor");
		narrative.cursor.set_parent(narrative.box);
		narrative.cursor.set_relative_position({ 235, 55 });
		narrative.cursor.set_visible(false);
		narrative.cursor.set_depth(-1);
		renderer->add_client(&narrative.cursor);
	}

	font.load("data/font.ttf"); // Font
	
	{ // Narrative text and option elements
		narrative.text.set_depth(NARRATIVE_TEXT_DEPTH);
		narrative.option1.set_depth(NARRATIVE_TEXT_DEPTH);

		narrative.text.set_font(font);
		narrative.option1.set_font(font);
		narrative.option2.set_font(font);

		set_text_default(narrative.text);
		set_text_default(narrative.option1);
		set_text_default(narrative.option2);

		narrative.box.add_child(narrative.text);
		narrative.box.add_child(narrative.option1);
		narrative.box.add_child(narrative.option2);

		narrative.text.set_relative_position({ 64, 5 });
		narrative.option1.set_relative_position({ 70, 50 });
		narrative.option2.set_relative_position({ 130, 55 });

		narrative.text.set_visible(false);
		narrative.option1.set_visible(false);
		narrative.option2.set_visible(false);

		renderer->add_client(&narrative.text);
		renderer->add_client(&narrative.option1);
		renderer->add_client(&narrative.option2);
	}

	{ // Setup tile system
		tile_system.ground.set_tile_size({ 32, 32 });
		tile_system.ground.set_depth(TILE_DEPTH_RANGE_MAX);
		root.add_child(tile_system.ground);
		renderer->add_client(&tile_system.ground);

		tile_system.misc.set_tile_size(TILE_SIZE);
		root.add_child(tile_system.misc);
	}
	{
		narrative.expression.set_visible(false);
		narrative.expression.set_depth(NARRATIVE_TEXT_DEPTH);
		renderer->add_client(&narrative.expression);
	}
	{
		graphic_fx.fade_overlap.set_color({ 0, 0, 0, 0 });
		graphic_fx.fade_overlap.set_size(renderer->get_size());
		graphic_fx.fade_overlap.set_depth(ABSOLUTE_OVERLAP_DEPTH);
		renderer->add_client(&graphic_fx.fade_overlap);
	}
	{
		auto& nf0 = graphic_fx.narrow_focus[0];
		auto& nf1 = graphic_fx.narrow_focus[1];
		nf0.set_color({ 0, 0, 0, 255 });
		nf1.set_color({ 0, 0, 0, 255 });
		nf0.set_size({ renderer->get_size().x, 100 });
		nf1.set_size({ renderer->get_size().x, 100 });
		nf1.set_position({ 0, renderer->get_size().y - 100 });
		nf0.set_visible(false);
		nf1.set_visible(false);
		nf0.set_depth(FX_DEPTH);
		nf1.set_depth(FX_DEPTH);
		renderer->add_client(&nf0);
		renderer->add_client(&nf1);
	}
	{
		sound.buffers.dialog_click_buf.load("data/sound/dialog.ogg");
		sound.FX_dialog_click.set_buffer(sound.buffers.dialog_click_buf);
	}
	return utility::error::NOERROR;
}

texture_manager& 
game::get_texture_manager()
{
	return tm;
}

void
game::set_renderer(engine::renderer& r)
{
	renderer = &r;
}

utility::error
game::load_entity_anim(
	tinyxml2::XMLElement* e,
	entity& c)
{
	if (!renderer) return 1;

	auto ele = e->FirstChildElement();
	while (ele)
	{
		c.world.emplace_back();
		entity::animation& na = c.world.back();

		na.node.set_visible(true);

		int frames    = ele->IntAttribute("frames");
		int interval  = ele->IntAttribute("interval");
		bool loop     = ele->BoolAttribute("loop");
		auto atlas    = ele->Attribute("atlas");
		auto tex      = ele->Attribute("tex");
		auto type     = ele->Attribute("type");

		if (!tex) return "Please provide texture attibute for character";
		if (!atlas) return "Please provide atlas attribute for character";

		if (type)
		{
			std::string play_type = type;
			if (play_type       == "constant")
				na.type = entity::CONSTANT;
			else if (play_type == "speech")
				na.type = entity::SPEECH;
			else if (play_type == "walk")
				na.type = entity::WALK;
			else if (play_type == "user")
				na.type = entity::USER_TRIGGERED;
			else
				return "Invalid play type '" + play_type + "'";
		}else
			na.type = entity::WALK; // Default walk

		auto t = tm.get_texture(tex);
		if (!t) return "Texture '" + std::string(tex) + "' not found";

		na.node.set_interval(interval);
		na.node.set_loop(loop);
		na.node.set_texture(*t);
		na.node.generate_sequence(
			(frames <= 0 ? 1 : frames), // Default one frame
			*t,
			atlas);

		na.node.set_parent(c);

		na.name = ele->Name();

		ele = ele->NextSiblingElement();
	}
	return utility::error::NOERROR;
}

utility::error
game::load_entities_list(tinyxml2::XMLElement* e, bool is_global_entity)
{
	using namespace tinyxml2;
	auto ele = e->FirstChildElement();
	while (ele)
	{
		auto name = ele->Name();
		if (!name) return "Please specify name of entity";
		auto path = ele->Attribute("path");
		if (!path) return "Please specify path to entity file";
		load_entity(path, is_global_entity);
		entities.back().set_name(name);
		ele = ele->NextSiblingElement();
	}
	return 0;
}

utility::error
game::load_entity(std::string path, bool is_global_entity)
{
	using namespace tinyxml2;

	entities.emplace_back();
	auto &nentity = entities.back();
	utility::get_shadow(nentity) = is_global_entity;

	XMLDocument doc;
	if (doc.LoadFile(path.c_str()))
		return "Failed to load entity file '" + path + "'";

	XMLElement* main_e = doc.FirstChildElement("entity");
	if (!main_e) return "Please add root node. <entity>...</entity>";

	/*if (auto _char_name = main_e->FirstChildElement("name"))
		nentity.set_name(_char_name->GetText());
	else
		return "Please specify name or entity. <name>...</name>";*/
	
	if (auto world_e = main_e->FirstChildElement("animations"))
		load_entity_anim(world_e, nentity);
	else
		return "Please add animations section. <animations>...</animations>";

	// Set character defaults
	auto _err = nentity.set_cycle_group("default");
	if (_err) return _err;

	nentity.set_cycle(entity::DEFAULT);
	nentity.set_depth(CHARACTER_DEPTH);

	root.add_child(nentity);
	nentity.set_relative_position({ 100, 50 });
	renderer->add_client(&nentity);
	return 0;
}

int
game::set_maincharacter(std::string name)
{
	auto* mc = find_entity(name);
	if (!mc) return 1;
	main_character = mc;
	return 0;
}

int
game::load_tilemap_individual(tinyxml2::XMLElement* e, size_t layer)
{
	using namespace tinyxml2;
	auto &ground = tile_system.ground; // Convenience

	ground.set_layer(layer);

	XMLElement* c = e->FirstChildElement();
	while (c)
	{
		std::string name = c->Name();

		int rfill = c->IntAttribute("f");
		int dfill = c->IntAttribute("fd");
		int rot = c->IntAttribute("r");

		engine::ivector pos(
			c->IntAttribute("x"),
			c->IntAttribute("y"));

		// Is wall
		if (c->BoolAttribute("w"))
		{
			scene::collisionbox nbox;
			nbox.pos = pos * TILE_SIZE;
			nbox.size = engine::ivector(rfill ? rfill : 1, dfill ? dfill : 1) * TILE_SIZE; // Default at one; convert to pixels
			nbox.type = nbox.WALL;
			c_scene->collisionboxes.push_back(nbox);
		}

		// Fill row
		if (rfill)
		{
			for (int i = 0; i < rfill; i++)
			{
				ground.set_tile(pos, name, rot);
				++pos.x;
			}
		}

		// Fill column
		else if (dfill)
		{
			for (int i = 0; i < dfill; i++)
			{
				ground.set_tile(pos, name, rot);
				++pos.y;
			}
		}
		else
		{  // No fill
			ground.set_tile(pos, name, rot);
		}

		c = c->NextSiblingElement();
	}
	return 0;
}

// Will be replaced with one above, just backwords compatability right now...
int
game::load_tilemap(tinyxml2::XMLElement* e, size_t layer)
{
	using namespace tinyxml2;

	auto &ground = tile_system.ground; // Convenience

	ground.set_layer(layer);

	engine::ivector pos;
	XMLElement* c = e->FirstChildElement();
	while (c)
	{
		std::string name = c->Name();

		// skip
		if (name == "_s")
		{
			int amnt = c->IntAttribute("a");
			pos.x += (amnt ? amnt : 1);
		}

		// end row
		else if (name == "_e")
		{
			int amnt = c->IntAttribute("a");
			pos = { 0, pos.y + (amnt > 0 ? amnt : 1) };
		}

		// set tile
		else
		{
			int rfill    = c->IntAttribute("f");
			int dfill    = c->IntAttribute("fd");
			int rot      = c->IntAttribute("r");

			// Is wall
			if (c->BoolAttribute("w"))
			{
				scene::collisionbox nbox;
				nbox.pos = pos * TILE_SIZE;
				nbox.size = engine::ivector(rfill ? rfill : 1, dfill ? dfill : 1) * TILE_SIZE; // Default at one; convert to pixels
				nbox.type = nbox.WALL;
				c_scene->collisionboxes.push_back(nbox);
			}

			// Fill row
			if (rfill)
			{
				for (int i = 0; i < rfill; i++)
				{
					ground.set_tile(pos, name, rot);
					++pos.x;
				}
			}

			// Fill column
			else if (dfill)
			{
				engine::ivector fpos = pos;
				for (int i = 0; i < dfill; i++)
				{
					ground.set_tile(fpos, name, rot);
					++fpos.y;
				}
				++pos.x;
			}
			else
			{  // No fill
				ground.set_tile(pos, name, rot);
				++pos.x;
			}
		}
		c = c->NextSiblingElement();
	}
	return 0;
}

utility::error
game::load_game(std::string path)
{
	using namespace tinyxml2;

	bool scene_exists = false;

	XMLDocument doc;
	if (doc.LoadFile(path.c_str()))
		return "Failed to open game.xml";

	XMLElement* main_e = doc.FirstChildElement("game");
	if (!main_e)
		return "Please add root element named 'game'. <game>...</game>";

	// Textures
	XMLElement* tex_e = main_e->FirstChildElement("textures");
	if (!tex_e)
		return "Error: Please specify the texture file. '<textures path=\"\"/>'\n";
	auto texture_path = tex_e->Attribute("path");
	if (load_textures(texture_path)) return "Failed to load textures";

	// Global Entities
	XMLElement* global_entities = main_e->FirstChildElement("global_entities");
	if (global_entities)
		load_entities_list(global_entities, true);

	// Start scene
	XMLElement* start_e = main_e->FirstChildElement("start_scene");
	if (!start_e)
		return "Please specify the starting scene. '<start_scene path=\"\"/>'\n";
	if (load_scene(start_e->Attribute("path"))) return "Failed to load starting scene";

	return 0;
}