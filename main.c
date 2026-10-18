/* Unlimited Fighting Champs 0.3 - original polygonal PS1 combat prototype.
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
    int attack,age,walk,fall;
} Fighter;
static Fighter player,cpu;
static int round_frames,finished,restart_delay,paused,ticks;
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
    player=(Fighter){.x=-100,.z=0,.hp=100,.stamina=100,.facing=1};
    cpu=(Fighter){.x=100,.z=0,.hp=100,.stamina=100,.facing=-1};
    round_frames=60*90;finished=0;restart_delay=0;paused=0;
}
static void boundary(Fighter *f) {
    f->x=clamp(f->x,-230,230); f->z=clamp(f->z,-145,145);
    if(absi(f->x)+absi(f->z)>320) {
        int limit=320-absi(f->z);
        f->x=clamp(f->x,-limit,limit);
    }
}
static void start_attack(Fighter *f,int type) {
    if(f->attack||f->guard||f->stamina<power[type]/2||finished) return;
    f->attack=type;f->age=0;f->stamina-=power[type]/2;
}
static void animate_and_hit(Fighter *a,Fighter *b) {
    if(a->flash) a->flash--;
    if((ticks&3)==0 && a->stamina<100) a->stamina++;
    if(!a->attack) return;
    int type=a->attack;
    a->age++;
    if(a->age==contact[type] && absi(a->x-b->x)<reach[type]
        && absi(a->z-b->z)<36 && (b->x-a->x)*a->facing>0) {
        b->hp=clamp(b->hp-(b->guard?power[type]/4:power[type]),0,100);
        b->flash=7;
        b->x+=a->facing*(b->guard?3:9);boundary(b);
        if(!b->hp) {finished=1;restart_delay=120;}
    }
    if(a->age>=duration[type]) {a->attack=0;a->age=0;}
}
static void game_tick(uint16_t p) {
    uint16_t pressed=p & ~previous_pad; previous_pad=p;
    if(pressed&PAD_START) paused=!paused;
    if(paused) return;
    ticks++;
    if(finished) {
        if(!player.hp && player.fall<18) player.fall++;
        if(!cpu.hp && cpu.fall<18) cpu.fall++;
        if(restart_delay) restart_delay--;
        if(!restart_delay && (pressed&PAD_CROSS)) reset_match();
        return;
    }
    if(--round_frames<=0) {round_frames=0;finished=1;restart_delay=90;return;}
    player.guard=!!(p&(PAD_L1|PAD_L2)) && !player.attack;
    int speed=player.guard?1:(player.attack?1:3);
    int ox=player.x,oz=player.z;
    if(p&PAD_LEFT) player.x-=speed;
    if(p&PAD_RIGHT) player.x+=speed;
    if(p&PAD_UP) player.z+=speed;
    if(p&PAD_DOWN) player.z-=speed;
    boundary(&player);
    if(ox!=player.x||oz!=player.z) player.walk+=180;else player.walk=0;
    player.facing=player.x<=cpu.x?1:-1;cpu.facing=-player.facing;
    cpu.guard=!cpu.attack && player.attack && absi(cpu.x-player.x)<115 && rnd()%3==0;
    ox=cpu.x;oz=cpu.z;
    if(!cpu.attack && !cpu.guard) {
        if(absi(cpu.x-player.x)>73) cpu.x+=cpu.facing*2;
        else if(absi(cpu.x-player.x)<53) cpu.x-=cpu.facing;
        if(absi(cpu.z-player.z)>5) cpu.z+=cpu.z<player.z?2:-2;
    }
    boundary(&cpu);
    if(ox!=cpu.x||oz!=cpu.z) cpu.walk+=160;else cpu.walk=0;
    /* Keep bodies separate while allowing a sidestep around the opponent. */
    if(absi(player.z-cpu.z)<26 && absi(player.x-cpu.x)<39) {
        int middle=(player.x+cpu.x)/2;
        player.x=middle-player.facing*20;cpu.x=middle+player.facing*20;
        boundary(&player);boundary(&cpu);
    }
    if(p&PAD_SQUARE) start_attack(&player,1);
    else if(p&PAD_TRIANGLE) start_attack(&player,2);
    else if(p&PAD_CROSS) start_attack(&player,3);
    else if(p&PAD_CIRCLE) start_attack(&player,4);
    if(!cpu.guard && rnd()%24==0) start_attack(&cpu,1+rnd()%4);
    animate_and_hit(&player,&cpu);
    if(!finished) animate_and_hit(&cpu,&player);
}

