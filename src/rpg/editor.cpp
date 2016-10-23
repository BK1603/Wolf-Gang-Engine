
#include <rpg/editor.hpp>

using namespace editors;

void tgui_list_layout::updateWidgetPositions()
{
	auto& widgets = getWidgets();

	Widget::Ptr last;
	for (auto i : widgets)
	{
		if (last)
		{
			auto pos = last->getPosition();
			pos.y += last->getSize().y;
			i->setPosition(pos.x, pos.y);
		}

		last = i;
	}
}

void editor_gui::initualize()
{
	mLayout = std::make_shared<tgui_list_layout>();
	mLayout->setBackgroundColor({ 0, 0, 0,  90});
	mLayout->setSize(300, 500);
	mLayout->hide();
	mTgui.add(mLayout);

	mLb_mode = std::make_shared<tgui::Label>();
	mLb_mode->setMaximumTextWidth(0);
	mLb_mode->setTextColor({ 200, 200, 200, 255 });
	mLb_mode->setText("Mode: Normal Play");
	mLayout->add(mLb_mode);

	mLb_fps = tgui::Label::copy(mLb_mode);
	mLb_fps->setText("FPS: n/a");
	mLayout->add(mLb_fps);

	mLb_mouse = tgui::Label::copy(mLb_mode);
	mLb_mouse->setText("(n/a, n/a) : (n/a, n/a)");
	mLb_mouse->setTextSize(14);
	mLayout->add(mLb_mouse);

	mEditor_layout = std::make_shared<tgui_list_layout>();
	mEditor_layout->setSize(mLayout->getSize().x, 400);
	mEditor_layout->setBackgroundColor({ 0, 0, 0, 0 });
	mLayout->add(mEditor_layout);
}

void editor_gui::clear()
{
	mEditor_layout->removeAllWidgets();
}

tgui::Label::Ptr editor_gui::add_label(const std::string & pText)
{
	auto nlb = tgui::Label::copy(mLb_mode);
	nlb->setText(pText);
	nlb->setTextStyle(sf::Text::Bold);
	mEditor_layout->add(nlb);
	return nlb;
}

tgui::TextBox::Ptr editors::editor_gui::add_textbox()
{
	auto ntb = std::make_shared<tgui::TextBox>();
	mEditor_layout->add(ntb);
	return ntb;
}

void editor_gui::update_camera_position(engine::fvector pPosition)
{
	mCamera_offset = pPosition;
}

void editor_gui::refresh_renderer(engine::renderer & pR)
{
	pR.set_gui(&mTgui);
}

int editor_gui::draw(engine::renderer& pR)
{
	if (pR.is_key_down(engine::renderer::key_type::LControl)
	&&  pR.is_key_pressed(engine::renderer::key_type::E))
	{
		if (mLayout->isVisible())
			mLayout->hide();
		else
			mLayout->show();
	}

	mUpdate_timer += pR.get_delta();
	if (mUpdate_timer >= 0.5f)
	{
		auto mouse_position = pR.get_mouse_position(mCamera_offset);
		std::string position;
		position += "(";
		//position += std::to_string(mouse_position.x);
		//position += ", ";
		//position += std::to_string(mouse_position.y);
		//position += ") : (";
		position += std::to_string(static_cast<int>(std::floor(mouse_position.x / 32)));
		position += ", ";
		position += std::to_string(static_cast<int>(std::floor(mouse_position.y / 32)));
		position += ")";
		mLb_mouse->setText(position);

		mLb_fps->setText("FPS: " + std::to_string(pR.get_fps()));

		mUpdate_timer = 0;
	}
	return 0;
}

void editor::set_editor_gui(editor_gui & pEditor_gui)
{
	pEditor_gui.clear();
	mEditor_gui = &pEditor_gui;
	setup_editor(pEditor_gui);
}

void editor::set_texture_manager(rpg::texture_manager & pTexture_manager)
{
	mTexture_manager = &pTexture_manager;
}

// ##########
// tilemap_editor
// ##########

