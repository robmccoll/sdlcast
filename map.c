#include "map.h"

#include "SDL_image.h"

#define MAP_VERSION "0"
#define MAP_EMPTY 255

static void
vert_gradient(SDL_Surface * surface, int32_t w, int32_t h, int sr, int sg, int sb, int er, int eg, int eb)
{
  SDL_Rect dest;
  dest.x = 0;
  dest.w = w;
  dest.h = 1;
  for(int y = 0; y < h; y++) {
    dest.y = y;
    int r = (sr + ((double)(er - sr) * (((double)(h - y)/((double)h)))));
    int g = (sg + ((double)(eg - sg) * (((double)(h - y)/((double)h)))));
    int b = (sb + ((double)(eb - sb) * (((double)(h - y)/((double)h)))));
    SDL_FillRect(surface, &dest,  SDL_MapRGB(surface->format, r, g, b));
  }
}

map_t *
map_new_load(char * file, int32_t width, int32_t height, const SDL_PixelFormat* pxlfmt)
{
  map_t * map = calloc(1, sizeof(map_t));
  if(!map) {
    return NULL;
  }

  FILE * fp = NULL;
  char * line = NULL;
  size_t len = 0;
  ssize_t read;
  char * texture_tokens = NULL;
  int32_t textures_read = 0;
  int32_t map_bytes_read = 0;

  if(!(fp = fopen(file, "r"))) {
    return NULL;
  }

  while ((read = getline(&line, &len, fp)) != -1) {
    line[read-1] = '\0';
    read--;

    printf("PARSING  [%zu] [%s]\n", read, line);
    if(read < 1 || line[0] == '#') {
      continue;
    }

    if(!map->version) {
      if(0 != strncmp(MAP_VERSION, line, read)) {
        goto parse_error;
      }
      map->version = strndup(line, read);
      continue;
    }

    if(!map->name) {
      map->name = strndup(line, read);
      continue;
    }

    if(!map->size.x) {
      char * token = NULL;
      char * save = NULL;
      token = strtok_r(line, " ", &save);
      if(!token) {
        goto parse_error;
      }
      map->size.x = atoi(token);
      token = strtok_r(NULL, " ", &save);
      if(!token) {
        goto parse_error;
      }
      map->size.y = atoi(token);
      token = strtok_r(NULL, " ", &save);
      if(!token) {
        goto parse_error;
      }
      map->texture_count = atoi(token);
      if(!map->size.x || !map->size.y || !map->texture_count) {
        goto parse_error;
      }

      map->texture_list = calloc(map->texture_count, sizeof(SDL_Surface *));
      map->world = calloc(map->size.x * map->size.y, sizeof(uint8_t));
      texture_tokens = calloc(map->texture_count, sizeof(char));
      if(!map->texture_list || !map->world) {
        goto parse_error;
      }
      continue;
    }

    if(textures_read != map->texture_count) {
      char * token = NULL;
      char * save = NULL;
      token = strtok_r(line, "=", &save);
      if(!token) {
        goto parse_error;
      }
      texture_tokens[textures_read] = token[0];
      token = strtok_r(NULL, "=", &save);
      if(!token) {
        goto parse_error;
      }
      SDL_Surface * tmp = IMG_Load(token);
      if(!tmp) {
        goto parse_error;
      }
      map->texture_list[textures_read] = SDL_ConvertSurface(tmp, pxlfmt, 0);
      SDL_FreeSurface(tmp);
      if(!map->texture_list[textures_read]) {
        goto parse_error;
      }
      textures_read++;
      continue;
    }

    for(int32_t c = 0; (c < read) && (map_bytes_read < (map->size.x * map->size.y)); c++, map_bytes_read++) {
      uint8_t texture_num = 0;
      if(line[c] == ' ') {
        map->world[map_bytes_read] = MAP_EMPTY;
        continue;
      }
      if(line[c] == 'S') {
        map->player_start_pos.y = ((double)(map_bytes_read / map->size.x)) + 0.5f;
        map->player_start_pos.x = ((double)(map_bytes_read % map->size.x)) + 0.5f;
        map->world[map_bytes_read] = MAP_EMPTY;
        continue;
      }
      while((texture_num < map->texture_count) && texture_tokens[texture_num] != line[c]) {
        texture_num++;
      }
      map->world[map_bytes_read] = texture_num;
    }
  }

  map->texture_ceil  = SDL_CreateRGBSurface(0, width, height/2, 32, 0, 0, 0, 0);
  vert_gradient(map->texture_ceil, width, height/2, 30, 0, 20, 0, 0, 120);
  map->texture_floor = SDL_CreateRGBSurface(0, width, height/2, 32, 0, 0, 0, 0);
  vert_gradient(map->texture_floor, width, height/2, 40, 40, 40, 20, 20, 20);

  SDL_LogVerbose(SDL_LOG_CATEGORY_APPLICATION, "loading map file %s succeeded.", file);

  free(texture_tokens);
  free(line);
  fclose(fp);
  return map;

parse_error:
  SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "loading map file %s failed.", file);
  map_free(map);
  free(texture_tokens);
  free(line);
  fclose(fp);
  return NULL;
}

