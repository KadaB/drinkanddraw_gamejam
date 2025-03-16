#include <SDL.h>
#include <SDL_main.h>

#include <stdint.h>
#include <math.h>

typedef int8_t  s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef u8  b8;
typedef u32 b32;

typedef float  f32;
typedef double f64;

//TODO(moritz): Use HandmadeMath.h instead of doing our own math stuff?
// https://github.com/HandmadeMath/HandmadeMath

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define MIN(a, b) (a) < (b) ? a : b
#define MAX(a, b) (a) > (b) ? a : b

f64 rand_0_to_1() {
  return (f64)rand() / RAND_MAX;
}

f64 rand_minus_one_to_one() {
  return (rand_0_to_1() - 0.5f) * 2.f;
}

typedef union {
  f32 e[2];
  struct {
    f32 x;
    f32 y;
  };
} v2;


typedef struct {
  b8 down;
  b8 pressed;
  b8 released;
} ButtonState;

typedef struct {
  ButtonState buttons[SDL_SCANCODE_COUNT];
} Input;

#define sndplr_SPEED 800.0f
#define sndplr_HALF_DIM 25.0f
#define GRAVITY_SPEED 400.0f;

#define make_ani(ani_array, delay) { .frames = ani_array, .num_frames = sizeof(ani_array) / sizeof(ani_array[0]), .duration = delay, .elapsed = 0., .cur_frame = 0 }
#define LEN(arr) (sizeof(arr) / sizeof(arr[0]))

SDL_Texture* load_tex_from_png(SDL_Renderer *renderer, const char *filename) {
  int width, height, channels;
  unsigned char *data = stbi_load(filename, &width, &height, &channels, 4); // 4 = RGBA
  if (!data) {
    SDL_Log("%s nicht geladen, weil %s", filename, stbi_failure_reason());
    return NULL;
  }

  SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, width, height);
  if (!texture) {
    SDL_Log("Texture konnte nicht erstellt werden für %s. Fehler: %s\n", filename, SDL_GetError());
    stbi_image_free(data);
    return NULL;
  }

  SDL_UpdateTexture(texture, NULL, data, width * 4);
  stbi_image_free(data);

  return texture;
}

SDL_FRect frame_at(v2 grid_coord, v2 spr_dims) {
  return (SDL_FRect) { spr_dims.x*grid_coord.x,  spr_dims.y*grid_coord.y, spr_dims.x, spr_dims.y};
}

typedef struct {
  v2* frames;
  int num_frames;
  f64 duration;

  f64 elapsed;
  int cur_frame;
} Animation;

enum PROP_STATE {
  WHOLE,
  BROKEN
};

typedef struct {
  int hp;
  v2* frames;
  enum PROP_STATE broken;
  SDL_Texture* texture;
  v2 frame_dims;
  v2 position;
} Prop;

enum {
  none     =  0,
  deleted  = (1 << 0),
  blocker  = (1 << 2),
  animated = (1 << 3),
};

#define MAX_ENTITY_COUNT 128

typedef struct {
  v2 position;
  v2 half_dim;
} Entity;

typedef struct {
  u32 entity_count; //TODO(moritz): init to 1
  Entity entities[2];
  // SDL_FRect entities[2];
  v2 player_velocity;
  b8 player_is_grounded;
} GameState;

typedef struct {
  Animation *animations;
  int num_anis;
  int cur_animation; // in welcher Animation sich der Spieler befindet.

  v2 position;
  v2 frame_dims;
  v2 display_dims;

  SDL_Texture *spr_tex;
} AnimatedObject;

// returns true if animation not yet ended?
b8 update_animation(Animation *animation, f64 elapsed_delta_sec) {
  animation->elapsed += elapsed_delta_sec;

  if(animation->elapsed > animation->duration) {
    animation->cur_frame++;
    if(animation->cur_frame >= animation->num_frames) animation->cur_frame = 0;
    animation->elapsed = 0.f;
    return false;
  }
  return true;
}


void update_animated_object(AnimatedObject* ani_obj, f64 elapsed_delta_sec) {
  if(!update_animation(&ani_obj->animations[ani_obj->cur_animation], elapsed_delta_sec)) {
    //ani_obj->cur_animation = (ani_obj->cur_animation+1) % ani_obj->num_anis;
  }
}

