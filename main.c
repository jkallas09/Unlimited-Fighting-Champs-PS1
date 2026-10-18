/* Unlimited Fighting Champs 0.2 - original polygonal PS1 combat prototype.
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
#define OT_LEN 512
#define PACKETS 65536
static DISPENV disp[2];static DRAWENV draw[2];
static uint32_t ot[2][OT_LEN],packet[2][PACKETS/4];
static uint8_t pad1[34],pad2[34];
static int db,cursor,font_top,font_bottom,cam_x,cam_z,cam_distance=790,cam_yaw=-180;
static int camera_side;
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
    int s=isin(cam_yaw),c=icos(cam_yaw);
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
    addPrim(ot[db]+(depth_override?depth_override:clamp(depth/4,1,OT_LEN-1)),p);
}
static void edge(Vec a,Vec b,int r,int g,int blue) {
    Screen u=project(a),v=project(b);if(u.d<0||v.d<0)return;
    LINE_F2 *p=alloc_prim(sizeof(LINE_F2));if(!p)return;
    setLineF2(p);setRGB0(p,r,g,blue);setXY2(p,u.x,u.y,v.x,v.y);
    addPrim(ot[db]+(depth_override?depth_override:clamp((u.d+v.d)/8-1,1,OT_LEN-1)),p);
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
static void body_box(Fighter *f,int x,int y,int z,int wx,int hy,int dz,int taper,int r,int g,int b) {
    Vec v[8];
    for(int i=0;i<8;i++) {
        int width=(i&2)?wx:wx-taper;
        v[i]=body_vertex(f,vec(x+((i&1)?width:-width),y+((i&2)?hy:-hy),z+((i&4)?dz:-dz)));
    }
    solid(v,r,g,b);
}
static void limb(Fighter *f,Vec a,Vec b,int width,int r,int g,int blue) {
    int dx=b.x-a.x,dy=b.y-a.y;
    int length=SquareRoot0(dx*dx+dy*dy);if(!length)length=1;
    int px=-dy*width/length,py=dx*width/length;
    Vec v[8];
    for(int i=0;i<8;i++) {
        Vec end=(i&2)?b:a;
        end.x+=(i&1)?px:-px;end.y+=(i&1)?py:-py;end.z+=(i&4)?width:-width;
        v[i]=body_vertex(f,end);
    }
    solid(v,r,g,blue);
}
static void head(Fighter *f,int lean,int bob,int skin_r,int skin_g,int skin_b) {
    for(int i=0;i<8;i++) {
        int a=i*512,b=(i+1)*512;
        Vec l=vec(lean+icos(a)*14/4096,135+bob,isin(a)*14/4096);
        Vec rr=vec(lean+icos(b)*14/4096,135+bob,isin(b)*14/4096);
        Vec u=l,v=rr;u.y=v.y=157+bob;
        face(body_vertex(f,l),body_vertex(f,rr),body_vertex(f,u),body_vertex(f,v),skin_r,skin_g,skin_b,180+(i%4)*20);
        Vec top=vec(lean,161+bob,0);
        face(body_vertex(f,u),body_vertex(f,v),body_vertex(f,top),body_vertex(f,top),45,29,25,220);
    }
    body_box(f,lean+14,148+bob,-6,2,2,3,0,22,20,20);
    body_box(f,lean+14,148+bob,6,2,2,3,0,22,20,20);
    body_box(f,lean+17,143+bob,0,3,3,3,0,skin_r,skin_g,skin_b);
}
static void fighter_draw(Fighter *f,int red,int green,int blue) {
    int pulse=0;
    if(f->attack) {
        int apex=contact[f->attack];
        pulse=f->age<=apex?f->age*256/apex:(duration[f->attack]-f->age)*256/(duration[f->attack]-apex);
    }
    int step=isin(f->walk)*9/4096,bob=absi(isin(ticks*80))*2/4096;
    int lean=(f->attack==2?pulse*9/256:0);
    int sr=199,sg=147,sb=111;
    if(f==&cpu){sr=155;sg=101;sb=75;}
    if(f->flash){sr=255;sg=228;sb=194;}
    /* Grounded feet, bent knees, shorts, tapered torso, neck, octagonal head. */
    Vec hip0=vec(-2,68,-12),hip1=vec(4,68,12);
    Vec knee0=vec(-10+step,38,-15),foot0=vec(-17+step,6,-17);
    Vec knee1=vec(12-step,38,15),foot1=vec(22-step,6,17);
    if(f->attack>=3) {
        knee1=vec(18+pulse*22/256,38+pulse*28/256,15);
        foot1=vec(22+pulse*(f->attack==4?72:60)/256,6+pulse*74/256,17);
    }
    limb(f,hip0,knee0,7,sr,sg,sb);limb(f,knee0,foot0,6,sr,sg,sb);
    limb(f,hip1,knee1,7,sr,sg,sb);limb(f,knee1,foot1,6,sr,sg,sb);
    body_box(f,foot0.x+4,foot0.y,foot0.z,11,5,7,0,42,39,40);
    body_box(f,foot1.x+4,foot1.y,foot1.z,11,5,7,0,42,39,40);
    body_box(f,0,72,0,15,12,20,0,red,green,blue);
    body_box(f,lean/2,106+bob,0,15,23,23,5,sr,sg,sb);
    body_box(f,lean,132+bob,0,6,6,7,0,sr,sg,sb);
    head(f,lean,bob,sr,sg,sb);
    for(int side=-1;side<=1;side+=2) {
        Vec shoulder=vec(lean,121+bob,side*25);
        Vec elbow=vec(8,99+bob,side*29),hand=vec(24,119+bob,side*21);
        if(f->guard){elbow=vec(14,120,side*28);hand=vec(24,148,side*14);}
        if(f->attack && f->attack<3 && side==(f->attack==1?-1:1)) {
            elbow.x+=pulse*34/256;elbow.y+=pulse*21/256;
            hand.x+=pulse*(f->attack==2?59:49)/256;hand.y+=pulse*7/256;
        }
        limb(f,shoulder,elbow,6,sr,sg,sb);limb(f,elbow,hand,5,sr,sg,sb);
        body_box(f,hand.x,hand.y,hand.z,8,7,7,0,red,green,blue);
    }
}
static const int ring_x[8]={-240,240,300,300,240,-240,-300,-300};
static const int ring_z[8]={-190,-190,-130,130,190,190,130,-130};
static void arena(void) {
    /* Eight-sided canvas fan; each quad uses a degenerate triangle. */
    for(int i=0;i<8;i++) {
        int j=(i+1)%8;
        depth_override=511;
        face(vec(0,0,0),vec(ring_x[i],0,ring_z[i]),vec(ring_x[j],0,ring_z[j]),vec(ring_x[j],0,ring_z[j]),91+(i%2)*7,116+(i%2)*7,127+(i%2)*7,256);
        depth_override=510;
        edge(vec(ring_x[i],1,ring_z[i]),vec(ring_x[j],1,ring_z[j]),221,192,91);
        depth_override=0;
        box(ring_x[i],55,ring_z[i],5,55,5,38,46,58);
        for(int y=35;y<=95;y+=30) edge(vec(ring_x[i],y,ring_z[i]),vec(ring_x[j],y,ring_z[j]),y==95?190:90,y==95?174:104,y==95?122:116);
        /* A low-poly audience silhouette beyond each post. */
        box(ring_x[i]*5/4,36,ring_z[i]*5/4,20,20,12,30+i*4,36+i*3,53);
        box(ring_x[i]*5/4,62,ring_z[i]*5/4,8,8,8,85,69,66);
    }
    /* World-space mat markings and central diamond. */
    depth_override=510;
    for(int i=-2;i<=2;i++) {
        edge(vec(-230,2,i*60),vec(230,2,i*60),104,135,144);
        edge(vec(i*80,2,-145),vec(i*80,2,145),104,135,144);
    }
    edge(vec(-48,3,0),vec(0,3,40),220,201,150);edge(vec(0,3,40),vec(48,3,0),220,201,150);
    edge(vec(48,3,0),vec(0,3,-40),220,201,150);edge(vec(0,3,-40),vec(-48,3,0),220,201,150);
    depth_override=509;
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
    int target=650+absi(player.x-cpu.x)*2/3;
    cam_x+=(tx-cam_x)/8;cam_z+=(tz-cam_z)/8;
    cam_distance+=(target-cam_distance)/8;cam_yaw+=((camera_side?180:-180)-cam_yaw)/8;
    db^=1;cursor=0;ClearOTagR(ot[db],OT_LEN);hud_tail=ot[db];
    arena();fighter_draw(&player,43,111,216);fighter_draw(&cpu,210,53,52);
    hud_rect(8,31,142,10,49,26,32);hud_rect(170,31,142,10,49,26,32);
    hud_rect(9,32,player.hp*140/100,8,60,190,119);hud_rect(171,32,cpu.hp*140/100,8,60,190,119);
    hud_rect(9,44,player.stamina*140/100,3,237,189,61);hud_rect(171,44,cpu.stamina*140/100,3,237,189,61);
    FntPrint(font_top,"UNLIMITED FIGHTING CHAMPS  3D v0.2\nBLUE: YOU      %02d      RED: CPU",round_frames/60);
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
