#include <rpg/editor.hpp>

using namespace editors;

#include <vector>
#include <memory>
#include <cassert>

#include <engine/parsers.hpp>
#include <engine/log.hpp>

void populate_combox_with_scene_names(tgui::ComboBox::Ptr pCB)
{
	pCB->removeAllItems();
	const auto scenelist = rpg::get_scene_list();
	for (auto i : scenelist)
		pCB->addItem(i.string()); // Refresh the list of scenes
}

inline engine::fvector read_args_vector(const engine::terminal_arglist& pArgs, float pDefx = 0, float pDefy = 0, size_t pIndex = 0)
{
	engine::fvector ret;
	if (pArgs[pIndex].get_raw() == "-")
		ret.x = pDefx;
	else
		ret.x = util::to_numeral<float>(pArgs[pIndex]);

	if (pArgs[pIndex + 1].get_raw() == "-")
		ret.y = pDefy;
	else
		ret.y = util::to_numeral<float>(pArgs[pIndex + 1]);
	return ret;
}


bool command_manager::execute(std::shared_ptr<command> pCommand)
{
	assert(!mCurrent);
	mRedo.clear();
	mUndo.push_back(pCommand);
	return pCommand->execute();
}

bool command_manager::add(std::shared_ptr<command> pCommand)
{
	assert(!mCurrent);
	mRedo.clear();
	mUndo.push_back(pCommand);
	return true;
}

void command_manager::start(std::shared_ptr<command> pCommand)
{
	mCurrent = pCommand;
}

void command_manager::complete()
{
	assert(mCurrent);
	mRedo.clear();
	mUndo.push_back(mCurrent);
	mCurrent.reset();
}

bool command_manager::undo()
{
	if (mUndo.size() == 0)
		return false;
	auto undo_cmd = mUndo.back();
	mUndo.pop_back();
	mRedo.push_back(undo_cmd);
	return undo_cmd->undo();
}

bool command_manager::redo()
{
	if (mRedo.size() == 0)
		return false;
	auto redo_cmd = mRedo.back();
	mRedo.pop_back();
	mUndo.push_back(redo_cmd);
	return redo_cmd->redo();
}

void command_manager::clean()
{
	mUndo.clear();
	mRedo.clear();
}

class command_set_tiles :
	public command
{
public:
	command_set_tiles(int pLayer, rpg::tilemap_manipulator* pTilemap_manipulator)
	{
		mLayer = pLayer;
		mTilemap_manipulator = pTilemap_manipulator;
	}

	bool execute()
	{
		assert(mTilemap_manipulator);

		// Track all the tiles that are replaced
		for (auto& i : mTiles)
		{
			mTilemap_manipulator->explode_tile(i.get_position(), mLayer);
			auto replaced_tile = mTilemap_manipulator->get_tile(i.get_position(), mLayer);
			if (replaced_tile)
				mReplaced_tiles.push_back(*replaced_tile);
		}

		// Replace all the tiles
		for (auto& i : mTiles)
		{
			mTilemap_manipulator->set_tile(i, mLayer);
		}
		return true;
	}

	bool undo()
	{
		assert(mTilemap_manipulator);

		// Remove all placed tiles
		for (auto& i : mTiles)
		{
			mTilemap_manipulator->remove_tile(i.get_position(), mLayer);
		}

		// Place all replaced tiles back
		for (auto& i : mReplaced_tiles)
		{
			mTilemap_manipulator->set_tile(i, mLayer);
		}

		return true;
	}

	bool redo()
	{
		return execute();
	}

	void add(rpg::tile& pTile)
	{
		mTiles.push_back(pTile);
	}

private:
	int mLayer;

	std::vector<rpg::tile> mReplaced_tiles;
	std::vector<rpg::tile> mTiles;

	rpg::tilemap_manipulator* mTilemap_manipulator;
};

class command_remove_tiles :
	public command
{
public:
	command_remove_tiles(int pLayer, rpg::tilemap_manipulator* pTilemap_manipulator)
	{
		mLayer = pLayer;
		mTilemap_manipulator = pTilemap_manipulator;
	}

	bool execute()
	{
		assert(mTilemap_manipulator);

		// Track all the tiles that are replaced
		for (auto& i : mTiles_to_remove)
		{
			mTilemap_manipulator->explode_tile(i, mLayer);
			auto replaced_tile = mTilemap_manipulator->get_tile(i, mLayer);
			if (replaced_tile)
			{
				mRemoved_tiles.push_back(*replaced_tile);
				mTilemap_manipulator->remove_tile(i, mLayer);
			}
		}

		return true;
	}

	bool undo()
	{
		for (auto& i : mRemoved_tiles)
		{
			mTilemap_manipulator->set_tile(i, mLayer);
		}
		return true;
	}

	bool redo()
	{
		return execute();
	}

	void add(engine::fvector pPosition)
	{
		mTiles_to_remove.push_back(pPosition);
	}

private:
	int mLayer;

	std::vector<rpg::tile> mRemoved_tiles;
	std::vector<engine::fvector> mTiles_to_remove;

	rpg::tilemap_manipulator* mTilemap_manipulator;
};

void tgui_list_layout::updateWidgetPositions()
{
	auto& widgets = getWidgets();
	Widget::Ptr last;
	for (auto i : widgets)
	{
		if (!i->isVisible())
			continue;
		if (last)
		{
			auto pos = last->getPosition();
			pos.y += last->getFullSize().y;
			i->setPosition(pos);
		}
		last = i;
	}
}

editor_gui::editor_gui()
{
	mGui_base = std::make_shared<tgui::VerticalLayout>();
	mGui_base->setSize("&.width", "&.height");
	mGui_base->hide();


	// Top bar

	auto topbar = std::make_shared<tgui::HorizontalLayout>();
	topbar->setSize("&.width", 100);
	topbar->setBackgroundColor(sf::Color(default_gui_bg_color));
	mGui_base->add(topbar);
	mGui_base->setFixedSize(topbar, 100);

	auto game_control = std::make_shared<tgui::VerticalLayout>();
	topbar->add(game_control);
	topbar->setFixedSize(game_control, 100);

	auto bt_scene_stop = std::make_shared<tgui::Button>();
	bt_scene_stop->setText("Stop Game");
	game_control->add(bt_scene_stop);

	auto bt_scene_restart = std::make_shared<tgui::Button>();
	bt_scene_restart->setText("Restart Scene");
	game_control->add(bt_scene_restart);

	topbar->addSpace();

	mTabs = std::make_shared<tgui::Tab>();
	mTabs->add("Game", true);
	mTabs->add("Tilemap", false);
	mTabs->add("Collision", false);
	mTabs->add("Atlas", false);
	topbar->add(mTabs);
	topbar->setFixedSize(mTabs, 400);

	topbar->addSpace();

	mCb_scene = std::make_shared<tgui::ComboBox>();
	//mCb_scene->setPosition(200, 0);
	//mCb_scene->setSize("&.width - x", 25);
	mCb_scene->setItemsToDisplay(10);
	mCb_scene->connect("ItemSelected", [&](sf::String pItem)
	{
		mScene->load_scene(pItem);
	});
	mCb_scene->hide();
	mGui_base->add(mCb_scene);
	mGui_base->setFixedSize(mCb_scene, 25);

	// Middle section
	auto middle = std::make_shared<tgui::HorizontalLayout>();
	mGui_base->add(middle);

	// Side bar

	mSidebar = std::make_shared<tgui_list_layout>();
	mSidebar->setBackgroundColor(sf::Color(default_gui_bg_color));
	mSidebar->getRenderer()->setBorders(2);
	mSidebar->getRenderer()->setBorderColor({ 0, 0, 0, 255 });
	middle->add(mSidebar);
	middle->setFixedSize(mSidebar, 200);

	mLb_fps = std::make_shared<tgui::Label>();
	mLb_fps->setMaximumTextWidth(0);
	mLb_fps->setTextColor({ 255, 255, 255, 255 });
	mLb_fps->setText("FPS: N/A");
	mLb_fps->setTextSize(15);
	mSidebar->add(mLb_fps);

	mLb_mouse = tgui::Label::copy(mLb_fps);
	mLb_mouse->setText("(n/a, n/a)\n(n/a, n/a)");
	mLb_mouse->setTextSize(12);
	mSidebar->add(mLb_mouse);

	{
		add_small_label("Visualizations:", mSidebar);
		mVisualizations_layout = std::make_shared<tgui_list_layout>();
		mVisualizations_layout->setSize("&.width", 100);
		mSidebar->add(mVisualizations_layout);

		// Collision visualization checkbox
		auto chb_collision = std::make_shared<tgui::CheckBox>();
		chb_collision->getRenderer()->setTextColor({ 255, 255, 255, 255 });
		chb_collision->setText("Collision");
		chb_collision->connect("Checked", [&]()
		{ mScene->get_visualizer().visualize_collision(true); });
		chb_collision->connect("Unchecked", [&]()
		{ mScene->get_visualizer().visualize_collision(false); });
		mVisualizations_layout->add(chb_collision);

		// Entities visualization checkbox
		auto chb_entities = std::make_shared<tgui::CheckBox>();
		chb_entities->getRenderer()->setTextColor({ 255, 255, 255, 255 });
		chb_entities->setText("Entities");
		chb_entities->connect("Checked", [&]()
		{ mScene->get_visualizer().visualize_entities(true); });
		chb_entities->connect("Unchecked", [&]()
		{ mScene->get_visualizer().visualize_entities(false); });
		mVisualizations_layout->add(chb_entities);
	}

	mEditor_layout = std::make_shared<tgui_list_layout>();
	mEditor_layout->setSize("&.width", "&.height - y");
	mEditor_layout->setBackgroundColor({ 0, 0, 0, 0 });
	mSidebar->add(mEditor_layout);

	// Game window

	mRender_container = std::make_shared<tgui::Panel>();
	mRender_container->setBackgroundColor({ 0, 0, 0, 0 });
	middle->add(mRender_container);

	// Bottom bar

	auto bottombar = std::make_shared<tgui::HorizontalLayout>();
	bottombar->setBackgroundColor(sf::Color(default_gui_bg_color));
	bottombar->getRenderer()->setBorders(2);
	bottombar->getRenderer()->setBorderColor({ 0, 0, 0, 255 });
	mGui_base->add(bottombar);
	mGui_base->setFixedSize(bottombar, 25);

	mBottom_text = std::make_shared<tgui::Label>();
	bottombar->add(mBottom_text);
}

