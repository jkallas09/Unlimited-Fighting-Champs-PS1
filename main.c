/* Unlimited Fighting Champs 0.5 - original polygonal PS1 combat prototype.
 * Integer-only perspective renderer; no commercial meshes or textures.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#ifndef HOST_TEST
#include <psxgpu.h>
#include <psxapi.h>
#include <psxetc.h>
#include <psxgte.h>
#include <psxspu.h>
#include "music_meta.h"
#endif

#define PAD_LEFT 0x0080
#define PAD_RIGHT 0x0020
#define PAD_UP 0x0010
#define PAD_DOWN 0x0040
#define PAD_SQUARE 0x8000
#define PAD_CROSS 0x4000
#define PAD_CIRCLE 0x2000
#define PAD_TRIANGLE 0x1000
#define PAD_L1 0x0004
#define PAD_L2 0x0001
#define PAD_START 0x0800
#define PAD_SELECT 0x0100

typedef struct {
    int x,z,hp,stamina,facing,guard,flash;
    int attack,age,walk,fall,character;
    int vx,vz,rem_x,rem_z,stun,attack_hit,face_x,face_z,elevation;
} Fighter;
typedef struct {
    const char *name,*nickname;
    int skin_r,skin_g,skin_b,short_r,short_g,short_b;
    int hair_r,hair_g,hair_b,bulk,height,beard,hair_style;
} Character;
static const Character roster[4]={
    {"CONOR MCSNACK","THE NOTORIOUSLY HUNGRY",211,162,125,26,146,86,115,56,25,0,100,1,0},
    {"JON BONELESS","BONES? WHAT BONES?",151,101,76,112,70,171,27,23,23,1,106,1,1},
    {"BROCK LUNCHNER","THE LUNCH BREAK",221,176,139,176,45,39,166,123,64,5,108,0,2},
    {"CHUCK LIDDOODLE","THE ICED LATTE",198,145,111,42,99,195,43,29,24,2,99,1,3}
};
enum {SCREEN_TITLE,SCREEN_MODE,SCREEN_CHARACTER,SCREEN_WALKOUT,SCREEN_FIGHT};
static int app_screen=SCREEN_TITLE,chosen_character,opponent_character=1,menu_ticks;
static uint16_t menu_previous;
static int walkout_phase,walkout_frame;
#define WALKOUT_FRAMES 300

static Fighter player,cpu;
static int round_frames,finished,restart_delay,paused,ticks;
static int impact_frames,impact_shake;
static uint16_t previous_pad;
static unsigned int seed=0x1234567u;
static int clamp(int v,int lo,int hi) { return v<lo?lo:(v>hi?hi:v); }
static int absi(int v) { return v<0?-v:v; }
static unsigned int rnd(void) { seed=seed*1664525u+1013904223u;return seed; }
static const int duration[5]={0,18,28,24,34};
static const int contact[5]={0,7,12,10,15};
static const int power[5]={0,6,13,9,17};
static const int reach[5]={0,81,92,102,113};
static void reset_match(void) {
    player=(Fighter){.x=-100,.z=0,.hp=100,.stamina=100,.facing=1,.character=chosen_character,.face_x=1024};
    cpu=(Fighter){.x=100,.z=0,.hp=100,.stamina=100,.facing=-1,.character=opponent_character,.face_x=1024};
    round_frames=60*90;finished=0;restart_delay=0;paused=0;impact_frames=impact_shake=0;
}
static void boundary(Fighter *f) {
    f->x=clamp(f->x,-230,230); f->z=clamp(f->z,-145,145);
    if(absi(f->x)+absi(f->z)>320) {
        int limit=320-absi(f->z);
        f->x=clamp(f->x,-limit,limit);
    }
}
static void start_attack(Fighter *f,int type) {
    if(f->attack||f->guard||f->stun||f->stamina<power[type]/2||finished) return;
    f->attack=type;f->age=0;f->attack_hit=0;f->stamina-=power[type]/2;
}
static int root_int(int value) {
    unsigned int n=(unsigned int)value,result=0,bit=1u<<30;
    while(bit>n)bit>>=2;
    while(bit) {if(n>=result+bit){n-=result+bit;result=(result>>1)+bit;}else result>>=1;bit>>=2;}
    return (int)result;
}
static int approach(int value,int target,int amount) {
    return value<target?clamp(value+amount,value,target):clamp(value-amount,target,value);
}
static void face_opponent(Fighter *a,Fighter *b) {
    if(a->attack && a->age>=3)return;
    int dx=b->x-a->x,dz=b->z-a->z,dist=root_int(dx*dx+dz*dz);
    if(!dist)return;
    a->facing=dx>=0?1:-1;
    int tx=absi(dx)*1024/dist,tz=dz*a->facing*1024/dist;
    a->face_x+=(tx-a->face_x)/3;a->face_z+=(tz-a->face_z)/3;
}
static void move_body(Fighter *f,int input_x,int input_z) {
    int maxspeed=f->guard?280:(f->attack?330:(f->stamina<15?490:720));
    int diagonal=input_x && input_z;
    if(diagonal)maxspeed=maxspeed*181/256;
    if(f->stun) {input_x=input_z=0;f->stun--;}
    int acceleration=f->stun?70:100;
    f->vx=approach(f->vx,input_x*maxspeed,acceleration);
    f->vz=approach(f->vz,input_z*maxspeed,acceleration);
    int px=f->vx+f->rem_x,pz=f->vz+f->rem_z;
    int ox=f->x,oz=f->z;
    f->x+=px/256;f->z+=pz/256;f->rem_x=px%256;f->rem_z=pz%256;
    boundary(f);
    if(f->x!=ox+px/256)f->vx=f->rem_x=0;
    if(f->z!=oz+pz/256)f->vz=f->rem_z=0;
    if(f->x!=ox||f->z!=oz)f->walk+=150;else f->walk=0;
}
static void separate_bodies(void) {
    int dx=cpu.x-player.x,dz=cpu.z-player.z,sq=dx*dx+dz*dz;
    if(sq>=40*40)return;
    if(!sq){dx=player.facing;dz=0;sq=1;}
    int dist=root_int(sq);if(!dist)dist=1;
    int overlap=41-dist;
    int den=dist*2;
    int push_x=(absi(dx)*overlap+den-1)/den*(dx>=0?1:-1);
    int push_z=(absi(dz)*overlap+den-1)/den*(dz>=0?1:-1);
    if(!push_x&&!push_z)push_x=dx>=0?1:-1;
    player.x-=push_x;cpu.x+=push_x;player.z-=push_z;cpu.z+=push_z;
    player.vx/=2;cpu.vx/=2;player.vz/=2;cpu.vz/=2;
    boundary(&player);boundary(&cpu);
}
static void animate_and_hit(Fighter *a,Fighter *b) {
    if(a->flash)a->flash--;
    if(!a->attack && (ticks & (a->guard?7:3))==0 && a->stamina<100)a->stamina++;
    if(!a->attack)return;
    int type=a->attack;a->age++;
    int dx=b->x-a->x,dz=b->z-a->z;
    int fx=a->face_x*a->facing,fz=a->face_z*a->facing;
    int forward=(dx*fx+dz*fz)/1024,lateral=absi(-dx*fz+dz*fx)/1024;
    int dist2=dx*dx+dz*dz;
    if(!a->attack_hit && a->age>=contact[type] && a->age<=contact[type]+2
        && forward>0 && lateral<(type>=3?32:25) && dist2<reach[type]*reach[type]) {
        int bfx=b->face_x*b->facing,bfz=b->face_z*b->facing;
        int defends=(-dx*bfx-dz*bfz)>0;
        int broken=defends && b->guard && b->stamina<4;
        int blocked=defends && b->guard==(type==3?2:1) && !broken;
        if(broken)b->guard=0;
        int counter=b->attack && b->age<contact[b->attack];
        int dmg=power[type]+(counter?2:0);
        if(a->stamina<15)dmg=dmg*3/4;
        if(dist2>reach[type]*reach[type]*81/100)dmg=dmg*3/4;
        if(blocked)dmg=power[type]/4;
        b->hp=clamp(b->hp-dmg,0,100);b->flash=blocked?3:7;
        a->attack_hit=1;
        int dist=root_int(dist2);if(!dist)dist=1;
        int force=blocked?320:(type==1?650:1050);
        b->vx+=dx*force/dist;b->vz+=dz*force/dist;
        b->stun=broken?14:(blocked?2:(type==1?6:10));
        if(!blocked && b->attack && b->age<contact[b->attack])b->attack=b->age=0;
        if(blocked)b->stamina=clamp(b->stamina-power[type]/3,0,100);
        impact_frames=blocked?0:(type==1?1:2);
        impact_shake=blocked?0:(type>=2?3:1);
        if(!b->hp){finished=1;restart_delay=120;}
    }
    if(a->age>=duration[type]){a->attack=0;a->age=0;a->attack_hit=0;}
}
static void game_tick(uint16_t p) {
    uint16_t pressed=p & ~previous_pad;previous_pad=p;
    if(pressed&PAD_START)paused=!paused;
    if(paused)return;
    ticks++;
    if(finished) {
        if(!player.hp&&player.fall<18)player.fall++;
        if(!cpu.hp&&cpu.fall<18)cpu.fall++;
        if(restart_delay)restart_delay--;
        if(!restart_delay&&(pressed&PAD_CROSS))reset_match();
        return;
    }
    if(--round_frames<=0){round_frames=0;finished=1;restart_delay=90;return;}
    if(impact_frames){impact_frames--;return;}
    player.guard=(!player.attack&&!player.stun)?((p&PAD_L2)?2:((p&PAD_L1)?1:0)):0;
    face_opponent(&player,&cpu);face_opponent(&cpu,&player);
    move_body(&player,!!(p&PAD_RIGHT)-!!(p&PAD_LEFT),!!(p&PAD_UP)-!!(p&PAD_DOWN));
    int dx=player.x-cpu.x,dz=player.z-cpu.z;
    cpu.guard=(!cpu.attack&&!cpu.stun&&player.attack&&absi(dx)<115 && rnd()%3==0)
        ?(player.attack==3?2:1):0;
    int ix=0,iz=0;
    if(!cpu.attack&&!cpu.guard) {
        if(absi(dx)>72)ix=dx>0?1:-1;
        else if(absi(dx)<50)ix=dx>0?-1:1;
        if(absi(dz)>12)iz=dz>0?1:-1;
    }
    /* CPU uses the same acceleration, collision and recovery rules. */
    move_body(&cpu,ix,iz);separate_bodies();
    if(p&PAD_SQUARE)start_attack(&player,1);
    else if(p&PAD_TRIANGLE)start_attack(&player,2);
    else if(p&PAD_CROSS)start_attack(&player,3);
    else if(p&PAD_CIRCLE)start_attack(&player,4);
    if(!cpu.guard && rnd()%24==0)start_attack(&cpu,1+rnd()%4);
    animate_and_hit(&player,&cpu);if(!finished)animate_and_hit(&cpu,&player);
}