tilemap_editor::tilemap_editor()
{
	set_depth(-1000);

	mTilemap_display.set_parent(*this);
	mTilemap_display.set_depth(-1001);

	mBlackout.set_color({ 0, 0, 0, 255 });
	mBlackout.set_size({ 1000, 1000 });
	mBlackout.set_depth(-1000);

	setup_lines();

	mPreview.set_color({ 255, 255, 255, 150 });
	mPreview.set_parent(*this);

	mCurrent_tile = 0;
	mRotation = 0;
	mLayer = 0;

	mIs_highlight = false;
}

int tilemap_editor::open_scene(std::string pPath)
{
	if (mLoader.load(pPath))
	{
		util::error("Could not load scene tilemap to edit");
		return 1;
	}
	
	mTexture = mTexture_manager->get_texture(mLoader.get_tilemap_texture());
	if (!mTexture)
	{
		util::error("Invalid tilemap texture");
		return 1;
	}
	mTilemap_display.set_texture(*mTexture);

	mTilemap_loader.load_tilemap_xml(mLoader.get_tilemap());
	mTilemap_loader.update_display(mTilemap_display);

	mTile_list = std::move(mTexture->compile_list());
	assert(mTile_list.size() != 0);

	update_preview();
	update_labels();

	update_lines(mLoader.get_boundary());

	return 0;
}

int tilemap_editor::draw(engine::renderer & pR)
{
	if (pR.is_key_down(engine::renderer::key_type::Return))
		save();

	const engine::fvector mouse_position = pR.get_mouse_position(mTilemap_display.get_exact_position());

	engine::fvector tile_position_exact;
	if (mPreview.get_size() == engine::fvector(16, 16)) // Half-size tile
		tile_position_exact = (mouse_position / 16) / 2;
	else
		tile_position_exact = (mouse_position / 32);

	const engine::fvector tile_position = tile_position_exact.floor();

	const bool control_add_tile    = pR.is_mouse_down(engine::renderer::mouse_button::mouse_left);
	const bool control_remove_tile = pR.is_mouse_down(engine::renderer::mouse_button::mouse_right);
	if (!control_add_tile && !control_remove_tile)
		last_tile = { -10000000, -1000000 };

	// Add tile
	if (control_add_tile
		&& last_tile != tile_position)
	{
		assert(mTile_list.size() != 0);
		last_tile = tile_position;

		mTilemap_loader.explode_tile(tile_position, mLayer);
		mTilemap_loader.set_tile(tile_position, mLayer, mTile_list[mCurrent_tile], mRotation);
		mTilemap_loader.update_display(mTilemap_display);
		update_highlight();
	}

	// Remove tile
	if (control_remove_tile
		&& last_tile != tile_position)
	{
		assert(mTile_list.size() != 0);
		last_tile = tile_position;

		mTilemap_loader.explode_tile(tile_position, mLayer);
		mTilemap_loader.remove_tile(tile_position, mLayer);
		mTilemap_loader.update_display(mTilemap_display);
		update_highlight();
	}

	// Copy tile
	if (pR.is_mouse_down(engine::renderer::mouse_button::mouse_middle))
	{
		std::string atlas = mTilemap_loader.find_tile_name(tile_position_exact, mLayer);
		if (!atlas.empty())
			for (size_t i = 0; i < mTile_list.size(); i++) // Find tile in tile_list and set it as current tile
				if (mTile_list[i] == atlas)
				{
					mCurrent_tile = i;
					update_preview();
					update_labels();
					break;
				}
	}

	// Next tile
	if (pR.is_key_pressed(engine::renderer::key_type::Period)) 
	{
		++mCurrent_tile %= mTile_list.size();
		mPreview.set_texture(*mTexture, mTile_list[mCurrent_tile]);
		update_preview();
		update_labels();
	}

	// Previous tile
	if (pR.is_key_pressed(engine::renderer::key_type::Comma))
	{
		mCurrent_tile = mCurrent_tile == 0 ? (mTile_list.size() - 1) : (mCurrent_tile - 1);
		update_preview();
		update_labels();
	}

	// Layer +1
	if (pR.is_key_pressed(engine::renderer::key_type::Quote))
	{
		++mLayer;
		update_labels();
		update_highlight();
	}

	// Layer -1
	if (pR.is_key_pressed(engine::renderer::key_type::Slash))
	{
		--mLayer;
		update_labels();
		update_highlight();
	}

	// Rotate Clockwise
	if (pR.is_key_pressed(engine::renderer::key_type::SemiColon))
	{
		++mRotation %= 4;
		update_preview();
		update_labels();
	}


	// Rotate Counter-clockwise
	if (pR.is_key_pressed(engine::renderer::key_type::L))
	{
		mRotation = mRotation == 0 ? 3 : (mRotation - 1);
		update_preview();
		update_labels();
	}
	
	tick_highlight(pR);

	mBlackout.draw(pR);
	mTilemap_display.draw(pR);

	for (int i = 0; i < 4; i++)
		mLines[i].draw(pR);

	mPreview.set_position(tile_position * 32);
	mPreview.draw(pR);

	return 0;
}


