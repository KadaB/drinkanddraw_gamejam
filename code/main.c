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


//#define make_ani(ani_array, delay) { .frames = ani_array, sizeof(ani_array) / sizeof(ani_array[0]), delay, 0., 0 }
#define make_ani(ani_array, delay) { .frames = ani_array, .num_frames = sizeof(ani_array) / sizeof(ani_array[0]), .duration = delay, .elapsed = 0., .cur_frame = 0 }

// returns true if animation not yet ended?
b8 update_animation(Animation *animation, f64 elapsed_delta_sec) {
  animation->elapsed += elapsed_delta_sec;

  if(animation->elapsed > animation->duration) {
    animation->cur_frame++;
    if(animation->cur_frame > animation->num_frames) animation->cur_frame = 0;
    animation->elapsed = 0.f;
    return false;
  }
  return true;
}


void update_animated_object(AnimatedObject* ani_obj, f64 elapsed_delta_sec) {
  if(!update_animation(&ani_obj->animations[ani_obj->cur_animation], elapsed_delta_sec)) {
    //ani_obj->cur_animation = (ani_obj->cur_animation+1) % 16;//ani_obj->num_anis;
  }
}

void display_animation(v2 player_pos, Animation *animation, v2 spr_dims, SDL_Texture* spr_tex,  SDL_Renderer *renderer) {
  SDL_FRect srcRect = frame_at(animation->frames[animation->cur_frame], spr_dims);

  SDL_FRect spr_rect = (SDL_FRect) {
    .x = player_pos.x - PLAYER_HALF_DIM,
    .y = player_pos.y - PLAYER_HALF_DIM,
    .w = 50 * 3,
    .h = 37 * 3
  };

  SDL_RenderTexture(renderer, spr_tex, &srcRect, &spr_rect);
}

void display_animated_object(AnimatedObject* ani_obj, SDL_Renderer *renderer) {
  display_animation(*ani_obj->position, &ani_obj->animations[ani_obj->cur_animation], ani_obj->spr_dims, ani_obj->spr_tex, renderer);
}

int main(int argc, char **argv)
{
  // anim def
  v2 batch_limits = {7, 11};  // how many sprites row, cols
  v2 spr_dims = {50, 37};     // sprite size w, h pixels

  v2   idle1[] = { {0, 0}, {1, 0}, {2, 0}, {3, 0} };
  v2  crouch[] = { {4, 0}, {5, 0}, {6, 0}, {0, 1} };
  v2     run[] = { {1, 1}, {2, 1}, {3, 1}, {4, 1}, {5, 1}, {6, 1} };
  v2    jump[] = { {0, 2}, {1, 2}, {2, 2}, {3, 2} };
  v2     mid[] = { {4, 2}, {5, 2}, {6, 2}, {0, 3} };
  v2    fall[] = { {1, 3}, {2, 3} };
  v2   slide[] = { {3, 3}, {4, 3}, {5, 3}, {6, 3}, {0, 4} };
  v2    grab[] = { {1, 4}, {2, 4}, {3, 4}, {4, 4} };
  v2   climb[] = { {5, 4}, {6, 4}, {0, 5}, {1, 5}, {2, 5} };
  v2   idle2[] = { {3, 5}, {4, 5}, {5, 5}, {6, 5} };
  v2 attack1[] = { {0, 6}, {1, 6}, {2, 6}, {3, 6}, {4, 6} };
  v2 attack2[] = { {5, 6}, {6, 6}, {0, 7}, {1, 7}, {2, 7}, {3, 7} };
  v2 attack3[] = { {4, 7}, {5, 7}, {6, 7}, {0, 8}, {1, 8}, {2, 8} };
  v2    hurt[] = { {3, 8}, {4, 8}, {5, 8} };
  v2     die[] = { {6, 8}, {0, 9}, {1, 9}, {2, 9}, {3, 9}, {4, 9}, {5, 9} };
  v2   jump2[] = { {6, 9}, {0, 10}, {1, 10} };

  Animation animations[] = {
    make_ani(idle1, 1.2),
    make_ani(crouch, 1.2),
    make_ani(run, 1.2),
    make_ani(jump, 1.2),
    make_ani(mid, 1.2),
    make_ani(fall, 1.2),
    make_ani(slide, 1.2),
    make_ani(grab, 1.2),
    make_ani(climb, 1.2),
    make_ani(idle2, 1.2),
    make_ani(attack1, 1.2),
    make_ani(attack2, 1.2),
    make_ani(attack3, 1.2),
    make_ani(hurt, 1.2),
    make_ani(die, 1.2),
    make_ani(jump2, 1.2)
  };
  
  for(int i = 0; i < sizeof(animations)/sizeof(animations[0]); ++i) {
    SDL_Log("animation %d: num of frames: %d",i, animations[i].num_frames);
  }

  //NOTE(moritz): Initialization
  if (!SDL_Init(SDL_INIT_VIDEO))
  {
    SDL_Log("Could not initialize SDL: %s", SDL_GetError());
    return 1;
  }

  SDL_Window *main_window = SDL_CreateWindow("SDL Window",
                                             800,
                                             600,
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

  SDL_Texture *spr_tex = SDL_CreateTextureFromSurface(renderer, SDL_LoadBMP("res/hero.bmp"));
  
  input previous_input = {0};
  v2 player_pos = {
    .x = 400,
    .y = 300
  };


  AnimatedObject ani_obj = {
    .animations = animations,
    .num_anis = sizeof(animations) / sizeof(animations[0]),
    .cur_animation = 14,
    .position = &player_pos,
    .spr_dims = {50,37},
    .spr_tex = spr_tex, 
  };

  SDL_Log("Num anis: %d", ani_obj.num_anis);

  u64 time_stamp_now  = SDL_GetPerformanceCounter();
  u64 time_stamp_last = 0;
  f64 dt_for_previous_frame = 0;
  
  //NOTE(moritz): Game loop
  b8 quit = false;
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
    SDL_SetRenderDrawColor(renderer, 255, 0, 255, 255);
    SDL_RenderClear(renderer);

    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
    SDL_FRect rect = (SDL_FRect){
      .x = player_pos.x - PLAYER_HALF_DIM,
      .y = player_pos.y - PLAYER_HALF_DIM,
      .w = 2*PLAYER_HALF_DIM,
      .h = 2*PLAYER_HALF_DIM
    };


    if(spr_tex) {
      //update_animation(&animations[0], dt_for_previous_frame);
      //display_animation(player_pos, &animations[0], spr_dims, spr_tex, renderer);
      update_animated_object(&ani_obj, dt_for_previous_frame);
      display_animated_object(&ani_obj, renderer);
    }
    else 
      SDL_RenderFillRect(renderer, &rect);

    SDL_RenderPresent(renderer);
  }

  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(main_window);
  SDL_Quit();
  return 0;
}