/* Menu input uses press edges: holding Start cannot skip multiple screens. */
static void app_tick(uint16_t p) {
    uint16_t pressed=p & ~menu_previous;
    menu_previous=p;
    if(app_screen==SCREEN_FIGHT) {
        if((paused || (finished && !restart_delay)) && (pressed&PAD_CIRCLE)) {
            app_screen=SCREEN_CHARACTER;paused=0;previous_pad=p;return;
        }
        game_tick(p);return;
    }
    menu_ticks++;ticks++;
    if(app_screen==SCREEN_WALKOUT) {
        walkout_frame++;
        if(walkout_frame>=WALKOUT_FRAMES || (pressed&PAD_START)) {
            if(!walkout_phase){walkout_phase=1;walkout_frame=0;}
            else {reset_match();previous_pad=p;app_screen=SCREEN_FIGHT;}
        }
        return;
    }
    if(app_screen==SCREEN_TITLE) {
        if(pressed&PAD_START)app_screen=SCREEN_MODE;
    } else if(app_screen==SCREEN_MODE) {
        if(pressed&PAD_CIRCLE)app_screen=SCREEN_TITLE;
        else if(pressed&(PAD_START|PAD_CROSS))app_screen=SCREEN_CHARACTER;
    } else if(app_screen==SCREEN_CHARACTER) {
        if(pressed&PAD_CIRCLE){app_screen=SCREEN_MODE;return;}
        if(pressed&(PAD_RIGHT|PAD_DOWN))chosen_character=(chosen_character+1)%4;
        else if(pressed&(PAD_LEFT|PAD_UP))chosen_character=(chosen_character+3)%4;
        if(pressed&(PAD_START|PAD_CROSS)) {
            opponent_character=(chosen_character+1+(int)(rnd()%3))%4;
            reset_match();previous_pad=p;walkout_phase=walkout_frame=0;app_screen=SCREEN_WALKOUT;
        }
    }
}