void tilemap_editor::setup_editor(editor_gui & pEditor_gui)
{
	mLb_layer = pEditor_gui.add_label("Layer: 0");
	mLb_tile = pEditor_gui.add_label("Tile: N/A");
	mLb_rotation = pEditor_gui.add_label("Rotation: N/A");
}

void tilemap_editor::setup_lines()
{
	const engine::color line_color(255, 255, 255, 150);

	mLines[0].set_color(line_color);
	mLines[0].set_parent(*this);

	mLines[1].set_color(line_color);
	mLines[1].set_parent(*this);

	mLines[2].set_color(line_color);
	mLines[2].set_parent(*this);

	mLines[3].set_color(line_color);
	mLines[3].set_parent(*this);
}

void tilemap_editor::update_labels()
{
	assert(mLb_tile != nullptr);
	assert(mLb_layer != nullptr);
	assert(mLb_rotation != nullptr);

	mLb_layer->setText(std::string("Layer: ") + std::to_string(mLayer));
	mLb_tile->setText(std::string("Tile: ") + mTile_list[mCurrent_tile]);
	mLb_rotation->setText(std::string("Rotation: ") + std::to_string(mRotation));
}

void tilemap_editor::update_preview()
{
	mPreview.set_texture(*mTexture, mTile_list[mCurrent_tile]);
	mPreview.set_rotation(90.f * mRotation);
	mPreview.set_anchor(engine::anchor::topleft);
}

void tilemap_editor::update_highlight()
{
	if (mIs_highlight)
		mTilemap_display.highlight_layer(mLayer, { 200, 255, 200, 255 }, { 50, 50, 50, 100 });
	else
		mTilemap_display.remove_highlight();
}

void tilemap_editor::update_lines(engine::frect pBoundary)
{
	mLines[0].set_size({ pBoundary.w, 1 });
	mLines[1].set_size({ 1, pBoundary.h });
	mLines[2].set_size({ pBoundary.w, 1 });
	mLines[3].set_size({ 1, pBoundary.h });

	mLines[0].set_position(pBoundary.get_offset());
	mLines[1].set_position(pBoundary.get_offset());
	mLines[2].set_position(pBoundary.get_corner() - mLines[2].get_size());
	mLines[3].set_position(pBoundary.get_corner() - mLines[3].get_size());
}

void tilemap_editor::tick_highlight(engine::renderer& pR)
{
	if (pR.is_key_pressed(engine::renderer::key_type::RShift))
	{
		mIs_highlight = !mIs_highlight;
		update_highlight();
	}
}

int tilemap_editor::save()
{
	auto& doc = mLoader.get_document();
	auto ele_map = mLoader.get_tilemap();

	auto ele_layer = ele_map->FirstChildElement("layer");
	while (ele_layer)
	{
		ele_layer->DeleteChildren();
		doc.DeleteNode(ele_layer);
		ele_layer = ele_map->FirstChildElement("layer");
	}
	mTilemap_loader.condense_tiles();
	mTilemap_loader.generate(doc, ele_map);
	doc.SaveFile(mLoader.get_scene_path().c_str());
	return 0;
}

// ##########
// collisionbox_editor
// ##########