void editor_gui::set_scene(rpg::scene * pScene)
{
	mScene = pScene;
}

void editor_gui::clear()
{
	mEditor_layout->removeAllWidgets();
}

inline void editor_gui::add_group(const std::string & pText)
{
	auto lb = std::make_shared<tgui::Label>();
	lb->setMaximumTextWidth(0);
	lb->setTextColor({ 200, 200, 200, 255 });
	lb->setText(pText);
	lb->setTextSize(15);
	lb->setTextStyle(sf::Text::Bold);
	mEditor_layout->add(lb);
}

tgui::HorizontalLayout::Ptr editor_gui::create_value_line(const std::string& pText)
{
	auto hl = std::make_shared<tgui::HorizontalLayout>();
	hl->setBackgroundColor({ 0, 0, 0, 0 });
	hl->setSize("&.width",25);
	hl->addSpace(0.1f);
	mEditor_layout->add(hl);

	auto lb = std::make_shared<tgui::Label>();
	lb->setMaximumTextWidth(0);
	lb->setTextColor({ 200, 200, 200, 255 });
	lb->setText(pText);
	lb->setTextSize(12);
	lb->setVerticalAlignment(tgui::Label::VerticalAlignment::Center);
	hl->add(lb);

	return hl;
}

tgui::EditBox::Ptr editor_gui::add_value_int(const std::string & pLabel, std::function<void(int)> pCallback, bool pNeg)
{
	assert(pCallback);
	auto hl = create_value_line(pLabel);
	auto tb = std::make_shared<tgui::EditBox>();
	//tb->setInputValidator(std::string(pNeg ? "[-+]" : "") + "[0-9]*");
	auto apply = [=]()
	{
		try {
			pCallback(std::stoi(std::string(tb->getText())));
		}
		catch (...)
		{
			logger::warning("Failed to get value of '" + pLabel + "'");
		}
	};

	tb->connect("ReturnKeyPressed", apply);
	tb->connect("Unfocused", apply);
	hl->add(tb);
	return tb;
}

tgui::EditBox::Ptr editor_gui::add_value_int(const std::string & pLabel, int & pValue, bool pNeg)
{
	return add_value_int(pLabel, std::function<void(int)>([&](int pCallback_value) { pValue = pCallback_value; }), pNeg);
}

tgui::EditBox::Ptr editor_gui::add_value_string(const std::string & pLabel, std::function<void(std::string)> pCallback)
{
	auto hl = create_value_line(pLabel);
	auto tb = std::make_shared<tgui::EditBox>();
	auto apply = [=]()
	{
		try {
			if (pCallback)
				pCallback(tb->getText());
		}
		catch (...)
		{
			logger::warning("Failed to get value of '" + pLabel + "'");
		}
	};
	tb->connect("ReturnKeyPressed", apply);
	tb->connect("Unfocused", apply);
	hl->add(tb);
	return tb;
}

tgui::EditBox::Ptr editor_gui::add_value_string(const std::string & pLabel, std::string& pValue)
{
	return add_value_string(pLabel, [&](std::string pCallback_value) { pValue = pCallback_value; });
}

tgui::EditBox::Ptr editor_gui::add_value_float(const std::string & pLabel, std::function<void(float)> pCallback, bool pNeg)
{
	assert(pCallback);
	auto hl = create_value_line(pLabel);
	auto tb = std::make_shared<tgui::EditBox>();
	//tb->setInputValidator(std::string(pNeg ? "[-+]" : "") + "[0-9]*\\.[0-9]*");

	auto apply = [=]()
	{
		try {
			pCallback(std::stof(std::string(tb->getText())));
		}
		catch (...)
		{
			logger::warning("Failed to get value of '" + pLabel + "'");
		}
	};
	tb->connect("ReturnKeyPressed", apply);
	tb->connect("Unfocused", apply);
	hl->add(tb);
	return tb;
}

tgui::EditBox::Ptr editor_gui::add_value_float(const std::string & pLabel, float & pValue, bool pNeg)
{
	return add_value_float(pLabel, std::function<void(float)>([&](float pCallback_value) { pValue = pCallback_value; }), pNeg);
}

tgui::ComboBox::Ptr editor_gui::add_value_enum(const std::string & pLabel, std::function<void(size_t)> pCallback, const std::vector<std::string>& pValues, size_t pDefault, bool pBig_mode)
{
	assert(pCallback);
	auto hl = create_value_line(pLabel);
	auto cb = std::make_shared<tgui::ComboBox>();
	for (const auto& i : pValues)
	{
		cb->addItem(i);
	}
	//tb->setInputValidator(std::string(pNeg ? "[-+]" : "") + "[0-9]*\\.[0-9]*");
	cb->connect("ItemSelected", [=](sf::String pVal)
	{
		try {
			pCallback(cb->getSelectedItemIndex());
		}
		catch (...)
		{
			logger::warning("Failed to get value of '" + pLabel + "'");
		}
	});

	// Big mode is more lists with items that are too long for the default size
	if (pBig_mode)
	{
		const float big_mode_width = 400;
		std::shared_ptr<tgui::Layout> original_w_layout = std::make_shared<tgui::Layout>(cb->getSizeLayout().x);
		std::shared_ptr<bool> is_big = std::make_shared<bool>(false);
		auto resize_big = [=]()
		{
			if (!*is_big)
			{
				*original_w_layout = cb->getSizeLayout().x;
				cb->setSize(big_mode_width, cb->getSizeLayout().y);
				*is_big = true;
			}
		};
		auto resize_original = [=]()
		{
			if (*is_big)
			{
				cb->setSize(*original_w_layout, cb->getSizeLayout().y);
				*is_big = false;
			}
		};

		cb->connect("MouseEntered", resize_big);
		cb->getListBox()->connect("MouseEntered", resize_big);
		cb->connect("MouseLeft", resize_original);
		cb->getListBox()->connect("MouseLeft", resize_original);
	}

	cb->setSelectedItemByIndex(pDefault);
	cb->setTextSize(12);
	cb->setItemsToDisplay(10);
	hl->add(cb);
	return cb;
}

tgui::ComboBox::Ptr editor_gui::add_value_enum(const std::string & pLabel, size_t & pSelection, const std::vector<std::string>& pValues, size_t pDefault, bool pBig_mode)
{
	return add_value_enum(pLabel, std::function<void(size_t)>([&](size_t pCallback_value) { pSelection = pCallback_value; }), pValues, pDefault, pBig_mode);
}

void editor_gui::add_horizontal_buttons(const std::vector<std::tuple<std::string, std::function<void()>>> pName_callbacks)
{
	auto hl = std::make_shared<tgui::HorizontalLayout>();
	hl->setBackgroundColor({ 0, 0, 0, 0 });
	hl->setSize("&.width", 25);
	hl->addSpace(0.1f);
	mEditor_layout->add(hl);
	
	for (auto& i : pName_callbacks)
	{
		auto bt = std::make_shared<tgui::Button>();
		bt->setText(std::get<0>(i));
		bt->setTextSize(12);
		bt->connect("Pressed", std::get<1>(i));
		hl->add(bt);
	}
}

void editors::editor_gui::add_button(const std::string & pLabel, std::function<void()> pCallback)
{
	add_horizontal_buttons({ {pLabel, pCallback} });
}


