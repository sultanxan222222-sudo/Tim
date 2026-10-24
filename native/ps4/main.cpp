#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <wchar.h>
#include <sys/stat.h>
#include <orbis/Pad.h>
#include <orbis/UserService.h>
#include <orbis/CommonDialog.h>
#include <orbis/Sysmodule.h>

/* Browser launch entry point used by PS4 system service. */
extern "C" int sceSystemServiceLaunchWebBrowser(const char *uri, void *param);
#include "graphics.h"
#include "../src/http_download.h"
#include "../src/http_url.h"

/* Local declarations compensate for incomplete generated OpenOrbis IME headers. */
typedef enum { ORBIS_DIALOG_OK = 0, ORBIS_DIALOG_CANCEL = 1, ORBIS_DIALOG_ABORD = 2 } OrbisDialogResultStatus;
typedef enum { ORBIS_V_TOP = 0, ORBIS_V_CENTER = 1, ORBIS_V_VALIGN_BOTTOM = 2 } OrbisVAlignment;
typedef enum { ORBIS_H_LEFT = 0, ORBIS_H_CENTER = 1, ORBIS_H_RIGHT = 2 } OrbisHAlignment;
typedef enum { ORBIS_TYPE_DEFAULT = 0, ORBIS_TYPE_BASIC_LATIN = 1, ORBIS_TYPE_TYPE_URL = 2, ORBIS_TYPE_MAIL = 3, ORBIS_TYPE_NUMBER = 4 } OrbisImeType;
typedef enum { ORBIS__DEFAULT = 0 } OrbisInput;
typedef enum { ORBIS_BUTTON_LABEL_DEFAULT = 0, ORBIS_BUTTON_LABEL_SEND = 1, ORBIS_BUTTON_LABEL_SEARCH = 2, ORBIS_BUTTON_LABEL_GO = 3 } OrbisButtonLabel;
typedef enum { ORBIS_DIALOG_STATUS_NONE = 0, ORBIS_DIALOG_STATUS_RUNNING = 1, ORBIS_DIALOG_STATUS_STOPPED = 2 } OrbisDialogStatus;
typedef struct OrbisImeDialogSetting {
    uint32_t userId;
    OrbisImeType type;
    uint64_t supportedLanguages;
    OrbisButtonLabel enterLabel;
    OrbisInput inputMethod;
    void *filter;
    uint32_t option;
    uint32_t maxTextLength;
    wchar_t *inputTextBuffer;
    float posx;
    float posy;
    OrbisHAlignment horizontalAlignment;
    OrbisVAlignment verticalAlignment;
    const wchar_t *placeholder;
    const wchar_t *title;
    int8_t reserved[16];
} OrbisImeDialogSetting;
typedef struct OrbisDialogResult { OrbisDialogResultStatus endstatus; int8_t reserved[12]; } OrbisDialogResult;
extern "C" int32_t sceImeDialogInit(const OrbisImeDialogSetting *param, void *paramExtended);
extern "C" OrbisDialogStatus sceImeDialogGetStatus(void);
extern "C" int32_t sceImeDialogGetResult(OrbisDialogResult *result);
extern "C" int32_t sceImeDialogTerm(void);


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
static int g_common_dialog_ready = 0;

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
static int open_url_dialog(wchar_t *buffer, size_t capacity){
    OrbisImeDialogSetting setting;
    if(buffer == NULL || capacity < 2) return -1;
    memset(buffer, 0, capacity * sizeof(wchar_t));
    if (!g_common_dialog_ready) return -1;
    memset(&setting, 0, sizeof(setting));
    setting.userId = 0;
    setting.type = ORBIS_TYPE_TYPE_URL;
    setting.maxTextLength = (uint32_t)(capacity - 1);
    setting.inputTextBuffer = buffer;
    setting.placeholder = L"https://server.example/file.pkg";
    setting.title = L"Enter a direct URL for an authorized PKG";
    setting.horizontalAlignment = ORBIS_H_CENTER;
    setting.verticalAlignment = ORBIS_V_CENTER;
    return sceImeDialogInit(&setting, NULL);
}

