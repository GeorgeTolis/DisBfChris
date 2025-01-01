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

#define PRESENT_V 120
#define MAX_VERT_V 960

typedef enum { HOMESCREEN, CONTEXT_SCENE, PENGUIN_CHASE, PENG_TO_SLEIGH, SLEIGH, WIN, LOSE } Scene;

static struct
{
    ng_game_t game;
    ng_interval_t game_tick;

    // A collection of assets used by entities
    // Ideally, they should have been automatically loaded
    // by iterating over the res/ folder and filling in a hastable
    SDL_Texture *player_texture, *penguin_texture, *present_texture, *home_bg_texture, *penguin_bg_texture, *sleigh_bg_texture, *sleigh_texture;
    TTF_Font *main_font;

    Scene current_scene;
    ng_label_t welcome_label;
    ng_sprite_t home_bg;
    ng_label_t penguin_context_label;

    ng_sprite_t penguin_bg;

    ng_animated_sprite_t player;
    ng_animated_sprite_t penguins[3];
    short int penguin_velocity[3];

    short int present_countdown;
    short int max_present_countdown;

    ng_sprite_t presents[10];
    short int score;
    ng_label_t score_label;

    ng_label_t peng_to_sleigh_label;

    ng_sprite_t sleigh_bg;
    ng_animated_sprite_t sleigh;
    bool carrying_present;
    short unsigned int top_present;
    int santa_countdown;
    int vertical_velocity;
    bool is_jumping;
    int floor;
} ctx;

