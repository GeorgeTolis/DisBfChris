#include <SDL2/SDL_image.h>
#include <stdio.h>
#include <math.h>
#include "engine/game.h"
#include "engine/common.h"
#include "engine/sprite.h"
#include "engine/interface.h"
#include "engine/timers.h"
#include "engine/audio.h"

#define WIDTH 1280
#define HEIGHT 640*1.4

#define PRESENT_V 80

static struct
{
    ng_game_t game;
    ng_interval_t game_tick;

    // A collection of assets used by entities
    // Ideally, they should have been automatically loaded
    // by iterating over the res/ folder and filling in a hastable
    SDL_Texture *player_texture, *penguin_texture, *present_texture;
    Mix_Chunk *gem_sfx;
    TTF_Font *main_font;

    ng_animated_sprite_t player;
    ng_sprite_t presents[10];
    //ng_label_t welcome_text;
    ng_animated_sprite_t penguins[3];
    short int penguin_velocity[3];
    short int present_countdown;
    short int max_present_countdown;
} ctx;

static void create_actors(void)
{
    ng_game_create(&ctx.game, "Elf Life", WIDTH, HEIGHT);

    ctx.main_font = TTF_OpenFont("res/free_mono.ttf", 16);
    ctx.player_texture = IMG_LoadTexture(ctx.game.renderer, "res/elf_sprite.png");
    ctx.penguin_texture = IMG_LoadTexture(ctx.game.renderer, "res/penquin.png");
    ctx.present_texture = IMG_LoadTexture(ctx.game.renderer, "res/present.png");
    ctx.gem_sfx = ng_audio_load("res/gem.wav");

    ng_interval_create(&ctx.game_tick, 50);

    ng_animated_create(&ctx.player, ctx.player_texture, 3);
    ng_sprite_set_scale(&ctx.player.sprite, 4.0f);
    ctx.player.sprite.transform.x = (WIDTH - ctx.player.sprite.transform.w - 10)/2;
    ctx.player.sprite.transform.y = HEIGHT - ctx.player.sprite.transform.h - 10;

    for (size_t i = 0; i < 3; i++){
        ng_animated_create(&ctx.penguins[i], ctx.penguin_texture, 2);
        ng_sprite_set_scale(&ctx.penguins[i].sprite, 3.0f);
        ctx.penguins[i].sprite.transform.x = (WIDTH - ctx.penguins[i].sprite.transform.w - 10) / 3.0 * (i) + 50;
        ctx.penguins[i].sprite.transform.y = 30;
        ctx.penguin_velocity[i] = pow(-1, i) * 120 * (i+1);
    }
    ng_animated_set_frame(&ctx.penguins[1], (ctx.penguins[1].frame + 1) % ctx.penguins[1].total_frames);


    for (size_t i = 0; i < 10; i++){
        ng_sprite_create(&ctx.presents[i], ctx.present_texture);
        ng_sprite_set_scale(&ctx.presents[i], 3.0f);
        ctx.presents[i].transform.x = -100;//WIDTH/2.0;
        ctx.presents[i].transform.y = -100;//HEIGHT/2.0;
    }
    ctx.max_present_countdown = ctx.present_countdown = 30;

    /*ng_label_create(&ctx.welcome_text, ctx.main_font, 300);
    ng_label_set_content(&ctx.welcome_text, ctx.game.renderer,
            "I don't really know what to write in here. "
            "You'll notice that you can't type Greek. To do that, "
            "just replace the internals with TTF_RenderUTF8_Solid");*/
}

// A place to handle queued events.
static void handle_event(SDL_Event *event)
{
    switch (event->type)
    {
    case SDL_KEYDOWN:
        // Press space for a sound effect!
        //if (event->key.keysym.sym == SDLK_SPACE)
        //    ng_audio_play(ctx.gem_sfx);

        break;

    case SDL_MOUSEMOTION:
        // Move label on mouse position
        // By the way, that's how you can implement a custom cursor
        //ctx.welcome_text.sprite.transform.x = event->motion.x;
        //ctx.welcome_text.sprite.transform.y = event->motion.y;
        break;
    }
}