tgui::Label::Ptr editor_gui::add_label(const std::string & pText, tgui::Container::Ptr pContainer)
{
	auto nlb = tgui::Label::copy(mLb_fps);
	nlb->setText(pText);
	nlb->setTextStyle(sf::Text::Bold);
	if (pContainer)
		pContainer->add(nlb);
	else
		mEditor_layout->add(nlb);
	return nlb;
}

tgui::Label::Ptr editor_gui::add_small_label(const std::string & text, tgui::Container::Ptr pContainer)
{
	auto label = add_label(text, pContainer);
	label->setTextSize(10);
	return label;
}

tgui::TextBox::Ptr editor_gui::add_textbox(tgui::Container::Ptr pContainer)
{
	auto ntb = std::make_shared<tgui::TextBox>();
	ntb->setSize("&.width", "25");
	(pContainer ? pContainer : mEditor_layout)->add(ntb);
	return ntb;
}

tgui::ComboBox::Ptr editor_gui::add_combobox(tgui::Container::Ptr pContainer)
{
	auto ncb = std::make_shared<tgui::ComboBox>();
	ncb->setSize("&.width", "25");
	(pContainer ? pContainer : mEditor_layout)->add(ncb);
	ncb->setItemsToDisplay(10);
	return ncb;
}

tgui::CheckBox::Ptr editor_gui::add_checkbox(const std::string& text, tgui::Container::Ptr pContainer)
{
	auto ncb = std::make_shared<tgui::CheckBox>();
	ncb->setText(text);
	ncb->uncheck();
	(pContainer ? pContainer : mEditor_layout)->add(ncb);
	return ncb;
}

tgui::Button::Ptr editor_gui::add_button(const std::string& text, tgui::Container::Ptr pContainer)
{
	auto nbt = std::make_shared<tgui::Button>();
	nbt->setText(text);
	(pContainer ? pContainer : mEditor_layout)->add(nbt);
	return nbt;
}

std::shared_ptr<tgui_list_layout> editor_gui::add_sub_container(tgui::Container::Ptr pContainer)
{
	auto slo = std::make_shared<tgui_list_layout>();
	slo->setBackgroundColor({ 0, 0, 0, 0 });
	slo->setSize(200, 500);
	(pContainer ? pContainer : mEditor_layout)->add(slo);
	return slo;
}


void editor_gui::update_scene()
{
	if (mCb_scene->getSelectedItem() != mScene->get_name()
		&& !mScene->get_name().empty())
	{
		populate_combox_with_scene_names(mCb_scene);
		mCb_scene->setSelectedItem(mScene->get_name());
	}
}


void editor_gui::refresh_renderer(engine::renderer & pR)
{
	pR.get_tgui().add(mGui_base);

	mRender_container->connect("Focused", [&]()
	{
		pR.set_transparent_gui_input(true);
		mRender_container->getRenderer()->setBorders(3);
		mRender_container->getRenderer()->setBorderColor({ 255, 255, 0, 100 });
	});
	mRender_container->connect("Unfocused", [&]()
	{
		pR.set_transparent_gui_input(false);
		mRender_container->getRenderer()->setBorderColor({ 0, 0, 0, 0 });
	});
}

int editor_gui::draw(engine::renderer& pR)
{
	update_scene();

	if (pR.is_key_down(engine::renderer::key_type::LControl)
	&&  pR.is_key_pressed(engine::renderer::key_type::E))
	{
		if (mGui_base->isVisible())
		{
			mGui_base->hide();
			mCb_scene->hide();
			pR.set_subwindow_enabled(false);
		}
		else
		{
			mGui_base->show();
			mCb_scene->show();
			pR.set_subwindow_enabled(true);
		}
	}

	// Keep the sub window for the renderer updated
	if (mGui_base->isVisible())
	{
		const engine::fvector position(mRender_container->getAbsolutePosition().x
			, mRender_container->getAbsolutePosition().y);
		const engine::fvector size = mRender_container->getFullSize();

		pR.set_subwindow({ position, size});
	}

	// Lock the scene selector to not mess things up
	if (!mEditor_layout->getWidgets().empty())
		mCb_scene->disable();
	else
		mCb_scene->enable();

	mUpdate_timer += pR.get_delta();
	if (mUpdate_timer >= 0.5f)
	{
		const auto mouse_position_exact = pR.get_mouse_position();
		const auto mouse_position = pR.get_mouse_position(get_exact_position()) / get_unit();
		std::string position;
		position += "(";
		position += std::to_string(static_cast<int>(mouse_position_exact.x));
		position += ", ";
		position += std::to_string(static_cast<int>(mouse_position_exact.y));
		position += ")\n(";
		position += std::to_string(mouse_position.x);
		position += ", ";
		position += std::to_string(mouse_position.y);
		position += ")";
		mLb_mouse->setText(position);

		mLb_fps->setText("FPS: " + std::to_string(pR.get_fps()));

		mUpdate_timer = 0;
	}
	return 0;
}


editor::editor()
{
	mBlackout.set_color({ 0, 0, 0, 255 });
	mBlackout.set_size({ 1000, 1000 });
}

void editor::set_editor_gui(editor_gui & pEditor_gui)
{
	pEditor_gui.clear();
	mEditor_gui = &pEditor_gui;
	setup_editor(pEditor_gui);
}

void editor::set_resource_manager(engine::resource_manager& pResource_manager)
{
	mResource_manager = &pResource_manager;
}

// ##########
// scene_editor
// ##########

scene_editor::scene_editor()
{
	mBoundary_visualization.set_parent(*this);
	mTilemap_display.set_parent(*this);
	mResource_manager = nullptr;
}

bool scene_editor::open_scene(std::string pPath)
{
	mTilemap_manipulator.clean();
	mTilemap_display.clean();

	auto path = engine::encoded_path(pPath);
	if (!mLoader.load(path.parent(), path.filename()))
	{
		logger::error("Unable to open scene '" + pPath + "'");
		return false;
	}

	assert(mResource_manager != nullptr);
	auto texture = mResource_manager->get_resource<engine::texture>(engine::resource_type::texture, mLoader.get_tilemap_texture());
	if (!texture)
	{
		logger::warning("Invalid tilemap texture in scene");
		logger::info("If you have yet to specify a tilemap texture, you can ignore the last warning");
	}
	else
	{
		mTilemap_display.set_texture(texture);
		mTilemap_display.set_color({ 100, 100, 255, 150 });

		mTilemap_manipulator.load_tilemap_xml(mLoader.get_tilemap());
		mTilemap_manipulator.update_display(mTilemap_display);
	}

	mBoundary_visualization.set_boundary(mLoader.get_boundary());

	return true;
}

// ##########
// tilemap_editor
// ##########

tilemap_editor::tilemap_editor()
{
	set_depth(-1000);

	mPreview.set_color({ 255, 255, 255, 150 });
	mPreview.set_parent(*this);

	mCurrent_tile = 0;
	mRotation = 0;
	mLayer = 0;

	mIs_highlight = false;

	mState = state::none;
}

bool tilemap_editor::open_editor()
{
	clean();

	// Get tilemap texture
	// The user should be allowed to add a texture when there isn't any.
	mTexture = mTilemap_display.get_texture();
	if (mTexture)
	{
		mCurrent_texture_name = mLoader.get_tilemap_texture();

		mTile_list = std::move(mTexture->compile_list());

		mPreview.set_texture(mTexture);
	}

	mTb_texture->setText(mLoader.get_tilemap_texture());

	update_tile_combobox_list();
	update_preview();
	update_labels();

	mTilemap_group->set_enabled(true);

	return true;
}

