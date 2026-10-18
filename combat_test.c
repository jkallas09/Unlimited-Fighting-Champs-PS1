#define HOST_TEST
#include "main.c"
#include <assert.h>
int main(void) {
    reset_match();player.x=0;cpu.x=65;start_attack(&player,1);
    for(int i=0;i<6;i++)animate_and_hit(&player,&cpu);
    assert(cpu.hp==100);animate_and_hit(&player,&cpu);assert(cpu.hp==94);
    for(int i=0;i<18;i++)animate_and_hit(&player,&cpu);
    assert(cpu.hp==94 && !player.attack);
    reset_match();player.x=0;cpu.x=65;cpu.guard=1;start_attack(&player,2);
    for(int i=0;i<12;i++)animate_and_hit(&player,&cpu);assert(cpu.hp==97);
    reset_match();player.stamina=0;start_attack(&player,4);assert(!player.attack);
    reset_match();player.x=0;cpu.x=65;cpu.z=100;start_attack(&player,4);
    for(int i=0;i<15;i++)animate_and_hit(&player,&cpu);assert(cpu.hp==100);
    reset_match();player.x=0;cpu.x=65;cpu.hp=5;start_attack(&player,1);
    for(int i=0;i<7;i++)animate_and_hit(&player,&cpu);assert(finished && cpu.hp==0);
    previous_pad=0;for(int i=0;i<120;i++)game_tick(0);assert(cpu.fall==18);
    game_tick(PAD_CROSS);assert(!finished && cpu.hp==100);
    previous_pad=0;int time=round_frames;game_tick(PAD_START);assert(paused && time==round_frames);
    game_tick(0);game_tick(PAD_START);assert(!paused && round_frames==time-1);
    reset_match();previous_pad=0;
    for(int i=0;i<15000;i++) {
        uint16_t controls=(uint16_t)rnd() & ~PAD_START;
        game_tick(controls);assert(player.hp>=0 && player.hp<=100);
        assert(cpu.hp>=0 && cpu.hp<=100 && player.stamina>=0);
        assert(absi(player.x)<=230 && absi(player.z)<=145);
        assert(absi(player.x)+absi(player.z)<=320);
    }
    /* Title -> mode -> roster, including input latching and back navigation. */
    app_screen=SCREEN_TITLE;chosen_character=0;menu_previous=previous_pad=0;
    reset_match();time=round_frames;
    for(int i=0;i<120;i++)app_tick(0);
    assert(app_screen==SCREEN_TITLE && round_frames==time);
    app_tick(PAD_START);assert(app_screen==SCREEN_MODE);
    for(int i=0;i<30;i++)app_tick(PAD_START);
    assert(app_screen==SCREEN_MODE);
    app_tick(0);app_tick(PAD_CROSS);assert(app_screen==SCREEN_CHARACTER);
    app_tick(PAD_CROSS);assert(app_screen==SCREEN_CHARACTER);
    app_tick(0);app_tick(PAD_LEFT);assert(chosen_character==3);
    app_tick(0);app_tick(PAD_RIGHT);assert(chosen_character==0);
    app_tick(0);app_tick(PAD_CIRCLE);assert(app_screen==SCREEN_MODE);
    app_tick(0);app_tick(PAD_CIRCLE);assert(app_screen==SCREEN_TITLE);
    for(int id=0;id<4;id++) {
        app_screen=SCREEN_CHARACTER;chosen_character=id;menu_previous=0;
        app_tick(PAD_START);assert(app_screen==SCREEN_WALKOUT && walkout_phase==0);
        app_tick(PAD_START);assert(app_screen==SCREEN_WALKOUT && walkout_phase==0);
        app_tick(0);app_tick(PAD_START);assert(app_screen==SCREEN_WALKOUT && walkout_phase==1);
        app_tick(PAD_START);assert(app_screen==SCREEN_WALKOUT && walkout_phase==1);
        app_tick(0);app_tick(PAD_START);assert(app_screen==SCREEN_FIGHT);
        assert(player.character==id && cpu.character!=id && cpu.character>=0 && cpu.character<4);
        app_tick(PAD_START);assert(!paused && round_frames==60*90-1);
        app_tick(0);app_tick(PAD_START);assert(paused);
        app_tick(0);app_tick(PAD_CIRCLE);assert(app_screen==SCREEN_CHARACTER && !paused);
        app_tick(0);app_tick(PAD_CROSS);assert(app_screen==SCREEN_WALKOUT && player.character==id);
        int walk_time=round_frames;
        for(int f=0;f<600;f++)app_tick(0);
        assert(app_screen==SCREEN_FIGHT && round_frames==walk_time);
        finished=1;restart_delay=0;app_tick(0);app_tick(PAD_CROSS);
        assert(!finished && player.character==id);
        finished=1;restart_delay=0;app_tick(0);app_tick(PAD_CIRCLE);
        assert(app_screen==SCREEN_CHARACTER);
    }
    /* Momentum, separation, guard coverage, active frames and hit response. */
    reset_match();int start_x=player.x;move_body(&player,1,0);
    assert(player.x==start_x && player.vx==100);
    for(int i=0;i<9;i++)move_body(&player,1,0);
    assert(player.x>start_x && player.vx==720);
    start_x=player.x;move_body(&player,0,0);assert(player.x>start_x && player.vx>0);
    for(int i=0;i<10;i++)move_body(&player,0,0);
    assert(player.vx==0);
    reset_match();player.x=cpu.x=0;player.z=cpu.z=0;separate_bodies();
    assert(absi(player.x-cpu.x)>=40);
    reset_match();player.x=0;cpu.x=20;player.z=0;cpu.z=20;separate_bodies();
    assert((cpu.x-player.x)*(cpu.x-player.x)+(cpu.z-player.z)*(cpu.z-player.z)>=40*40);
    reset_match();player.x=0;cpu.x=65;cpu.guard=1;start_attack(&player,3);
    for(int i=0;i<contact[3];i++)animate_and_hit(&player,&cpu);
    assert(cpu.hp==91);
    reset_match();player.x=0;cpu.x=65;cpu.guard=2;start_attack(&player,3);
    for(int i=0;i<contact[3];i++)animate_and_hit(&player,&cpu);
    assert(cpu.hp==98 && cpu.stamina<100);
    reset_match();player.x=0;cpu.x=65;cpu.guard=1;cpu.facing=1;start_attack(&player,1);
    for(int i=0;i<contact[1];i++)animate_and_hit(&player,&cpu);
    assert(cpu.hp==94 && cpu.vx>0 && cpu.stun>0);
    start_attack(&cpu,1);assert(!cpu.attack);
    reset_match();player.x=0;cpu.x=200;start_attack(&player,1);
    for(int i=0;i<contact[1];i++)animate_and_hit(&player,&cpu);
    assert(cpu.hp==100);cpu.x=65;animate_and_hit(&player,&cpu);assert(cpu.hp==94);
    for(int i=0;i<10;i++)animate_and_hit(&player,&cpu);assert(cpu.hp==94);
    reset_match();player.x=0;cpu.x=65;cpu.guard=1;cpu.stamina=0;start_attack(&player,1);
    for(int i=0;i<contact[1];i++)animate_and_hit(&player,&cpu);
    assert(cpu.hp==94 && cpu.guard==0 && cpu.stun==14);
    puts("PASS: acceleration, momentum, radial body collision, high/body guard coverage, guard direction, recoil, stun and active hit window");
    puts("PASS: title/mode/roster navigation, held-button latching, wraparound, all 4 playable characters, distinct CPU picks, both walkouts, skip/auto transitions, pause/back and rematch");
    puts("PASS: attack timing, one hit per attack, guard, stamina, depth misses, knockout, rematch, pause and 15000 simulation frames");
}