static void player_n_enemy_movement(float delta){
    // Handling "continuous" events, which are now repeatable
    const Uint8* keys = SDL_GetKeyboardState(NULL);
    bool movement_input = false;

    if (keys[SDL_SCANCODE_LEFT]){
        ctx.player.sprite.transform.x -= 640* delta;
    }
    if (keys[SDL_SCANCODE_RIGHT]){
        ctx.player.sprite.transform.x += 640* delta;
    }

    // Moving penguins back and forth
    ng_vec2 left = { 10 , 10 }, right = { WIDTH - 100, 10 };
    ng_vec2 left_bound, right_bound;
    float left_threshold, right_threshold, threshold = 30;

    size_t i;
    for (i = 0; i < 3; i++){
        ng_vec2 penguin_pos = { ctx.penguins[i].sprite.transform.x, ctx.penguins[i].sprite.transform.y };

        ng_vectors_substract(&left_bound, &penguin_pos, &left);
        ng_vectors_substract(&right_bound, &right, &penguin_pos);
        
        float left_threshold = ng_vector_get_magnitude(&left_bound);
        float right_threshold = ng_vector_get_magnitude(&right_bound);
        if (right_threshold < threshold || left_threshold < threshold){
            ctx.penguin_velocity[i] *= -1;
            ng_animated_set_frame(&ctx.penguins[i], (ctx.penguins[i].frame + 1) % ctx.penguins[i].total_frames);
        }

        ctx.penguins[i].sprite.transform.x += ctx.penguin_velocity[i] * delta;
    }

    // Once every 100ms
    bool spawn_present = false;
    if (ng_interval_is_ready(&ctx.game_tick)){
        ctx.present_countdown--;
        if (ctx.present_countdown < 0){
            ctx.present_countdown = ctx.max_present_countdown;
            if (ctx.max_present_countdown > 10) ctx.max_present_countdown--;
            spawn_present = true;
        }
        //ng_animated_set_frame(&ctx.explosion, (ctx.explosion.frame + 1) % ctx.explosion.total_frames);
        //if (movement_input) ng_animated_set_frame(&ctx.player, (ctx.player.frame + 1) % ctx.player.total_frames);
    }

    // Spawn present if needed
    if (spawn_present){
        for (i = 0; i < 10; i++){
            if (ctx.presents[i].transform.y < 0 || ctx.presents[i].transform.y > HEIGHT)
                break;
        }

        if (i < 10){
            int j = ng_random_int_in_range(0, 3);
            ctx.presents[i].transform.x = ctx.penguins[j].sprite.transform.x;
            ctx.presents[i].transform.y = ctx.penguins[j].sprite.transform.y;
        }
    }

    // Move presents
    for (i = 0; i < 10; i++){
        if (ctx.presents[i].transform.y < 0 || ctx.presents[i].transform.y > HEIGHT) continue;

        ctx.presents[i].transform.y += PRESENT_V * delta;
    }
}

static void update_and_render_scene(float delta)
{
    
    // Moving charaters in the scene
    player_n_enemy_movement(delta);

    ng_sprite_render(&ctx.player.sprite, ctx.game.renderer);
    for (size_t i = 0; i < 3; i++){
        ng_sprite_render(&ctx.penguins[i].sprite, ctx.game.renderer);    
    }
    for (size_t i = 0; i < 10; i++){
        if (ctx.presents[i].transform.y < 0 || ctx.presents[i].transform.y > HEIGHT) continue;

        ng_sprite_render(&ctx.presents[i], ctx.game.renderer);
    }
    //ng_sprite_render(&ctx.welcome_text.sprite, ctx.game.renderer);
}

int main()
{
    create_actors();
    ng_game_start_loop(&ctx.game, handle_event, update_and_render_scene);
    return 0;
}
