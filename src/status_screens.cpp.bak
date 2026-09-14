#include "config.h"
#include "status_screens.h"
#include "pico/stdlib.h"
#include "hardware/sync.h"
#include <string.h>
namespace { struct Yuv{uint8_t y,u,v;}; alignas(4) uint8_t img[3][UVC_FRAME_BYTES]; volatile screen_mode_t mode=SCREEN_LOADING;
uint8_t clip(int v){return(uint8_t)(v<0?0:v>255?255:v);} Yuv rgb(int r,int g,int b){return{clip(((66*r+129*g+25*b+128)>>8)+16),clip(((-38*r-74*g+112*b+128)>>8)+128),clip(((112*r-94*g-18*b+128)>>8)+128)};}
void pair(uint8_t*f,int x,int y,Yuv a,Yuv b){uint8_t*p=f+(y*UVC_W+x)*2;p[0]=a.y;p[1]=(a.u+b.u)/2;p[2]=b.y;p[3]=(a.v+b.v)/2;}
void rect(uint8_t*f,int x,int y,int w,int h,Yuv c){if(x&1){--x;++w;}if(w&1)++w;if(x<0){w+=x;x=0;}if(y<0){h+=y;y=0;}if(x+w>UVC_W)w=UVC_W-x;if(y+h>UVC_H)h=UVC_H-y;for(int yy=y;yy<y+h;yy++)for(int xx=x;xx<x+w;xx+=2)pair(f,xx,yy,c,c);}
void bars(uint8_t*f){const int c[8][3]={{255,255,255},{255,255,0},{0,255,255},{0,255,0},{255,0,255},{255,0,0},{0,0,255},{32,32,32}};for(int i=0;i<8;i++)rect(f,i*16,0,16,UVC_H,rgb(c[i][0],c[i][1],c[i][2]));}
void glyph(char c,uint8_t o[7]){memset(o,0,7);static const char*k="ADEFGILNORSTU/";static const uint8_t g[][7]={{14,17,17,31,17,17,17},{30,17,17,17,17,17,30},{31,16,16,30,16,16,31},{31,16,16,30,16,16,16},{14,17,16,23,17,17,14},{31,4,4,4,4,4,31},{16,16,16,16,16,16,31},{17,25,21,19,17,17,17},{14,17,17,17,17,17,14},{30,17,17,30,20,18,17},{15,16,16,14,1,1,30},{31,4,4,4,4,4,4},{17,17,17,17,17,17,14},{1,2,2,4,8,8,16}};for(int i=0;k[i];i++)if(c==k[i]){memcpy(o,g[i],7);return;}}
void text(uint8_t*f,const char*s){int n=strlen(s),tw=n*12-2,x0=(UVC_W-tw)/2,y0=41;Yuv black={16,128,128};for(int i=0;i<n;i++){uint8_t q[7];glyph(s[i],q);for(int y=0;y<7;y++)for(int x=0;x<5;x++)if(q[y]&(16>>x))rect(f,x0+i*12+x*2,y0+y*2,2,2,black);}}
}
extern "C" void status_screens_init(){bars(img[0]);rect(img[0],17,35,94,26,rgb(255,255,255));text(img[0],"LOADING");rect(img[1],0,0,UVC_W,UVC_H,rgb(255,255,255));text(img[1],"NO SENSOR");rect(img[2],0,0,UVC_W,UVC_H,rgb(255,255,255));text(img[2],"RANGE/ERR");__dmb();mode=SCREEN_LOADING;}
extern "C" void status_screen_set_mode(screen_mode_t m){__dmb();mode=m;__sev();}
extern "C" screen_mode_t status_screen_get_mode(){return mode;}
extern "C" const uint8_t*status_screen_frame(screen_mode_t m){return m<=SCREEN_RANGE_ERROR?img[(int)m]:nullptr;}