double vec2f_dist(vec2f_t * a, vec2f_t * b) {
  vec2f_t dif = {x: a->x - b->x, y: a->y - b->y};
  return sqrtf(dif.x * dif.x + dif.y * dif.y);
}

double
map_cast_sample(map_t * map, SDL_Surface * outbuf, vec2f_t point, double angle, double range) {
  double sina = sin(angle);
  double cosa = cos(angle);

  vec2f_t curpoint = point;

  while(range < 0 || vec2f_dist(&point, &curpoint) < range) {
    /* determine next x and y lines hit */
    vec2f_t next;
    next.y = sina > 0 ? ceil(curpoint.y + 0.001f) : floor(curpoint.y - 0.001f);
    next.x = cosa > 0 ? ceil(curpoint.x + 0.001f) : floor(curpoint.x - 0.001f);

    /* project distance on current vector */
    vec2f_t dist;
    dist.y = (next.y - curpoint.y) / sina;
    dist.x = (next.x - curpoint.x) / cosa;

    /* determine which is closer on current vector */
    if(dist.x < dist.y) {
      curpoint.x = next.x;
      curpoint.y += dist.x * sina;
      vec2i32_t check = {x: curpoint.x, y: floor(curpoint.y)};
      if (check.y >= 0 && check.y < map->size.y) {
        if(check.x >= 0 && check.x < map->size.x) {
          uint8_t wall = map->world[check.x + check.y * map->size.x];
          if(wall != MAP_EMPTY) {
            int32_t img_col = (((double)map->texture_list[wall]->w) * 
                (curpoint.x > point.x ? ceil(curpoint.y) - curpoint.y : curpoint.y - floor(curpoint.y)));
            SDL_Rect col = {x: img_col, y: 0, w: 1, h: map->texture_list[wall]->h};
            SDL_BlitScaled(map->texture_list[wall], &col, outbuf, NULL);
            return vec2f_dist(&point, &curpoint);
          }
        }
        check.x -= 1;
        if(check.x >= 0 && check.x < map->size.x) {
          uint8_t wall = map->world[check.x + check.y * map->size.x];
          if(wall != MAP_EMPTY) {
            int32_t img_col = (((double)map->texture_list[wall]->w) * 
                (curpoint.x > point.x ? ceil(curpoint.y) - curpoint.y : curpoint.y - floor(curpoint.y)));
            SDL_Rect col = {x: img_col, y: 0, w: 1, h: map->texture_list[wall]->h};
            SDL_BlitScaled(map->texture_list[wall], &col, outbuf, NULL);
            return vec2f_dist(&point, &curpoint);
          }
        }
      }
    } else {
      curpoint.y = next.y;
      curpoint.x += dist.y * cosa;
      vec2i32_t check = {x: floor(curpoint.x), y: curpoint.y};
      if (check.x >= 0 && check.x < map->size.x) {
        if(check.y >= 0 && check.y < map->size.y) {
          uint8_t wall = map->world[check.x + check.y * map->size.x];
          if(wall != MAP_EMPTY) {
            int32_t img_col = (((double)map->texture_list[wall]->w) * 
                (curpoint.y < point.y ? ceil(curpoint.x) - curpoint.x : curpoint.x - floor(curpoint.x)));
            SDL_Rect col = {x: img_col, y: 0, w: 1, h: map->texture_list[wall]->h};
            SDL_BlitScaled(map->texture_list[wall], &col, outbuf, NULL);
            return vec2f_dist(&point, &curpoint);
          }
        }
        check.y -= 1;
        if(check.y >= 0 && check.y < map->size.y) {
          uint8_t wall = map->world[check.x + check.y * map->size.x];
          if(wall != MAP_EMPTY) {
            int32_t img_col = (((double)map->texture_list[wall]->w) * 
                (curpoint.y < point.y ? ceil(curpoint.x) - curpoint.x : curpoint.x - floor(curpoint.x)));
            SDL_Rect col = {x: img_col, y: 0, w: 1, h: map->texture_list[wall]->h};
            SDL_BlitScaled(map->texture_list[wall], &col, outbuf, NULL);
            return vec2f_dist(&point, &curpoint);
          }
        }
      }
    }

    if(curpoint.x < 0 || curpoint.x > map->size.x) return -1;
    if(curpoint.y < 0 || curpoint.y > map->size.y) return -1;
  }

  /* no hit found */
  return -1;
}

map_t *
map_free(map_t * map)
{
  if(!map) return NULL;

  if(map->version) free(map->version);
  if(map->name) free(map->name);
  if(map->world) free(map->world);

  if(map->texture_ceil) SDL_FreeSurface(map->texture_ceil);
  if(map->texture_floor) SDL_FreeSurface(map->texture_floor);

  if(map->texture_list) {
    for(int32_t i = 0; i < map->texture_count; i++) {
      if(map->texture_list[i]) {
        SDL_FreeSurface(map->texture_list[i]);
      }
    }
    free(map->texture_list);
  }

  free(map);
  return NULL;
}

int
map_is_block(map_t * map, int32_t x, int32_t y)
{
  return MAP_EMPTY != map->world[x + y * map->size.x];
}