#ifndef HOST_TEST
#define W 320
#define H 240
#define OT_LEN 1024
#define PACKETS 98304
static DISPENV disp[2];static DRAWENV draw[2];
static uint32_t ot[2][OT_LEN],packet[2][PACKETS/4];
static uint8_t pad1[34],pad2[34];
static int db,cursor,font_top,font_bottom,font_menu,cam_x,cam_z,cam_distance=790,cam_yaw=-180;
static int camera_side;
static int view_sin,view_cos;
static int cam_pitch=250,view_pitch_sin=250,view_pitch_cos=993,cam_target_y=95,view_center_y=124;
static int shake_x,shake_y;
static void *background_tail;
static void *hud_tail;
static int depth_override;
typedef struct {int x,y,z;} Vec;
typedef struct {int x,y,d;} Screen;
static Vec vec(int x,int y,int z) {return (Vec){x,y,z};}
static uint16_t pad_buttons(void) {
    if(pad1[0] || (pad1[1]!=0x41 && pad1[1]!=0x73 && pad1[1]!=0x53)) return 0;
    return (uint16_t)~((unsigned)pad1[2]|((unsigned)pad1[3]<<8));
}
static void *alloc_prim(int size) {
    if(cursor+size>PACKETS) return NULL;
    void *p=(uint8_t *)packet[db]+cursor;cursor+=size;return p;
}
static Screen project(Vec p) {
    int x=p.x-cam_x,z=p.z-cam_z;
    int s=view_sin,c=view_cos;
    int xx=(x*c-z*s)>>12,zz=(x*s+z*c)>>12;
    int yy=cam_target_y-p.y;
    int depth=cam_distance+(zz*view_pitch_cos+yy*view_pitch_sin)/1024;
    if(depth<64) return (Screen){0,0,-1};
    return (Screen){160+shake_x+xx*420/depth,view_center_y+shake_y+(yy*view_pitch_cos-zz*view_pitch_sin)*420/(1024*depth),depth};
}
static void face(Vec a,Vec b,Vec c,Vec d,int r,int g,int blue,int shade) {
    Screen v[4]={project(a),project(b),project(c),project(d)};
    for(int i=0;i<4;i++) if(v[i].d<0 || absi(v[i].x)>1000 || absi(v[i].y)>1000) return;
    int depth=(v[0].d+v[1].d+v[2].d+v[3].d)/4;
    POLY_F4 *p=alloc_prim(sizeof(POLY_F4));if(!p)return;
    setPolyF4(p);setRGB0(p,r*shade/256,g*shade/256,blue*shade/256);
    setXY4(p,v[0].x,v[0].y,v[1].x,v[1].y,v[2].x,v[2].y,v[3].x,v[3].y);
    addPrim(ot[db]+(depth_override?depth_override:clamp(depth/2,1,OT_LEN-1)),p);
}
static void edge(Vec a,Vec b,int r,int g,int blue) {
    Screen u=project(a),v=project(b);if(u.d<0||v.d<0||absi(u.x)>1000||absi(u.y)>1000||absi(v.x)>1000||absi(v.y)>1000)return;
    LINE_F2 *p=alloc_prim(sizeof(LINE_F2));if(!p)return;
    setLineF2(p);setRGB0(p,r,g,blue);setXY2(p,u.x,u.y,v.x,v.y);
    addPrim(ot[db]+(depth_override?depth_override:clamp((u.d+v.d)/4-1,1,OT_LEN-1)),p);
}
/* Vertex order for GPU quads is a triangle strip, not a perimeter loop. */
static void solid(Vec v[8],int r,int g,int b) {
    face(v[0],v[1],v[2],v[3],r,g,b,178);
    face(v[4],v[5],v[6],v[7],r,g,b,215);
    face(v[0],v[4],v[2],v[6],r,g,b,155);
    face(v[1],v[5],v[3],v[7],r,g,b,235);
    face(v[0],v[1],v[4],v[5],r,g,b,140);
    face(v[2],v[3],v[6],v[7],r,g,b,256);
}
static void box(int x,int y,int z,int wx,int hy,int dz,int r,int g,int b) {
    Vec v[8];for(int i=0;i<8;i++)v[i]=vec(x+((i&1)?wx:-wx),y+((i&2)?hy:-hy),z+((i&4)?dz:-dz));
    solid(v,r,g,b);
}
static Vec body_vertex(Fighter *f,Vec p) {
    /* Collapse the articulated model sideways on a knockout. */
    int oldy=p.y;
    p.x+=oldy*f->fall/20;p.y=oldy*(18-f->fall)/18+f->fall/2;
    p.y=p.y*roster[f->character].height/100+f->elevation;
    int cx=f->face_x,cz=f->face_z;if(!cx&&!cz)cx=1024;
    int xx=p.x*f->facing;
    return vec(f->x+(xx*cx-p.z*cz)/1024,p.y,f->z+(xx*cz+p.z*cx)/1024);
}
/* Anatomical lofts share projected vertices and use Gouraud lighting. */
typedef struct {int y,rx,rz,dx;} Ring;
static const int oct_cos[8]={1024,724,0,-724,-1024,-724,0,724};
static const int oct_sin[8]={0,724,1024,724,0,-724,-1024,-724};
static void shaded_quad(Fighter *f,Screen a,Screen b,Screen c,Screen d,
    int sa,int sb,int sc,int sd,int r,int g,int blue) {
    if(a.d<0||b.d<0||c.d<0||d.d<0)return;
    /* Mirroring the local model also reverses the projected winding. */
    int area=(b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x);
    int area2=(d.x-b.x)*(c.y-b.y)-(d.y-b.y)*(c.x-b.x);
    if(area*f->facing>=0 && area2*f->facing>=0)return;
    POLY_G4 *p=alloc_prim(sizeof(POLY_G4));if(!p)return;
    setPolyG4(p);
    setRGB0(p,r*sa/256,g*sa/256,blue*sa/256);
    setRGB1(p,r*sb/256,g*sb/256,blue*sb/256);
    setRGB2(p,r*sc/256,g*sc/256,blue*sc/256);
    setRGB3(p,r*sd/256,g*sd/256,blue*sd/256);
    setXY4(p,a.x,a.y,b.x,b.y,c.x,c.y,d.x,d.y);
    int depth=(a.d+b.d+c.d+d.d)/4;
    addPrim(ot[db]+clamp(depth/2,1,OT_LEN-1),p);
}
static int vertex_light(Fighter *f,int index,int slope) {
    int nx=oct_cos[index]*f->facing,nz=oct_sin[index];
    return clamp(198+nx*22/1024-nz*37/1024+slope,120,256);
}
static void loft(Fighter *f,int x,int y,int z,const Ring *rings,int count,int r,int g,int b) {
    Screen projected[10][8];int light[10][8];
    if(count>10)return;
    for(int j=0;j<count;j++)for(int i=0;i<8;i++) {
        Vec p=vec(x+rings[j].dx+oct_cos[i]*rings[j].rx/1024,
            y+rings[j].y,z+oct_sin[i]*rings[j].rz/1024);
        projected[j][i]=project(body_vertex(f,p));
        int slope=j?clamp((rings[j-1].rx-rings[j].rx)*3,-12,18):0;
        light[j][i]=vertex_light(f,i,slope);
    }
    for(int j=0;j<count-1;j++)for(int i=0;i<8;i++) {
        int k=(i+1)%8;
        shaded_quad(f,projected[j][i],projected[j][k],projected[j+1][i],projected[j+1][k],
            light[j][i],light[j][k],light[j+1][i],light[j+1][k],r,g,b);
    }
}
static void oval(Fighter *f,int x,int y,int z,int rx,int ry,int rz,int r,int g,int b) {
    Ring rings[5]={{-ry,0,0,0},{-ry*2/3,rx*3/4,rz*3/4,0},
        {0,rx,rz,0},{ry*2/3,rx*3/4,rz*3/4,0},{ry,0,0,0}};
    loft(f,x,y,z,rings,5,r,g,b);
}
/* Four elliptical cross-sections form a muscle belly and tapered joints. */
static void muscle(Fighter *f,Vec a,Vec b,int joint_a,int belly,int joint_b,int r,int g,int blue) {
    int dx=b.x-a.x,dy=b.y-a.y,dz=b.z-a.z;
    int xy=SquareRoot0(dx*dx+dy*dy),length=SquareRoot0(dx*dx+dy*dy+dz*dz);
    if(!xy)xy=1;
    if(!length)length=1;
    int ux=-dy*1024/xy,uy=dx*1024/xy;
    int vx=-dz*dx*1024/(xy*length),vy=-dz*dy*1024/(xy*length),vz=xy*1024/length;
    const int t[4]={0,88,184,256};int radius[4]={joint_a,belly,(belly+joint_b)/2,joint_b};
    Screen projected[4][8];int light[8];
    for(int i=0;i<8;i++)light[i]=vertex_light(f,i,5);
    for(int j=0;j<4;j++)for(int i=0;i<8;i++) {
        int u=oct_cos[i]*radius[j]/1024,v=oct_sin[i]*radius[j]/1024;
        Vec p=vec(a.x+dx*t[j]/256+(ux*u+vx*v)/1024,
            a.y+dy*t[j]/256+(uy*u+vy*v)/1024,a.z+dz*t[j]/256+vz*v/1024);
        projected[j][i]=project(body_vertex(f,p));
    }
    /* Ring orientation follows a->b; anatomy is submitted from low to high. */
    for(int j=0;j<3;j++)for(int i=0;i<8;i++) {
        int k=(i+1)%8;
        /* Muscle rings have the opposite orientation to horizontal lofts. */
        shaded_quad(f,projected[j][k],projected[j][i],projected[j+1][k],projected[j+1][i],
            light[k],light[i],light[k],light[i],r,g,blue);
    }
}
static void skin_line(Fighter *f,Vec a,Vec b,int r,int g,int blue) {
    edge(body_vertex(f,a),body_vertex(f,b),r,g,blue);
}
static void detailed_head(Fighter *f,int lean,int bob,int sr,int sg,int sb) {
    /* Chin -> jaw -> cheekbones -> brow -> rounded skull. */
    const Ring skull[7]={{0,6,5,2},{3,8,7,1},{8,10,9,0},
        {13,10,9,0},{18,9,8,-1},{21,6,5,-1},{23,0,0,-1}};
    loft(f,lean,152+bob,0,skull,7,sr,sg,sb);
    const Ring hair[4]={{0,9,9,-1},{4,8,8,-1},{7,5,5,-1},{9,0,0,-1}};
    const Character *ch=&roster[f->character];
    int hr=ch->hair_r,hg=ch->hair_g,hb=ch->hair_b;
    if(ch->hair_style==3) {
        const Ring mohawk[4]={{0,9,3,0},{5,9,3,0},{11,5,2,-1},{13,0,0,-2}};
        loft(f,lean,168+bob,0,mohawk,4,hr,hg,hb);
    } else if(ch->hair_style==2) {
        const Ring buzz[3]={{0,9,8,-1},{5,6,5,-1},{7,0,0,-1}};
        loft(f,lean,169+bob,0,buzz,3,hr,hg,hb);
    } else loft(f,lean,168+bob,0,hair,4,hr,hg,hb);
    if(ch->beard) {
        const Ring beard[3]={{0,7,6,2},{3,9,8,1},{6,9,8,1}};
        loft(f,lean,149+bob,0,beard,3,hr,hg,hb);
    }
    for(int side=-1;side<=1;side+=2) {
        oval(f,lean-1,162+bob,side*9,3,5,2,sr-12,sg-12,sb-10);
        /* Eye sockets, brows and small eye highlights follow the cheek plane. */
        oval(f,lean+8,166+bob,side*6,3,2,3,sr-40,sg-37,sb-30);
        skin_line(f,vec(lean+10,168+bob,side*3),vec(lean+8,168+bob,side*8),hr,hg,hb);
        skin_line(f,vec(lean+11,166+bob,side*4),vec(lean+9,166+bob,side*7),55,42,36);
        skin_line(f,vec(lean+11,167+bob,side*4),vec(lean+10,167+bob,side*5),208,197,174);
    }
    /* A wedge gives the nose a bridge and tip instead of a box. */
    Vec bridge=vec(lean+11,167+bob,0),tip=vec(lean+16,161+bob,0);
    Vec nl=vec(lean+11,160+bob,-3),nr=vec(lean+11,160+bob,3);
    face(body_vertex(f,bridge),body_vertex(f,nl),body_vertex(f,tip),body_vertex(f,tip),sr,sg,sb,245);
    face(body_vertex(f,bridge),body_vertex(f,tip),body_vertex(f,nr),body_vertex(f,nr),sr,sg,sb,190);
    skin_line(f,vec(lean+12,157+bob,-4),vec(lean+12,157+bob,4),119,72,59);
    skin_line(f,vec(lean+11,155+bob,-3),vec(lean+11,155+bob,3),sr-18,sg-18,sb-12);
}
static void bare_foot(Fighter *f,Vec foot,int sr,int sg,int sb) {
    Ring shape[4]={{-4,7,4,-2},{-1,13,6,4},{3,12,6,4},{7,5,4,-2}};
    loft(f,foot.x,foot.y,foot.z,shape,4,sr,sg,sb);
    skin_line(f,vec(foot.x+15,foot.y+1,foot.z-4),vec(foot.x+15,foot.y+1,foot.z+4),sr-45,sg-35,sb-25);
}
static void fighter_draw(Fighter *f,int red,int green,int blue) {
    int pulse=0;
    if(f->attack) {
        int apex=contact[f->attack];
        pulse=f->age<=apex?f->age*256/apex:(duration[f->attack]-f->age)*256/(duration[f->attack]-apex);
        /* Ease joint motion in and out instead of a mechanical linear swing. */
        pulse=pulse*pulse*(768-2*pulse)/65536;
    }
    int step=isin(f->walk)*7/4096,bob=isin(ticks*62)/2048;
    int lean=3+(f->attack==2?pulse*10/256:0);
    const Character *ch=&roster[f->character];
    int sr=ch->skin_r,sg=ch->skin_g,sb=ch->skin_b;
    if(f->flash){sr=249;sg=221;sb=185;}
    int broad=ch->bulk;
    /* Adult proportions: long legs, narrow waist, broad upper chest, smaller head. */
    Vec hip[2]={vec(-2,85,-12),vec(3,85,12)};
    Vec knee[2]={vec(-12+step,46,-16),vec(15-step,47,15)};
    Vec foot[2]={vec(-21+step,5,-19),vec(28-step,5,18)};
    if(f->attack>=3) {
        knee[1]=vec(22+pulse*22/256,47+pulse*28/256,15);
        foot[1]=vec(28+pulse*(f->attack==4?69:57)/256,5+pulse*85/256,18);
    }
    for(int side=0;side<2;side++) {
        Vec upper=vec((hip[side].x*3+knee[side].x)/4,75,hip[side].z);
        muscle(f,knee[side],upper,6,10+broad,9,sr,sg,sb);
        muscle(f,foot[side],knee[side],4,7+broad,6,sr-6,sg-5,sb-4);
        oval(f,knee[side].x,knee[side].y,knee[side].z,6,6,6,sr-7,sg-7,sb-5);
        bare_foot(f,foot[side],sr,sg,sb);
        /* Individual short legs avoid a solid rectangular pelvis. */
        Ring short_leg[3]={{0,11,10,0},{7,12+broad,10,0},{17,12,10,0}};
        loft(f,hip[side].x,70,hip[side].z,short_leg,3,red,green,blue);
        Ring hem[2]={{0,11,10,0},{2,11,10,0}};
        loft(f,hip[side].x,70,hip[side].z,hem,2,red*3/4,green*3/4,blue*3/4);
    }
    Ring shorts[3]={{0,12,18,0},{8,13,19,0},{15,10,16,0}};
    loft(f,0,79,0,shorts,3,red,green,blue);
    Ring belt[2]={{0,11,17,0},{3,11,17,0}};
    loft(f,0,91,0,belt,2,24,27,32);
    Ring torso[7]={{0,9,15,0},{9,10,16,0},{20,12,20,1},
        {31,13,24+broad,2},{40,12,25+broad,2},{45,9,19,2},{50,5,7,2}};
    loft(f,lean/2,94+bob,0,torso,7,sr,sg,sb);
    /* Pectoral masses and abdominal plane remain subtle under the lighting. */
    for(int side=-1;side<=1;side+=2) {
        oval(f,lean/2+12,127+bob,side*10,4,8,10,sr+4,sg+3,sb+2);
        skin_line(f,vec(lean/2+14,120+bob,side*3),vec(lean/2+12,120+bob,side*17),sr-25,sg-23,sb-18);
    }
    skin_line(f,vec(lean/2+12,100+bob,0),vec(lean/2+15,119+bob,0),sr-18,sg-17,sb-13);
    for(int y=105;y<=115;y+=5)skin_line(f,vec(lean/2+12,y+bob,-6),vec(lean/2+12,y+bob,6),sr-13,sg-12,sb-10);
    Ring neck[3]={{0,6,7,0},{7,5,6,0},{11,6,6,1}};
    loft(f,lean+1,139+bob,0,neck,3,sr-10,sg-8,sb-6);
    detailed_head(f,lean+2,bob,sr,sg,sb);
    for(int side=-1;side<=1;side+=2) {
        Vec shoulder=vec(lean/2+1,133+bob,side*(25+broad));
        Vec elbow=vec(10,108+bob,side*28),hand=vec(28,133+bob,side*18);
        if(f->guard==1){elbow=vec(16,126,side*25);hand=vec(28,157,side*12);}
        if(f->guard==2){elbow=vec(15,105,side*25);hand=vec(27,120,side*13);}
        if(f->attack && f->attack<3 && side==(f->attack==1?-1:1)) {
            elbow.x+=pulse*32/256;elbow.y+=pulse*20/256;
            hand.x+=pulse*(f->attack==2?55:47)/256;hand.y+=pulse*5/256;
        }
        oval(f,shoulder.x,shoulder.y,shoulder.z,8,9,8,sr,sg,sb);
        muscle(f,elbow,shoulder,4,7+broad,7,sr,sg,sb);
        muscle(f,elbow,hand,4,6,4,sr-3,sg-2,sb-2);
        oval(f,hand.x,hand.y,hand.z,7,6,6,29,30,33);
        oval(f,hand.x+5,hand.y+1,hand.z,4,4,5,sr,sg,sb);
        muscle(f,vec(hand.x-4,hand.y-3,hand.z),vec(hand.x-4,hand.y+2,hand.z),5,5,5,red,green,blue);
        skin_line(f,vec(hand.x+8,hand.y-1,hand.z-3),vec(hand.x+8,hand.y-1,hand.z+3),sr-45,sg-35,sb-25);
    }
}