int tilemap_editor::draw(engine::renderer & pR)
{
	// Draw the black thing first
	mBlackout.draw(pR);

	// Editing is not allowed as there are no tiles to use.
	if (mTile_list.empty())
		return 1;

	const engine::fvector mouse_position = pR.get_mouse_position(mTilemap_display.get_exact_position());

	const engine::fvector tile_position_exact = mouse_position / get_unit();
	const engine::fvector tile_position
		= mCb_half_grid->isChecked()
		? engine::fvector(tile_position_exact * 2).floor() / 2
		: engine::fvector(tile_position_exact).floor();

	switch (mState)
	{
	case state::none:
	{
		if (pR.is_mouse_down(engine::renderer::mouse_button::mouse_left)
			&& pR.is_key_down(engine::renderer::key_type::LShift))
		{
			mState = state::drawing_region;
			mLast_tile = tile_position;
		}
		else if (pR.is_key_down(engine::renderer::key_type::LControl))
		{
			if (pR.is_key_pressed(engine::renderer::key_type::Z)) // Undo
			{
				mCommand_manager.undo();
				update_tilemap();
			}
			else if (pR.is_key_pressed(engine::renderer::key_type::Y)) // Redo
			{
				mCommand_manager.redo();
				update_tilemap();
			}
		}
		else if (pR.is_mouse_down(engine::renderer::mouse_button::mouse_left))
		{
			mState = state::drawing;
			draw_tile_at(tile_position); // Draw first tile
			mLast_tile = tile_position;
		}
		else if (pR.is_mouse_down(engine::renderer::mouse_button::mouse_right))
		{
			mState = state::erasing;
			erase_tile_at(tile_position); // Erase first tile
			mLast_tile = tile_position;
		}
		else if (pR.is_mouse_down(engine::renderer::mouse_button::mouse_middle))
		{
			copy_tile_type_at(tile_position_exact); // Copy tile (no need for a specific state for this)
		}
		else if (pR.is_key_pressed(engine::renderer::key_type::Period))
		{
			next_tile();
		}
		else if (pR.is_key_pressed(engine::renderer::key_type::Comma))
		{
			previous_tile();
		}
		else if (pR.is_key_pressed(engine::renderer::key_type::Quote))
		{
			layer_up();
		}
		else if (pR.is_key_pressed(engine::renderer::key_type::Slash))
		{
			layer_down();
		}
		else if (pR.is_key_pressed(engine::renderer::key_type::R))
		{
			rotate_clockwise();
		}
		break;
	}
	case state::drawing:
	{
		if (!pR.is_mouse_down(engine::renderer::mouse_button::mouse_left))
		{
			mState = state::none;
			break;
		}

		// Don't draw at the same place twice
		if (mLast_tile == tile_position)
			break;
		mLast_tile = tile_position;

		draw_tile_at(tile_position);

		break;
	}
	case state::erasing:
	{
		if (!pR.is_mouse_down(engine::renderer::mouse_button::mouse_right))
		{
			mState = state::none;
			break;
		}

		// Don't erase at the same place twice
		if (mLast_tile == tile_position)
			break;
		mLast_tile = tile_position;

		erase_tile_at(tile_position);

		break;
	}
	case state::drawing_region:
	{
		// Apply the region only after releasing left mouse button
		if (!pR.is_mouse_down(engine::renderer::mouse_button::mouse_left))
		{
			mState = state::none;

			// TODO : Place tiles
		}

		break;
	}
	}

	tick_highlight(pR);

	mTilemap_display.draw(pR);

	mBoundary_visualization.draw(pR);

	if (mState == state::drawing_region)
	{
		// TODO : Draw rectangle specifying region
	}
	else
	{
		mPreview.set_position(tile_position);
		mPreview.draw(pR);
	}

	return 0;
}

void tilemap_editor::load_terminal_interface(engine::terminal_system & pTerminal)
{
	mTilemap_group = std::make_shared<engine::terminal_command_group>();
	mTilemap_group->set_root_command("tilemap");

	mTilemap_group->add_command("clear",
		[&](const engine::terminal_arglist& pArgs)->bool
	{
		mTilemap_manipulator.clean();
		update_tilemap();
		return true;
	}, "- Clear the entire tilemap (Warning: Can't undo)");

	mTilemap_group->add_command("shift",
		[&](const engine::terminal_arglist& pArgs)->bool
	{
		if (pArgs.size() < 2)
		{
			logger::error("Not enough arguments");
			return false;
		}

		engine::fvector shift_amount;
		try {
			shift_amount = read_args_vector(pArgs);
		}
		catch (...)
		{
			logger::error("Invalid offset input");
			return false;
		}

		if (pArgs.size() >= 3)
		{
			if (pArgs[2].get_raw() == "current")
			{
				mTilemap_manipulator.shift(shift_amount, mLayer);
			}
			else
			{
				int layer;
				try {
					layer = util::to_numeral<int>(pArgs[2]);
				}
				catch (...)
				{
					logger::error("Invalid layer input");
					return false;
				}

				mTilemap_manipulator.shift(shift_amount, layer);
			}
		}
		else
		{
			mTilemap_manipulator.shift(shift_amount); // Shift entire tilemap
		}
		update_tilemap();
		return true;
	}, "<X> <Y> [Layer#/current] - Shift the entire/layer of tilemap (Warning: Can't undo)");

	mTilemap_group->set_enabled(false);
	pTerminal.add_group(mTilemap_group);
}


void tilemap_editor::setup_editor(editor_gui & pEditor_gui)
{
	mTb_texture = pEditor_gui.add_value_string("Texture", [&](std::string) { apply_texture(); });

	mCb_tile = pEditor_gui.add_value_enum("Tile", [&](size_t pSelection) {
		mCurrent_tile = pSelection;
		update_labels();
		update_preview();
	}, {}, 0, true);

	mLb_layer = pEditor_gui.add_label("Layer: 0");
	mLb_rotation = pEditor_gui.add_label("Rotation: N/A");
	mCb_half_grid = pEditor_gui.add_checkbox("Half Grid");
}

void tilemap_editor::copy_tile_type_at(engine::fvector pAt)
{
	const std::string atlas = mTilemap_manipulator.find_tile_name(pAt, mLayer);
	if (atlas.empty())
		return;

	for (size_t i = 0; i < mTile_list.size(); i++) // Find tile in tile_list and set it as current tile
	{
		if (mTile_list[i] == atlas)
		{
			mCurrent_tile = i;
			update_preview();
			update_labels();
			update_tile_combobox_selected();
			break;
		}
	}
}

void tilemap_editor::draw_tile_at(engine::fvector pAt)
{
	assert(mTile_list.size() != 0);
	mTilemap_manipulator.explode_tile(pAt, mLayer);

	rpg::tile new_tile;
	new_tile.set_position(pAt);
	new_tile.set_atlas(mTile_list[mCurrent_tile]);
	new_tile.set_rotation(mRotation);

	std::shared_ptr<command_set_tiles> command
	(new command_set_tiles(mLayer, &mTilemap_manipulator));
	command->add(new_tile);

	mCommand_manager.execute(command);
	update_tilemap();
}

void tilemap_editor::erase_tile_at(engine::fvector pAt)
{
	mTilemap_manipulator.explode_tile(pAt, mLayer);

	std::shared_ptr<command_remove_tiles> command
		(new command_remove_tiles(mLayer, &mTilemap_manipulator));
	command->add(pAt);

	mCommand_manager.execute(command);

	mTilemap_manipulator.update_display(mTilemap_display);
	update_tilemap();
}

void tilemap_editor::next_tile()
{
	++mCurrent_tile %= mTile_list.size();
	update_tile_combobox_selected();
	update_preview();
	update_labels();
}

void tilemap_editor::previous_tile()
{
	mCurrent_tile = mCurrent_tile == 0 ? (mTile_list.size() - 1) : (mCurrent_tile - 1);
	update_tile_combobox_selected();
	update_preview();
	update_labels();
}

void tilemap_editor::layer_up()
{
	++mLayer;
	update_labels();
	update_highlight();
}

void tilemap_editor::layer_down()
{
	--mLayer;
	update_labels();
	update_highlight();
}

void tilemap_editor::rotate_clockwise()
{
	++mRotation %= 4;
	update_preview();
	update_labels();
}


void tilemap_editor::update_tile_combobox_list()
{
	assert(mCb_tile != nullptr);
	mCb_tile->removeAllItems();
	for (auto& i : mTile_list)
		mCb_tile->addItem(i);
	update_tile_combobox_selected();
}

void tilemap_editor::update_tile_combobox_selected()
{
	mCb_tile->setSelectedItemByIndex(mCurrent_tile);
}

void tilemap_editor::update_labels()
{
	assert(mLb_layer != nullptr);
	assert(mLb_rotation != nullptr);

	mLb_layer->setText(std::string("Layer: ") + std::to_string(mLayer));
	mLb_rotation->setText(std::string("Rotation: ") + std::to_string(mRotation));
}

void tilemap_editor::update_preview()
{
	if (!mTexture)
		return;
	mPreview.set_texture_rect(mTexture->get_entry(mTile_list[mCurrent_tile])->get_root_frame());

	mPreview.set_rotation(90.f * mRotation);

	// Align the preview correctly after the rotation
	// Possibly could just use engine::anchor::center
	switch (mRotation)
	{
	case 0:
		mPreview.set_anchor(engine::anchor::topleft);
		break;
	case 1:
		mPreview.set_anchor(engine::anchor::bottomleft);
		break;
	case 2:
		mPreview.set_anchor(engine::anchor::bottomright);
		break;
	case 3:
		mPreview.set_anchor(engine::anchor::topright);
		break;
	}
}

void tilemap_editor::update_highlight()
{
	if (mIs_highlight)
		mTilemap_display.highlight_layer(mLayer, { 200, 255, 200, 255 }, { 50, 50, 50, 100 });
	else
		mTilemap_display.remove_highlight();
}

void tilemap_editor::update_tilemap()
{
	mTilemap_manipulator.update_display(mTilemap_display);
	update_highlight();
}

void tilemap_editor::tick_highlight(engine::renderer& pR)
{
	if (pR.is_key_pressed(engine::renderer::key_type::RShift))
	{
		mIs_highlight = !mIs_highlight;
		update_highlight();
	}
}

