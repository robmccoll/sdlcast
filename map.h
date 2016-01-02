#ifndef  MAP_H
#define  MAP_H

#include "SDL.h"
#include "common_types.h"

typedef struct map {
  char    * version;
  char    * name;
  vec2i32_t size;

  int32_t texture_count;
  SDL_Surface ** texture_list;

  SDL_Surface * texture_ceil;
  SDL_Surface * texture_floor;

  uint8_t * world;

  vec2f_t player_start_pos;
  double   player_start_rot;
} map_t;

map_t *
map_new_load(char * file, int32_t width, int32_t height, const SDL_PixelFormat* pxlfmt);

map_t *
map_free(map_t * map);

double
map_cast_sample(map_t * map, SDL_Surface ** outtextptr, SDL_Rect * outtextrect, vec2f_t point, double angle, double range);

int
map_is_block(map_t * map, int32_t x, int32_t y);

#endif  /*MAP_H*/
