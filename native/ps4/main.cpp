#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <orbis/Pad.h>
#include "graphics.h"

static const uint8_t FONT[][5] = {
    {0x7e,0x11,0x11,0x11,0x7e},{0x7f,0x49,0x49,0x49,0x36},
    {0x3e,0x41,0x41,0x41,0x22},{0x7f,0x41,0x41,0x22,0x1c},
    {0x7f,0x49,0x49,0x49,0x41},{0x7f,0x09,0x09,0x09,0x01},
    {0x3e,0x41,0x49,0x49,0x7a},{0x7f,0x08,0x08,0x08,0x7f},
    {0x00,0x41,0x7f,0x41,0x00},{0x20,0x40,0x41,0x3f,0x01},
    {0x7f,0x08,0x14,0x22,0x41},{0x7f,0x40,0x40,0x40,0x40},
    {0x7f,0x02,0x0c,0x02,0x7f},{0x7f,0x04,0x08,0x10,0x7f},
    {0x3e,0x41,0x41,0x41,0x3e},{0x7f,0x09,0x09,0x09,0x06},
    {0x3e,0x41,0x51,0x21,0x5e},{0x7f,0x09,0x19,0x29,0x46},
    {0x46,0x49,0x49,0x49,0x31},{0x01,0x01,0x7f,0x01,0x01},
    {0x3f,0x40,0x40,0x40,0x3f},{0x1f,0x20,0x40,0x20,0x1f},
    {0x3f,0x40,0x38,0x40,0x3f},{0x63,0x14,0x08,0x14,0x63},
    {0x07,0x08,0x70,0x08,0x07},{0x61,0x51,0x49,0x45,0x43},
    {0x3e,0x51,0x49,0x45,0x3e},{0x00,0x42,0x7f,0x40,0x00},
    {0x42,0x61,0x51,0x49,0x46},{0x21,0x41,0x45,0x4b,0x31},
    {0x18,0x14,0x12,0x7f,0x10},{0x27,0x45,0x45,0x45,0x39},
    {0x3c,0x4a,0x49,0x49,0x30},{0x01,0x71,0x09,0x05,0x03},
    {0x36,0x49,0x49,0x49,0x36},{0x06,0x49,0x49,0x29,0x1e}
};
static Color rgb(uint8_t r,uint8_t g,uint8_t b){ Color c={r,g,b}; return c; }
static void rect(Scene2D*s,int x,int y,int w,int h,uint8_t r,uint8_t g,uint8_t b){
    s->DrawRectangle(x,y,w,h,rgb(r,g,b));
}
static void txt(Scene2D*s,const char*t,int x,int y,int scale,Color color){
    for(int n=0;t[n];++n){
        char ch=t[n]; if(ch>='a'&&ch<='z')ch=(char)(ch-'a'+'A');
        const uint8_t*g=0;
        if(ch>='A'&&ch<='Z')g=FONT[ch-'A'];
        else if(ch>='0'&&ch<='9')g=FONT[26+ch-'0'];
        if(g){for(int c=0;c<5;c++)for(int r=0;r<7;r++)if((g[c]>>r)&1)
            s->DrawRectangle(x+c*scale,y+r*scale,scale,scale,color);}
        else if(ch==':'){s->DrawRectangle(x,y+scale,scale,scale,color);s->DrawRectangle(x,y+4*scale,scale,scale,color);}
        else if(ch=='-')s->DrawRectangle(x,y+3*scale,4*scale,scale,color);
        else if(ch=='.')s->DrawRectangle(x,y+6*scale,scale,scale,color);
        x+=6*scale;
    }
}
static void card(Scene2D*s,int x,int y,int w,int h,const char*name,const char*category,bool selected){
    rect(s,x,y,w,h,selected?35:20,selected?75:33,selected?88:52);
    rect(s,x,y,w,7,selected?50:31,selected?220:80,selected?220:110);
    txt(s,name,x+28,y+35,4,rgb(235,242,255));
    txt(s,category,x+28,y+95,2,rgb(135,180,205));
    if(selected)txt(s,"SELECTED",x+28,y+h-44,2,rgb(50,220,220));
}
int main(){
    setvbuf(stdout,NULL,_IONBF,0);
    scePadInit();
    int pad=scePadOpen(0,ORBIS_PAD_PORT_TYPE_STANDARD,0,NULL);
    Scene2D*scene=new Scene2D(1920,1080,4);
    if(!scene->Init(0x1200000,2))return 1;
    scene->SetActiveFrameBuffer(0);
    const Color white=rgb(235,242,255),muted=rgb(135,157,183),cyan=rgb(50,220,220);
    int frame=0,selected=0,tab=0,queueCount=0,previous=0;
    const char*games[]={"ROCKET LEAGUE","BRAWLHALLA","PACKAGE URL"};
    const char*types[]={"FREE TO PLAY","FREE TO PLAY","ADD AUTHORIZED PKG"};
    for(;;){
        OrbisPadData state; memset(&state,0,sizeof(state));
        if(pad>=0)scePadReadState(pad,&state);
        int buttons=(int)state.buttons;
        if((buttons&ORBIS_PAD_BUTTON_UP)&&!(previous&ORBIS_PAD_BUTTON_UP))selected=(selected+2)%3;
        if((buttons&ORBIS_PAD_BUTTON_DOWN)&&!(previous&ORBIS_PAD_BUTTON_DOWN))selected=(selected+1)%3;
        if((buttons&ORBIS_PAD_BUTTON_LEFT)&&!(previous&ORBIS_PAD_BUTTON_LEFT))tab=0;
        if((buttons&ORBIS_PAD_BUTTON_RIGHT)&&!(previous&ORBIS_PAD_BUTTON_RIGHT))tab=1;
        if((buttons&ORBIS_PAD_BUTTON_CROSS)&&!(previous&ORBIS_PAD_BUTTON_CROSS)){
            if(tab==0&&selected<2)queueCount=queueCount<2?queueCount+1:queueCount;
        }
        if((buttons&ORBIS_PAD_BUTTON_CIRCLE)&&!(previous&ORBIS_PAD_BUTTON_CIRCLE)){
            if(queueCount>0)queueCount--;
        }
        previous=buttons;
        scene->FrameBufferClear();
        rect(scene,0,0,1920,1080,8,14,25); rect(scene,0,0,1920,10,50,220,220);
        rect(scene,100,70,1720,130,20,33,52);
        txt(scene,"TIM",135,100,8,cyan); txt(scene,"PS4 GAME LIBRARY",390,100,4,white);
        txt(scene,"CATALOG",140,240,3,tab==0?cyan:muted);
        txt(scene,"DOWNLOAD QUEUE",410,240,3,tab==1?cyan:muted);
        rect(scene,120,290,1680,3,40,70,92);
        if(tab==0){
            card(scene,120,335,520,300,games[0],types[0],selected==0);
            card(scene,700,335,520,300,games[1],types[1],selected==1);
            card(scene,1280,335,520,300,games[2],types[2],selected==2);
            rect(scene,120,680,1680,200,20,33,52);
            txt(scene,"CROSS - ADD SELECTED FREE TITLE TO QUEUE",155,720,3,white);
            txt(scene,"D-PAD - MOVE   LEFT RIGHT - SWITCH TAB",155,785,3,muted);
            txt(scene,"SOURCE CATALOG: PIRATEGAMES.RU/PS4",155,830,2,muted);
        }else{
            rect(scene,120,335,1680,545,20,33,52);
            txt(scene,"DOWNLOAD QUEUE",160,375,4,cyan);
            txt(scene,queueCount?"QUEUED ITEMS":"QUEUE IS EMPTY",160,470,4,white);
            if(queueCount>0)txt(scene,queueCount==1?"1 ITEM SELECTED":"2 ITEMS SELECTED",160,550,3,muted);
            txt(scene,"CIRCLE - REMOVE LAST ITEM",160,720,3,white);
            txt(scene,"REAL FILE TRANSFER BACKEND NOT YET CONNECTED",160,800,2,muted);
        }
        txt(scene,"TIM - NATIVE PS4 APPLICATION",120,950,2,muted);
        scene->SubmitFlip(frame);scene->FrameWait(frame);scene->FrameBufferSwap();++frame;
    }
    return 0;
}
