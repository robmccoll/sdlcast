#include <unistd.h>

#include "SDL.h"
#include "map.h"
#include "game.h"

//#define LOG_ENTER() SDL_LogVerbose(SDL_LOG_CATEGORY_APPLICATION, "%8u ENTER %s()", SDL_GetTicks(), __func__);
#define LOG_ENTER() 

void
game_init(game_t * game, int32_t width, int32_t height)
{
  LOG_ENTER()

  memset(game, 0, sizeof(game_t));

  game->width = width;
  game->height = height;

  game->player_speed = 100.0f;
  game->player_rot_speed = 1000.0f;
  game->player_focal_len = 1.0f;

  game->window = SDL_CreateWindow(
      "game", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
      game->width, game->height, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);

  if(!game->window) {
    fprintf(stderr, "window create failed");
    game->state = STATE_QUIT;
  }

  game->window_surface = SDL_GetWindowSurface(game->window);
  SDL_Surface * tmp = SDL_CreateRGBSurface(0, 1, height, 32, 0xff, 0xff00, 0xff0000, 0xff000000);
  game->column_buf = SDL_ConvertSurface(tmp, game->window_surface->format, 0);
  SDL_FreeSurface(tmp);

  game->latest_tick = SDL_GetTicks();
}

void
game_exit(game_t * game)
{
  LOG_ENTER()

  SDL_FreeSurface(game->column_buf);
  game->column_buf = NULL;

  if(game->map) {
    game->map = map_free(game->map);
  }

  if(game->window) {
    SDL_HideWindow(game->window);
  }

  if(game->window_surface) {
    SDL_FreeSurface(game->window_surface);
    game->window_surface = NULL;
  }

  if(game->window) {
    SDL_DestroyWindow(game->window);
    game->window = NULL;
  }

  game->state = STATE_QUIT;
}

void
game_process_event(game_t * game, SDL_Event * event)
{
  LOG_ENTER()

  switch(event->type) {
    case SDL_QUIT: {
      game->state = STATE_QUIT;
    }  break;
    case SDL_KEYDOWN: {
      switch(event->key.keysym.scancode) {
        case SDL_SCANCODE_ESCAPE: {
          game->state = STATE_QUIT;
        }  break;
        case SDL_SCANCODE_Q: {
          game->player_focal_len += 0.05;
        }  break;
        case SDL_SCANCODE_E: {
          game->player_focal_len -= 0.05;
        }  break;
        case SDL_SCANCODE_W: {
          game->player_vel = 1.0;
        }  break;
        case SDL_SCANCODE_S: {
          game->player_vel = -1.0;
        }  break;
        case SDL_SCANCODE_A: {
          game->player_rot_vel = -2.0f * M_PI;
        }  break;
        case SDL_SCANCODE_D: {
          game->player_rot_vel = 2.0f * M_PI;
        }  break;
        default: {
          SDL_LogVerbose(SDL_LOG_CATEGORY_APPLICATION, "Unhandled keydown scancode %d", (int)event->key.keysym.scancode);
        } break;
      }
    } break;
    case SDL_KEYUP: {
      switch(event->key.keysym.scancode) {
        case SDL_SCANCODE_W: {
          game->player_vel = 0;
        }  break;
        case SDL_SCANCODE_S: {
          game->player_vel = 0;
        }  break;
        case SDL_SCANCODE_A: {
          game->player_rot_vel = 0;
        }  break;
        case SDL_SCANCODE_D: {
          game->player_rot_vel = 0;
        }  break;
        default: {
          SDL_LogVerbose(SDL_LOG_CATEGORY_APPLICATION, "Unhandled keyup scancode %d", (int)event->key.keysym.scancode);
        } break;
      }
    } break;
    default: {
      SDL_LogVerbose(SDL_LOG_CATEGORY_APPLICATION, "Unhandled event type %d", (int)event->type);
    } break;
  }
}

