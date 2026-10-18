/* Unlimited Fighting Champs - original PS1 homebrew technical prototype.
 * PSn00bSDK; simple 2D/2.5D combat placeholder for eventual 3D engine.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <psxgpu.h>
#include <psxapi.h>
#include <psxetc.h>

#define W 320
#define H 240
#define OT_LEN 8
#define PACKETS 8192
#define PAD_LEFT  0x0080
#define PAD_RIGHT 0x0020
#define PAD_UP    0x0010
#define PAD_DOWN  0x0040
#define PAD_SQUARE 0x8000
#define PAD_CROSS 0x4000
#define PAD_CIRCLE 0x2000
#define PAD_TRIANGLE 0x1000
#define PAD_L1 0x0004
#define PAD_L2 0x0001

static DISPENV disp[2];
static DRAWENV draw[2];
static uint32_t ot[2][OT_LEN];
static uint32_t packet[2][PACKETS / sizeof(uint32_t)];
static void *primitive_tail;
static uint8_t pad1[34], pad2[34];
static int db, cursor;

typedef struct { int x, y, hp, stamina, facing, cooldown, guard, flash, wins; } Fighter;
static Fighter player, cpu;
static int round_frames, finished, restart_delay;
static unsigned int seed=0x1234567u;

static unsigned int rnd(void) { seed=seed*1664525u+1013904223u; return seed; }
static int clamp(int v,int lo,int hi) { return v<lo?lo:(v>hi?hi:v); }
static int absi(int v) { return v<0?-v:v; }
static uint16_t pad_buttons(void) {
    if (pad1[0] != 0 || (pad1[1] != 0x41 && pad1[1] != 0x73 && pad1[1] != 0x53)) return 0;
    return (uint16_t)~((unsigned)pad1[2] | ((unsigned)pad1[3]<<8));
}
static void rect(int x,int y,int w,int h,int r,int g,int b) {
    if (w<=0||h<=0||cursor+(int)sizeof(POLY_F4)>PACKETS) return;
    POLY_F4 *p=(POLY_F4 *)((uint8_t *)packet[db]+cursor);
    cursor+=sizeof(POLY_F4);
    setPolyF4(p); setRGB0(p,r,g,b);
    setXY4(p,x,y,x+w,y,x,y+h,x+w,y+h);
    addPrim(primitive_tail,p);
    primitive_tail=p;
}
static void line(int x1,int y1,int x2,int y2,int r,int g,int b) {
    if(cursor+(int)sizeof(LINE_F2)>PACKETS) return;
    LINE_F2 *p=(LINE_F2 *)((uint8_t *)packet[db]+cursor);
    cursor+=sizeof(LINE_F2);
    setLineF2(p); setRGB0(p,r,g,b); setXY2(p,x1,y1,x2,y2);
    addPrim(primitive_tail,p);
    primitive_tail=p;
}
static void reset_match(void) {
    player=(Fighter){94,164,100,100,1,0,0,0,0};
    cpu=(Fighter){226,164,100,100,-1,0,0,0,0};
    round_frames=60*90; finished=0; restart_delay=0;
}
static void hit(Fighter *a,Fighter *b,int power,int reach) {
    if(a->cooldown || a->stamina < power/2 || finished) return;
    a->cooldown=power>12?21:13;
    a->stamina=clamp(a->stamina-power/2,0,100);
    if(absi(a->x-b->x)<reach && absi(a->y-b->y)<22 && (b->x-a->x)*a->facing>0) {
        int dmg=b->guard ? power/4 : power;
        b->hp=clamp(b->hp-dmg,0,100);
        b->flash=8;
        b->x=clamp(b->x+a->facing*(b->guard?2:6),26,294);
        if(b->hp==0) { finished=1; restart_delay=150; }
    }
}
static void update_fighter(Fighter *f) {
    if(f->cooldown) f->cooldown--;
    if(f->flash) f->flash--;
    if((round_frames&3)==0 && f->stamina<100) f->stamina++;
}
static void logic(void) {
    uint16_t p=pad_buttons();
    if(finished) { if(restart_delay) restart_delay--; if(!restart_delay && (p&PAD_CROSS)) reset_match(); return; }
    if(round_frames>0) round_frames--; else { finished=1; restart_delay=90; }
    player.guard=!!(p&(PAD_L1|PAD_L2));
    if(p&PAD_LEFT) player.x-=2;
    if(p&PAD_RIGHT) player.x+=2;
    if(p&PAD_UP) player.y-=1;
    if(p&PAD_DOWN) player.y+=1;
    player.x=clamp(player.x,25,295); player.y=clamp(player.y,142,190);
    player.facing=player.x<cpu.x?1:-1;
    cpu.facing=-player.facing;
    if(!player.guard) {
        if(p&PAD_SQUARE) hit(&player,&cpu,6,47);
        else if(p&PAD_TRIANGLE) hit(&player,&cpu,13,52);
        else if(p&PAD_CROSS) hit(&player,&cpu,9,57);
        else if(p&PAD_CIRCLE) hit(&player,&cpu,17,64);
    }
    cpu.guard=(absi(cpu.x-player.x)<65 && player.cooldown>5 && (rnd()%4==0));
    if(absi(cpu.x-player.x)>48) cpu.x+=cpu.facing==1?1:-1;
    else if(absi(cpu.x-player.x)<34) cpu.x-=cpu.facing==1?1:-1;
    if(absi(cpu.y-player.y)>3) cpu.y+=cpu.y<player.y?1:-1;
    cpu.x=clamp(cpu.x,25,295);
    if((rnd()%23)==0 && !cpu.guard) hit(&cpu,&player,(rnd()%3)?6:13,53);
    update_fighter(&player); update_fighter(&cpu);
}
static void fighter_draw(Fighter *f,int r,int g,int b) {
    int x=f->x,y=f->y;
    if(f->flash) {r=245;g=230;b=210;}
    rect(x-13,y+18,26,4,18,18,24); /* shadow */
    rect(x-10,y-19,20,34,r,g,b); /* torso */
    rect(x-8,y-37,16,17,190,140,108); /* head */
    rect(x-10,y+13,8,17,55,55,64); rect(x+2,y+13,8,17,55,55,64);
    rect(x-17,y-16,7,22,190,140,108); rect(x+10,y-16,7,22,190,140,108);
    rect(x+(f->facing>0?10:-18),y-18,9,8,f->guard?220:45,45,45);
    if(f->guard) rect(x-6,y-27,12,6,230,230,230);
}
static void render(void) {
    db^=1; cursor=0;
    ClearOTagR(ot[db],OT_LEN);
    primitive_tail=ot[db];
    rect(0,0,W,H,12,17,28);
    rect(15,100,290,111,35,43,48);
    for(int i=0;i<7;i++) { int xx=18+i*47; line(xx,100,xx,209,65,79,83); }
    for(int i=0;i<4;i++) line(15,108+i*29,305,108+i*29,63,76,82);
    line(15,210,305,210,180,181,172);
    rect(13,17,133,10,66,18,25); rect(13,17,player.hp*133/100,10,53,195,104);
    rect(174,17,133,10,66,18,25); rect(174,17,cpu.hp*133/100,10,53,195,104);
    rect(13,31,player.stamina*133/100,4,235,185,55);
    rect(174,31,cpu.stamina*133/100,4,235,185,55);
    if(player.y<=cpu.y) { fighter_draw(&player,50,105,180); fighter_draw(&cpu,170,55,55); }
    else { fighter_draw(&cpu,170,55,55); fighter_draw(&player,50,105,180); }
    FntPrint(-1,"UNLIMITED FIGHTING CHAMPS\n");
    FntPrint(-1,"BLUE: YOU      RED: CPU\n");
    FntPrint(-1,"TIME %02d\n",round_frames/60);
    if(finished) FntPrint(-1,"MATCH OVER - PRESS X TO RESTART\n");
    else FntPrint(-1,"[] JAB  /\\ POWER  X KICK  O HEAVY\nL1/L2 BLOCK  D-PAD MOVE\n");
    DrawSync(0); VSync(0);
    PutDispEnv(&disp[db]); PutDrawEnv(&draw[db]);
    DrawOTag(ot[db]+OT_LEN-1);
    FntFlush(-1);
}
int main(void) {
    ResetGraph(0);
    SetDefDispEnv(&disp[0],0,0,W,H); SetDefDrawEnv(&draw[0],0,H,W,H);
    SetDefDispEnv(&disp[1],0,H,W,H); SetDefDrawEnv(&draw[1],0,0,W,H);
    draw[0].isbg=draw[1].isbg=1;
    setRGB0(&draw[0],0,0,0); setRGB0(&draw[1],0,0,0);
    FntLoad(960,0); FntOpen(8,48,304,55,0,220);
    InitPAD(pad1,sizeof(pad1),pad2,sizeof(pad2)); StartPAD(); ChangeClearPAD(0);
    SetDispMask(1); reset_match();
    while(1) { logic(); render(); }
    return 0;
}
