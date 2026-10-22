#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <orbis/Pad.h>
#include "graphics.h"
#include "file_browser.h"

#define MAX_ROOTS 8
static const char *USB_CANDIDATES[] = {
    "/mnt/usb0", "/mnt/usb1", "/mnt/usb2", "/mnt/usb3",
    "/mnt/ext0", "/mnt/ext1", "/mnt/usb"
};
static const char *HDD_CANDIDATES[] = {
    "/data", "/user/data", "/mnt/sandbox"
};
static char usb_roots[MAX_ROOTS][TIM_PATH_MAX];
static char hdd_roots[MAX_ROOTS][TIM_PATH_MAX];
static size_t usb_count, hdd_count;

static Color rgb(uint8_t r, uint8_t g, uint8_t b) { Color c = {r,g,b}; return c; }
static void rect(Scene2D *s, int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b) {
    s->DrawRectangle(x,y,w,h,rgb(r,g,b));
}
static const uint8_t FONT[][5] = {
 {0x7e,0x11,0x11,0x11,0x7e},{0x7f,0x49,0x49,0x49,0x36},{0x3e,0x41,0x41,0x41,0x22},
 {0x7f,0x41,0x41,0x22,0x1c},{0x7f,0x49,0x49,0x49,0x41},{0x7f,0x09,0x09,0x09,0x01},
 {0x3e,0x41,0x49,0x49,0x7a},{0x7f,0x08,0x08,0x08,0x7f},{0x00,0x41,0x7f,0x41,0x00},
 {0x20,0x40,0x41,0x3f,0x01},{0x7f,0x08,0x14,0x22,0x41},{0x7f,0x40,0x40,0x40,0x40},
 {0x7f,0x02,0x0c,0x02,0x7f},{0x7f,0x04,0x08,0x10,0x7f},{0x3e,0x41,0x41,0x41,0x3e},
 {0x7f,0x09,0x09,0x09,0x06},{0x3e,0x41,0x51,0x21,0x5e},{0x7f,0x09,0x19,0x29,0x46},
 {0x46,0x49,0x49,0x49,0x31},{0x01,0x01,0x7f,0x01,0x01},{0x3f,0x40,0x40,0x20,0x7f},
 {0x1f,0x20,0x40,0x20,0x1f},{0x3f,0x40,0x38,0x40,0x3f},{0x63,0x14,0x08,0x14,0x63},
 {0x07,0x08,0x70,0x08,0x07},{0x61,0x51,0x49,0x45,0x43},
 {0x3e,0x51,0x49,0x45,0x3e},{0x00,0x42,0x7f,0x40,0x00},{0x42,0x61,0x51,0x49,0x46},
 {0x21,0x41,0x45,0x4b,0x31},{0x18,0x14,0x12,0x7f,0x10},{0x27,0x45,0x45,0x45,0x39},
 {0x3c,0x4a,0x49,0x49,0x30},{0x01,0x71,0x09,0x05,0x03},{0x36,0x49,0x49,0x49,0x36},
 {0x06,0x49,0x49,0x29,0x1e}
};
static void text(Scene2D *s, const char *v, int x, int y, int scale, Color color) {
    for (size_t i=0; v[i]; ++i) {
        char ch=v[i]; if(ch>='a'&&ch<='z') ch=(char)(ch-'a'+'A');
        const uint8_t *g=0;
        if(ch>='A'&&ch<='Z') g=FONT[ch-'A'];
        else if(ch>='0'&&ch<='9') g=FONT[26+ch-'0'];
        if(g) {
            for(int cx=0;cx<5;cx++) for(int cy=0;cy<7;cy++)
                if((g[cx]>>cy)&1) s->DrawRectangle(x+cx*scale,y+cy*scale,scale,scale,color);
        } else if(ch=='.') s->DrawRectangle(x,y+6*scale,scale,scale,color);
        else if(ch=='/') s->DrawRectangle(x+2*scale,y,scale,7*scale,color);
        else if(ch=='-') s->DrawRectangle(x,y+3*scale,4*scale,scale,color);
        else if(ch=='_') s->DrawRectangle(x,y+6*scale,4*scale,scale,color);
        else if(ch==':') { s->DrawRectangle(x,y+scale,scale,scale,color); s->DrawRectangle(x,y+4*scale,scale,scale,color); }
        x += 6*scale;
    }
}
static int is_dir(const char *p) { struct stat st; return stat(p,&st)==0 && S_ISDIR(st.st_mode); }
static void discover(void) {
    size_t i;
    for(i=0;i<sizeof(USB_CANDIDATES)/sizeof(USB_CANDIDATES[0]) && usb_count<MAX_ROOTS;i++)
        if(is_dir(USB_CANDIDATES[i]) && access(USB_CANDIDATES[i],R_OK)==0) {
            snprintf(usb_roots[usb_count++],TIM_PATH_MAX,"%s",USB_CANDIDATES[i]);
        }
    for(i=0;i<sizeof(HDD_CANDIDATES)/sizeof(HDD_CANDIDATES[0]) && hdd_count<MAX_ROOTS;i++)
        if(is_dir(HDD_CANDIDATES[i]) && access(HDD_CANDIDATES[i],R_OK|W_OK)==0) {
            snprintf(hdd_roots[hdd_count++],TIM_PATH_MAX,"%s",HDD_CANDIDATES[i]);
        }
}
static int open_browser(tim_file_browser *b, const char *root) {
    memset(b,0,sizeof(*b));
    return tim_fm_open(b,root);
}
int main(void) {
    setvbuf(stdout,NULL,_IONBF,0);
    scePadInit();
    int pad=scePadOpen(0,ORBIS_PAD_PORT_TYPE_STANDARD,0,NULL);
    Scene2D *scene=new Scene2D(1920,1080,4);
    if(!scene->Init(0x1200000,2)) return 1;
    scene->SetActiveFrameBuffer(0);
    discover();
    tim_file_browser src, dst;
    memset(&src,0,sizeof(src)); memset(&dst,0,sizeof(dst));
    int src_ok=usb_count>0 && open_browser(&src,usb_roots[0])==0;
    int dst_ok=hdd_count>0 && open_browser(&dst,hdd_roots[0])==0;
    int active=0,previous=0,frame=0,notice=0;
    char message[128]="READY";
    const Color white=rgb(235,242,255), muted=rgb(135,157,183), cyan=rgb(50,220,220);
    for(;;) {
        OrbisPadData state; memset(&state,0,sizeof(state)); if(pad>=0) scePadReadState(pad,&state);
        int buttons=(int)state.buttons;
        if((buttons&ORBIS_PAD_BUTTON_L1)&&!(previous&ORBIS_PAD_BUTTON_L1)) active=0;
        if((buttons&ORBIS_PAD_BUTTON_R1)&&!(previous&ORBIS_PAD_BUTTON_R1)) active=1;
        tim_file_browser *b=active?&dst:&src;
        int ok=active?dst_ok:src_ok;
        if(ok && (buttons&ORBIS_PAD_BUTTON_UP)&&!(previous&ORBIS_PAD_BUTTON_UP)&&b->count)
            tim_fm_select(b,b->selected?b->selected-1:b->count-1);
        if(ok && (buttons&ORBIS_PAD_BUTTON_DOWN)&&!(previous&ORBIS_PAD_BUTTON_DOWN)&&b->count)
            tim_fm_select(b,(b->selected+1)%b->count);
        if(ok && (buttons&ORBIS_PAD_BUTTON_CROSS)&&!(previous&ORBIS_PAD_BUTTON_CROSS)&&b->count) {
            if(b->entries[b->selected].is_directory) {
                if(tim_fm_enter_selected(b)==0) snprintf(message,sizeof(message),"OPENED FOLDER");
                else snprintf(message,sizeof(message),"CANNOT OPEN FOLDER");
            } else snprintf(message,sizeof(message),"FILE SELECTED");
        }
        if(ok && (buttons&ORBIS_PAD_BUTTON_CIRCLE)&&!(previous&ORBIS_PAD_BUTTON_CIRCLE)) {
            if(tim_fm_parent(b)==0) snprintf(message,sizeof(message),"PARENT DIRECTORY");
            else snprintf(message,sizeof(message),"CANNOT GO UP");
        }
        if(!active && src_ok && dst_ok && (buttons&ORBIS_PAD_BUTTON_SQUARE)&&!(previous&ORBIS_PAD_BUTTON_SQUARE)&&src.count) {
            if(tim_fm_copy_selected(&src,dst.current,NULL,NULL)==0) {
                snprintf(message,sizeof(message),"COPY COMPLETE");
                tim_fm_refresh(&dst);
            } else snprintf(message,sizeof(message),"COPY FAILED - CHECK SPACE / PERMISSIONS");
        }
        if((buttons&ORBIS_PAD_BUTTON_TRIANGLE)&&!(previous&ORBIS_PAD_BUTTON_TRIANGLE)) {
            if(src_ok) tim_fm_refresh(&src);
            if(dst_ok) tim_fm_refresh(&dst);
            snprintf(message,sizeof(message),"LIST REFRESHED");
        }
        previous=buttons;
        scene->FrameBufferClear(); rect(scene,0,0,1920,1080,8,14,25); rect(scene,0,0,1920,9,50,220,220);
        text(scene,"TIM FILE MANAGER",100,55,6,cyan);
        text(scene,"L1 SOURCE USB   R1 DESTINATION HDD",105,135,3,white);
        rect(scene,80,205,850,700,active?20:28,active?33:48,active?52:70);
        rect(scene,990,205,850,700,active?28:20,active?48:33,active?70:52);
        text(scene,"USB SOURCE",110,230,3,active?muted:cyan);
        text(scene,"HDD DESTINATION",1020,230,3,active?cyan:muted);
        if(src_ok) {
            text(scene,src.current,110,275,2,white);
            for(size_t i=0;i<src.count && i<12;i++) {
                char line[128]; snprintf(line,sizeof(line),"%c %s%s",i==src.selected?'>':' ',src.entries[i].is_directory?"[DIR] ":"",src.entries[i].name);
                text(scene,line,115,325+(int)i*42,2,i==src.selected?cyan:white);
            }
        } else text(scene,"NO READABLE USB MOUNT FOUND",115,330,2,muted);
        if(dst_ok) {
            text(scene,dst.current,1020,275,2,white);
            for(size_t i=0;i<dst.count && i<12;i++) {
                char line[128]; snprintf(line,sizeof(line),"%c %s%s",i==dst.selected?'>':' ',dst.entries[i].is_directory?"[DIR] ":"",dst.entries[i].name);
                text(scene,line,1025,325+(int)i*42,2,i==dst.selected?cyan:white);
            }
        } else text(scene,"NO WRITABLE HDD DIRECTORY FOUND",1025,330,2,muted);
        rect(scene,80,930,1760,80,20,33,52);
        text(scene,"UP DOWN MOVE   X OPEN   O PARENT   SQUARE COPY   TRIANGLE REFRESH",105,945,2,white);
        text(scene,message,105,980,2,cyan);
        scene->SubmitFlip(frame); scene->FrameWait(frame); scene->FrameBufferSwap(); ++frame;
    }
    return 0;
}
