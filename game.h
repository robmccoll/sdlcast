#ifndef  GAME_H
#define  GAME_H

#include "common_types.h"
#include "map.h"

typedef enum gamestate {
  STATE_QUIT = -1,
  STATE_INIT = 0,
  STATE_INITIALIZED
} gamestate_t;

typedef struct game {
  gamestate_t   state;
  int32_t       height;
  int32_t       width;
  SDL_Window  * window;
  SDL_Surface * window_surface;
  uint32_t      latest_tick;
  uint32_t      delta_tick;

  SDL_Surface * column_buf;;

  vec2f_t player_pos;
  double   player_rot;
  double   player_vel;
  double   player_rot_vel;
  double   player_speed;
  double   player_rot_speed;
  double   player_focal_len;

  map_t * map;
} game_t;

#endif  /*GAME_H*/