static const int ring_x[8]={-240,240,300,300,240,-240,-300,-300};
static const int ring_z[8]={-190,-190,-130,130,190,190,130,-130};
static void world_gradient(Vec a,Vec b,Vec c,Vec d,int r,int g,int blue,int sa,int sb,int sc,int sd) {
    Screen v[4]={project(a),project(b),project(c),project(d)};
    for(int i=0;i<4;i++)if(v[i].d<0 || absi(v[i].x)>1000 || absi(v[i].y)>1000)return;
    POLY_G4 *p=alloc_prim(sizeof(POLY_G4));if(!p)return;
    setPolyG4(p);setXY4(p,v[0].x,v[0].y,v[1].x,v[1].y,v[2].x,v[2].y,v[3].x,v[3].y);
    setRGB0(p,r*sa/256,g*sa/256,blue*sa/256);setRGB1(p,r*sb/256,g*sb/256,blue*sb/256);
    setRGB2(p,r*sc/256,g*sc/256,blue*sc/256);setRGB3(p,r*sd/256,g*sd/256,blue*sd/256);
    int dep=(v[0].d+v[1].d+v[2].d+v[3].d)/4;
    addPrim(ot[db]+(depth_override?depth_override:clamp(dep/2,1,OT_LEN-5)),p);
}
static int mat_light(int x,int z) {return clamp(256-(absi(x)+absi(z))*2/7,140,256);}
static void world_text(int x,int y,int z,const char *text,int size,int upright,int r,int g,int b);
static Vec on_fence(int i,int t,int y) {
    int j=(i+1)%8;
    return vec(ring_x[i]+(ring_x[j]-ring_x[i])*t/256,y,ring_z[i]+(ring_z[j]-ring_z[i])*t/256);
}
static void stadium(void) {
    /* Seats and audience are inexpensive lit billboards, well beyond the cage. */
    for(int row=0;row<3;row++) {
        int z=300+row*85,y=12+row*30;
        world_gradient(vec(-520,y,z),vec(520,y,z),vec(-520,y+27,z+55),vec(520,y+27,z+55),29,39,57,170,170,256,256);
        for(int i=0;i<18;i++) {
            int x=-490+i*58+(row&1)*18;
            if(absi(x)<75)continue;
            int tone=(i*13+row*21)%5;
            world_gradient(vec(x-12,y+5,z),vec(x+12,y+5,z),vec(x-9,y+32,z),vec(x+9,y+32,z),42+tone*13,45+tone*5,67+tone*4,140,140,230,230);
            face(vec(x-5,y+32,z-1),vec(x+5,y+32,z-1),vec(x-5,y+43,z-1),vec(x+5,y+43,z-1),103+tone*16,76+tone*12,61+tone*9,210);
            if((ticks/5+i*17+row*9)%101==0)box(x+9,y+31,z-3,2,2,1,220,236,255);
        }
    }
    /* Entrance tunnel, lit truss and an aisle leading to the fighting floor. */
    box(-73,100,470,8,100,12,28,35,49);box(73,100,470,8,100,12,28,35,49);
    box(0,200,470,80,7,12,35,42,56);
    box(-66,100,452,2,92,2,42,164,210);box(66,100,452,2,92,2,227,61,70);
    world_gradient(vec(-60,0,200),vec(60,0,200),vec(-60,15,475),vec(60,15,475),48,55,69,256,256,125,125);
    edge(vec(-60,2,200),vec(-60,16,475),45,148,191);edge(vec(60,2,200),vec(60,16,475),181,49,63);
    for(int i=-2;i<=2;i++) {
        box(i*150,245,220,15,3,10,184,204,228);
        edge(vec(i*150-40,238,220),vec(i*150+40,252,220),70,84,104);
    }
    for(int i=-4;i<=4;i++) {
        int x=i*70;
        world_gradient(vec(x-32,25,223),vec(x+32,25,223),vec(x-32,48,223),vec(x+32,48,223),i%2?180:31,i%2?42:115,i%2?57:151,120,120,256,256);
    }
    world_text(-98,70,455,"FIGHT NIGHT",3,1,179,205,225);
}
static void arena(void) {
    stadium();
    /* Subdivided canvas with a bright center and darker perimeter. */
    const int rows[7]={-190,-130,-65,0,65,130,190};
    depth_override=1022;
    for(int row=0;row<6;row++) {
        int z0=rows[row],z1=rows[row+1];
        int w0=300-(absi(z0)>130?absi(z0)-130:0);
        int w1=300-(absi(z1)>130?absi(z1)-130:0);
        for(int col=0;col<10;col++) {
            int x0=-w0+2*w0*col/10,x1=-w0+2*w0*(col+1)/10;
            int x2=-w1+2*w1*col/10,x3=-w1+2*w1*(col+1)/10;
            world_gradient(vec(x0,0,z0),vec(x1,0,z0),vec(x2,0,z1),vec(x3,0,z1),188,196,193,
                mat_light(x0,z0),mat_light(x1,z0),mat_light(x2,z1),mat_light(x3,z1));
        }
    }
    depth_override=1021;
    world_text(-106,3,30,"UNLIMITED",4,0,47,56,66);
    world_text(-71,3,-14,"CHAMPS",4,0,149,37,45);
    for(int i=0;i<8;i++) {
        int j=(i+1)%8;
        edge(vec(ring_x[i],3,ring_z[i]),vec(ring_x[j],3,ring_z[j]),190,43,53);
        edge(vec(ring_x[i]*9/10,3,ring_z[i]*9/10),vec(ring_x[j]*9/10,3,ring_z[j]*9/10),77,92,99);
    }
    depth_override=0;
    /* Hide the camera-side cage panels so mesh cannot obscure the fighters. */
    for(int i=0;i<8;i++) {
        int j=(i+1)%8;
        int mx=(ring_x[i]+ring_x[j])/2-cam_x,mz=(ring_z[i]+ring_z[j])/2-cam_z;
        int view_z=(mx*view_sin+mz*view_cos)>>12;
        if(view_z<-50)continue;
        box(ring_x[i],91,ring_z[i],5,91,5,28,37,47);
        box(ring_x[i],132,ring_z[i],8,42,8,(i&1)?140:34,(i&1)?41:84,(i&1)?49:117);
        edge(on_fence(i,0,182),on_fence(i,256,182),103,119,131);
        edge(on_fence(i,0,10),on_fence(i,256,10),64,75,84);
        for(int segment=0;segment<8;segment++)for(int row=0;row<4;row++) {
            if(i==4 && (segment==3 || segment==4))continue;
            int t0=segment*32,t1=t0+32,mid=t0+16,y=12+row*40;
            edge(on_fence(i,t0,y),on_fence(i,mid,y+20),48,60,71);
            edge(on_fence(i,mid,y+20),on_fence(i,t1,y),48,60,71);
            edge(on_fence(i,t0,y+40),on_fence(i,mid,y+20),48,60,71);
            edge(on_fence(i,mid,y+20),on_fence(i,t1,y+40),48,60,71);
        }
    }
    depth_override=1020;
    Fighter *fighters[2]={&player,&cpu};
    for(int f=0;f<2;f++)for(int i=0;i<8;i++) {
        Fighter *who=fighters[f];int j=(i+1)%8;
        Vec center=vec(who->x,2,who->z);
        Vec a=vec(who->x+oct_cos[i]*24/1024,2,who->z+oct_sin[i]*17/1024);
        Vec b=vec(who->x+oct_cos[j]*24/1024,2,who->z+oct_sin[j]*17/1024);
        face(center,a,b,b,86,98,103,210);
    }
    depth_override=0;
}