void display_animation(v2 sndplr_pos, Animation *animation, v2 spr_dims, SDL_Texture* spr_tex,  SDL_Renderer *renderer) {
  SDL_FRect srcRect = frame_at(animation->frames[animation->cur_frame], spr_dims);

  SDL_FRect spr_rect = (SDL_FRect) {
    .x = sndplr_pos.x - spr_dims.x/2,
    .y = sndplr_pos.y - spr_dims.y/2,
    .w = spr_dims.x,
    .h = spr_dims.y
  };

  SDL_RenderTexture(renderer, spr_tex, &srcRect, &spr_rect);
}

void display_animated_object(AnimatedObject* ani_obj, SDL_Renderer *renderer) {
  Animation *animation = &ani_obj->animations[ani_obj->cur_animation];
  v2 frame_coord_on_grid = animation->frames[animation->cur_frame];
  SDL_FRect srcRect = frame_at(frame_coord_on_grid, ani_obj->frame_dims);
  v2 center = {ani_obj->display_dims.x/2, ani_obj->display_dims.y/2 };

  SDL_FRect dst_spr_rect = {
    .x = ani_obj->position.x - center.x,
    .y = ani_obj->position.y - center.y,
    .w = ani_obj->display_dims.x,
    .h = ani_obj->display_dims.y
  };

  SDL_RenderTexture(renderer, ani_obj->spr_tex, &srcRect, &dst_spr_rect);

}

typedef struct {
  SDL_AudioStream *audio_stream;
  SDL_AudioSpec wave_spec;
  Uint8 *wave_buf;
  u32 wave_len;
} SoundPlayer;

void sndplr_destroy(SoundPlayer *player) {
  SDL_free(player->wave_buf);
  SDL_zerop(player);
}

bool sndplr_loadwav(SoundPlayer *player, char *filename) {
  SDL_AudioSpec wave_spec = {0, 0, 0};
  if (!SDL_LoadWAV(filename, &wave_spec, &player->wave_buf, &player->wave_len)) {
    SDL_Log("Audio datei NICHT geladen, weil: %s", SDL_GetError());
    return false;
  }

  player->audio_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &wave_spec, NULL, NULL);
  if (player->audio_stream == NULL) {
    SDL_Log("Audio stream nicht erstellt, weil: %s", SDL_GetError());
    return false;
  }

  return true;
}

void sndplr_start(SoundPlayer *player) {
  SDL_ResumeAudioStreamDevice(player->audio_stream);
}

void sndplr_continue(SoundPlayer *player) {
  if (SDL_GetAudioStreamQueued(player->audio_stream) < (int) player->wave_len) {
    SDL_PutAudioStreamData(player->audio_stream, player->wave_buf, player->wave_len);
  }
}

void sndplr_stop(SoundPlayer *player) {
  SDL_PauseAudioStreamDevice(player->audio_stream);
  SDL_ClearAudioStream(player->audio_stream);
}

void sndplr_play_once(SoundPlayer *player) {
  SDL_ClearAudioStream(player->audio_stream);
  SDL_PutAudioStreamData(player->audio_stream, player->wave_buf, player->wave_len);
  SDL_ResumeAudioStreamDevice(player->audio_stream);
}

void draw_rect_thickness(SDL_Renderer *renderer, SDL_FRect *rect, f32 thickness)
{
  SDL_FRect top_rect = {
    .x = rect->x,
    .y = rect->y,
    .w = rect->w,
    .h = thickness
  };
  SDL_RenderFillRect(renderer, &top_rect);

  SDL_FRect right_rect = {
    .x = rect->x + rect->w - thickness,
    .y = rect->y,
    .w = thickness,
    .h = rect->h
  };
  SDL_RenderFillRect(renderer, &right_rect);

  SDL_FRect bot_rect = {
    .x = rect->x,
    .y = rect->y + rect->h - thickness,
    .w = rect->w,
    .h = thickness
  };
  SDL_RenderFillRect(renderer, &bot_rect);

  SDL_FRect left_rect = {
    .x = rect->x,
    .y = rect->y,
    .w = thickness,
    .h = rect->h
  };
  SDL_RenderFillRect(renderer, &left_rect);
}

enum PropType {
  // LVL1
  DUCK,
  VASE,
  TOSTER,
  FLOWER,

  // LVL2
  LAMP,
  PC,
  PLANT,

  // LVL3
  STATUE,
  MIRROR,
  BEAR,

  NUM_TYPES,
};