void
game_update(game_t * game)
{
  LOG_ENTER()

  uint32_t new_time = SDL_GetTicks();
  if(new_time <= game->latest_tick + 5) {
    usleep(2000);
    return;
  }

  game->delta_tick = new_time - game->latest_tick;
  game->latest_tick = new_time;
  SDL_LogVerbose(SDL_LOG_CATEGORY_APPLICATION, "%f FPS", (double)(1000.0f/(double)(game->delta_tick)));

  /* update player position */
  game->player_rot += ((double)game->delta_tick) * game->player_rot_vel / (game->player_rot_speed);
  while(game->player_rot < 0.0f) game->player_rot += 2.0f * M_PI;
  while(game->player_rot > 2.0f * M_PI) game->player_rot -= 2.0f * M_PI;

  vec2f_t possible = {
    x: game->player_pos.x + (game->delta_tick * game->player_vel * cosf(game->player_rot)) / game->player_speed,
    y: game->player_pos.y + (game->delta_tick * game->player_vel * sinf(game->player_rot)) / game->player_speed
  };

  if(!map_is_block(game->map, possible.x, possible.y)) {
    game->player_pos = possible;
  } else if(!map_is_block(game->map, possible.x, game->player_pos.y)) {
    game->player_pos.x = possible.x;
  } else if(!map_is_block(game->map, game->player_pos.x, possible.y)) {
    game->player_pos.y = possible.y;
  }

  SDL_LogVerbose(SDL_LOG_CATEGORY_APPLICATION, "%f x %f y %f rot", game->player_pos.x, game->player_pos.y, game->player_rot);
  SDL_LogVerbose(SDL_LOG_CATEGORY_APPLICATION, "%f velocity", game->player_vel);
  SDL_LogVerbose(SDL_LOG_CATEGORY_APPLICATION, "%f rot velocity", game->player_rot_vel);
}

void
game_render(game_t * game)
{
  LOG_ENTER()

  /* render floor and ceiling to infinity, everything else renders on top of these */
  SDL_Rect ceil = {x: 0, y: 0, w: game->width, h: game->height / 2};
  SDL_BlitSurface(game->map->texture_ceil, NULL, game->window_surface, &ceil);

  SDL_Rect floor = {x: 0, y: game->height / 2, w: game->width, h: game->height / 2};
  SDL_BlitSurface(game->map->texture_floor, NULL, game->window_surface, &floor);

  // make the world sqeeze in and out
  // double foc_len = (0.8 + sin(game->latest_tick / 500.0f)) / 4.0 + game->player_focal_len;
  
  double foc_len = game->player_focal_len;

  for(int32_t w = 0; w < game->width; w++) {
    double x = ((double)w) / ((double)game->width) - 0.5f;
    double angle = atan2(x, foc_len);

    SDL_Rect srcrect = {0};
    SDL_Surface * srctext = NULL;
    double distance = map_cast_sample(game->map, &srctext, &srcrect, game->player_pos, game->player_rot + angle, -1);
    double z = distance * cos(angle);
    double height = ((double)game->height * 1.3f) / z;
    if(height > 0) {
      SDL_Rect col = {x: w, y: ((((double)game->height) / 2.0f) * (1.0f + 1.0f / z)) - height, w: 1, h: height};
      SDL_BlitScaled(srctext, &srcrect, game->window_surface, &col);
      uint8_t a = z > 15 ? 255 : 15 * z;
      SDL_FillRect(game->column_buf, NULL,  SDL_MapRGBA(game->column_buf->format, 0, 0, 0, a));
      SDL_SetSurfaceBlendMode(game->column_buf, SDL_BLENDMODE_BLEND);
      SDL_BlitScaled(game->column_buf, NULL, game->window_surface, &col);
    }
  }

  SDL_UpdateWindowSurface(game->window);
}

int
main(int argc, char *argv[])
{
  if(argc < 2) {
    printf("expected map file argument\n");
    return -1;
  }

  SDL_Init(SDL_INIT_VIDEO);
  SDL_LogSetAllPriority(SDL_LOG_PRIORITY_VERBOSE);

  game_t game = {0};

  game_init(&game, 1920, 1080);
  game.map = map_new_load(argv[1], game.width, game.height, game.window_surface->format);
  if(!game.map) {
    printf("map parsing failed\n");
    return -1;
  }
  game.player_pos = game.map->player_start_pos;
  game.player_rot = game.map->player_start_rot;

  while(game.state != STATE_QUIT) {
    SDL_Event event;
    while(SDL_PollEvent(&event)) {
      game_process_event(&game, &event);
    }

    game_update(&game);
    game_render(&game);
  }

  game_exit(&game);
  SDL_Quit();
  return 0;
}