static void hud_rect(int x,int y,int w,int h,int r,int g,int b) {
    if(w<=0)return;
    POLY_F4 *p=alloc_prim(sizeof(POLY_F4));if(!p)return;
    setPolyF4(p);setRGB0(p,r,g,b);setXY4(p,x,y,x+w,y,x,y+h,x+w,y+h);
    addPrim(hud_tail,p);hud_tail=p;
}
/* Compact display lettering; horizontal runs reduce GPU packet usage. */
static const uint8_t alphabet[36][7]={
 {14,17,17,31,17,17,17},{30,17,17,30,17,17,30},{14,17,16,16,16,17,14},
 {30,17,17,17,17,17,30},{31,16,16,30,16,16,31},{31,16,16,30,16,16,16},
 {14,17,16,23,17,17,15},{17,17,17,31,17,17,17},{31,4,4,4,4,4,31},
 {7,2,2,2,18,18,12},{17,18,20,24,20,18,17},{16,16,16,16,16,16,31},
 {17,27,21,21,17,17,17},{17,25,21,19,17,17,17},{14,17,17,17,17,17,14},
 {30,17,17,30,16,16,16},{14,17,17,17,21,18,13},{30,17,17,30,20,18,17},
 {15,16,16,14,1,1,30},{31,4,4,4,4,4,4},{17,17,17,17,17,17,14},
 {17,17,17,17,17,10,4},{17,17,17,21,21,21,10},{17,17,10,4,10,17,17},
 {17,17,10,4,4,4,4},{31,1,2,4,8,16,31},
 {14,17,19,21,25,17,14},{4,12,4,4,4,4,14},{14,17,1,2,4,8,31},
 {30,1,1,14,1,1,30},{2,6,10,18,31,2,2},{31,16,16,30,1,1,30},
 {14,16,16,30,17,17,14},{31,1,2,4,8,8,8},{14,17,17,14,17,17,14},
 {14,17,17,15,1,1,14}
};
static void lettering(int x,int y,const char *text,int scale,int r,int g,int b) {
    for(;*text;text++,x+=6*scale) {
        int glyph=-1;char c=*text;
        if(c>='A'&&c<='Z')glyph=c-'A';
        else if(c>='0'&&c<='9')glyph=26+c-'0';
        if(glyph<0) {
            if(c=='.')hud_rect(x+2*scale,y+6*scale,scale,scale,r,g,b);
            if(c=='-')hud_rect(x,y+3*scale,5*scale,scale,r,g,b);
            continue;
        }
        for(int row=0;row<7;row++) {
            int bits=alphabet[glyph][row];
            for(int col=0;col<5;) {
                if(!(bits&(16>>col))){col++;continue;}
                int start=col;while(col<5&&(bits&(16>>col)))col++;
                hud_rect(x+start*scale,y+row*scale,(col-start)*scale,scale,r,g,b);
            }
        }
    }
}
static void center_text(int y,const char *text,int scale,int r,int g,int b) {
    int n=0;while(text[n])n++;
    lettering((W-(n*6-1)*scale)/2,y,text,scale,r,g,b);
}
static void menu_panel(int x,int y,int w,int h,int selected,int r,int g,int b) {
    hud_rect(x,y,w,h,selected?r:43,selected?g:52,selected?b:68);
    hud_rect(x+2,y+2,w-4,h-4,selected?31:18,selected?40:24,selected?51:35);
    if(selected)hud_rect(x+4,y+4,3,h-8,r,g,b);
}
static void world_text(int x,int y,int z,const char *text,int size,int upright,int r,int g,int b) {
    for(;*text;text++,x+=6*size) {
        if(*text<'A'||*text>'Z')continue;
        for(int row=0;row<7;row++)for(int col=0;col<5;) {
            int bits=alphabet[*text-'A'][row];if(!(bits&(16>>col))){col++;continue;}
            int start=col;while(col<5&&(bits&(16>>col)))col++;
            int xx=x+start*size,xx1=x+col*size;
            Vec a=vec(xx,y,z),bb=vec(xx1,y,z),c=a,d=bb;
            if(upright){a.y=bb.y=y-row*size;c.y=d.y=y-(row+1)*size;}
            else {a.z=bb.z=z-row*size;c.z=d.z=z-(row+1)*size;}
            face(a,bb,c,d,r,g,b,256);
        }
    }
}
static void screen_glow(int x0,int y0,int x1,int y1,int x2,int y2,int x3,int y3,
    int r,int g,int b,int top,int bottom) {
    POLY_G4 *p=alloc_prim(sizeof(POLY_G4));if(!p)return;
    setPolyG4(p);setXY4(p,x0,y0,x1,y1,x2,y2,x3,y3);
    setRGB0(p,r*top/256,g*top/256,b*top/256);setRGB1(p,r*top/256,g*top/256,b*top/256);
    setRGB2(p,r*bottom/256,g*bottom/256,b*bottom/256);setRGB3(p,r*bottom/256,g*bottom/256,b*bottom/256);
    addPrim(background_tail,p);background_tail=p;
}
static void backdrop(void) {
    background_tail=ot[db]+1023;
    screen_glow(0,0,320,0,0,240,320,240,20,30,46,90,256);
    screen_glow(-75,0,-30,0,100,240,230,240,35,70,95,190,70);
    screen_glow(300,0,335,0,70,240,200,240,91,31,43,155,55);
    for(int i=0;i<18;i++) {
        int x=7+i*18;
        screen_glow(x,57,x+8,57,x-1,60,x+9,60,152,187,207,110,220);
    }
}
static void hud_quad(int x0,int y0,int x1,int y1,int x2,int y2,int x3,int y3,int r,int g,int b) {
    POLY_F4 *p=alloc_prim(sizeof(POLY_F4));if(!p)return;
    setPolyF4(p);setRGB0(p,r,g,b);setXY4(p,x0,y0,x1,y1,x2,y2,x3,y3);addPrim(hud_tail,p);hud_tail=p;
}
static void set_view(int yaw,int pitch,int target_y) {
    cam_yaw=yaw;cam_pitch=pitch;cam_target_y=target_y;
    view_sin=isin(yaw);view_cos=icos(yaw);
    view_pitch_sin=pitch;view_pitch_cos=SquareRoot0(1024*1024-pitch*pitch);
    view_center_y=124;shake_x=shake_y=0;
}
static void broadcast_camera(void) {
    int tx=(player.x+cpu.x)/2,tz=(player.z+cpu.z)/2;
    int dx=cpu.x-player.x,dz=cpu.z-player.z;
    if(dx<0)dz=-dz;
    int base=camera_side==0?-70:(camera_side==1?-285:95);
    int yaw=base-clamp(dz*430/(absi(dx)+70),-270,270);
    int target_pitch=camera_side==0?250:(camera_side==1?420:150);
    int span=absi(dx)+absi(dz)/2;
    int target=clamp(620+span*3/5,660,990);
    if(camera_side==1)target+=60;
    cam_x+=(tx-cam_x)/7;cam_z+=(tz-cam_z)/7;
    cam_distance+=(target-cam_distance)/10;
    cam_yaw+=(yaw-cam_yaw)/10;cam_pitch+=(target_pitch-cam_pitch)/10;
    cam_target_y=96;view_center_y=124;
    view_sin=isin(cam_yaw);view_cos=icos(cam_yaw);
    view_pitch_sin=cam_pitch;view_pitch_cos=SquareRoot0(1024*1024-cam_pitch*cam_pitch);
    shake_x=shake_y=0;
    if(impact_shake && !paused) {
        shake_x=(ticks&1)?impact_shake:-impact_shake;shake_y=impact_shake/2;
        impact_shake--;
    }
}
static int text_width(const char *s) {int n=0;while(s[n])n++;return n*6-1;}
static void fight_hud(void) {
    const Character *pc=&roster[player.character],*cc=&roster[cpu.character];
    hud_rect(0,0,320,46,9,13,20);
    hud_rect(8,3,129,2,pc->short_r,pc->short_g,pc->short_b);
    hud_rect(183,3,129,2,cc->short_r,cc->short_g,cc->short_b);
    lettering(8,10,pc->name,1,231,235,235);
    lettering(312-text_width(cc->name),10,cc->name,1,231,235,235);
    menu_panel(143,4,34,33,1,191,48,61);
    char clock[4];snprintf(clock,sizeof(clock),"%02d",clamp(round_frames/60,0,99));
    lettering(149,12,clock,2,238,230,220);
    center_text(38,"ROUND 1",1,148,167,181);
    Fighter *f[2]={&player,&cpu};
    for(int i=0;i<2;i++) {
        int x=i?183:8,hp=f[i]->hp;
        hud_rect(x,23,129,8,39,44,50);
        hud_rect(x+1,24,hp*127/100,6,clamp(235-hp*2,40,235),50+hp*14/10,78);
        for(int t=1;t<10;t++)hud_rect(x+t*13,24,1,6,32,42,48);
        hud_rect(x,34,129,3,39,44,50);hud_rect(x,34,f[i]->stamina*129/100,3,205,174,99);
    }
    hud_rect(0,207,320,33,9,13,20);
    hud_rect(8,205,304,1,56,72,89);
}
static void render(void) {
    broadcast_camera();db^=1;cursor=0;ClearOTagR(ot[db],OT_LEN);hud_tail=ot[db];
    backdrop();arena();
    const Character *pc=&roster[player.character],*cc=&roster[cpu.character];
    fighter_draw(&player,pc->short_r,pc->short_g,pc->short_b);
    fighter_draw(&cpu,cc->short_r,cc->short_g,cc->short_b);
    fight_hud();
    if(paused)FntPrint(font_bottom,"PAUSED - START RESUME  O ROSTER\nSELECT: CHANGE CAMERA");
    else if(finished) {
        const char *winner=player.hp==cpu.hp?"DRAW":(player.hp>cpu.hp?"YOU WIN":"CPU WINS");
        FntPrint(font_bottom,"%s - %s\nX REMATCH  O FIGHTER SELECT",winner,(!player.hp||!cpu.hp)?"KNOCKOUT":"TIME UP");
    } else FntPrint(font_bottom,"[] JAB /\\ POWER X BODY O HIGH KICK\nL1 HIGH GUARD L2 BODY GUARD START PAUSE");
    DrawSync(0);VSync(0);PutDispEnv(&disp[db]);PutDrawEnv(&draw[db]);DrawOTag(ot[db]+OT_LEN-1);
    FntFlush(font_top);FntFlush(font_bottom);
}

