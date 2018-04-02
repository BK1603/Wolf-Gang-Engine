#ifndef RPG_EDITOR_HPP
#define RPG_EDITOR_HPP

#include <engine/renderer.hpp>
#include <engine/rect.hpp>
#include <engine/terminal.hpp>

#include <rpg/tilemap_manipulator.hpp>
#include <rpg/tilemap_display.hpp>
#include <rpg/scene_loader.hpp>
#include <rpg/collision_box.hpp>
#include <rpg/scene.hpp>
#include <rpg/rpg.hpp>

#include <vector>
#include <string>
#include <list>
#include <functional>
#include <tuple>

#include <imgui.h>
#include <imgui-SFML.h>

class command
{
public:
	virtual bool execute() = 0;
	virtual bool undo() = 0;
	virtual bool redo() { return execute(); };
};

class command_manager
{
public:
	bool execute(std::shared_ptr<command> pCommand);
	bool add(std::shared_ptr<command> pCommand);

	void start(std::shared_ptr<command> pCommand);
	template<typename T>
	std::shared_ptr<T> current()
	{
		return std::dynamic_pointer_cast<T>(mCurrent);
	}
	void complete();

	bool undo();
	bool redo();
	void clear();

private:
	std::shared_ptr<command> mCurrent;
	std::vector<std::shared_ptr<command>> mUndo;
	std::vector<std::shared_ptr<command>> mRedo;
};

namespace editors
{

const engine::color default_gui_bg_color(30, 30, 30, 255);

class editor :
	public engine::render_object
{
public:
	editor();

	void set_game(rpg::game& pGame);

	virtual bool open_editor() { return true;  }

	bool is_changed() const;

	virtual bool save() { mIs_changed = false; return true; };

protected:
	rpg::game* mGame;

	void editor_changed();

private:
	bool mIs_changed;
};

class scene_editor :
	public editor
{
public:
	scene_editor();
	bool open_scene(std::string pName); // Should be called before open_editor()

protected:
	float mZoom;
	void update_zoom(engine::renderer& pR);

	rpg::scene_loader mLoader;
	rpg::tilemap_manipulator mTilemap_manipulator;
	rpg::tilemap_display     mTilemap_display;
};

class game_editor :
	public editor
{
public:
	game_editor();

	bool save() { return 0; }

	int draw(engine::renderer& pR);

	rpg::game& get_game();

	void load_game(const std::string& pSource);

protected:
	virtual void refresh_renderer(engine::renderer& pR);

private:
	engine::renderer mRenderer;
	rpg::game mGame;

	engine::timer mInfo_update_timer;
};

class tilemap_editor :
	public scene_editor
{
public:
	tilemap_editor();
	virtual bool open_editor();
	int draw(engine::renderer& pR);
	void load_terminal_interface(engine::terminal_system& pTerminal);

	bool save() override;

	void clean();

private:
	std::shared_ptr<engine::terminal_command_group> mTilemap_group;

	enum class state
	{
		none,
		drawing,
		drawing_region,
		erasing,
	} mState;

	size_t mCurrent_tile; // Index of mTile_list
	int    mRotation;
	int    mLayer;
	bool   mIs_highlight;

	engine::fvector mLast_tile;

	std::vector<std::string> mTile_list;

	std::shared_ptr<engine::texture> mTexture;
	std::string mCurrent_texture_name;

	engine::sprite_node mPreview;

	engine::grid mGrid;

	command_manager mCommand_manager;

	// User Actions

	void copy_tile_type_at(engine::fvector pAt);
	void draw_tile_at(engine::fvector pAt);
	void erase_tile_at(engine::fvector pAt);
	void next_tile();
	void previous_tile();
	void rotate_clockwise();

	void update_preview();
	void update_highlight();
	void update_tilemap();

	void tick_highlight(engine::renderer& pR);

	void apply_texture();
};

class collisionbox_editor :
	public scene_editor
{
public:
	collisionbox_editor();

	virtual bool open_editor();

	int draw(engine::renderer& pR);
	void load_terminal_interface(engine::terminal_system& pTerminal);

	bool save();
	
private:
	std::shared_ptr<engine::terminal_command_group> mCollision_editor_group;

	command_manager mCommand_manager;

	std::shared_ptr<rpg::collision_box> mSelection;


	enum class state
	{
		normal,
		size_mode,
		move_mode,
		resize_mode
	};

	state mState;
	engine::fvector mDrag_from;

	enum class grid_snap
	{
		none,
		pixel,
		eighth,
		quarter,
		full
	} mGrid_snap;

	engine::frect mResize_mask;
	engine::frect mOriginal_rect; // For resize_mode

	rpg::collision_box::type     mCurrent_type;
	rpg::collision_box_container mContainer;

	engine::rectangle_node mWall_display;
	engine::rectangle_node mResize_display;

	engine::grid mGrid;

	bool tile_selection(engine::fvector pCursor, bool pCycle = true);
};

class atlas_editor :
	public editor
{
public:
	atlas_editor();

	virtual bool open_editor();

	int draw(engine::renderer& pR);

	bool save();

private:
	void get_textures(const std::string& pPath);
	void setup_for_texture(const engine::encoded_path& pPath);

	enum class state
	{
		normal,
		size_mode,
		move_mode,
		resize_mode,
	} mState;

	bool mAtlas_changed;
	engine::encoded_path mLoaded_texture;
	std::vector<engine::encoded_path> mTexture_list;
	std::shared_ptr<engine::texture> mTexture;

	engine::texture_atlas mAtlas;
	engine::subtexture::ptr mSelection;

	void new_entry();
	void remove_selected();

	engine::fvector mDrag_offset;

	float mZoom;

	engine::rectangle_node mBackground;
	engine::sprite_node mSprite;
	engine::rectangle_node mPreview_bg;
	engine::animation_node mPreview;
	engine::rectangle_node mFull_animation;
	engine::rectangle_node mSelected_firstframe;

	void atlas_selection(engine::fvector pPosition);

	void black_background();
	void white_background();

	void update_preview();
};

class editor_settings_loader
{
public:
	bool load(const engine::fs::path& pPath);

	std::string generate_open_cmd(const std::string& pFilepath) const;
	std::string generate_opento_cmd(const std::string& pFilepath, size_t pRow, size_t pCol);

private:
	std::string mPath;
	std::string mOpen_param;
	std::string mOpento_param;
};

class WGE_editor
{
public:
	WGE_editor();

	void initualize(const std::string& pCustom_location);

	void run();

private:
	engine::renderer mRenderer;
	engine::display_window mWindow;

	game_editor         mGame_editor;
	tilemap_editor      mTilemap_editor;
	collisionbox_editor mCollisionbox_editor;
	atlas_editor        mAtlas_editor;

	editor_settings_loader mSettings;
};


class WGE_imgui_editor
{
public:
	WGE_imgui_editor();
	void run();

private:

	int mTile_size;
	void draw_game_window();

	bool mIs_game_view_window_focused; // Used to determine if game recieves events or not
	void draw_game_view_window();

	size_t mSelected_tile;
	int mTile_rotation;
	void draw_tile_window();

	void draw_tilemap_layers_window();

	void draw_collision_settings_window();


	std::vector<std::string> mScene_list;

	sf::RenderWindow mWindow;

	sf::RenderTexture mGame_render_target;
	rpg::game mGame;
	engine::renderer mGame_renderer;
};

}

#endif