collisionbox_editor::collisionbox_editor()
{
	set_depth(-1000);

	add_child(mTilemap_display);

	mBlackout.set_color({ 0, 0, 0, 255 });
	mBlackout.set_size({ 1000, 1000 });

	mWall_display.set_color({ 100, 255, 100, 200 });
	mWall_display.set_outline_color({ 255, 255, 255, 255 });
	mWall_display.set_outline_thinkness(1);
	add_child(mWall_display);

	mTile_preview.set_color({ 0, 0, 0, 0 });
	mTile_preview.set_outline_color({ 255, 255, 255, 100 });
	mTile_preview.set_outline_thinkness(1);
	mTile_preview.set_size({ 32, 32 });
	add_child(mTile_preview);

	mSize_mode = false;
}

int collisionbox_editor::open_scene(std::string pPath)
{
	if (mLoader.load(pPath))
	{
		util::error("Unable to open scene");
		return 1;
	}

	auto tilemap_texture = mTexture_manager->get_texture(mLoader.get_tilemap_texture());
	if (!tilemap_texture)
	{
		util::error("Invalid tilemap texture");
		return 1;
	}
	mTilemap_display.set_texture(*tilemap_texture);

	mTilemap_loader.load_tilemap_xml(mLoader.get_tilemap());
	mTilemap_loader.update_display(mTilemap_display);
	mTilemap_display.set_color({ 100, 100, 255, 150 });
	
	mWalls = mLoader.construct_wall_list();

	return 0;
}

int collisionbox_editor::draw(engine::renderer& pR)
{
	if (pR.is_key_down(engine::renderer::key_type::Return))
		save();

	const engine::fvector mouse_position = pR.get_mouse_position(get_exact_position());
	const engine::fvector exact_tile_position = mouse_position / 32;
	const engine::fvector tile_position = engine::fvector(exact_tile_position).floor();

	// Selection
	if (pR.is_mouse_pressed(engine::renderer::mouse_button::mouse_left))
	{
		if (!tile_selection(tile_position))
		{
			mSelection = mWalls.size();
			mWalls.push_back(engine::frect(tile_position, { 1, 1 }));
			mSize_mode = true;
			mDrag_from = tile_position;
		}
	}

	// Resize mode
	if (pR.is_mouse_down(engine::renderer::mouse_button::mouse_left)
		&& mSize_mode)
	{
		engine::fvector resize_to = tile_position;

		if (tile_position.x <= mDrag_from.x)
		{
			mWalls[mSelection].x = tile_position.x;
			resize_to.x = mDrag_from.x;
		}
		if (tile_position.y <= mDrag_from.y)
		{
			mWalls[mSelection].y = tile_position.y;
			resize_to.y = mDrag_from.y;
		}
		mWalls[mSelection].set_size(resize_to - mWalls[mSelection].get_offset() + engine::fvector(1, 1));
	}
	else
	{
		mSize_mode = false;
	}

	// Remove tile
	if (pR.is_mouse_pressed(engine::renderer::mouse_button::mouse_right))
	{
		tile_selection(exact_tile_position);
		if (mSelection < mWalls.size())
			mWalls.erase(mWalls.begin() + mSelection);
		mSelection = 0;
		update_labels();
	}

	mBlackout.draw(pR);
	mTilemap_display.draw(pR);
	for (size_t i = 0; i < mWalls.size(); i++) // TODO: Optimize
	{
		if (i == mSelection)
			mWall_display.set_outline_color({ 255, 90, 90, 255 });
		else
			mWall_display.set_outline_color({ 255, 255, 255, 255 });

		mWall_display.set_position(mWalls[i].get_offset() * 32);
		mWall_display.set_size(mWalls[i].get_size() * 32);
		mWall_display.draw(pR);
	}

	mTile_preview.set_position(tile_position *32);
	mTile_preview.draw(pR);

	return 0;
}