void tilemap_editor::apply_texture()
{
	const std::string tilemap_texture_name = mTb_texture->getText();

	logger::info("Applying tilemap Texture '" + tilemap_texture_name + "'...");
	auto new_texture = mResource_manager->get_resource<engine::texture>(engine::resource_type::texture, tilemap_texture_name);
	if (!new_texture)
	{
		logger::error("Failed to load texture '" + tilemap_texture_name + "'");
		return;
	}

	mTexture = new_texture;

	mTilemap_display.set_texture(mTexture);
	update_tilemap();
	mTile_list = std::move(mTexture->compile_list());
	assert(mTile_list.size() != 0);

	mCurrent_tile = 0;

	mPreview.set_texture(mTexture);

	update_tile_combobox_list();
	update_preview();
	update_labels();

	mCurrent_texture_name = tilemap_texture_name;

	logger::info("Tilemap texture applied");
}

int tilemap_editor::save()
{
	auto& doc = mLoader.get_document();
	auto ele_map = mLoader.get_tilemap();

	logger::info("Saving tilemap...");

	// Update tilemap texture name
	auto ele_texture = ele_map->FirstChildElement("texture");
	ele_texture->SetText(mCurrent_texture_name.c_str());

	auto ele_layer = ele_map->FirstChildElement("layer");
	while (ele_layer)
	{
		ele_layer->DeleteChildren();
		doc.DeleteNode(ele_layer);
		ele_layer = ele_map->FirstChildElement("layer");
	}
	mTilemap_manipulator.condense_map();
	mTilemap_manipulator.generate(doc, ele_map);
	doc.SaveFile(mLoader.get_scene_path().c_str());

	logger::info("Tilemap saved");

	return 0;
}

void tilemap_editor::clean()
{
	mTile_list.clear();
	mLayer = 0;
	mRotation = 0;
	mIs_highlight = false;
	mCurrent_tile = 0;
	mTexture = nullptr;
	mCurrent_texture_name.clear();
	mPreview.set_texture(nullptr);
	mCommand_manager.clean();
	mTilemap_group->set_enabled(false);
}

// ##########
// collisionbox_editor
// ##########

class command_add_wall :
	public command
{
public:
	command_add_wall(std::shared_ptr<rpg::collision_box> pBox, rpg::collision_box_container* pContainer)
	{
		mBox = pBox;
		mContainer = pContainer;
	}

	bool execute()
	{
		mContainer->add_collision_box(mBox);
		return true;
	}

	bool undo()
	{
		mContainer->remove_box(mBox);
		return true;
	}

	bool redo()
	{
		return execute();
	}

private:
	std::shared_ptr<rpg::collision_box> mBox;
	rpg::collision_box_container* mContainer;
};

class command_remove_wall :
	public command
{
public:
	command_remove_wall(std::shared_ptr<rpg::collision_box> pBox, rpg::collision_box_container* pContainer)
		: pOpposing(pBox, pContainer)
	{}

	bool execute()
	{
		return pOpposing.undo();
	}

	bool undo()
	{
		return pOpposing.execute();
	}

	bool redo()
	{
		return execute();
	}

private:
	command_add_wall pOpposing;
};

class command_transform_wall :
	public command
{
public:
	command_transform_wall(std::shared_ptr<rpg::collision_box> pBox)
		: mBox(pBox), mOpposing(pBox->get_region())
	{}

	bool execute()
	{
		auto temp = mBox->get_region();
		mBox->set_region(mOpposing);
		mOpposing = temp;
		return true;
	}

	bool undo()
	{
		return execute();
	}

	bool redo()
	{
		return execute();
	}

private:
	std::shared_ptr<rpg::collision_box> mBox;
	engine::frect mOpposing;
};

collisionbox_editor::collisionbox_editor()
{
	set_depth(-1000);

	add_child(mTilemap_display);

	mWall_display.set_color({ 100, 255, 100, 200 });
	mWall_display.set_outline_color({ 255, 255, 255, 255 });
	mWall_display.set_outline_thinkness(1);
	add_child(mWall_display);

	mGrid.set_color({ 0, 0, 0, 0 });
	mGrid.set_outline_color({ 100, 100, 100, 100 });
	mGrid.set_outline_thinkness(0.5f);
	add_child(mGrid);

	mCurrent_type = rpg::collision_box::type::wall;
	mGrid_snap = grid_snap::full;

	mState = state::normal;
}

bool collisionbox_editor::open_editor()
{
	mCollision_editor_group->set_enabled(true);
	mCommand_manager.clean();

	return mContainer.load_xml(mLoader.get_collisionboxes());
}

int collisionbox_editor::draw(engine::renderer& pR)
{
	const bool button_left = pR.is_mouse_pressed(engine::renderer::mouse_button::mouse_left);
	const bool button_left_down = pR.is_mouse_down(engine::renderer::mouse_button::mouse_left);
	const bool button_right = pR.is_mouse_pressed(engine::renderer::mouse_button::mouse_right);
	const bool button_shift  = pR.is_key_down(engine::renderer::key_type::LShift);
	const bool button_ctrl   = pR.is_key_down(engine::renderer::key_type::LControl);

	const engine::fvector mouse_position = pR.get_mouse_position(get_exact_position());
	const engine::fvector exact_tile_position = (mouse_position * get_unit()).floor()/std::pow(get_unit(), 2);

	engine::fvector tile_position;
	engine::fvector selection_size;
	if (mGrid_snap == grid_snap::none) // No grid
	{
		tile_position = exact_tile_position;
		selection_size = engine::fvector(0, 0);
	}
	else
	{
		// Snap the tile position and adjust the selection size for the new grid
		float scale = 1;
		switch (mGrid_snap)
		{
		case grid_snap::pixel:   scale = 1 / get_unit(); break;
		case grid_snap::quarter: scale = 0.25f;          break;
		case grid_snap::half:    scale = 0.5f;           break;
		case grid_snap::full:    scale = 1;              break;
		}
		tile_position = (exact_tile_position / scale).floor() * scale;
		selection_size = engine::fvector(scale, scale);
	}

	switch (mState)
	{
	case state::normal:
	{
		// Select tile
		if (button_left)
		{
			if (button_ctrl && mSelection) // Resize
			{
				const engine::fvector pos = (exact_tile_position - mSelection->get_region().get_center())/**mSelection->get_region().get_size().normalize()*/;
				if (std::abs(pos.x) > std::abs(pos.y))
				{
					if (pos.x > 0)
						mResize_mask = engine::frect(0, 0, 1, 0); // Right
					else
						mResize_mask = engine::frect(1, 0, -1, 0); // Left
				}
				else
				{
					if (pos.y > 0)
						mResize_mask = engine::frect(0, 0, 0, 1); // Bottom
					else
						mResize_mask = engine::frect(0, 1, 0, -1); // Top
				}
				mState = state::resize_mode;
				mCommand_manager.add(std::make_shared<command_transform_wall>(mSelection));
				mOriginal_rect = mSelection->get_region();
				mDrag_from = tile_position;
			}
			else if (!tile_selection(exact_tile_position) // Create/Select
				|| button_shift) // Left shift allows use to place wall on another wall
			{
				mSelection = mContainer.add_collision_box(mCurrent_type);

				mCommand_manager.add(std::shared_ptr<command_add_wall>(new command_add_wall(mSelection, &mContainer)));

				mSelection->set_region({ tile_position, selection_size });

				mState = state::size_mode;
				mDrag_from = tile_position;
			}
			else // Move
			{
				mCommand_manager.add(std::make_shared<command_transform_wall>(mSelection));
				mState = state::move_mode;
				mDrag_from = tile_position - mSelection->get_region().get_offset();
			}
			update_labels();
		}

		// Remove tile
		else if (button_right)
		{
			// No cycling when removing tile.
			if (tile_selection(exact_tile_position, false))
			{
				mCommand_manager.add(std::shared_ptr<command_remove_wall>(new command_remove_wall(mSelection, &mContainer)));

				mContainer.remove_box(mSelection);

				mSelection = nullptr;
				update_labels();
			}
		}

		else if (pR.is_key_down(engine::renderer::key_type::LControl))
		{
			if (pR.is_key_pressed(engine::renderer::key_type::Z)) // Undo
				mCommand_manager.undo();
			else if (pR.is_key_pressed(engine::renderer::key_type::Y)) // Redo
				mCommand_manager.redo();
		}

		break;
	}
	case state::size_mode:
	{
		// Size mode only last while user is holding down left-click
		if (!button_left_down)
		{
			mState = state::normal;
			update_labels();
			break;
		}

		engine::frect   rect = mSelection->get_region();
		engine::fvector resize_to = tile_position;

		// Cursor moves behind the initial point where the wall is created
		if (tile_position.x <= mDrag_from.x)
		{
			rect.x = tile_position.x;   // Move the wall back
			resize_to.x = mDrag_from.x; // Resize accordingly
		}
		if (tile_position.y <= mDrag_from.y)
		{
			rect.y = tile_position.y;
			resize_to.y = mDrag_from.y;
		}
		rect.set_size(resize_to - rect.get_offset() + selection_size);

		// Update wall
		mSelection->set_region(rect);

		break;
	}
	case state::move_mode:
	{		
		if (!button_left_down)
		{
			mState = state::normal;
			update_labels();
			break;
		}

		auto rect = mSelection->get_region();
		rect.set_offset(tile_position - mDrag_from);
		mSelection->set_region(rect);
		break;
	}
	case state::resize_mode:
	{
		if (!button_left_down)
		{
			mState = state::normal;
			update_labels();
			break;
		}
		auto rect = mSelection->get_region();
		rect.set_offset(mOriginal_rect.get_offset() + (tile_position - mDrag_from)*mResize_mask.get_offset());
		rect.set_size(mOriginal_rect.get_size() + (tile_position - mDrag_from)*mResize_mask.get_size());

		// Limit size
		rect.w = std::max(rect.w, selection_size.x);
		rect.h = std::max(rect.h, selection_size.y);

		mSelection->set_region(rect);
		break;
	}
	}

	mBlackout.draw(pR);
	mTilemap_display.draw(pR);

	if (mGrid_snap != grid_snap::none)
	{
		// TODO: draw grid
	}

	for (auto& i : mContainer.get_boxes()) // TODO: Optimize
	{
		if (i == mSelection)
			mWall_display.set_outline_color({ 180, 90, 90, 255 });   // Outline wall red if selected...
		else
			mWall_display.set_outline_color({ 255, 255, 255, 255 }); // ...Otherwise it is white

		if (!i->get_wall_group())
			mWall_display.set_color({ 100, 255, 100, 200 }); // Green wall if not in a wall group...
		else
			mWall_display.set_color({ 200, 100, 200, 200 }); // ...Purple-ish otherwise

		// The wall region has to be scaled to pixel coordinates
		mWall_display.set_position(i->get_region().get_offset());
		mWall_display.set_size(i->get_region().get_size() * get_unit());
		mWall_display.draw(pR);
	}

	mBoundary_visualization.draw(pR);

	return 0;
}

