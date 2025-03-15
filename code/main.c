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
} button_state;

typedef struct {
  button_state buttons[SDL_SCANCODE_COUNT];
} input;

#define PLAYER_SPEED 200.0f
#define PLAYER_HALF_DIM 25.0f

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

typedef struct {
  Animation *animations;
  int num_anis;
  int cur_animation; // in welcher Animation sich der Spieler befindet.

  // TODO Kada: nur zu Testzwecken pointer
  v2 *position;

  v2 spr_dims;
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

void display_animation(v2 player_pos, Animation *animation, v2 spr_dims, SDL_Texture* spr_tex,  SDL_Renderer *renderer) {
  SDL_FRect srcRect = frame_at(animation->frames[animation->cur_frame], spr_dims);

  SDL_FRect spr_rect = (SDL_FRect) {
    .x = player_pos.x - spr_dims.x/2,
    .y = player_pos.y - spr_dims.y/2,
    .w = spr_dims.x,
    .h = spr_dims.y
  };

  SDL_RenderTexture(renderer, spr_tex, &srcRect, &spr_rect);
}

void display_animated_object(AnimatedObject* ani_obj, SDL_Renderer *renderer) {
  display_animation(*ani_obj->position, &ani_obj->animations[ani_obj->cur_animation], ani_obj->spr_dims, ani_obj->spr_tex, renderer);
}

int main(int argc, char **argv)
{

  //NOTE(moritz): Initialization
  if (!SDL_Init(SDL_INIT_VIDEO))
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


  input previous_input = {0};

  v2 player_pos = {
    .x = 400,
    .y = 300
  };

  v2   idle1[] = { {0, 0} };
  v2 attack1[] = { {0, 0}, {1, 0}, {2, 0} };

  Animation animations[] = {
    make_ani(idle1, .6),
    make_ani(attack1, .6),
  };

  AnimatedObject ani_obj = {
    .animations = animations,
    .num_anis = LEN(animations),
    .cur_animation = 1,
    .position = &player_pos,
    .spr_dims = {364, 352},
    .spr_tex = load_tex_from_png(renderer, "../res/cat_animation_hit.png"),
  };

  SDL_Log("Num anis: %d", ani_obj.num_anis);
  for(int i = 0; i < ani_obj.num_anis; ++i) {
    SDL_Log("animation %d: num of frames: %d",i, animations[i].num_frames);
  }

  SDL_Texture *bg_tex = SDL_CreateTextureFromSurface(renderer, SDL_LoadBMP("../res/background.bmp"));
  SDL_Texture *spawn = load_tex_from_png(renderer, "../res/pngs/item_spawn.png");
  SDL_Texture *belt = load_tex_from_png(renderer, "../res/pngs/base_production_line.png");
  SDL_Texture *wheels = load_tex_from_png(renderer, "../res/pngs/circles.png");

  u64 time_stamp_now  = SDL_GetPerformanceCounter();
  u64 time_stamp_last = 0;
  f64 dt_for_previous_frame = 0;
  
  //NOTE(moritz): Game loop
  b8 quit = false;

  int angle = 0.f;
  while (!quit)
  {
    time_stamp_last = time_stamp_now;
    time_stamp_now  = SDL_GetPerformanceCounter();
    dt_for_previous_frame = (f64)((time_stamp_now - time_stamp_last)/(f64)SDL_GetPerformanceFrequency());
    // SDL_Log("dt: %g seconds", dt_for_previous_frame);

    //NOTE(moritz): Events/Input
    input current_input = {0};
    current_input = previous_input;

    for (s32 idx = 0; idx < SDL_SCANCODE_COUNT; idx += 1)
    {
      button_state *state = &current_input.buttons[idx];
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

    if (current_input.buttons[SDL_SCANCODE_LEFT].down)
      input_direction.x -= 1.0f;
    if (current_input.buttons[SDL_SCANCODE_RIGHT].down)
      input_direction.x += 1.0f;
    if (current_input.buttons[SDL_SCANCODE_UP].down)
      input_direction.y -= 1.0f;
    if (current_input.buttons[SDL_SCANCODE_DOWN].down)
      input_direction.y += 1.0f;

    f32 one_over_input_length = input_direction.x*input_direction.x + input_direction.y*input_direction.y;
    if (one_over_input_length != 0.0)
      one_over_input_length = 1.0f/sqrtf(one_over_input_length);

    input_direction.x *= one_over_input_length;
    input_direction.y *= one_over_input_length;

    player_pos.x += input_direction.x*PLAYER_SPEED*dt_for_previous_frame;
    player_pos.y += input_direction.y*PLAYER_SPEED*dt_for_previous_frame;

    //NOTE(moritz): Drawing
    if (bg_tex) {
      SDL_RenderTexture(renderer, bg_tex, NULL, NULL);
    }
    else {
      SDL_SetRenderDrawColor(renderer, 255, 0, 255, 255);
      SDL_RenderClear(renderer);
    }

    if(ani_obj.spr_tex) {
      //update_animation(&animations[0], dt_for_previous_frame);
      //display_animation(player_pos, &animations[0], spr_dims, spr_tex, renderer);
      update_animated_object(&ani_obj, dt_for_previous_frame);
      display_animated_object(&ani_obj, renderer);
    }
    else {
      SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
      SDL_FRect rect = (SDL_FRect){
        .x = player_pos.x - PLAYER_HALF_DIM,
        .y = player_pos.y - PLAYER_HALF_DIM,
        .w = 2*PLAYER_HALF_DIM,
        .h = 2*PLAYER_HALF_DIM
      };
      SDL_RenderFillRect(renderer, &rect);
    }

    if (belt) {
      SDL_RenderTexture(renderer, belt, NULL, &(SDL_FRect){0, 890, belt->w, belt->h});
    }
    else {
      SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255);
      SDL_RenderFillRect(renderer, &(SDL_FRect){0, 890, 1920, 205});
    }

    if (spawn) {
      SDL_RenderTexture(renderer, spawn, NULL, &(SDL_FRect){1820 - (spawn->w/2), -275, spawn->w, spawn->h});
    }
    else {
      SDL_SetRenderDrawColor(renderer, 255, 0, 255, 255);
      SDL_RenderFillRect(renderer, &(SDL_FRect){0, 890, 400, 400});
    }

    if (wheels) {
      SDL_FPoint center = {wheels->w/2, wheels->h/2};
      angle += 180. * dt_for_previous_frame;
      SDL_RenderTextureRotated(renderer, wheels, NULL, &(SDL_FRect){wheels->w*2, 908, wheels->w, wheels->h}, angle, &center, SDL_FLIP_NONE);
      SDL_RenderTextureRotated(renderer, wheels, NULL, &(SDL_FRect){wheels->w*4, 908, wheels->w, wheels->h}, angle, &center, SDL_FLIP_NONE);

      SDL_RenderTextureRotated(renderer, wheels, NULL, &(SDL_FRect){wheels->w*12, 908, wheels->w, wheels->h},angle, &center, SDL_FLIP_NONE);
      SDL_RenderTextureRotated(renderer, wheels, NULL, &(SDL_FRect){wheels->w*14, 908, wheels->w, wheels->h}, angle, &center, SDL_FLIP_NONE);
    }

    SDL_RenderPresent(renderer);
  }

  SDL_DestroyTexture(ani_obj.spr_tex);
  SDL_DestroyTexture(bg_tex);
  SDL_DestroyTexture(spawn);
  SDL_DestroyTexture(belt);
  SDL_DestroyTexture(wheels);

  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(main_window);
  SDL_Quit();
  return 0;
}