static void render_menu(void) {
    db^=1;cursor=0;ClearOTagR(ot[db],OT_LEN);hud_tail=ot[db];depth_override=0;
    backdrop();
    if(app_screen==SCREEN_CHARACTER) {
        /* A single live preview keeps selection within the PS1 polygon budget. */
        cam_x=0;cam_z=0;cam_distance=700;set_view(-180,220,96);
        Fighter preview={.x=135,.hp=100,.stamina=100,.facing=-1,.character=chosen_character,.face_x=1024};
        const Character *ch=&roster[chosen_character];
        depth_override=1023;
        face(vec(80,0,-60),vec(190,0,-60),vec(80,0,60),vec(190,0,60),36,49,61,256);
        depth_override=0;
        fighter_draw(&preview,ch->short_r,ch->short_g,ch->short_b);
        hud_rect(0,0,320,47,12,16,29);
        center_text(13,"SELECT YOUR FIGHTER",2,230,232,230);
        center_text(36,"FOUR FIGHTERS - BETA ROSTER",1,134,163,181);
        for(int i=0;i<4;i++) {
            const Character *card=&roster[i];
            menu_panel(9,52+i*32,166,29,i==chosen_character,card->short_r,card->short_g,card->short_b);
            FntPrint(font_menu,"%s\n%.18s\n\n\n",card->name,card->nickname);
        }
        lettering(184,191,ch->name,1,238,215,168);
        FntPrint(font_bottom,"D-PAD CHOOSE   X/START FIGHT\nO BACK   4 PLAYABLE BETA FIGHTERS");
    } else if(app_screen==SCREEN_MODE) {
        hud_quad(0,0,238,0,0,208,162,208,11,17,27);
        lettering(20,22,"SELECT MODE",3,235,237,234);
        lettering(21,51,"BETA 0.5 - THE MAIN EVENT",1,132,156,175);
        menu_panel(24,83,272,85,1,200,43,57);
        lettering(39,94,"01",1,203,67,77);
        lettering(39,112,"FIGHT NOW",3,244,239,232);
        lettering(40,148,"EXHIBITION - PLAYER VS CPU",1,164,184,197);
        FntPrint(font_bottom,"X/START SELECT FIGHT NOW\nO BACK TO TITLE");
    } else {
        cam_x=0;cam_z=0;cam_distance=560;set_view(-90,210,100);
        arena();
        Fighter hero={.x=132,.hp=100,.stamina=100,.facing=-1,.character=0,.face_x=1024};
        int phase=menu_ticks%140;
        if(phase<28){hero.attack=2;hero.age=phase;}else hero.guard=1;
        fighter_draw(&hero,roster[0].short_r,roster[0].short_g,roster[0].short_b);
        hud_quad(0,0,202,0,0,208,171,208,8,12,19);
        hud_quad(194,0,201,0,163,208,170,208,123,32,46);
        hud_rect(18,19,23,3,205,46,60);
        lettering(48,17,"MAIN EVENT BETA",1,157,180,196);
        lettering(18,53,"UNLIMITED",2,240,240,233);
        lettering(18,79,"FIGHTING",3,240,240,233);
        lettering(18,108,"CHAMPS",3,219,54,67);
        lettering(19,140,"FOUR FIGHTERS. ONE OCTAGON.",1,139,161,178);
        int glow=170+isin(menu_ticks*35)*45/4096;
        menu_panel(18,162,153,31,1,glow,45,57);
        lettering(29,171,"PRESS START",2,244,239,232);
        hud_rect(0,207,320,33,9,13,20);hud_rect(18,207,284,1,53,72,89);
        FntPrint(font_bottom,"BETA 0.5     UNLIMITED FIGHTING CHAMPS\n      FOUR-FIGHTER SHOWCASE");
    }
    DrawSync(0);VSync(0);PutDispEnv(&disp[db]);PutDrawEnv(&draw[db]);
    DrawOTag(ot[db]+OT_LEN-1);FntFlush(font_menu);FntFlush(font_bottom);
}