void collisionbox_editor::load_terminal_interface(engine::terminal_system & pTerminal)
{
	mCollision_editor_group = std::make_shared<engine::terminal_command_group>();
	mCollision_editor_group->set_root_command("collision");
	mCollision_editor_group->add_command("clear",
		[&](const engine::terminal_arglist& pArgs)->bool
	{
		mContainer.clean();
		mSelection.reset();
		return true;
	}, "- Clear all collision boxes (Warning: Can't undo)");
	pTerminal.add_group(mCollision_editor_group);
}

int collisionbox_editor::save()
{
	logger::info("Saving collision boxes");

	mContainer.generate_xml(mLoader.get_document(), mLoader.get_collisionboxes());
	
	mLoader.save();

	logger::info("Saved " + std::to_string(mContainer.get_count()) +" collision box(es)");

	return 0;
}

void collisionbox_editor::setup_editor(editor_gui & pEditor_gui)
{
	pEditor_gui.add_group("Editor");
	pEditor_gui.add_value_enum("Grid Snapping", [&](size_t pI)
	{
		mGrid_snap = (grid_snap)pI;
	}, { "None", "Pixel", "Quarter Tile", "Half Tile", "Full Tile" }, 4);

	pEditor_gui.add_group("Box Properties");

	mCb_type = pEditor_gui.add_value_enum("Type", [&](size_t pI) {
		if (!mSelection) return;
		mCurrent_type = static_cast<rpg::collision_box::type>(pI); // Lazy cast
		auto temp = mSelection;
		mContainer.remove_box(mSelection);
		auto new_box = mContainer.add_collision_box((rpg::collision_box::type)mCb_type->getSelectedItemIndex());
		new_box->set_region(temp->get_region());
		new_box->set_wall_group(temp->get_wall_group());
		mSelection = new_box;
	}, {"Wall", "Trigger", "Button", "Door"}, 0);

	mCb_type->setSelectedItemByIndex(0);

	mTb_wallgroup = pEditor_gui.add_value_string("Wall Group", [&](std::string)
	{
		if (!mSelection) return;
		if (mTb_wallgroup->getText().isEmpty())
			mSelection->set_wall_group(nullptr);
		else
			mSelection->set_wall_group(mContainer.create_group(mTb_wallgroup->getText()));
	});

	mTb_box_x = pEditor_gui.add_value_float("X", [&](float pVal)
	{
		if (!mSelection) return;
		auto rect = mSelection->get_region();
		rect.x = pVal;
		mSelection->set_region(rect);
	});
	mTb_box_y = pEditor_gui.add_value_float("Y", [&](float pVal)
	{
		if (!mSelection) return;
		auto rect = mSelection->get_region();
		rect.y = pVal;
		mSelection->set_region(rect);
	});
	mTb_box_width = pEditor_gui.add_value_float("Width", [&](float pVal)
	{
		if (!mSelection) return;
		auto rect = mSelection->get_region();
		rect.w = pVal;
		mSelection->set_region(rect);
	}, false);
	mTb_box_height = pEditor_gui.add_value_float("Height", [&](float pVal)
	{
		if (!mSelection) return;
		auto rect = mSelection->get_region();
		rect.h = pVal;
		mSelection->set_region(rect);
	}, false);

	pEditor_gui.add_group("Door Properties");

	mTb_door_name = pEditor_gui.add_value_string("Name", [&](std::string pVal)
	{
		if (mSelection && mSelection->get_type() == rpg::collision_box::type::door)
			std::dynamic_pointer_cast<rpg::door>(mSelection)->set_name(pVal);
	});

	mTb_door_scene = pEditor_gui.add_value_enum("Destination Scene", [&](size_t pVal)
	{
		if (mSelection && mSelection->get_type() == rpg::collision_box::type::door)
			std::dynamic_pointer_cast<rpg::door>(mSelection)->set_scene(mTb_door_scene->getSelectedItem());
	}, {}, 0, true);
	populate_combox_with_scene_names(mTb_door_scene);

	mTb_door_destination = pEditor_gui.add_value_string("Destination Door", [&](std::string pVal)
	{
		if (mSelection && mSelection->get_type() == rpg::collision_box::type::door)
			std::dynamic_pointer_cast<rpg::door>(mSelection)->set_destination(pVal);
	});

	mTb_door_offsetx = pEditor_gui.add_value_float("Offset X", [&](float pVal)
	{
		if (mSelection && mSelection->get_type() != rpg::collision_box::type::door)
		{
			auto door = std::dynamic_pointer_cast<rpg::door>(mSelection);
			door->set_offset({ pVal, door->get_offset().y });
		}
	});

	mTb_door_offsety = pEditor_gui.add_value_float("Offset Y", [&](float pVal)
	{
		if (mSelection && mSelection->get_type() == rpg::collision_box::type::door)
		{
			auto door = std::dynamic_pointer_cast<rpg::door>(mSelection);
			door->set_offset({ door->get_offset().x, pVal });
		}
	});
}

bool collisionbox_editor::tile_selection(engine::fvector pCursor, bool pCycle)
{
	const auto hits = mContainer.collision(pCursor);
	if (hits.empty())
	{
		mSelection = nullptr;
		return false;
	}

	// Cycle through overlapping walls.
	// Check if selection is selected again.
	if (mSelection
		&& mSelection->get_region().is_intersect(pCursor))
	{
		// It will not select the tile underneath by it will retain the current
		// selection if selected
		if (!pCycle)
			return true;

		// Find the hit that is underneath the current selection.
		// Start at 1 because it does require that there is one wall underneath
		// and overall works well when looping through.
		for (size_t i = 1; i < hits.size(); i++)
			if (hits[i] == mSelection)
			{
				mSelection = hits[i - 1];
				return true;
			}
	}

	mSelection = hits.back(); // Top hit
	return true;
}

void collisionbox_editor::update_labels()
{
	if (!mSelection)
		return;

	const auto rect = mSelection->get_region();

	// Update current wall group tb
	if (!mSelection->get_wall_group())
		mTb_wallgroup->setText("");
	else
		mTb_wallgroup->setText(mSelection->get_wall_group()->get_name());

	// Update current type
	mCurrent_type = mSelection->get_type();
	mCb_type->setSelectedItemByIndex(static_cast<size_t>(mCurrent_type));

	mTb_box_x->setText(std::to_string(mSelection->get_region().x));
	mTb_box_y->setText(std::to_string(mSelection->get_region().y));
	mTb_box_width->setText(std::to_string(mSelection->get_region().w));
	mTb_box_height->setText(std::to_string(mSelection->get_region().h));

	if (mSelection->get_type() == rpg::collision_box::type::door)
	{
		auto door = std::dynamic_pointer_cast<rpg::door>(mSelection);
		mTb_door_name->setText(door->get_name());
		mTb_door_scene->setSelectedItem(door->get_scene());
		mTb_door_destination->setText(door->get_destination());
		mTb_door_offsetx->setText(std::to_string(door->get_offset().x));
		mTb_door_offsety->setText(std::to_string(door->get_offset().y));
	}
}