enum PropLvl {
  SMALL,
  MEDIUM,
  LARGE
};

#define ENM_RAND_RNG(startenm, endenm) (assert((startenm) < (endenm)), (startenm) + (rand() % ((endenm) - (startenm))))
// lvl from 0 to 2
Prop rands_prop(enum PropLvl lvl, SDL_Texture** prop_texture_list) {

  enum PropType type;
  if (lvl == SMALL) {
    type = ENM_RAND_RNG(DUCK, FLOWER);
  } else if (lvl == MEDIUM) {
    type = ENM_RAND_RNG(LAMP, PLANT);
  } else if (lvl == LARGE) {
    type = ENM_RAND_RNG(STATUE, BEAR);
  }
  v2 prop_frames[] = { {0, 0}, {1, 0} };
  v2 start_pos = {0, 1000};
  SDL_Texture* spr_tex = prop_texture_list[type];
  Prop prop = {
    .hp = lvl + 1,
    .frames =  prop_frames,
    .broken = (rand() % 2) == 0 ? false : true,
    .texture = spr_tex,
    .frame_dims = {spr_tex->w / 2, spr_tex->h},
    .position = start_pos
  };

  return prop;
}

int main(int argc, char **argv)
{

  //NOTE(moritz): Initialization
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO))
  {
    SDL_Log("Could not initialize SDL: %s", SDL_GetError());
    return 1;
  }

  SDL_Window *main_window = SDL_CreateWindow("SDL Window",
                                             1920,
                                             1080,
                                             0);
  if (!main_window)
  {
    SDL_Log("Could not create main window: %s", SDL_GetError());
    return 1;
  }

  SDL_Renderer *renderer = SDL_CreateRenderer(main_window, 0);

  if (!renderer)
  {
    SDL_Log("Could not create renderer: %s", SDL_GetError());
    return 1;
  }

  if (!SDL_SetRenderVSync(renderer, 1))
  {
    SDL_Log("Was not able to set vsync");
  }


  Input previous_input = {0};

  GameState game_state = {0};

  v2 player_pos = {
    .x = 800,
    .y = 300
  };

  v2 cat_pos = {180, 780};

  v2 tail_frames[] = { {0, 0}, {1, 0}, {2, 0} };
  Animation tails[] = {
    make_ani(tail_frames, .6)
  };
  AnimatedObject cat_tail_obj = {
    .animations = tails,
    .num_anis = LEN(tails),
    .cur_animation = 0,
    .position = cat_pos,
    .frame_dims = {1000.f, 1000.f},
    .display_dims = {356.,  356.},
    // TODO: free mem
    .spr_tex = load_tex_from_png(renderer, "../res/cat_animation_tail.png"),
  };

  v2 face_frames[] = { {0, 0}, {1, 0}, {2, 0} };
  Animation faces[] = {
    make_ani(face_frames, .6)
  };
  AnimatedObject cat_face_obj = {
    .animations = faces,
    .num_anis = LEN(faces),
    .cur_animation = 0,
    .position = cat_pos,
    .frame_dims = {1000.f, 1000.f},
    .display_dims = {356., 356.},
    // TODO: free mem
    .spr_tex = load_tex_from_png(renderer, "../res/cat_animation_face.png"),
  };

  v2   idle1[] = { {0, 0} };
  v2 attack1[] = { {0, 0}, {1, 0}, {2, 0} };

  Animation animations[] = {
    make_ani(idle1, .6),
    make_ani(attack1, .6),
  };
  AnimatedObject cat_ani = {
    .animations = animations,
    .num_anis = LEN(animations),
    .cur_animation = 1,
    .position = cat_pos,
    .frame_dims = {1000.f, 1000.f},
    .display_dims = {356., 356.},
    // TODO: free mem
    .spr_tex = load_tex_from_png(renderer, "../res/cat_animation_body.png"),
  };
  player_pos.x = cat_ani.position.x;
  player_pos.y = cat_ani.position.y;

  game_state.entities[0] = (Entity){
    .position = {
      .x = player_pos.x + 600.0f,
      .y = player_pos.y
    },
    .half_dim = {
      .x = 50.0f,
      .y = 100.0f
    },
  };

  game_state.entities[1] = (Entity){
    .position = {
      .x = player_pos.x + 900.0f,
      .y = player_pos.y
    },
    .half_dim = {
      .x = 70.0f,
      .y = 120.0f
    },
  };

  SDL_Log("Num anis: %d", cat_ani.num_anis);
  for(int i = 0; i < cat_ani.num_anis; ++i) {
    SDL_Log("animation %d: num of frames: %d",i, animations[i].num_frames);
  }

  SDL_Texture *bg_tex = load_tex_from_png(renderer, "../res/background_nolight1.png");
  SDL_Texture *spawn = load_tex_from_png(renderer, "../res/conveyorbelt_static1.png");
  SDL_Texture *spawn_bg = load_tex_from_png(renderer, "../res/conveyorbelt_interior.png");
  SDL_Texture *belt = load_tex_from_png(renderer, "../res/conveyorbelt_frontwheel1.png");
  SDL_Texture *wheels = load_tex_from_png(renderer, "../res/conveyorbelt_circle1.png");
  SDL_Texture *dot = load_tex_from_png(renderer, "../res/conveyorbelt_dot1.png");

  SDL_Texture* prop_textures[NUM_TYPES];
  for(int i = 0; i < NUM_TYPES; ++i) prop_textures[i] = NULL;

  // TODO: free mem
  prop_textures[DUCK] = load_tex_from_png(renderer, "../res/item_duck.png");
  prop_textures[VASE] = load_tex_from_png(renderer, "../res/item_vase.png");
  prop_textures[TOSTER] = load_tex_from_png(renderer, "../res/item_toster.png");
  prop_textures[FLOWER] = load_tex_from_png(renderer, "../res/item_flower.png");
  prop_textures[LAMP] = load_tex_from_png(renderer, "../res/item_lamp.png");
  prop_textures[PC] = load_tex_from_png(renderer, "../res/item_computer.png");
  prop_textures[PLANT] = load_tex_from_png(renderer, "../res/item_plant.png");
  prop_textures[STATUE] = load_tex_from_png(renderer, "../res/item_statue.png");
  prop_textures[MIRROR] = load_tex_from_png(renderer, "../res/item_mirror.png");
  prop_textures[BEAR] = load_tex_from_png(renderer, "../res/item_bear.png");

  u64 time_stamp_now  = SDL_GetPerformanceCounter();
  u64 time_stamp_last = 0;
  f64 dt_for_previous_frame = 0;

  //NOTE(moritz): Game loop
  b8 quit = false;

  int angle = 0.f;
  f32 dot_shift = 0;


  while (!quit)
  {
    time_stamp_last = time_stamp_now;
    time_stamp_now  = SDL_GetPerformanceCounter();
    dt_for_previous_frame = (f64)((time_stamp_now - time_stamp_last)/(f64)SDL_GetPerformanceFrequency());
    // SDL_Log("dt: %g seconds", dt_for_previous_frame);

    //NOTE(moritz): Events/Input
    Input current_input = {0};
    current_input = previous_input;

    for (s32 idx = 0; idx < SDL_SCANCODE_COUNT; idx += 1)
    {
      ButtonState *state = &current_input.buttons[idx];
      state->pressed  = false;
      state->released = false;
    }

    SDL_Event e = {0};
    while (SDL_PollEvent(&e))
    {
      switch(e.type)
      {
        case SDL_EVENT_QUIT:
        {
          quit = true;
        } break;

        case SDL_EVENT_KEY_DOWN:
        {
          int scancode = e.key.scancode;

          current_input.buttons[scancode].down    = true;
          current_input.buttons[scancode].pressed = true;

          if(scancode == SDL_SCANCODE_ESCAPE) quit = true;
        } break;
        case SDL_EVENT_KEY_UP:
        {
          int scancode = e.key.scancode;

          current_input.buttons[scancode].down     = false;
          current_input.buttons[scancode].released = true;
          }
        } break;
    }

    previous_input = current_input;

    //NOTE(moritz):Update game state
    v2 input_direction = {0};

    //NOTE(moritz): Drawing
    if (bg_tex) {
      SDL_RenderTexture(renderer, bg_tex, NULL, NULL);
    }
    else {
      SDL_SetRenderDrawColor(renderer, 255, 0, 255, 255);
      SDL_RenderClear(renderer);
    }

    if(cat_ani.spr_tex) {
      //NOTE(moritz): Hack
      cat_ani.position.x = player_pos.x;
      cat_ani.position.y = player_pos.y;
      //update_animation(&animations[0], dt_for_previous_frame);
      //display_animation(sndplr_pos, &animations[0], spr_dims, spr_tex, renderer);
      update_animated_object(&cat_ani, dt_for_previous_frame);
      display_animated_object(&cat_ani, renderer);
    }
    else {
      SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
      SDL_FRect rect = (SDL_FRect){
        .x = player_pos.x - sndplr_HALF_DIM,
        .y = player_pos.y - sndplr_HALF_DIM,
        .w = 2*sndplr_HALF_DIM,
        .h = 2*sndplr_HALF_DIM
      };
      SDL_RenderFillRect(renderer, &rect);
    }

    if(cat_tail_obj.spr_tex) {
      //NOTE(moritz): Hack
      cat_ani.position.x = player_pos.x;
      cat_ani.position.y = player_pos.y;
      //update_animation(&animations[0], dt_for_previous_frame);
      //display_animation(sndplr_pos, &animations[0], spr_dims, spr_tex, renderer);
      display_animated_object(&cat_tail_obj, renderer);
    }


    if(cat_face_obj.spr_tex) {
      //NOTE(moritz): Hack
      cat_ani.position.x = player_pos.x;
      cat_ani.position.y = player_pos.y;
      //update_animation(&animations[0], dt_for_previous_frame);
      //display_animation(sndplr_pos, &animations[0], spr_dims, spr_tex, renderer);
      display_animated_object(&cat_face_obj, renderer);
    }

    if (spawn_bg) {
      SDL_RenderTexture(renderer, spawn_bg, NULL, &(SDL_FRect){0, 0, spawn_bg->w, spawn_bg->h});
    }
    else {
      SDL_SetRenderDrawColor(renderer, 255, 0, 255, 255);
      SDL_RenderFillRect(renderer, &(SDL_FRect){0, 890, 400, 400});
    }

    // item placing

    if (spawn) {
      SDL_RenderTexture(renderer, spawn, NULL, &(SDL_FRect){0, 0, spawn->w, spawn->h});
    }
    else {
      SDL_SetRenderDrawColor(renderer, 255, 0, 255, 255);
      SDL_RenderFillRect(renderer, &(SDL_FRect){0, 890, 400, 400});
    }

    if (wheels) {
      SDL_FPoint center = {wheels->w/2 + rand_minus_one_to_one(), wheels->h/2 + rand_minus_one_to_one()};
      f32 posxs[] = {98, 300, 490, 664, 827, 1026, 1219, 1432};
      angle -= 290. * dt_for_previous_frame;
      // angle = angle < 0 ? 360 : 0;
      for (int i = 0; i < LEN(posxs); i++)
        SDL_RenderTextureRotated(renderer, wheels, NULL,
          &(SDL_FRect){posxs[i] - center.x, 990 - center.y, wheels->w, wheels->h}, angle, &center, SDL_FLIP_NONE);
    }

    if (dot) {
      int num_dots = 17;
      SDL_FPoint center = {dot->w/2 + rand_minus_one_to_one(), dot->h/2 + rand_minus_one_to_one()};
      dot_shift += 100 * dt_for_previous_frame;
      f32 spacing = 100;
      dot_shift = dot_shift > spacing ? 0.0 : dot_shift;
      for (int i = 0; i < num_dots; i++) {
        SDL_FRect dest_rect = { -dot_shift + i*spacing - center.x, 944 - center.y, dot->w, dot->h};
        SDL_RenderTexture(renderer, dot, NULL, &dest_rect);
      }

      for (int i = 0; i < num_dots; i++) {
        SDL_FRect dest_rect = { dot_shift + (i-1)*spacing - center.x, 1032 - center.y, dot->w, dot->h};
        SDL_RenderTexture(renderer, dot, NULL, &dest_rect);
      }
    }


    if (belt) {
      SDL_RenderTexture(renderer, belt, NULL, &(SDL_FRect){0, 0, belt->w, belt->h});
    }
    else {
      SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255);
      SDL_RenderFillRect(renderer, &(SDL_FRect){0, 890, 1920, 205});
    }

    SDL_RenderPresent(renderer);
  }

  SDL_DestroyTexture(cat_ani.spr_tex);
  SDL_DestroyTexture(bg_tex);
  SDL_DestroyTexture(spawn);
  SDL_DestroyTexture(belt);
  SDL_DestroyTexture(wheels);

  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(main_window);
  SDL_Quit();
  return 0;
}
