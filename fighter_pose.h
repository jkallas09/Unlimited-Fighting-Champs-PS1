/* Original fixed-point two-bone animation rig. Local +X faces the opponent. */
typedef struct {int x,y,z;} Vec;
typedef struct {
    Vec hip[2],knee[2],foot[2],shoulder[2],elbow[2],hand[2];
    int lean,twist,bob,head;
} FightPose;
static Vec vec(int x,int y,int z) {return (Vec){x,y,z};}
static int pose_sin(int angle) {
    static const int wave[17]={0,392,724,946,1024,946,724,392,0,-392,-724,-946,-1024,-946,-724,-392,0};
    unsigned a=(unsigned)angle&4095;int k=a>>8,t=a&255;
    return wave[k]+(wave[k+1]-wave[k])*t/256;
}
static int smooth_pose(int t) {t=clamp(t,0,256);return t*t*(768-2*t)/65536;}
static Vec blend_pose(Vec a,Vec b,int t) {
    t=smooth_pose(t);return vec(a.x+(b.x-a.x)*t/256,a.y+(b.y-a.y)*t/256,a.z+(b.z-a.z)*t/256);
}
static Vec twist_point(Vec p,int angle) {
    int s=pose_sin(angle),c=pose_sin(angle+1024);
    return vec((p.x*c-p.z*s)/1024,p.y,(p.x*s+p.z*c)/1024);
}
static inline int joint_distance(Vec a,Vec b) {int x=a.x-b.x,y=a.y-b.y,z=a.z-b.z;return root_int(x*x+y*y+z*z);}
static Vec solve_joint(Vec origin,Vec *end,int upper,int lower,Vec hint) {
    int dx=end->x-origin.x,dy=end->y-origin.y,dz=end->z-origin.z;
    int dist=root_int(dx*dx+dy*dy+dz*dz);if(dist<1)dist=1;
    int limit=upper+lower-2;
    if(dist>limit) {dx=dx*limit/dist;dy=dy*limit/dist;dz=dz*limit/dist;
        *end=vec(origin.x+dx,origin.y+dy,origin.z+dz);dist=root_int(dx*dx+dy*dy+dz*dz);}
    if(dist<1)dist=1;
    int along=clamp((upper*upper-lower*lower+dist*dist)/(2*dist),1,upper);
    int height=root_int(upper*upper-along*along);
    int dot=(hint.x*dx+hint.y*dy+hint.z*dz)/dist;
    Vec bend=vec(hint.x-dx*dot/dist,hint.y-dy*dot/dist,hint.z-dz*dot/dist);
    int length=root_int(bend.x*bend.x+bend.y*bend.y+bend.z*bend.z);
    if(!length){bend=vec(-dy,dx,0);length=root_int(dx*dx+dy*dy);}
    if(!length){bend=vec(1,0,0);length=1;}
    return vec(origin.x+dx*along/dist+bend.x*height/length,
        origin.y+dy*along/dist+bend.y*height/length,origin.z+dz*along/dist+bend.z*height/length);
}
/* Every strike has its own anticipation, contact, recoil and return targets. */
static FightPose fighter_pose(const Fighter *f,int clock) {
    FightPose p={0};int type=f->attack;
    int cycle=pose_sin(f->walk),step=cycle*10/1024;
    p.bob=pose_sin(clock*62)/700;p.lean=3;
    int wind=0,extend=0,recover=0;
    if(type) {
        int peak=contact[type],chamber=peak*2/5;
        if(f->age<=chamber)wind=smooth_pose(f->age*256/(chamber?chamber:1));
        else if(f->age<=peak){wind=256;extend=smooth_pose((f->age-chamber)*256/(peak-chamber));}
        else {wind=256;extend=256;recover=smooth_pose((f->age-peak)*256/(duration[type]-peak));}
        int force=extend*(256-recover)/256;
        p.twist=(type==1?-85:type==2?220:type==3?-180:-320)*force/256;
        p.lean+=(type<3?9:-8)*force/256;
        p.bob-=((type==2?3:1)*wind*(256-extend))/65536;
        p.head=(type==2?3:0)*force/256;
    }
    for(int i=0;i<2;i++) {
        int side=i?1:-1;
        p.hip[i]=vec(i?6:-4,85,side*12);
        p.foot[i]=vec((i?28:-23)+(i?-step:step),5,side*19);
        p.foot[i].y+=((i?cycle:-cycle)>0)?absi(cycle)*5/1024:0;
        p.shoulder[i]=twist_point(vec(3+p.lean/2,133+p.bob,side*(25+roster[f->character].bulk)),p.twist);
        Vec hand=vec(i?27:35, i?143:149,side*17);
        if(f->guard==1)hand=vec(28,159,side*12);
        if(f->guard==2)hand=vec(28,121,side*15);
        if(type && type<3 && i==(type==1?0:1)) {
            Vec chamber=type==1?vec(26,147,-22):vec(15,144,30);
            Vec hit=type==1?vec(67,151,-6):vec(70,150,-4);
            hand=blend_pose(hand,chamber,wind);
            hand=blend_pose(hand,hit,extend);
            hand=blend_pose(hand,vec(i?27:35,i?143:149,side*17),recover);
        } else if(type)hand=blend_pose(hand,vec(26,157,side*13),wind*(256-recover)/256);
        p.hand[i]=hand;
        p.elbow[i]=solve_joint(p.shoulder[i],&p.hand[i],32,31,vec(0,-45,side*24));
    }
    if(type>=3) {
        Vec rest=p.foot[1];
        Vec chamber=vec(27,62,9),hit=type==3?vec(86,103,-6):vec(75,158,-9);
        Vec target=blend_pose(rest,chamber,wind);
        target=blend_pose(target,hit,extend);
        p.foot[1]=blend_pose(target,rest,recover);
        int force=extend*(256-recover)/256;
        p.hip[1].x+=(type==4?20:12)*force/256;p.hip[1].y+=(type==4?6:0)*force/256;p.hip[0].x+=6*force/256;
        p.foot[0].z-=6*force/256; /* Support foot pivots out as the hips turn. */
    }
    for(int i=0;i<2;i++)p.knee[i]=solve_joint(p.hip[i],&p.foot[i],43,42,vec(32,0,i?4:-4));
    if(f->stun && !f->attack){p.head=-3;p.lean-=clamp(f->stun,0,8);}
    return p;
}