// ##########
// atlas_editor
// ##########

atlas_editor::atlas_editor()
{
	mTexture.reset(new engine::texture);

	mFull_animation.set_color({ 100, 100, 255, 100 });
	mFull_animation.set_parent(mBackground);

	mSelected_firstframe.set_color({ 0, 0, 0, 0 });
	mSelected_firstframe.set_outline_color({ 255, 255, 0, 255 });
	mSelected_firstframe.set_outline_thinkness(1);
	mSelected_firstframe.set_parent(mBackground);
	
	mPreview_bg.set_anchor(engine::anchor::bottom);
	mPreview_bg.set_color({ 0, 0, 0, 200 });
	mPreview_bg.set_outline_color({ 255, 255, 255, 200 });
	mPreview_bg.set_outline_thinkness(1);
	mPreview_bg.set_parent(mBackground);

	mPreview.set_anchor(engine::anchor::bottom);
	mPreview.set_parent(mPreview_bg);
}

bool atlas_editor::open_editor()
{
	black_background();
	mZoom = 1;
	mPreview.set_visible(false);
	get_textures("./data/textures");
	if (!mTexture_list.empty())
	{
		mCb_texture_select->setSelectedItemByIndex(0);
		setup_for_texture(mTexture_list[0]);
	}
	return true;
}

int atlas_editor::draw(engine::renderer & pR)
{
	const bool button_left = pR.is_mouse_pressed(engine::renderer::mouse_button::mouse_left);
	const bool button_left_down = pR.is_mouse_down(engine::renderer::mouse_button::mouse_left);
	const bool button_right = pR.is_mouse_pressed(engine::renderer::mouse_button::mouse_right);
	const bool button_shift = pR.is_key_down(engine::renderer::key_type::LShift);
	const bool button_ctrl = pR.is_key_down(engine::renderer::key_type::LControl);

	const engine::fvector mouse_position = pR.get_mouse_position();

	if (pR.is_mouse_pressed(engine::renderer::mouse_button::mouse_right))
		mDrag_offset = mBackground.get_position() - mouse_position;
	else if (pR.is_mouse_down(engine::renderer::mouse_button::mouse_right))
		mBackground.set_position(mouse_position + mDrag_offset);

	if (pR.is_key_pressed(engine::renderer::key_type::Add))
	{
		mZoom += 1;
		mBackground.set_scale({ mZoom, mZoom });
		update_preview();
	}
	
	if (pR.is_key_pressed(engine::renderer::key_type::Subtract))
	{
		mZoom -= 1;
		mBackground.set_scale({ mZoom, mZoom });
		update_preview();
	}

	if (pR.is_mouse_pressed(engine::renderer::mouse_button::mouse_left))
		atlas_selection((mouse_position - mBackground.get_position())/mZoom);

	mBlackout.draw(pR);
	mBackground.draw(pR);

	for (auto& i : mAtlas.get_raw_atlas())
	{
		auto full_region = i->full_region()*mZoom;
		if (i == mSelection)
			mFull_animation.set_color({ 255, 255, 100, 50 });
		else
			mFull_animation.set_color({ 100, 100, 255, 100 });
		mFull_animation.set_position(full_region.get_offset());
		mFull_animation.set_size(full_region.get_size());
		mFull_animation.draw(pR);

		if (i == mSelection)
		{
			auto rect = i->get_frame_at(i->get_default_frame())*mZoom;
			mSelected_firstframe.set_position(rect.get_offset());
			mSelected_firstframe.set_size(rect.get_size());
			mSelected_firstframe.draw(pR);
		}
	}

	// Animation Preview
	if (mSelection && mSelection->get_frame_count() > 1) // Only display if there is an animation
	{
		mPreview_bg.draw(pR);
		mPreview.draw(pR);
	}

	return 0;
}

int atlas_editor::save()
{
	if (mTexture_list.empty() || !mAtlas_changed)
		return 0;
	const std::string xml_path = mLoaded_texture.string() + ".xml";
	logger::info("Saving atlas '" + xml_path + "'...");
	
	mAtlas.remove_entry("_name_here_");
	mAtlas.save(xml_path);
	mAtlas_changed = false;
	logger::info("Atlas save");
	return 0;
}

void atlas_editor::get_textures(const std::string & pPath)
{
	mTexture_list.clear();
	for (auto& i : engine::fs::recursive_directory_iterator(pPath))
	{
		engine::encoded_path path = i.path().string();
		if (path.extension() == ".png")
		{
			mTexture_list.push_back(path.parent() / path.stem());

			if (engine::fs::exists(i.path().parent_path() / (i.path().stem().string() + ".xml")))
				mCb_texture_select->addItem(mTexture_list.back().filename());
			else
				mCb_texture_select->addItem("*" + mTexture_list.back().filename());
		}
	}
}

void atlas_editor::setup_for_texture(const engine::encoded_path& pPath)
{
	mAtlas_changed = false;
	mLoaded_texture = pPath;

	const std::string texture_path = pPath.string() + ".png";
	mTexture->unload();
	mTexture->set_texture_source(texture_path);
	mTexture->load();
	mPreview.set_texture(mTexture);
	mBackground.set_texture(mTexture);
	mBackground.set_texture_rect({ engine::fvector(0, 0), mTexture->get_size() });

	mSelection = nullptr;
	mAtlas.clear();

	const std::string xml_path = pPath.string() + ".xml";
	if (!engine::fs::exists(xml_path))
	{
		logger::info("Starting a new atlas");
		clear_gui();
		new_entry();
		return;
	}

	mAtlas.load(pPath.string() + ".xml");
	if (!mAtlas.get_raw_atlas().empty())
	{
		mCb_entry_select->setSelectedItemByIndex(0);
		mSelection = mAtlas.get_raw_atlas().back();
		update_settings();
		update_preview();
	}
	update_entry_list();
}

void atlas_editor::new_entry()
{
	if (auto find = mAtlas.get_entry("_Name_here_"))
	{
		logger::warning("A new, unnamed, entry has already been created");
		mSelection = find;
		update_settings();
		update_preview();
		return;
	}
	mSelection = std::make_shared<engine::subtexture>();
	mSelection->set_name("_Name_here_");
	mSelection->set_frame_count(1);
	mSelection->set_loop(engine::animation::loop_type::none);
	mAtlas.add_entry(mSelection);
	update_entry_list();
	update_settings();
	update_preview();
	mAtlas_changed = true;
}

void atlas_editor::remove_selected()
{
	mAtlas.remove_entry(mSelection);
	if (mAtlas.is_empty())
		mSelection = nullptr;
	else
		mSelection = mAtlas.get_raw_atlas().back();
	update_entry_list();
	update_settings();
	update_preview();
	mAtlas_changed = true;
}

void atlas_editor::atlas_selection(engine::fvector pPosition)
{
	std::vector<engine::subtexture::ptr> hits;
	for (auto& i : mAtlas.get_raw_atlas())
		if (i->full_region().is_intersect(pPosition))
			hits.push_back(i);

	if (hits.empty())
		return;

	// Similar cycling as the collisionbox editor
	for (size_t i = 1; i < hits.size(); i++)
	{
		if (hits[i] == mSelection)
		{
			mSelection = hits[i - 1];
			update_settings();
			update_preview();
			return;
		}
	}
	mSelection = hits.back();
	update_settings();
	update_preview();
}