/* Original music is uploaded once to SPU RAM, then played only during walkouts. */
extern const uint8_t walkout_music[];
static int music_phase=-1;
static void audio_init(void) {
    SpuInit();SpuSetCommonMasterVolume(0x2400,0x2400);
    SpuSetTransferMode(SPU_TRANSFER_BY_DMA);SpuSetTransferStartAddr(0x1010);
    SpuWrite((const uint32_t *)walkout_music,WALKOUT_MUSIC_BYTES);
    SpuIsTransferCompleted(SPU_TRANSFER_WAIT);
}
static void audio_tick(void) {
    if(app_screen!=SCREEN_WALKOUT) {
        if(music_phase>=0){(void)SpuSetKey(0,1);music_phase=-1;}
        return;
    }
    if(music_phase!=walkout_phase) {
        (void)SpuSetKey(0,1);
        SPU_CH_FREQ(0)=getSPUSampleRate(WALKOUT_MUSIC_RATE);
        SPU_CH_ADDR(0)=getSPUAddr(0x1010);SPU_CH_LOOP_ADDR(0)=getSPUAddr(0x1010);
        SPU_CH_ADSR1(0)=0x00ff;SPU_CH_ADSR2(0)=0x0000;
        (void)SpuSetKey(1,1);music_phase=walkout_phase;
    }
    int volume=0x2600;
    if(walkout_frame<12)volume=volume*(walkout_frame+1)/12;
    if(walkout_frame>WALKOUT_FRAMES-40)volume=volume*(WALKOUT_FRAMES-walkout_frame)/40;
    SpuSetVoiceVolume(0,volume,volume);
}
static void render_walkout(void) {
    int time=clamp(walkout_frame,0,WALKOUT_FRAMES);
    Fighter actor=walkout_phase?cpu:player;
    actor.z=420-time*270/WALKOUT_FRAMES;
    actor.x=time<255?0:(walkout_phase?1:-1)*(time-255)*90/45;
    actor.elevation=actor.z>200?(actor.z-200)*15/275:0;
    actor.walk=time*165;actor.facing=1;actor.face_x=0;actor.face_z=-1024;
    actor.guard=0;actor.attack=0;
    cam_x=actor.x+18;cam_z=actor.z;cam_distance=560;
    set_view(75+isin(time*4)*65/4096,190,103);
    db^=1;cursor=0;ClearOTagR(ot[db],OT_LEN);hud_tail=ot[db];backdrop();arena();
    const Character *ch=&roster[actor.character];
    fighter_draw(&actor,ch->short_r,ch->short_g,ch->short_b);
    hud_rect(0,0,320,40,9,13,20);
    lettering(17,10,"THE WALKOUT",2,235,237,234);
    lettering(18,29,walkout_phase?"CPU ENTRANCE - 02 OF 02":"PLAYER ENTRANCE - 01 OF 02",1,145,171,190);
    menu_panel(15,173,290,32,1,ch->short_r,ch->short_g,ch->short_b);
    lettering(26,183,ch->name,2,239,234,226);
    hud_rect(15,207,time*290/WALKOUT_FRAMES,2,196,53,66);
    FntPrint(font_bottom,"START SKIP TO NEXT ENTRANCE/FIGHT\nORIGINAL MUSIC: MAIN EVENT PULSE");
    DrawSync(0);VSync(0);PutDispEnv(&disp[db]);PutDrawEnv(&draw[db]);DrawOTag(ot[db]+OT_LEN-1);
    FntFlush(font_bottom);
}

