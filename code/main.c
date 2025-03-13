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

enum game_buttons
{
  LEFT_BUTTON,
  RIGHT_BUTTON,
  UP_BUTTON,
  DOWN_BUTTON,

  BUTTON_COUNT
};

typedef struct {
  button_state buttons[BUTTON_COUNT];
} input;

#define PLAYER_SPEED 200.0f
#define PLAYER_HALF_DIM 25.0f

int main(int argc, char **argv)
{
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

  input previous_input = {0};
  v2 player_pos = {
    .x = 400,
    .y = 300
  };

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
    SDL_Log("dt: %g seconds", dt_for_previous_frame);

    //NOTE(moritz): Events/Input
    input current_input = {0};
    current_input = previous_input;

    for (s32 idx = 0; idx < BUTTON_COUNT; idx += 1)
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
          switch(e.key.key)
          {
            case SDLK_ESCAPE:
            {
              quit = true;
            } break;

            case SDLK_LEFT:
            {
              current_input.buttons[LEFT_BUTTON].down    = true;
              current_input.buttons[LEFT_BUTTON].pressed = true;
            } break;

            case SDLK_RIGHT:
            {
              current_input.buttons[RIGHT_BUTTON].down    = true;
              current_input.buttons[RIGHT_BUTTON].pressed = true;
            } break;

            case SDLK_UP:
            {
              current_input.buttons[UP_BUTTON].down    = true;
              current_input.buttons[UP_BUTTON].pressed = true;
            } break;

            case SDLK_DOWN:
            {
              current_input.buttons[DOWN_BUTTON].down    = true;
              current_input.buttons[DOWN_BUTTON].pressed = true;
            } break;
          }
        } break;

        case SDL_EVENT_KEY_UP:
        {
          switch(e.key.key)
          {
            case SDLK_LEFT:
            {
              current_input.buttons[LEFT_BUTTON].down     = false;
              current_input.buttons[LEFT_BUTTON].released = true;
            } break;

            case SDLK_RIGHT:
            {
              current_input.buttons[RIGHT_BUTTON].down     = false;
              current_input.buttons[RIGHT_BUTTON].released = true;
            } break;

            case SDLK_UP:
            {
              current_input.buttons[UP_BUTTON].down     = false;
              current_input.buttons[UP_BUTTON].released = true;
            } break;

            case SDLK_DOWN:
            {
              current_input.buttons[DOWN_BUTTON].down     = false;
              current_input.buttons[DOWN_BUTTON].released = true;
            } break;
          }
        } break;

      }
    }

    previous_input = current_input;

    //NOTE(moritz):Update game state
    v2 input_direction = {0};

    if (current_input.buttons[LEFT_BUTTON].down)
      input_direction.x -= 1.0f;
    if (current_input.buttons[RIGHT_BUTTON].down)
      input_direction.x += 1.0f;
    if (current_input.buttons[UP_BUTTON].down)
      input_direction.y -= 1.0f;
    if (current_input.buttons[DOWN_BUTTON].down)
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
    SDL_RenderFillRect(renderer, &rect);

    SDL_RenderPresent(renderer);
  }

  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(main_window);
  SDL_Quit();
  return 0;
}