void atlas_editor::setup_editor(editor_gui & pEditor_gui)
{
	mCb_texture_select = pEditor_gui.add_combobox();
	mCb_texture_select->connect("ItemSelected",
		[&]() {
		const int item = mCb_texture_select->getSelectedItemIndex();
		if (item == -1)
		{
			logger::warning("No item selected");
			return;
		}
		save();
		setup_for_texture(mTexture_list[item]);
	});

	pEditor_gui.add_small_label("Entry: ");
	mCb_entry_select = pEditor_gui.add_combobox();
	mCb_entry_select->connect("ItemSelected",
		[&]() {
		const int item = mCb_entry_select->getSelectedItemIndex();
		if (item == -1)
		{
			logger::warning("No item selected");
			return;
		}
		mSelection = mAtlas.get_raw_atlas()[item];
		update_settings();
		update_preview();
	});

	pEditor_gui.add_group("Properties");

	mTb_name = pEditor_gui.add_value_string("Name", [&](std::string pVal)
	{
		if (!mSelection) return;
		// Rename
		if (pVal != mSelection->get_name()
			&& util::shortcuts::validate_potential_xml_name(pVal))
		{
			if (!mAtlas.get_entry(pVal))
			{
				mSelection->set_name(pVal);
				update_entry_list();
			}
			else
				logger::error("Animation with name '" + pVal + "' already exists");
		}
		mAtlas_changed = true;
	});

	mTb_frames = pEditor_gui.add_value_int("Frames", [&](int pVal)
	{
		if (!mSelection || pVal < 1) return;
		mSelection->set_frame_count(pVal);
		mAtlas_changed = true;
	}, false); 

	mTb_interval = pEditor_gui.add_value_int("interval", [&](int pVal)
	{
		if (!mSelection || pVal < 1) return;
		mSelection->add_interval(0, static_cast<float>(pVal));
		mAtlas_changed = true;
	}, false);
	
	mTb_default_frame = pEditor_gui.add_value_int("Default Frame", [&](int pVal)
	{
		if (!mSelection || pVal < 1) return;
		mSelection->set_default_frame(pVal);
		mAtlas_changed = true;
	}, false);

	mTb_size_x = pEditor_gui.add_value_int("X", [&](int pVal)
	{
		if (!mSelection || pVal < 0) return;
		engine::frect rect = mSelection->get_frame_at(0);
		rect.x = static_cast<float>(pVal);
		mSelection->set_frame_rect(rect);
		mAtlas_changed = true;
	}, false);

	mTb_size_y = pEditor_gui.add_value_int("Y", [&](int pVal)
	{
		if (!mSelection || pVal < 0) return;
		engine::frect rect = mSelection->get_frame_at(0);
		rect.y = static_cast<float>(pVal);
		mSelection->set_frame_rect(rect);
		mAtlas_changed = true;
	}, false);

	mTb_size_w = pEditor_gui.add_value_int("Width", [&](int pVal)
	{
		if (!mSelection || pVal < 0) return;
		engine::frect rect = mSelection->get_frame_at(0);
		rect.w = static_cast<float>(pVal);
		mSelection->set_frame_rect(rect);
		mAtlas_changed = true;
	}, false);

	mTb_size_h = pEditor_gui.add_value_int("Height", [&](int pVal)
	{
		if (!mSelection || pVal < 0) return;
		engine::frect rect = mSelection->get_frame_at(0);
		rect.h = static_cast<float>(pVal);
		mSelection->set_frame_rect(rect);
		mAtlas_changed = true;
	}, false);

	mCb_loop = pEditor_gui.add_value_enum("Loop", [&](size_t pVal)
	{
		mSelection->set_loop(
			static_cast<engine::animation::loop_type>
			(pVal)); // Lazy cast
	}, { "Disabled", "Linear", "Pingpong" });

	pEditor_gui.add_horizontal_buttons(
	{ 
		{"New", [&]() { new_entry(); } },
		{"Delete", [&]() { remove_selected(); } }
	});
	
	pEditor_gui.add_group("Preview");

	pEditor_gui.add_button("Reload",
	[&](){
		mTexture->unload();
		mTexture->load();
		mBackground.set_texture(mTexture);
		mBackground.set_texture_rect(engine::frect(engine::fvector(0, 0), mTexture->get_size()));
	});

	pEditor_gui.add_value_enum("Background", [&](size_t pVal)
	{
		if (pVal == 0)
			black_background();
		else
			white_background();
	}, { "Black", "White" });
}


void atlas_editor::black_background()
{
	mBlackout.set_color({ 0, 0, 0, 255 });
	mPreview_bg.set_color({ 0, 0, 0, 150 });
	mPreview_bg.set_outline_color({ 255, 255, 255, 200 });
}

void atlas_editor::white_background()
{
	mBlackout.set_color({ 255, 255, 255, 255 });
	mPreview_bg.set_color({ 255, 255, 255, 150 });
	mPreview_bg.set_outline_color({ 0, 0, 0, 200 });
}


void atlas_editor::update_entry_list()
{
	mCb_entry_select->removeAllItems();
	for (auto& i : mAtlas.get_raw_atlas())
		mCb_entry_select->addItem(i->get_name());
	if (mSelection)
		mCb_entry_select->setSelectedItem(mSelection->get_name());
}

void atlas_editor::update_settings()
{
	if (!mSelection)
		return;
	mCb_entry_select->setSelectedItem(mSelection->get_name());
	mTb_name->setText(mSelection->get_name());
	mTb_frames->setText(std::to_string(mSelection->get_frame_count()));
	mTb_default_frame->setText(std::to_string(mSelection->get_default_frame()));
	mTb_interval->setText(std::to_string(mSelection->get_interval()));
	mCb_loop->setSelectedItemByIndex(static_cast<size_t>(mSelection->get_loop()));

	engine::frect size = mSelection->get_frame_at(0);
	mTb_size_x->setText(std::to_string(size.x));
	mTb_size_y->setText(std::to_string(size.y));
	mTb_size_w->setText(std::to_string(size.w));
	mTb_size_h->setText(std::to_string(size.h));
}

void atlas_editor::update_preview()
{
	if (!mSelection)
		return;

	auto position = mSelection->get_frame_at(0).get_offset();
	position.x += mSelection->full_region().w / 2;
	mPreview_bg.set_position(position);

	mPreview.set_scale({ mZoom, mZoom });
	mPreview.set_animation(mSelection);
	mPreview.restart();
	mPreview.start();

	mPreview_bg.set_size(mPreview.get_size());

}

void atlas_editor::clear_gui()
{
	mCb_entry_select->removeAllItems();
	mTb_frames->setText("0");
	mTb_interval->setText("0");
	mTb_size_x->setText("0");
	mTb_size_y->setText("0");
	mTb_size_w->setText("0");
	mTb_size_h->setText("0");
}


// ##########
// scroll_control_node
// ##########

void scroll_control_node::movement(engine::renderer & pR)
{
	float speed = pR.get_delta() * 4;
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
	logger::info("Opening tilemap editor...");
	mTilemap_editor.set_editor_gui(mEditor_gui);
	if (mTilemap_editor.open_scene(pScene_path))
		mCurrent_editor = &mTilemap_editor;
	mTilemap_editor.open_editor();
	logger::info("Editor loaded");
}

void editor_manager::open_collisionbox_editor(std::string pScene_path)
{
	logger::info("Opening collisionbox editor...");
	mCollisionbox_editor.set_editor_gui(mEditor_gui);
	if (mCollisionbox_editor.open_scene(pScene_path))
		mCurrent_editor = &mCollisionbox_editor;
	mCollisionbox_editor.open_editor();
	logger::info("Editor opened");
}

void editors::editor_manager::open_atlas_editor()
{
	logger::info("Opening texture/atlas editor...");
	mAtlas_editor.set_editor_gui(mEditor_gui);
	mCurrent_editor = &mAtlas_editor;
	mAtlas_editor.open_editor();
	logger::info("Editor opened");
}

void editor_manager::close_editor()
{
	if (!mCurrent_editor)
		return;

	logger::info("Closing Editor...");
	mCurrent_editor->save();
	mCurrent_editor = nullptr;
	mEditor_gui.clear();
	logger::info("Editor closed");
}

void editor_manager::set_world_node(node& pNode)
{
	mRoot_node.set_parent(pNode);
	mEditor_gui.set_parent(mRoot_node);
}

void editor_manager::set_resource_manager(engine::resource_manager& pResource_manager)
{
	mTilemap_editor.set_resource_manager(pResource_manager);
	mCollisionbox_editor.set_resource_manager(pResource_manager);
}

void editor_manager::load_terminal_interface(engine::terminal_system & pTerminal)
{
	mTilemap_editor.load_terminal_interface(pTerminal);
	mCollisionbox_editor.load_terminal_interface(pTerminal);
}

void editor_manager::set_scene(rpg::scene& pScene)
{
	mEditor_gui.set_scene(&pScene);
}

int editor_manager::draw(engine::renderer& pR)
{
	if (mCurrent_editor != nullptr)
	{
		if (pR.is_key_down(engine::renderer::key_type::LControl)
			&& pR.is_key_pressed(engine::renderer::key_type::S))
			mCurrent_editor->save();
		mRoot_node.movement(pR);
		mCurrent_editor->draw(pR);
	}else
		mRoot_node.set_position({ 0, 0 });
	return 0;
}

void editor_manager::refresh_renderer(engine::renderer & pR)
{
	mEditor_gui.set_renderer(pR);
	mEditor_gui.set_depth(-1001);
}

editor_boundary_visualization::editor_boundary_visualization()
{
	const engine::color line_color(255, 255, 255, 150);

	mLines.set_outline_color(line_color);
	mLines.set_color(engine::color(0, 0, 0, 0));
	mLines.set_parent(*this);
}

void editor_boundary_visualization::set_boundary(engine::frect pBoundary)
{
	// get_size requires pixel input
	const auto boundary = engine::scale(pBoundary, get_unit());

	mLines.set_position(boundary.get_offset());
	mLines.set_size(boundary.get_size());
}

int editor_boundary_visualization::draw(engine::renderer & pR)
{
	mLines.draw(pR);
	return 0;
}


