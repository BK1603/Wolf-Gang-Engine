#ifndef RPG_PLAYER_CHARACTER
#define RPG_PLAYER_CHARACTER

#include <rpg/character_entity.hpp>
#include <engine/controls.hpp>
#include <rpg/collision_system.hpp>

namespace rpg {

// The main player character_entity
class player_character :
	public character_entity
{
public:
	void reset();

	player_character();
	void set_locked(bool pLocked);
	bool is_locked();

	// Do movement with collision detection
	void movement(engine::controls &pControls, collision_system& pCollision_system, float pDelta);

	// Get point in front of player
	engine::fvector get_activation_point(float pDistance = 0.6f);
	engine::frect   get_collision_box() const;

private:
	void walking_direction(engine::fvector pMove);

	bool mLocked;
	bool mIs_walking;
};

}
#endif // !RPG_PLAYER_CHARACTER