static void create_actors(void){
    ng_game_create(&ctx.game, "DISASTER BEFORE CHRISTMAS", WIDTH, HEIGHT);

    ctx.main_font = TTF_OpenFont("res/free_mono.ttf", 16);
    ctx.player_texture = IMG_LoadTexture(ctx.game.renderer, "res/elf_sprite.png");
    ctx.penguin_texture = IMG_LoadTexture(ctx.game.renderer, "res/penquin.png");
    ctx.present_texture = IMG_LoadTexture(ctx.game.renderer, "res/present.png");
    ctx.home_bg_texture = IMG_LoadTexture(ctx.game.renderer, "res/home_background.png");
    ctx.penguin_bg_texture = IMG_LoadTexture(ctx.game.renderer, "res/penguin_background.png");
    ctx.sleigh_bg_texture = IMG_LoadTexture(ctx.game.renderer, "res/slay_bg.png");
    ctx.sleigh_texture = IMG_LoadTexture(ctx.game.renderer, "res/slay_sprite.png");

    ng_interval_create(&ctx.game_tick, 50);

    ctx.current_scene = HOMESCREEN;
    ctx.carrying_present = false;
    ctx.top_present = 9;
    ctx.vertical_velocity = 0;
    ctx.is_jumping = false;

    ng_sprite_create(&ctx.home_bg, ctx.home_bg_texture);
    ng_sprite_set_scale(&ctx.home_bg, 2.9f);
    ctx.home_bg.transform.x = -200;

    ng_sprite_create(&ctx.penguin_bg, ctx.penguin_bg_texture);
    ng_sprite_set_scale(&ctx.penguin_bg, 5.0f);

    ng_sprite_create(&ctx.sleigh_bg, ctx.sleigh_bg_texture);
    ng_sprite_set_scale(&ctx.sleigh_bg, 5.0f);

    ng_animated_create(&ctx.sleigh, ctx.sleigh_texture, 4);
    ng_sprite_set_scale(&ctx.sleigh.sprite, 8.0f);
    ctx.sleigh.sprite.transform.x = ctx.sleigh.sprite.transform.w - 100;
    ctx.sleigh.sprite.transform.y = HEIGHT - ctx.sleigh.sprite.transform.h - 125;

    ng_animated_create(&ctx.player, ctx.player_texture, 3);
    ng_sprite_set_scale(&ctx.player.sprite, 4.0f);
    ctx.player.sprite.transform.x = (WIDTH - ctx.player.sprite.transform.w - 10)/2;
    ctx.player.sprite.transform.y = HEIGHT - ctx.player.sprite.transform.h - 30;
    ctx.floor = ctx.player.sprite.transform.y;

    for (size_t i = 0; i < 3; i++){
        ng_animated_create(&ctx.penguins[i], ctx.penguin_texture, 2);
        ng_sprite_set_scale(&ctx.penguins[i].sprite, 3.0f);
        ctx.penguins[i].sprite.transform.x = (WIDTH - ctx.penguins[i].sprite.transform.w - 10) / 3.0 * (i) + 50;
        ctx.penguins[i].sprite.transform.y = 55;
        ctx.penguin_velocity[i] = pow(-1, i) * 120 * (i+1);
    }
    ng_animated_set_frame(&ctx.penguins[1], (ctx.penguins[1].frame + 1) % ctx.penguins[1].total_frames);


    for (size_t i = 0; i < 10; i++){
        ng_sprite_create(&ctx.presents[i], ctx.present_texture);
        ng_sprite_set_scale(&ctx.presents[i], 3.0f);
        ctx.presents[i].transform.x = -100;
        ctx.presents[i].transform.y = -100;
    }
    ctx.max_present_countdown = ctx.present_countdown = 30;

    ng_label_create(&ctx.welcome_label, ctx.main_font, 300);
    ng_label_set_content(&ctx.welcome_label, ctx.game.renderer, "DISASTER BEFORE CHRISTMAS\n  PRESS [SPACE] TO PLAY");
    ng_sprite_set_scale(&ctx.welcome_label.sprite, 3.0f);
    ctx.welcome_label.sprite.transform.x = WIDTH/2 - ctx.welcome_label.sprite.transform.w/2 + 110;
    ctx.welcome_label.sprite.transform.y = HEIGHT/4 - 15;

    ng_label_create(&ctx.penguin_context_label, ctx.main_font, 400);
    ng_label_set_content(&ctx.penguin_context_label, ctx.game.renderer, "You are a hard working elf\nlike no other, but at\n"
                                                                        "Christmas Eve, some pesky\npenguins stole Santa's presents.\n\n"
                                                                        "Your job now is to try and\ncollect the presents that fall\n"
                                                                        "from the penguins and load\nSanta's sleigh before he notices.");
    ng_sprite_set_scale(&ctx.penguin_context_label.sprite, 2.0f);
    ctx.penguin_context_label.sprite.transform.x = WIDTH/2 - ctx.penguin_context_label.sprite.transform.w/2 + 35;
    ctx.penguin_context_label.sprite.transform.y = HEIGHT/4 - 15;

    ng_label_create(&ctx.score_label, ctx.main_font, 300);
    ng_label_set_content(&ctx.score_label, ctx.game.renderer, "SCORE: 0");
    ctx.score_label.sprite.transform.x = 10;
    ctx.score_label.sprite.transform.y = 150;

    ng_label_create(&ctx.peng_to_sleigh_label, ctx.main_font, 300);
    ng_label_set_content(&ctx.peng_to_sleigh_label, ctx.game.renderer, "LOAD THE PRESENTS");
    ng_sprite_set_scale(&ctx.peng_to_sleigh_label.sprite, 4.0f);
    ctx.peng_to_sleigh_label.sprite.transform.x = WIDTH/2 - ctx.peng_to_sleigh_label.sprite.transform.w/2 + 35;
    ctx.peng_to_sleigh_label.sprite.transform.y = HEIGHT/2 - ctx.peng_to_sleigh_label.sprite.transform.h/2;
}

// A place to handle queued events.
static void handle_event(SDL_Event *event){
    switch (event->type)
    {
    case SDL_KEYDOWN:
        // Press space to start!
        if (event->key.keysym.sym == SDLK_SPACE && ctx.current_scene == HOMESCREEN){
            ctx.current_scene = CONTEXT_SCENE;
            ctx.score = 1;
        }

        break;
    case SDL_MOUSEMOTION:
        // Move label on mouse position
        // By the way, that's how you can implement a custom cursor
        //ctx.aaa.sprite.transform.x = event->motion.x;
        //ctx.aaa.sprite.transform.y = event->motion.y;
        break;
    }
}

