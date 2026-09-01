#include "PlayMode.hpp"
#include "read_write_chunk.hpp"
#include "data_path.hpp"
#include <algorithm>
#include <fstream>
#include <cstdlib>
#include <ctime>

// assets hopefully
// #include "assets.hpp"

// for the GL_ERRORS() macro:
#include "gl_errors.hpp"
// for glm::value_ptr() :
#include <glm/gtc/type_ptr.hpp>
#include <random>

// constants
static int32_t const LANES[8] = {32, 48, 64, 80, 128, 144, 160, 176};
static float const LANE_SPEED[8] = {30.0f, -40.0f, 35.0f, -45.0f, 40.0f, -30.0f};

static int32_t lane_at(int32_t y)
{
	for (int32_t i = 0; i < 6; ++i)
		if (y >= LANES[i] && y < LANES[i] + 24)
			return i;
	return -1;
}

PlayMode::PlayMode()
{
	std::ifstream in(data_path("assets.blob"), std::ios::binary);
	std::vector<PPU466::Tile> tiles;
	std::vector<PPU466::Palette> palettes;
	read_chunk(in, "tile", &tiles);
	read_chunk(in, "pal0", &palettes);
	std::copy(tiles.begin(), tiles.end(), ppu.tile_table.begin());
	std::copy(palettes.begin(), palettes.end(), ppu.palette_table.begin());

	for (uint32_t row = 0; row < 30; ++row)
	{
		int32_t lane = lane_at(row * 8);
		for (uint32_t x = 0; x < 32; ++x)
		{
			uint16_t tile, pal;
			if (lane < 0)
			{
				tile = uint16_t(9 + (x + row) % 3);
				pal = 4;
			}
			else
			{
				int32_t r = (int32_t(row) * 8 - LANES[lane]) / 8;
				if (r == 1)
				{
					tile = (x % 2 ? 5 : 7); // middle row is made up of the dash tiles
				}
				else
				{
					tile = (x % 2 ? 5 : 6); // other rows are made up of the road tiles
				}
				pal = 3;
			}
			ppu.background[x + PPU466::BackgroundWidth * row] = tile | (pal << 8);
		}
	}

	srand(uint32_t(time(nullptr)));

	// cars at random lanes and positions:
	for (uint32_t i = 0; i < 8; ++i)
	{
		car_lane[i] = (i < 6 ? int32_t(i) : rand() % 6);
		float back = float(rand() % 300);
		if (LANE_SPEED[car_lane[i]] > 0.0f)
			car_x[i] = -24.0f - back;
		else
			car_x[i] = 256.0f + back;
	}

	// three ducklings, anywhere on the map:
	for (uint32_t i = 0; i < 3; ++i)
	{
		ducks[i] = glm::ivec2((rand() % 30) * 8, (rand() % 28) * 8);
	}
}

PlayMode::~PlayMode()
{
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size)
{

	if (evt.type == SDL_EVENT_KEY_DOWN)
	{
		if (evt.key.key == SDLK_LEFT)
		{
			left.downs += 1;
			left.pressed = true;
			return true;
		}
		else if (evt.key.key == SDLK_RIGHT)
		{
			right.downs += 1;
			right.pressed = true;
			return true;
		}
		else if (evt.key.key == SDLK_UP)
		{
			up.downs += 1;
			up.pressed = true;
			return true;
		}
		else if (evt.key.key == SDLK_DOWN)
		{
			down.downs += 1;
			down.pressed = true;
			return true;
		}
	}
	else if (evt.type == SDL_EVENT_KEY_UP)
	{
		if (evt.key.key == SDLK_LEFT)
		{
			left.pressed = false;
			return true;
		}
		else if (evt.key.key == SDLK_RIGHT)
		{
			right.pressed = false;
			return true;
		}
		else if (evt.key.key == SDLK_UP)
		{
			up.pressed = false;
			return true;
		}
		else if (evt.key.key == SDLK_DOWN)
		{
			down.pressed = false;
			return true;
		}
	}

	return false;
}