#ifndef HOST_TEST
#define W 320
#define H 240
#define OT_LEN 1024
#define PACKETS 65536
static DISPENV disp[2];static DRAWENV draw[2];
static uint32_t ot[2][OT_LEN],packet[2][PACKETS/4];
static uint8_t pad1[34],pad2[34];
static int db,cursor,font_top,font_bottom,cam_x,cam_z,cam_distance=790,cam_yaw=-180;
static int camera_side;
static int view_sin,view_cos;
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
    int yy=85-p.y;
    int depth=cam_distance+(zz*15+yy*5)/16;
    if(depth<64) return (Screen){0,0,-1};
    return (Screen){160+xx*420/depth,126+(yy*15+zz*5)*420/(16*depth),depth};
}
static void face(Vec a,Vec b,Vec c,Vec d,int r,int g,int blue,int shade) {
    Screen v[4]={project(a),project(b),project(c),project(d)};
    for(int i=0;i<4;i++) if(v[i].d<0) return;
    int depth=(v[0].d+v[1].d+v[2].d+v[3].d)/4;
    POLY_F4 *p=alloc_prim(sizeof(POLY_F4));if(!p)return;
    setPolyF4(p);setRGB0(p,r*shade/256,g*shade/256,blue*shade/256);
    setXY4(p,v[0].x,v[0].y,v[1].x,v[1].y,v[2].x,v[2].y,v[3].x,v[3].y);
    addPrim(ot[db]+(depth_override?depth_override:clamp(depth/2,1,OT_LEN-1)),p);
}
static void edge(Vec a,Vec b,int r,int g,int blue) {
    Screen u=project(a),v=project(b);if(u.d<0||v.d<0)return;
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
    return vec(f->x+p.x*f->facing,p.y,f->z+p.z);
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
    int hr=f==&cpu?44:55,hg=f==&cpu?29:37,hb=27;
    loft(f,lean,168+bob,0,hair,4,hr,hg,hb);
    if(f==&cpu) {
        const Ring beard[3]={{0,7,6,2},{3,9,8,1},{6,9,8,1}};
        loft(f,lean,149+bob,0,beard,3,65,42,32);
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
    int sr=f==&cpu?166:211,sg=f==&cpu?115:162,sb=f==&cpu?86:125;
    if(f->flash){sr=249;sg=221;sb=185;}
    int broad=f==&cpu?2:0;
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
        if(f->guard){elbow=vec(16,126,side*25);hand=vec(28,157,side*12);}
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
static void arena(void) {
    /* Eight-sided canvas fan; each quad uses a degenerate triangle. */
    for(int i=0;i<8;i++) {
        int j=(i+1)%8;
        depth_override=1023;
        face(vec(0,0,0),vec(ring_x[i],0,ring_z[i]),vec(ring_x[j],0,ring_z[j]),vec(ring_x[j],0,ring_z[j]),91+(i%2)*7,116+(i%2)*7,127+(i%2)*7,256);
        depth_override=1022;
        edge(vec(ring_x[i],1,ring_z[i]),vec(ring_x[j],1,ring_z[j]),221,192,91);
        depth_override=0;
        box(ring_x[i],55,ring_z[i],5,55,5,38,46,58);
        for(int y=35;y<=95;y+=30) edge(vec(ring_x[i],y,ring_z[i]),vec(ring_x[j],y,ring_z[j]),y==95?190:90,y==95?174:104,y==95?122:116);
        /* A low-poly audience silhouette beyond each post. */
        box(ring_x[i]*5/4,36,ring_z[i]*5/4,20,20,12,30+i*4,36+i*3,53);
        box(ring_x[i]*5/4,62,ring_z[i]*5/4,8,8,8,85,69,66);
    }
    /* World-space mat markings and central diamond. */
    depth_override=1022;
    for(int i=-2;i<=2;i++) {
        edge(vec(-230,2,i*60),vec(230,2,i*60),104,135,144);
        edge(vec(i*80,2,-145),vec(i*80,2,145),104,135,144);
    }
    edge(vec(-48,3,0),vec(0,3,40),220,201,150);edge(vec(0,3,40),vec(48,3,0),220,201,150);
    edge(vec(48,3,0),vec(0,3,-40),220,201,150);edge(vec(0,3,-40),vec(-48,3,0),220,201,150);
    depth_override=1021;
    Fighter *fighters[2]={&player,&cpu};
    for(int i=0;i<2;i++) {
        Fighter *f=fighters[i];
        face(vec(f->x-23,3,f->z-23),vec(f->x+23,3,f->z-23),vec(f->x-23,3,f->z+23),vec(f->x+23,3,f->z+23),48,64,72,256);
    }
    depth_override=0;
}
static void hud_rect(int x,int y,int w,int h,int r,int g,int b) {
    if(w<=0)return;
    POLY_F4 *p=alloc_prim(sizeof(POLY_F4));if(!p)return;
    setPolyF4(p);setRGB0(p,r,g,b);setXY4(p,x,y,x+w,y,x,y+h,x+w,y+h);
    addPrim(hud_tail,p);hud_tail=p;
}
static void render(void) {
    int tx=(player.x+cpu.x)/2,tz=(player.z+cpu.z)/2;
    int target=590+absi(player.x-cpu.x)*2/3;
    cam_x+=(tx-cam_x)/8;cam_z+=(tz-cam_z)/8;
    cam_distance+=(target-cam_distance)/8;cam_yaw+=((camera_side?180:-180)-cam_yaw)/8;
    view_sin=isin(cam_yaw);view_cos=icos(cam_yaw);
    db^=1;cursor=0;ClearOTagR(ot[db],OT_LEN);hud_tail=ot[db];
    arena();fighter_draw(&player,43,111,216);fighter_draw(&cpu,210,53,52);
    hud_rect(8,31,142,10,49,26,32);hud_rect(170,31,142,10,49,26,32);
    hud_rect(9,32,player.hp*140/100,8,60,190,119);hud_rect(171,32,cpu.hp*140/100,8,60,190,119);
    hud_rect(9,44,player.stamina*140/100,3,237,189,61);hud_rect(171,44,cpu.stamina*140/100,3,237,189,61);
    FntPrint(font_top,"UNLIMITED FIGHTING CHAMPS  3D v0.3\nBLUE: YOU      %02d      RED: CPU",round_frames/60);
    if(paused) FntPrint(font_bottom,"PAUSED - START TO RESUME\nSELECT: CAMERA");
    else if(finished) {
        const char *winner=player.hp==cpu.hp?"DRAW":(player.hp>cpu.hp?"BLUE WINS":"RED WINS");
        FntPrint(font_bottom,"%s - %s\nPRESS X TO REMATCH",winner,(!player.hp||!cpu.hp)?"KNOCKOUT":"TIME UP");
    } else FntPrint(font_bottom,"[] JAB /\\ POWER X KICK O HEAVY\nL1/L2 GUARD  START PAUSE  SELECT CAM");
    DrawSync(0);VSync(0);PutDispEnv(&disp[db]);PutDrawEnv(&draw[db]);
    DrawOTag(ot[db]+OT_LEN-1);FntFlush(font_top);FntFlush(font_bottom);
}
int main(void) {
    ResetGraph(0);InitGeom();SetVideoMode(MODE_NTSC);
    SetDefDispEnv(&disp[0],0,0,W,H);SetDefDrawEnv(&draw[0],0,H,W,H);
    SetDefDispEnv(&disp[1],0,H,W,H);SetDefDrawEnv(&draw[1],0,0,W,H);
    draw[0].isbg=draw[1].isbg=1;setRGB0(&draw[0],12,16,29);setRGB0(&draw[1],12,16,29);
    FntLoad(960,0);font_top=FntOpen(8,6,304,24,0,100);font_bottom=FntOpen(8,211,304,25,0,110);
    InitPAD(pad1,sizeof(pad1),pad2,sizeof(pad2));StartPAD();ChangeClearPAD(0);
    SetDispMask(1);reset_match();
    while(1) {
        uint16_t p=pad_buttons();if((p&PAD_SELECT)&&!(previous_pad&PAD_SELECT))camera_side=!camera_side;
        game_tick(p);render();
    }
}
#endif
