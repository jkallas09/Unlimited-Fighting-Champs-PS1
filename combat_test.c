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
    puts("PASS: attack timing, one hit per attack, guard, stamina, depth misses, knockout, rematch, pause and 15000 simulation frames");
}