void PlayMode::update(float elapsed)
{
	// cars
	for (uint32_t i = 0; i < 8; ++i)
	{
		car_x[i] += LANE_SPEED[car_lane[i]] * elapsed;
		if (car_x[i] > 256.0f)
			car_x[i] = -24.0f;
		if (car_x[i] < -24.0f)
			car_x[i] = 256.0f;
	}

	// death check
	if (dead)
	{
		left.downs = right.downs = up.downs = down.downs = 0;
		return;
	}

	// one 8 pixel step in the dir of pressed button
	glm::ivec2 old = goose;
	if (left.downs)
		goose.x -= 8;
	if (right.downs)
		goose.x += 8;
	if (down.downs)
		goose.y -= 8;
	if (up.downs)
		goose.y += 8;
	goose.x = std::max(0, std::min(240, goose.x));
	goose.y = std::max(0, std::min(224, goose.y));

	//
	if (goose != old)
	{
		for (size_t i = chain.size(); i > 0; --i)
		{
			chain[i - 1] = (i == 1 ? old : chain[i - 2]);
		}

		if (lane_at(goose.y) >= 0)
			score += 10 * uint32_t(chain.size() + 1);
	}

	// duckling pick up stuff
	for (uint32_t i = 0; i < 3; ++i)
	{
		if (duck_alive[i] && chain.size() < 10 && goose.x - 8 < ducks[i].x + 8 && goose.x + 24 > ducks[i].x && goose.y - 8 < ducks[i].y + 8 && goose.y + 24 > ducks[i].y)
		{
			duck_alive[i] = false;
			chain.emplace_back(ducks[i]);
		}
	}

	// cars again but like hitboxes
	for (uint32_t i = 0; i < 8; ++i)
	{
		int32_t cx = int32_t(car_x[i]);
		int32_t cy = LANES[car_lane[i]] + 4;
		if (goose.x + 3 < cx + 24 && cx < goose.x + 13 && goose.y + 3 < cy + 16 && cy < goose.y + 13)
		{
			dead = true;
		}
		for (size_t d = 0; d < chain.size(); ++d)
		{
			if (chain[d].x < cx + 24 && cx < chain[d].x + 8 && chain[d].y < cy + 16 && cy < chain[d].y + 8)
			{
				chain.resize(d);
				break;
			}
		}
	}

	// respawn ducklings after goose made it up
	if (goose.y >= 192)
	{
		goose = glm::ivec2(120, 8);
		int32_t want = 10 - int32_t(chain.size());
		if (want > 3)
			want = 3;
		for (uint32_t i = 0; i < 3; i++)
		{
			duck_alive[i] = i < want;
			// random spot
			ducks[i] = glm::ivec2((rand() % 30) * 8, (rand() % 28) * 8);
		}
	}

	left.downs = right.downs = up.downs = down.downs = 0;
}

void PlayMode::draw(glm::uvec2 const &drawable_size)
{
	uint32_t s = score;
	for (uint32_t i = 6; i > 0; --i)
	{
		ppu.background[(i - 1) + PPU466::BackgroundWidth * 29] = uint16_t(s % 10 == 0 ? 249 : 239 + s % 10) | (1 << 8);
		s /= 10;
	}

	uint32_t n = 0;

	if (dead)
	{
		for (uint32_t i = 0; i < 32; ++i)
		{
			ppu.sprites[n].x = uint8_t(96 + (i % 8) * 8);
			ppu.sprites[n].y = uint8_t(128 - (i / 8) * 8);
			ppu.sprites[n].index = uint8_t(176 + 16 * (i / 8) + i % 8);
			ppu.sprites[n].attributes = 3;
			n++;
		}
	}
	else
	{
		int32_t gx[4] = {2, 8, 0, 8};
		int32_t gy[4] = {6, 8, 0, 0};
		for (uint32_t i = 0; i < 4; ++i)
		{
			ppu.sprites[n].x = uint8_t(goose.x + gx[i]);
			ppu.sprites[n].y = uint8_t(goose.y + gy[i]);
			ppu.sprites[n].index = uint8_t(i);
			ppu.sprites[n].attributes = (i == 0 ? 0 : 1);
			n++;
		}

		for (size_t i = 0; i < chain.size(); ++i)
		{
			ppu.sprites[n].x = uint8_t(chain[i].x);
			ppu.sprites[n].y = uint8_t(chain[i].y);
			ppu.sprites[n].index = 4;
			ppu.sprites[n].attributes = 2;
			n++;
		}

		for (uint32_t i = 0; i < 3; ++i)
		{
			if (!duck_alive[i])
				continue;
			ppu.sprites[n].x = uint8_t(ducks[i].x);
			ppu.sprites[n].y = uint8_t(ducks[i].y);
			ppu.sprites[n].index = 4;
			ppu.sprites[n].attributes = 2;
			n++;
		}
	}

	for (uint32_t i = 0; i < 8; ++i)
	{
		if (n + 6 > 64)
			break;
		int32_t cx = int32_t(car_x[i]);
		int32_t cy = LANES[car_lane[i]] + 4;
		for (uint32_t k = 0; k < 3; ++k)
		{
			int32_t x = cx + int32_t(k) * 8;
			if (x < 0 || x > 248)
				continue;
			ppu.sprites[n].x = uint8_t(x);
			ppu.sprites[n].y = uint8_t(cy + 8);
			ppu.sprites[n].index = uint8_t(48 + k);
			ppu.sprites[n].attributes = 5;
			n++;
			ppu.sprites[n].x = uint8_t(x);
			ppu.sprites[n].y = uint8_t(cy);
			ppu.sprites[n].index = uint8_t(64 + k);
			ppu.sprites[n].attributes = 5;
			n++;
		}
	}

	while (n < 64)
	{
		ppu.sprites[n].y = 240;
		n++;
	}

	ppu.draw(drawable_size);
}