static int finish_url_dialog(wchar_t *buffer, size_t bufferCapacity,
                             char *out, size_t outSize){
    OrbisDialogResult result;
    OrbisDialogStatus status = sceImeDialogGetStatus();
    size_t i;
    if (buffer == NULL || bufferCapacity == 0 || out == NULL || outSize == 0)
        return -1;
    out[0] = '\0';
    if(status == ORBIS_DIALOG_STATUS_RUNNING || status == ORBIS_DIALOG_STATUS_NONE)
        return 0;
    memset(&result, 0, sizeof(result));
    if(sceImeDialogGetResult(&result) < 0){
        sceImeDialogTerm();
        return -1;
    }
    sceImeDialogTerm();
    if(result.endstatus != ORBIS_DIALOG_OK) return -1;
    for(i = 0; i < bufferCapacity && buffer[i] != 0; ++i){
        if(i + 1 >= outSize) return -1;
        if(buffer[i] < 0x21 || buffer[i] > 0x7e) return -1;
        out[i] = (char)buffer[i];
    }
    if(i == bufferCapacity) return -1;
    out[i] = '\0';
    return i > 0 ? 1 : -1;
}

static void url_filename(const char *url, char *filename, size_t capacity){
    const char *path = strstr(url, "://");
    const char *end, *slash, *start;
    size_t n, i;
    if(capacity == 0) return;
    filename[0] = '\0';
    if(path != NULL) path += 3; else path = url;
    slash = strchr(path, '/');
    if(slash == NULL) { snprintf(filename, capacity, "download.pkg"); return; }
    end = strpbrk(slash, "?#");
    if(end == NULL) end = url + strlen(url);
    start = slash;
    for(i = (size_t)(slash - url); url + i < end; ++i)
        if(url[i] == '/') start = url + i;
    ++start;
    n = (size_t)(end - start);
    if(n == 0 || (n == 1 && start[0] == '.') ||
       (n == 2 && start[0] == '.' && start[1] == '.')){
        snprintf(filename, capacity, "download.pkg"); return;
    }
    if(n >= capacity) n = capacity - 1;
    for(i = 0; i < n; ++i){
        char ch = start[i];
        filename[i] = ((ch >= 'a' && ch <= 'z') ||
                       (ch >= 'A' && ch <= 'Z') ||
                       (ch >= '0' && ch <= '9') ||
                       ch == '.' || ch == '_' || ch == '-') ? ch : '_';
    }
    filename[n] = '\0';
    if(filename[0] == '\0') snprintf(filename, capacity, "download.pkg");
}

static int ensure_directory(const char *path){
    struct stat info;
    if (mkdir(path, 0755) == 0) return 1;
    if (errno != EEXIST) return 0;
    return stat(path, &info) == 0 && S_ISDIR(info.st_mode);
}

static int begin_download(TimHttpDownload *download, const char *url,
                          char *message, size_t messageSize){
    char filename[96], destination[1024];
    if (download == NULL || message == NULL || messageSize == 0) return -1;
    if (download->state == TIM_HTTP_DOWNLOADING ||
        download->state == TIM_HTTP_CONNECTING) {
        snprintf(message, messageSize, "A DOWNLOAD IS ALREADY ACTIVE");
        return -1;
    }
    if(!tim_http_url_is_valid(url)){
        snprintf(message, messageSize, "INVALID URL: USE HTTP OR HTTPS");
        return -1;
    }
    if (!ensure_directory("/data/Tim") ||
        !ensure_directory("/data/Tim/downloads")) {
        snprintf(message, messageSize, "CANNOT CREATE /DATA/TIM/DOWNLOADS");
        return -1;
    }
    url_filename(url, filename, sizeof(filename));
    if(snprintf(destination, sizeof(destination), "/data/Tim/downloads/%s",
                filename) >= (int)sizeof(destination)){
        snprintf(message, messageSize, "DESTINATION PATH TOO LONG");
        return -1;
    }
    if(tim_http_download_start(download, url, destination) != 0){
        snprintf(message, messageSize, "%s",
                 download->error[0] ? download->error : "DOWNLOAD START FAILED");
        return -1;
    }
    snprintf(message, messageSize, "DOWNLOAD STARTED");
    return 0;
}