int collisionbox_editor::save()
{
	auto& doc = mLoader.get_document();
	auto ele_collisionboxes = mLoader.get_collisionboxes();

	auto ele_wall = ele_collisionboxes->FirstChildElement("wall");
	while (ele_wall)
	{
		doc.DeleteNode(ele_wall);
		ele_wall = ele_collisionboxes->FirstChildElement("wall");
	}

	for (auto& i : mWalls)
	{
		auto new_wall = doc.NewElement("wall");
		new_wall->SetAttribute("x", i.x);
		new_wall->SetAttribute("y", i.y);
		new_wall->SetAttribute("w", i.w);
		new_wall->SetAttribute("h", i.h);
		ele_collisionboxes->InsertEndChild(new_wall);
	}

	doc.SaveFile(mLoader.get_scene_path().c_str());
	return 0;
}

void collisionbox_editor::setup_editor(editor_gui & pEditor_gui)
{
	mLb_tilesize = pEditor_gui.add_label("n/a, n/a");
}

bool collisionbox_editor::tile_selection(engine::fvector pCursor)
{
	if (mSize_mode)
		return false;

	bool new_selection = false;

	for (size_t i = 0; i < mWalls.size(); i++)
	{
		if (mWalls[i].is_intersect(pCursor))
		{
			mSelection = i;
			new_selection = true;
			break;
		}
	}
	update_labels();

	return new_selection;
}

void collisionbox_editor::update_labels()
{
	assert(mLb_tilesize != nullptr);

	if (!mWalls.size())
		return;

	std::string size_text = std::to_string(mWalls[mSelection].get_size().x);
	size_text += ", ";
	size_text += std::to_string(mWalls[mSelection].get_size().y);
	mLb_tilesize->setText(size_text);
}

// ##########
// scroll_control_node
// ##########

void scroll_control_node::movement(engine::renderer & pR)
{
	float speed = pR.get_delta() * 128;
	engine::fvector position = get_position();
	if (pR.is_key_down(engine::renderer::key_type::Left))
		position += engine::fvector(1, 0)*speed;
	if (pR.is_key_down(engine::renderer::key_type::Right))
		position -= engine::fvector(1, 0)*speed;
	if (pR.is_key_down(engine::renderer::key_type::Up))
		position += engine::fvector(0, 1)*speed;
	if (pR.is_key_down(engine::renderer::key_type::Down))
		position -= engine::fvector(0, 1)*speed;
	set_position(position);
}

// ##########
// editor_manager
// ##########

editor_manager::editor_manager()
{
	set_depth(-1000);
	mTilemap_editor.set_parent(mRoot_node);
	mCollisionbox_editor.set_parent(mRoot_node);
	mCurrent_editor = nullptr;
}

bool editors::editor_manager::is_editor_open()
{
	return mCurrent_editor != nullptr;
}

void editor_manager::open_tilemap_editor(std::string pScene_path)
{
	mTilemap_editor.set_editor_gui(mEditor_gui);
	mTilemap_editor.open_scene(pScene_path);
	mCurrent_editor = &mTilemap_editor;
}

void editor_manager::open_collisionbox_editor(std::string pScene_path)
{
	mCollisionbox_editor.set_editor_gui(mEditor_gui);
	mCollisionbox_editor.open_scene(pScene_path);
	mCurrent_editor = &mCollisionbox_editor;
}

void editor_manager::close_editor()
{
	if (!mCurrent_editor)
		return;
	mCurrent_editor->save();
	mCurrent_editor = nullptr;
	mEditor_gui.clear();
}

void editor_manager::update_camera_position(engine::fvector pPosition)
{
	if (!mCurrent_editor)
		mEditor_gui.update_camera_position(pPosition);
}

void editor_manager::set_texture_manager(rpg::texture_manager & pTexture_manager)
{
	mTilemap_editor.set_texture_manager(pTexture_manager);
	mCollisionbox_editor.set_texture_manager(pTexture_manager);
}

int editor_manager::draw(engine::renderer& pR)
{
	if (mCurrent_editor != nullptr)
	{
		mRoot_node.movement(pR);
		mEditor_gui.update_camera_position(mRoot_node.get_exact_position());
		mCurrent_editor->draw(pR);
	}
	return 0;
}

void editor_manager::refresh_renderer(engine::renderer & pR)
{
	mEditor_gui.set_renderer(pR);
	mEditor_gui.set_depth(-1001);
	mEditor_gui.initualize();
}