static void player_n_enemy_movement(float delta){
    // Handling "continuous" events, which are now repeatable
    const Uint8* keys = SDL_GetKeyboardState(NULL);

    if (keys[SDL_SCANCODE_LEFT]){
        ctx.player.sprite.transform.x -= 640* delta;
    }
    if (keys[SDL_SCANCODE_RIGHT]){
        ctx.player.sprite.transform.x += 640* delta;
    }
    if (keys[SDL_SCANCODE_SPACE] && !ctx.is_jumping){
        ctx.vertical_velocity = 960;
        ctx.is_jumping = true;
    }

    if (ctx.is_jumping){
        ctx.player.sprite.transform.y -= ctx.vertical_velocity * delta;
        ctx.vertical_velocity -= 40;
        if (ctx.player.sprite.transform.y > ctx.floor){
            ctx.player.sprite.transform.y = ctx.floor;
            ctx.is_jumping = false;
        }
    }

    // Moving penguins back and forth
    ng_vec2 left = { 20 , 50 }, right = { WIDTH - 100, 50 };
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

static void points_check(){
    ng_vec2 player_pos = { ctx.player.sprite.transform.x + ctx.player.sprite.transform.w/2, ctx.player.sprite.transform.y + ctx.player.sprite.transform.h/2 };
    ng_vec2 player_present_dist;
    float distance;
    for (size_t i = 0; i < 10; i++){
        if (ctx.presents[i].transform.y < 0 || ctx.presents[i].transform.y > HEIGHT) continue;

        ng_vec2 pres_pos = { ctx.presents[i].transform.x, ctx.presents[i].transform.y };
        ng_vectors_substract(&player_present_dist, &player_pos, &pres_pos);
        distance = ng_vector_get_magnitude(&player_present_dist);
        
        if (distance < 110){
            ctx.score++;
            ctx.presents[i].transform.y += HEIGHT;
        }

        if (ctx.score >= 1){
            ctx.current_scene = PENG_TO_SLEIGH;
            ctx.max_present_countdown = 17;
            ng_animated_set_frame(&ctx.player, 1);

            ctx.player.sprite.transform.x = WIDTH - 300;
            ctx.player.sprite.transform.y = ctx.sleigh.sprite.transform.y + ctx.player.sprite.transform.h - 15;
            ctx.floor = ctx.player.sprite.transform.y;

            for (i = 0; i < 10; i++){
                ctx.presents[i].transform.x = ctx.player.sprite.transform.x + 50;
                ctx.presents[i].transform.y = ctx.player.sprite.transform.y + ctx.presents[i].transform.h/2 + 10 - i * ctx.presents[i].transform.h / 2;
            }
        }
    }
}

static void update_home_to_penguin_scene(){
    if (ng_interval_is_ready(&ctx.game_tick)){
        ctx.score--;

        if (ctx.score <= 0){
            ctx.current_scene = PENGUIN_CHASE;
            ctx.score = 0;
        }
    }
}

static void update_peng_to_sleigh_scene(){
    if (ng_interval_is_ready(&ctx.game_tick)){
        ctx.max_present_countdown--;

        if (ctx.max_present_countdown <= 0){
            ctx.current_scene = SLEIGH;
            ctx.max_present_countdown = 0;
        }
    }
}

static void update_sleigh_scene(float delta){
    const Uint8* keys = SDL_GetKeyboardState(NULL);

    if (keys[SDL_SCANCODE_LEFT]){
        ctx.player.sprite.transform.x -= 640* delta;
        ng_animated_set_frame(&ctx.player, 1);
    }
    if (keys[SDL_SCANCODE_RIGHT]){
        ctx.player.sprite.transform.x += 640* delta;
        ng_animated_set_frame(&ctx.player, 2);
    }
    if (keys[SDL_SCANCODE_SPACE] && !ctx.is_jumping){
        ctx.vertical_velocity = 960;
        ctx.is_jumping = true;
    }

    if (ctx.is_jumping){
        ctx.player.sprite.transform.y -= ctx.vertical_velocity * delta;
        ctx.vertical_velocity -= 40;
        if (ctx.player.sprite.transform.y > ctx.floor){
            ctx.player.sprite.transform.y = ctx.floor;
            ctx.is_jumping = false;
        }
    }

    //ctx.sleigh.sprite.transform.y + ctx.player.sprite.transform.h - 15

    if (ctx.carrying_present){
        ng_animated_set_frame(&ctx.player, 0);
    }

    if (ctx.presents[0].transform.x < 0 && !ctx.carrying_present){
        ctx.current_scene = WIN;
    }

    ng_vec2 player_pos = { ctx.player.sprite.transform.x + ctx.player.sprite.transform.w/2, ctx.player.sprite.transform.y + ctx.player.sprite.transform.h/2 };
    ng_vec2 pres_pos = { ctx.presents[0].transform.x, ctx.presents[0].transform.y };
    ng_vec2 player_target_dist;

    ng_vectors_substract(&player_target_dist, &player_pos, &pres_pos);
    float distance = ng_vector_get_magnitude(&player_target_dist);
    if (distance < 20 && !ctx.carrying_present){
        ctx.presents[ctx.top_present].transform.x = -100;
        ctx.top_present--;
        ctx.carrying_present = true;
    }

    ng_vec2 sleigh_pos = { ctx.sleigh.sprite.transform.x + ctx.sleigh.sprite.transform.w/2, ctx.sleigh.sprite.transform.y + ctx.sleigh.sprite.transform.h/2 };
    
    ng_vectors_substract(&player_target_dist, &player_pos, &sleigh_pos);
    distance = ng_vector_get_magnitude(&player_target_dist);
    if (distance < 80 && ctx.carrying_present){
        if (ctx.top_present == 8 || ctx.top_present == 5 || ctx.top_present == 1){
            ng_animated_set_frame(&ctx.sleigh, (ctx.sleigh.frame + 1) % ctx.sleigh.total_frames);
        }

        ctx.carrying_present = false;
    }
}

static void render_home_scene(){
    ng_sprite_render(&ctx.home_bg, ctx.game.renderer);
    ng_sprite_render(&ctx.welcome_label.sprite, ctx.game.renderer);
}

static void render_home_to_penguin_scene(){
    ng_sprite_render(&ctx.penguin_context_label.sprite, ctx.game.renderer);
}

static void render_penguin_scene(){
    ng_sprite_render(&ctx.penguin_bg, ctx.game.renderer);
    ng_sprite_render(&ctx.player.sprite, ctx.game.renderer);
    for (size_t i = 0; i < 3; i++){
        ng_sprite_render(&ctx.penguins[i].sprite, ctx.game.renderer);    
    }
    for (size_t i = 0; i < 10; i++){
        if (ctx.presents[i].transform.y < 0 || ctx.presents[i].transform.y > HEIGHT) continue;

        ng_sprite_render(&ctx.presents[i], ctx.game.renderer);
    }
    //ng_sprite_render(&ctx.score_label.sprite, ctx.game.renderer);
}

static void render_peng_to_sleigh_scene(){
    ng_sprite_render(&ctx.peng_to_sleigh_label.sprite, ctx.game.renderer);
}

static void render_sleigh_scene(){
    ng_sprite_render(&ctx.sleigh_bg, ctx.game.renderer);
    ng_sprite_render(&ctx.sleigh.sprite, ctx.game.renderer);
    for (size_t i = 0; i < 10; i++){
        ng_sprite_render(&ctx.presents[i], ctx.game.renderer);
    }
    ng_sprite_render(&ctx.player.sprite, ctx.game.renderer);
}

static void update_correct_screen(float delta){
    switch (ctx.current_scene){
    case HOMESCREEN:
        break;
    case CONTEXT_SCENE:
        update_home_to_penguin_scene();
        break;
    case PENGUIN_CHASE:
        player_n_enemy_movement(delta);
        points_check();
        break;
    case PENG_TO_SLEIGH:
        update_peng_to_sleigh_scene();
        break;
    case SLEIGH:
        update_sleigh_scene(delta);
        break;
    case WIN:
    case LOSE:
    default:
        break;
    }  
}

static void render_correct_screen(){
    switch (ctx.current_scene){
    case HOMESCREEN:
        render_home_scene();
        break;
    case CONTEXT_SCENE:
        render_home_to_penguin_scene();
        break;
    case PENGUIN_CHASE:
        render_penguin_scene();
        break;
    case PENG_TO_SLEIGH:
        render_peng_to_sleigh_scene();
        break;
    case SLEIGH:
        render_sleigh_scene();
        break;
    case WIN:
    case LOSE:
    default:
        break;
    }    
}

static void update_and_render_scene(float delta){
    update_correct_screen(delta);
    render_correct_screen();
}

int main(){
    create_actors();
    ng_game_start_loop(&ctx.game, handle_event, update_and_render_scene);
    return 0;
}