static const char *download_state_text(const TimHttpDownload *d){
    if(d == NULL) return "NO DOWNLOAD";
    switch(d->state){
        case TIM_HTTP_CONNECTING: return "CONNECTING";
        case TIM_HTTP_DOWNLOADING: return "DOWNLOADING";
        case TIM_HTTP_COMPLETE: return "DOWNLOAD COMPLETE";
        case TIM_HTTP_ERROR: return "DOWNLOAD ERROR";
        case TIM_HTTP_CANCELLED: return "DOWNLOAD CANCELLED";
        default: return "NO DOWNLOAD";
    }
}
int main(){
    setvbuf(stdout,NULL,_IONBF,0);
    /*
     * scePadOpen needs the active PS4 user ID; literal 0 is not reliable.
     * Resolve the logged-in user before opening the controller.
     */
    int userId = -1;
    int pad = -1;
    (void)sceUserServiceInitialize(NULL);
    if (sceUserServiceGetInitialUser(&userId) == 0 && userId >= 0) {
        (void)scePadInit();
        pad = scePadOpen(userId, ORBIS_PAD_PORT_TYPE_STANDARD, 0, NULL);
        if (pad < 0)
            pad = scePadGetHandle(userId, ORBIS_PAD_PORT_TYPE_STANDARD, 0);
    }
    /* OpenOrbis exposes this initializer as void; IME init reports failure. */
    sceCommonDialogInitialize();
    g_common_dialog_ready = 1;
    Scene2D*scene=new Scene2D(1920,1080,4);
    if(!scene->Init(0x1200000,2)){
        delete scene;
        return 1;
    }
    scene->SetActiveFrameBuffer(0);
    const Color white=rgb(235,242,255),muted=rgb(135,157,183),cyan=rgb(50,220,220);
    int frame=0,selected=0,tab=0,previous=0,dialogOpen=0;
    TimHttpDownload download; memset(&download,0,sizeof(download));
    download.connection_id=-1; download.request_id=-1;
    wchar_t urlInput[2048]; char urlAscii[2048];
    char statusMessage[256]="READY - SELECT ENTER PKG URL";
    if (!g_common_dialog_ready)
        snprintf(statusMessage,sizeof(statusMessage),"SYSTEM URL KEYBOARD UNAVAILABLE");
    const char*games[]={"ROCKET LEAGUE","BRAWLHALLA","LOVE-GAMES1 PS4","ENTER PKG URL"};
    const char*types[]={"NO VERIFIED URL","NO VERIFIED URL","OPEN PS4 GAMES SECTION","HTTP OR HTTPS"};
    for(;;){
        OrbisPadData state; memset(&state,0,sizeof(state));
        if(pad>=0)scePadReadState(pad,&state);
        int buttons=(int)state.buttons;
        if(!dialogOpen){
            if((buttons&ORBIS_PAD_BUTTON_UP)&&!(previous&ORBIS_PAD_BUTTON_UP))selected=(selected+3)%4;
            if((buttons&ORBIS_PAD_BUTTON_DOWN)&&!(previous&ORBIS_PAD_BUTTON_DOWN))selected=(selected+1)%4;
            if((buttons&ORBIS_PAD_BUTTON_LEFT)&&!(previous&ORBIS_PAD_BUTTON_LEFT))tab=0;
            if((buttons&ORBIS_PAD_BUTTON_RIGHT)&&!(previous&ORBIS_PAD_BUTTON_RIGHT))tab=1;
            if((buttons&ORBIS_PAD_BUTTON_CROSS)&&!(previous&ORBIS_PAD_BUTTON_CROSS)){
                if(tab==0&&selected==3){
                    if(open_url_dialog(urlInput,sizeof(urlInput)/sizeof(urlInput[0]))>=0)
                        dialogOpen=1;
                    else snprintf(statusMessage,sizeof(statusMessage),"COULD NOT OPEN URL KEYBOARD");
                }else if(tab==0&&selected==2){
                    int browserResult = sceSystemServiceLaunchWebBrowser("https://love-games1.net/load/igry_dlja_konsolej/igry_dlja_playstation_4/42", NULL);
                    snprintf(statusMessage,sizeof(statusMessage), browserResult < 0 ? "BROWSER LAUNCH FAILED" : "OPENING LOVE-GAMES1 IN PS4 BROWSER");
                }else if(tab==0){
                    snprintf(statusMessage,sizeof(statusMessage),"NO VERIFIED DIRECT URL FOR THIS TITLE");
                }
            }
            if((buttons&ORBIS_PAD_BUTTON_CIRCLE)&&!(previous&ORBIS_PAD_BUTTON_CIRCLE)){
                if(tab==1&&download.state==TIM_HTTP_DOWNLOADING){
                    tim_http_download_cancel(&download);
                    snprintf(statusMessage,sizeof(statusMessage),"DOWNLOAD CANCELLED");
                }
            }
        }
        previous=buttons;

        if(dialogOpen){
            int dialogResult=finish_url_dialog(urlInput,
                sizeof(urlInput)/sizeof(urlInput[0]),urlAscii,sizeof(urlAscii));
            if(dialogResult!=0){
                dialogOpen=0;
                if(dialogResult>0) (void)begin_download(&download,urlAscii,statusMessage,sizeof(statusMessage));
                else snprintf(statusMessage,sizeof(statusMessage),"URL ENTRY CANCELLED");
            }
        }
        if(download.state==TIM_HTTP_DOWNLOADING){
            int step=tim_http_download_step(&download,8192);
            if(step==0)snprintf(statusMessage,sizeof(statusMessage),"DOWNLOAD COMPLETE");
            else if(step<0)snprintf(statusMessage,sizeof(statusMessage),"%s",
                download.error[0]?download.error:"DOWNLOAD FAILED");
        }

        /* Fill the frame once; avoid clearing to white and repainting it dark. */
        rect(scene,0,0,1920,1080,8,14,25);
        rect(scene,0,0,1920,10,50,220,220);
        rect(scene,100,70,1720,130,20,33,52);
        txt(scene,"TIM",135,100,8,cyan); txt(scene,"PS4 GAME LIBRARY",390,100,4,white);
        txt(scene,"CATALOG",140,240,3,tab==0?cyan:muted);
        txt(scene,"DOWNLOADS",410,240,3,tab==1?cyan:muted);
        rect(scene,120,290,1680,3,40,70,92);
        if(tab==0){
            card(scene,120,335,390,300,games[0],types[0],selected==0);
            card(scene,535,335,390,300,games[1],types[1],selected==1);
            card(scene,950,335,390,300,games[2],types[2],selected==2);
            card(scene,1365,335,390,300,games[3],types[3],selected==3);
            rect(scene,120,680,1680,200,20,33,52);
            txt(scene,"CROSS ON LOVE-GAMES1 OPENS PS4 BROWSER",155,720,3,white);
            txt(scene,"ONLY DOWNLOAD PACKAGES YOU ARE AUTHORIZED TO USE",155,785,2,muted);
            txt(scene,statusMessage,155,830,2,cyan);
        }else{
            rect(scene,120,335,1680,545,20,33,52);
            txt(scene,"DOWNLOAD STATUS",160,375,4,cyan);
            txt(scene,download_state_text(&download),160,450,4,white);
            if(download.state==TIM_HTTP_DOWNLOADING){
                char progress[96];
                if(download.total_bytes>0){
                    unsigned int pct=(unsigned int)(((long double)download.received_bytes*100.0L)/(long double)download.total_bytes);
                    if(pct>100)pct=100;
                    snprintf(progress,sizeof(progress),"PROGRESS %u",pct);
                }else snprintf(progress,sizeof(progress),"RECEIVED %llu BYTES",(unsigned long long)download.received_bytes);
                txt(scene,progress,160,530,3,white);
                txt(scene,"BYTES RECEIVED / TOTAL SHOWN WHEN AVAILABLE",160,590,2,muted);
            }else if(download.state==TIM_HTTP_COMPLETE){
                txt(scene,"FILE SAVED TO DATA TIM DOWNLOADS",160,530,3,white);
            }else if(download.state==TIM_HTTP_ERROR){
                txt(scene,download.error,160,530,2,white);
            }else{
                txt(scene,statusMessage,160,530,2,muted);
            }
            txt(scene,"CIRCLE - CANCEL ACTIVE DOWNLOAD",160,720,3,white);
            txt(scene,"LEFT RIGHT - SWITCH TAB",160,800,2,muted);
        }
        txt(scene,"TIM - NATIVE PS4 APPLICATION",120,950,2,muted);
        scene->SubmitFlip(frame);scene->FrameWait(frame);scene->FrameBufferSwap();++frame;
    }
    return 0;
}