int main(void) {
    ResetGraph(0);InitGeom();SetVideoMode(MODE_NTSC);
    SetDefDispEnv(&disp[0],0,0,W,H);SetDefDrawEnv(&draw[0],0,H,W,H);
    SetDefDispEnv(&disp[1],0,H,W,H);SetDefDrawEnv(&draw[1],0,0,W,H);
    draw[0].isbg=draw[1].isbg=1;draw[0].dtd=draw[1].dtd=1;setRGB0(&draw[0],12,16,29);setRGB0(&draw[1],12,16,29);
    FntLoad(960,0);font_top=FntOpen(8,6,304,24,0,100);font_bottom=FntOpen(8,211,304,25,0,110);
    font_menu=FntOpen(18,56,288,144,0,400);
    InitPAD(pad1,sizeof(pad1),pad2,sizeof(pad2));StartPAD();ChangeClearPAD(0);
    audio_init();SetDispMask(1);reset_match();
    while(1) {
        uint16_t p=pad_buttons();
        if(app_screen==SCREEN_FIGHT && (p&PAD_SELECT) && !(menu_previous&PAD_SELECT))camera_side=(camera_side+1)%3;
        app_tick(p);audio_tick();
        if(app_screen==SCREEN_FIGHT)render();
        else if(app_screen==SCREEN_WALKOUT)render_walkout();
        else render_menu();
    }
}
#endif
