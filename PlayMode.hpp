#include "PPU466.hpp"
#include "Mode.hpp"

#include <glm/glm.hpp>

#include <vector>
#include <deque>

struct PlayMode : Mode
{
	PlayMode();
	virtual ~PlayMode();

	// functions called by main loop:
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

	//----- game state -----

	// input tracking:
	struct Button
	{
		uint8_t downs = 0;
		uint8_t pressed = 0;
	} left, right, down, up;

	glm::ivec2 goose = glm::ivec2(120, 8);
	std::vector<glm::ivec2> chain;			 // duckling positions where chain[i] is the ith duck position
	glm::ivec2 ducks[3];					 // duckling positions in and around the map
	bool duck_alive[3] = {true, true, true}; // whether the duckling is alive or not
	float car_x[8];							 // x positions of the cars
	int32_t car_lane[8];					 // car lane for random spawning
	uint32_t score = 0;						 // score of player
	bool dead = false;						 // whether the goose is dead or alive, controls game stat between game and death

	//----- drawing handled by PPU466 -----

	PPU466 ppu;
};
