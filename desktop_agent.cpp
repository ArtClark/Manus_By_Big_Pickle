#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <gdiplus.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>

#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

using namespace Gdiplus;

#define TIMEOUT_SEC 5
#define VERSION "1.0.0"

/* ============================= MINIMAL JSON ============================= */
typedef enum { JT_OBJ, JT_ARR, JT_STR, JT_NUM, JT_BOOL, JT_NULL } JType;

typedef struct JVal {
    JType type;
    char *key;
    union {
        struct { struct JVal **items; int count; } children;
        char *s;
        double n;
    };
    struct JVal *next;
} JVal;

static JVal *j_alloc(JType t) { JVal *v = (JVal*)calloc(1, sizeof(JVal)); v->type = t; return v; }
static JVal *j_obj(void) { return j_alloc(JT_OBJ); }
static JVal *j_arr(void) { return j_alloc(JT_ARR); }
static JVal *j_str(const char *s) { JVal *v = j_alloc(JT_STR); v->s = _strdup(s); return v; }
static JVal *j_num(double n) { JVal *v = j_alloc(JT_NUM); v->n = n; return v; }
static JVal *j_bool(int b) { JVal *v = j_alloc(JT_BOOL); v->n = (double)b; return v; }

static void j_append(JVal *p, JVal *c) {
    p->children.items = (JVal**)realloc(p->children.items, (p->children.count+1)*sizeof(JVal*));
    p->children.items[p->children.count++] = c;
}

static void j_set(JVal *o, const char *k, JVal *v) { v->key = _strdup(k); j_append(o, v); }
static void j_set_str(JVal *o, const char *k, const char *v) { j_set(o, k, j_str(v)); }
static void j_set_num(JVal *o, const char *k, double v) { j_set(o, k, j_num(v)); }

static JVal *j_get(const JVal *o, const char *k) {
    for (int i=0; i<o->children.count; i++)
        if (o->children.items[i]->key && !strcmp(o->children.items[i]->key, k))
            return o->children.items[i];
    return NULL;
}

static const char *j_as_str(const JVal *v) { return v&&v->type==JT_STR?v->s:""; }
static double j_as_num(const JVal *v) { return v&&v->type==JT_NUM?v->n:0.0; }
// (j_check_str intentionally omitted)

static void j_free(JVal *v) {
    if (!v) return;
    free(v->key);
    if (v->type==JT_STR) { free(v->s); return; }
    if (v->type==JT_NUM || v->type==JT_BOOL || v->type==JT_NULL) return;
    for (int i=0; i<v->children.count; i++) j_free(v->children.items[i]);
    free(v->children.items); free(v);
}

/* parser */
static const char *j_skip(const char *p) { while (*p&&(unsigned char)*p<=32) p++; return p; }
static JVal *j_parse_val(const char **pp);

static char *j_parse_str(const char **pp) {
    const char *p=j_skip(*pp); if (*p!='"') return NULL; p++;
    size_t cap=64, len=0; char *buf=(char*)malloc(cap);
    while (*p && *p!='"') {
        if (*p=='\\') { p++;
            char c=0;
            switch(*p) {
                case'"':c='"';break; case'\\':c='\\';break; case'/':c='/';break;
                case'b':c='\b';break; case'f':c='\f';break; case'n':c='\n';break;
                case'r':c='\r';break; case't':c='\t';break;
                case'u':{unsigned u=0;for(int i=0;i<4;i++){p++;u=(u<<4)+(*p>='a'?*p-'a'+10:*p>='A'?*p-'A'+10:*p-'0');}
                    if(u<0x80)c=(char)u; else if(u<0x800){buf[len++]=(char)(0xC0|(u>>6));c=(char)(0x80|(u&0x3F));}
                    else{buf[len++]=(char)(0xE0|(u>>12));buf[len++]=(char)(0x80|((u>>6)&0x3F));c=(char)(0x80|(u&0x3F));}
                    break;}
                default:c=*p;break;
            } if(c) buf[len++]=c; if(*p)p++;
        } else buf[len++]=*p++;
        if(len>=cap-4){cap*=2;buf=(char*)realloc(buf,cap);}
    }
    if(*p=='"')p++; buf[len]=0; *pp=p; return buf;
}

static JVal *j_parse_obj(const char **pp) {
    const char *p=j_skip(*pp); if(*p!='{')return NULL; p++;
    JVal *o=j_obj(); p=j_skip(p);
    if(*p=='}'){*pp=p+1;return o;}
    while(*p){
        p=j_skip(p); char *key=j_parse_str(&p); if(!key)break;
        p=j_skip(p); if(*p!=':'){free(key);break;} p++;
        JVal *v=j_parse_val(&p); if(!v){free(key);break;}
        v->key=key; j_append(o,v);
        p=j_skip(p); if(*p==',')p++; else if(*p=='}')break; else break;
    }
    if(*p=='}')p++; *pp=p; return o;
}

static JVal *j_parse_arr(const char **pp) {
    const char *p=j_skip(*pp); if(*p!='[')return NULL; p++;
    JVal *a=j_arr(); p=j_skip(p);
    if(*p==']'){*pp=p+1;return a;}
    while(*p){JVal *v=j_parse_val(&p); if(!v)break; j_append(a,v);
        p=j_skip(p); if(*p==',')p++; else if(*p==']')break; else break;}
    if(*p==']')p++; *pp=p; return a;
}

static JVal *j_parse_val(const char **pp) {
    const char *p=j_skip(*pp);
    if(*p=='{')return j_parse_obj(pp); if(*p=='[')return j_parse_arr(pp);
    if(*p=='"'){char *s=j_parse_str(pp);JVal*v=s?j_str(s):NULL;free(s);return v;}
    if(*p=='t'&&*(p+1)=='r'&&*(p+2)=='u'&&*(p+3)=='e'){*pp=p+4;return j_bool(1);}
    if(*p=='f'&&*(p+1)=='a'&&*(p+2)=='l'&&*(p+3)=='s'&&*(p+4)=='e'){*pp=p+5;return j_bool(0);}
    if(*p=='n'&&*(p+1)=='u'&&*(p+2)=='l'&&*(p+3)=='l'){*pp=p+4;return j_alloc(JT_NULL);}
    if(*p=='-'||(*p>='0'&&*p<='9')){char*e;double d=strtod(p,&e);if(e!=p){*pp=e;return j_num(d);}}
    return NULL;
}

static JVal *j_parse(const char *s) { return j_parse_val(&s); }

/* serializer */
static void j_ser_inner(const JVal *v, char **b, size_t *l, size_t *c) {
    if(*l+1>=*c){*c=*c?*c*2:256;*b=(char*)realloc(*b,*c);}
    if(!v){memcpy(*b+*l,"null",4);*l+=4;return;}
    char tmp[64];
    switch(v->type){
        case JT_NULL: memcpy(*b+*l,"null",4);*l+=4; break;
        case JT_BOOL: {const char *s=v->n?"true":"false";size_t sl=strlen(s); while(*l+sl+1>=*c){*c*=2;*b=(char*)realloc(*b,*c);} memcpy(*b+*l,s,sl);*l+=sl; break;}
        case JT_NUM: {if(v->n==(double)(long long)v->n)sprintf(tmp,"%lld",(long long)v->n);else sprintf(tmp,"%.17g",v->n); size_t sl=strlen(tmp); while(*l+sl+1>=*c){*c*=2;*b=(char*)realloc(*b,*c);} memcpy(*b+*l,tmp,sl);*l+=sl; break;}
        case JT_STR: {(*b)[(*l)++]='"';
            for(const char *sp=v->s;*sp;sp++){unsigned char ch=(unsigned char)*sp;
                if(ch=='"'||ch=='\\'||ch<32){if(*l+8>=*c){*c*=2;*b=(char*)realloc(*b,*c);}
                    switch(ch){case'"':(*b)[(*l)++]='\\';\
(*b)[(*l)++]='"';break;case'\\':(*b)[(*l)++]='\\';(*b)[(*l)++]='\\';break;
                        case'\b':(*b)[(*l)++]='\\';(*b)[(*l)++]='b';break;
                        case'\f':(*b)[(*l)++]='\\';(*b)[(*l)++]='f';break;
                        case'\n':(*b)[(*l)++]='\\';(*b)[(*l)++]='n';break;
                        case'\r':(*b)[(*l)++]='\\';(*b)[(*l)++]='r';break;
                        case'\t':(*b)[(*l)++]='\\';(*b)[(*l)++]='t';break;
                        default:sprintf(tmp,"\\u%04x",ch);size_t sl=strlen(tmp);while(*l+sl+1>=*c){*c*=2;*b=(char*)realloc(*b,*c);}memcpy(*b+*l,tmp,sl);*l+=sl;break;}
                } else {(*b)[(*l)++]=ch;if(*l+1>=*c){*c*=2;*b=(char*)realloc(*b,*c);}}}
            (*b)[(*l)++]='"'; break;}
        case JT_ARR: {(*b)[(*l)++]='['; for(int i=0;i<v->children.count;i++){if(i>0)(*b)[(*l)++]=','; j_ser_inner(v->children.items[i],b,l,c);} (*b)[(*l)++]=']'; break;}
        case JT_OBJ: {(*b)[(*l)++]='{'; for(int i=0;i<v->children.count;i++){if(i>0)(*b)[(*l)++]=','; JVal*c2=v->children.items[i]; j_ser_inner(j_str(c2->key),b,l,c);(*b)[(*l)++]=':'; j_ser_inner(c2,b,l,c);} (*b)[(*l)++]='}'; break;}
    }
    if(*l+1>=*c){*c*=2;*b=(char*)realloc(*b,*c);}(*b)[*l]=0;
}

static char *j_ser(const JVal *v) { char *b=NULL; size_t l=0,c=0; j_ser_inner(v,&b,&l,&c); return b; }

/* ============================= MCP TRANSPORT ============================= */
static char *read_msg(void) {
    HANDLE h=GetStdHandle(STD_INPUT_HANDLE);
    unsigned long cl=0; char line[4096]; size_t pos=0;
    while(1){
        char c; DWORD r; if(!ReadFile(h,&c,1,&r,NULL)||r==0)return NULL;
        if(c=='\r')continue;
        if(c=='\n'){if(pos==0)break;line[pos]=0;if(sscanf(line,"Content-Length: %lu",&cl)==1){}pos=0;}
        else{if(pos<sizeof(line)-1)line[pos++]=c;}
    }
    if(cl==0)return NULL;
    char *body=(char*)malloc(cl+1); DWORD total=0;
    while(total<cl){DWORD r;if(!ReadFile(h,body+total,cl-total,&r,NULL)||r==0){free(body);return NULL;}total+=r;}
    body[cl]=0; return body;
}

static void write_msg(const char *json) {
    unsigned long len=(unsigned long)strlen(json); char hdr[64];
    int hl=sprintf(hdr,"Content-Length: %lu\r\n\r\n",len);
    HANDLE ho=GetStdHandle(STD_OUTPUT_HANDLE); DWORD w;
    WriteFile(ho,hdr,hl,&w,NULL); WriteFile(ho,json,len,&w,NULL);
}

static void send_resp(int id, JVal *result) {
    JVal *r=j_obj();
    j_set_str(r,"jsonrpc","2.0");
    j_set_num(r,"id",(double)id);
    j_set(r,"result",result?result:j_obj());
    char *s=j_ser(r);
    write_msg(s); free(s); j_free(r);
}

static void send_err(int id, int code, const char *msg) {
    JVal *e=j_obj(); j_set_num(e,"code",code); j_set_str(e,"message",msg);
    JVal *r=j_obj(); j_set_str(r,"jsonrpc","2.0"); j_set_num(r,"id",(double)id); j_set(r,"error",e);
    char *s=j_ser(r); write_msg(s); free(s); j_free(r);
}

/* ============================= DISPLAY INFO ============================= */
static int virt_w, virt_h, virt_x, virt_y;

static void init_display(void) {
    virt_x=GetSystemMetrics(SM_XVIRTUALSCREEN);
    virt_y=GetSystemMetrics(SM_YVIRTUALSCREEN);
    virt_w=GetSystemMetrics(SM_CXVIRTUALSCREEN);
    virt_h=GetSystemMetrics(SM_CYVIRTUALSCREEN);
}

/* ============================= BASE64 ============================= */
static char *b64enc(const BYTE *d, DWORD len, DWORD *olen) {
    static const char b64[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    DWORD elen=4*((len+2)/3); char *out=(char*)malloc(elen+1); DWORD i,j=0;
    for(i=0;i<len;i+=3){
        int n=((int)(len-i)<3)?(int)(len-i):3; DWORD val=(BYTE)d[i]<<16;
        if(n>1)val|=(BYTE)d[i+1]<<8; if(n>2)val|=(BYTE)d[i+2];
        out[j++]=b64[(val>>18)&0x3F]; out[j++]=b64[(val>>12)&0x3F];
        out[j++]=n>1?b64[(val>>6)&0x3F]:'='; out[j++]=n>2?b64[val&0x3F]:'=';
    }
    out[j]=0; if(olen)*olen=j; return out;
}

/* ============================= TOOL: SCREENSHOT (GDI+ PNG) ============================= */
static CLSID png_clsid;
static int png_clsid_init=0;

static JVal *tool_screenshot(void) {
    if(!png_clsid_init){
        UINT cnt,sz; GetImageEncodersSize(&cnt,&sz);
        ImageCodecInfo *enc=(ImageCodecInfo*)malloc(sz);
        GetImageEncoders(cnt,sz,enc);
        for(UINT i=0;i<cnt;i++){if(wcscmp(enc[i].MimeType,L"image/png")==0){png_clsid=enc[i].Clsid;break;}}
        free(enc); png_clsid_init=1;
    }
    HDC hdc=GetDC(NULL); HDC mem=CreateCompatibleDC(hdc);
    HBITMAP hbm=CreateCompatibleBitmap(hdc,virt_w,virt_h);
    SelectObject(mem,hbm);
    BitBlt(mem,0,0,virt_w,virt_h,hdc,virt_x,virt_y,SRCCOPY);
    ReleaseDC(NULL,hdc); DeleteDC(mem);

    Bitmap bmp(hbm,(HPALETTE)NULL); DeleteObject(hbm);
    IStream *st=NULL; CreateStreamOnHGlobal(NULL,TRUE,&st);
    bmp.Save(st,&png_clsid,NULL);

    STATSTG stats; st->Stat(&stats,STATFLAG_NONAME);
    LARGE_INTEGER z={}; st->Seek(z,STREAM_SEEK_SET,NULL);
    DWORD sz=stats.cbSize.LowPart; BYTE *img=(BYTE*)malloc(sz);
    st->Read(img,sz,&sz); st->Release();

    DWORD b64l; char *b64=b64enc(img,sz,&b64l); free(img);
    char cap[128]; sprintf(cap,"Screenshot captured (%dx%d)",virt_w,virt_h);

    JVal *c=j_arr();
    JVal *t=j_obj(); j_set_str(t,"type","text"); j_set_str(t,"text",cap); j_append(c,t);
    JVal *im=j_obj(); j_set_str(im,"type","image"); j_set_str(im,"data",b64); j_set_str(im,"mimeType","image/png"); j_append(c,im);
    free(b64);
    JVal *r=j_obj(); j_set(r,"content",c); return r;
}

/* ============================= TOOLS: MOUSE ============================= */
static JVal *tool_mouse_move(int x, int y) {
    SetCursorPos(x,y);
    char msg[64]; sprintf(msg,"Mouse moved to (%d,%d)",x,y);
    JVal *c=j_arr(); JVal *t=j_obj(); j_set_str(t,"type","text"); j_set_str(t,"text",msg); j_append(c,t);
    JVal *r=j_obj(); j_set(r,"content",c); return r;
}

static JVal *tool_mouse_down(const char *btn) {
    INPUT inp={0}; inp.type=INPUT_MOUSE;
    if(!strcmp(btn,"left")) inp.mi.dwFlags=MOUSEEVENTF_LEFTDOWN;
    else if(!strcmp(btn,"right")) inp.mi.dwFlags=MOUSEEVENTF_RIGHTDOWN;
    else return NULL;
    SendInput(1,&inp,sizeof(INPUT));
    char msg[64]; sprintf(msg,"Mouse %s pressed",btn);
    JVal *c=j_arr(); JVal *t=j_obj(); j_set_str(t,"type","text"); j_set_str(t,"text",msg); j_append(c,t);
    JVal *r=j_obj(); j_set(r,"content",c); return r;
}

static JVal *tool_mouse_up(const char *btn) {
    INPUT inp={0}; inp.type=INPUT_MOUSE;
    if(!strcmp(btn,"left")) inp.mi.dwFlags=MOUSEEVENTF_LEFTUP;
    else if(!strcmp(btn,"right")) inp.mi.dwFlags=MOUSEEVENTF_RIGHTUP;
    else return NULL;
    SendInput(1,&inp,sizeof(INPUT));
    char msg[64]; sprintf(msg,"Mouse %s released",btn);
    JVal *c=j_arr(); JVal *t=j_obj(); j_set_str(t,"type","text"); j_set_str(t,"text",msg); j_append(c,t);
    JVal *r=j_obj(); j_set(r,"content",c); return r;
}

static JVal *tool_mouse_click(const char *btn, int hx, int hy, int x, int y) {
    if(hx&&hy) SetCursorPos(x,y);
    INPUT in[2]={0}; in[0].type=INPUT_MOUSE; in[1].type=INPUT_MOUSE;
    DWORD df,uf;
    if(!strcmp(btn,"left")){df=MOUSEEVENTF_LEFTDOWN;uf=MOUSEEVENTF_LEFTUP;}
    else if(!strcmp(btn,"right")){df=MOUSEEVENTF_RIGHTDOWN;uf=MOUSEEVENTF_RIGHTUP;}
    else return NULL;
    in[0].mi.dwFlags=df; in[1].mi.dwFlags=uf;
    SendInput(2,in,sizeof(INPUT));
    char msg[64]; sprintf(msg,"Mouse %s clicked",btn);
    JVal *c=j_arr(); JVal *t=j_obj(); j_set_str(t,"type","text"); j_set_str(t,"text",msg); j_append(c,t);
    JVal *r=j_obj(); j_set(r,"content",c); return r;
}

static JVal *tool_get_cursor_pos(void) {
    POINT pt; GetCursorPos(&pt);
    char msg[64]; sprintf(msg,"Cursor position: (%d,%d)",(int)pt.x,(int)pt.y);
    JVal *c=j_arr(); JVal *t=j_obj(); j_set_str(t,"type","text"); j_set_str(t,"text",msg); j_append(c,t);
    JVal *r=j_obj(); j_set(r,"content",c); return r;
}

/* ============================= TOOLS: KEYBOARD ============================= */
static void send_keys(BYTE ctrl, BYTE key) {
    INPUT in[4]={0};
    in[0].type=INPUT_KEYBOARD; in[0].ki.wVk=ctrl;
    in[1].type=INPUT_KEYBOARD; in[1].ki.wVk=key;
    in[2].type=INPUT_KEYBOARD; in[2].ki.wVk=key; in[2].ki.dwFlags=KEYEVENTF_KEYUP;
    in[3].type=INPUT_KEYBOARD; in[3].ki.wVk=ctrl; in[3].ki.dwFlags=KEYEVENTF_KEYUP;
    SendInput(4,in,sizeof(INPUT));
}

static char *read_clipboard(void) {
    if(!OpenClipboard(NULL)) return _strdup("");
    HANDLE h=GetClipboardData(CF_UNICODETEXT);
    if(!h){CloseClipboard();return _strdup("");}
    wchar_t *w=(wchar_t*)GlobalLock(h);
    if(!w){CloseClipboard();return _strdup("");}
    int len=WideCharToMultiByte(CP_UTF8,0,w,-1,NULL,0,NULL,NULL);
    char *t=(char*)malloc(len); WideCharToMultiByte(CP_UTF8,0,w,-1,t,len,NULL,NULL);
    GlobalUnlock(h); CloseClipboard(); return t;
}

static void write_clipboard(const char *text) {
    int wlen=MultiByteToWideChar(CP_UTF8,0,text,-1,NULL,0);
    wchar_t *w=(wchar_t*)malloc(wlen*sizeof(wchar_t));
    MultiByteToWideChar(CP_UTF8,0,text,-1,w,wlen);
    if(OpenClipboard(NULL)){
        EmptyClipboard();
        HGLOBAL hg=GlobalAlloc(GMEM_MOVEABLE,wlen*sizeof(wchar_t));
        wchar_t *dst=(wchar_t*)GlobalLock(hg);
        memcpy(dst,w,wlen*sizeof(wchar_t));
        GlobalUnlock(hg); SetClipboardData(CF_UNICODETEXT,hg);
        CloseClipboard();
    }
    free(w);
}

static JVal *tool_copy(void) {
    send_keys(VK_CONTROL,0x43); Sleep(300);
    char *text=read_clipboard();
    JVal *c=j_arr(); JVal *t=j_obj(); j_set_str(t,"type","text"); j_set_str(t,"text",text); j_append(c,t);
    JVal *r=j_obj(); j_set(r,"content",c); free(text); return r;
}

static JVal *tool_paste(const char *text) {
    if(text&&*text) write_clipboard(text);
    send_keys(VK_CONTROL,0x56);
    JVal *c=j_arr(); JVal *t=j_obj(); j_set_str(t,"type","text"); j_set_str(t,"text","Paste performed"); j_append(c,t);
    JVal *r=j_obj(); j_set(r,"content",c); return r;
}

/* ============================= SAFETY & LOGGING ============================= */
typedef struct {
    volatile int btn_down; /* 0=none,1=left,2=right */
    time_t down_time;
    int timeout;
    volatile int running;
    HANDLE thread;
    CRITICAL_SECTION lock;
    FILE *log;
} Safety;

static Safety safety={0,0,TIMEOUT_SEC,0,NULL,{0},NULL};

static void log_event(const char *tool, const char *fmt, ...) {
    if(!safety.log) return;
    time_t n=time(NULL); struct tm *tm=localtime(&n);
    fprintf(safety.log,"[%04d-%02d-%02d %02d:%02d:%02d] %s: ",tm->tm_year+1900,tm->tm_mon+1,tm->tm_mday,tm->tm_hour,tm->tm_min,tm->tm_sec,tool);
    va_list ap; va_start(ap,fmt); vfprintf(safety.log,fmt,ap); va_end(ap);
    fputc('\n',safety.log); fflush(safety.log);
}

static DWORD WINAPI safety_thread(LPVOID) {
    while(safety.running){
        Sleep(1000);
        EnterCriticalSection(&safety.lock);
        if(safety.btn_down){
            time_t n=time(NULL);
            if(difftime(n,safety.down_time)>=safety.timeout){
                INPUT inp={0}; inp.type=INPUT_MOUSE;
                inp.mi.dwFlags=safety.btn_down==1?MOUSEEVENTF_LEFTUP:MOUSEEVENTF_RIGHTUP;
                SendInput(1,&inp,sizeof(INPUT));
                log_event("SAFETY","Auto-released %s (timeout %ds)",safety.btn_down==1?"left":"right",safety.timeout);
                safety.btn_down=0;
            }
        }
        LeaveCriticalSection(&safety.lock);
    }
    return 0;
}

static void safety_init(void) {
    InitializeCriticalSection(&safety.lock);
    safety.running=1;
    safety.thread=CreateThread(NULL,0,safety_thread,NULL,0,NULL);
    safety.log=fopen("desktop-agent.log","a");
    log_event("INIT","Desktop Agent MCP v%s starting",VERSION);
}

static void safety_shutdown(void) {
    safety.running=0;
    if(safety.thread){WaitForSingleObject(safety.thread,2000);CloseHandle(safety.thread);}
    EnterCriticalSection(&safety.lock);
    if(safety.btn_down){
        INPUT inp={0}; inp.type=INPUT_MOUSE;
        inp.mi.dwFlags=safety.btn_down==1?MOUSEEVENTF_LEFTUP:MOUSEEVENTF_RIGHTUP;
        SendInput(1,&inp,sizeof(INPUT));
    }
    LeaveCriticalSection(&safety.lock);
    DeleteCriticalSection(&safety.lock);
    if(safety.log){log_event("SHUTDOWN","Server shut down");fclose(safety.log);}
}

static void safety_track_down(const char *btn) {
    EnterCriticalSection(&safety.lock);
    safety.btn_down=!strcmp(btn,"left")?1:2;
    safety.down_time=time(NULL);
    LeaveCriticalSection(&safety.lock);
    log_event("INPUT","mouse_down(%s)",btn);
}

static void safety_track_up(const char *btn) {
    EnterCriticalSection(&safety.lock);
    int exp=!strcmp(btn,"left")?1:2;
    if(safety.btn_down==exp){
        double held=difftime(time(NULL),safety.down_time);
        log_event("INPUT","mouse_up(%s) held=%.0fs",btn,held);
        safety.btn_down=0;
    }
    LeaveCriticalSection(&safety.lock);
    log_event("INPUT","mouse_up(%s)",btn);
}

/* ============================= TOOL LIST BUILDER ============================= */
static JVal *build_tool_list(void) {
    JVal *a=j_arr();
#define T(n,d) do{JVal*t=j_obj();j_set_str(t,"name",n);j_set_str(t,"description",d);\
JVal*s=j_obj();j_set_str(s,"type","object");JVal*p=j_obj();JVal*rq=j_arr();
#define PN(k,t,d){JVal*x=j_obj();j_set_str(x,"type",t);j_set_str(x,"description",d);j_set(p,k,x);}
#define PE(k,d,...){JVal*x=j_obj();j_set_str(x,"type","string");j_set_str(x,"description",d);\
JVal*ev=j_arr();const char*evv[]={__VA_ARGS__};for(int _i=0;_i<(int)(sizeof(evv)/sizeof(evv[0]));_i++)j_append(ev,j_str(evv[_i]));j_set(x,"enum",ev);j_set(p,k,x);}
#define R(...){const char*rr[]={__VA_ARGS__};for(int _i=0;_i<(int)(sizeof(rr)/sizeof(rr[0]));_i++)j_append(rq,j_str(rr[_i]));}
#define E j_set(s,"properties",p);j_set(s,"required",rq);j_set(t,"inputSchema",s);j_append(a,t);}while(0)

    T("screenshot","Capture entire desktop (all monitors) as PNG. Returns image data the LLM can see.")
    E;

    T("mouse_move","Move mouse cursor to screen coordinates.")
    PN("x","integer","X coordinate"); PN("y","integer","Y coordinate");
    R("x","y");
    E;

    T("mouse_down","Press and hold mouse button. Safety auto-release after 5s.")
    PE("button","Mouse button","left","right");
    R("button");
    E;

    T("mouse_up","Release mouse button.")
    PE("button","Mouse button","left","right");
    R("button");
    E;

    T("mouse_click","Click mouse button (press+release). Optionally move first.")
    PE("button","Mouse button","left","right");
    PN("x","integer","Optional X to move before click"); PN("y","integer","Optional Y to move before click");
    R("button");
    E;

    T("get_cursor_position","Get current mouse cursor coordinates.")
    E;

    T("copy","Send Ctrl+C, return clipboard text.")
    E;

    T("paste","Send Ctrl+V. Optionally set clipboard text first.")
    PN("text","string","Optional text to write to clipboard before pasting");
    E;

#undef T
#undef PN
#undef PE
#undef R
#undef E
    return a;
}

/* ============================= DISPATCH ============================= */
static JVal *dispatch(const char *name, JVal *args) {
    if(!strcmp(name,"screenshot")) return tool_screenshot();
    if(!strcmp(name,"get_cursor_position")) return tool_get_cursor_pos();

    if(!strcmp(name,"mouse_move")){
        JVal *jx=j_get(args,"x"),*jy=j_get(args,"y");
        if(!jx||!jy) return NULL;
        return tool_mouse_move((int)j_as_num(jx),(int)j_as_num(jy));
    }
    if(!strcmp(name,"mouse_down")){
        const char *btn=j_as_str(j_get(args,"button"));
        if(!*btn) return NULL;
        JVal *r=tool_mouse_down(btn);
        if(r) safety_track_down(btn);
        return r;
    }
    if(!strcmp(name,"mouse_up")){
        const char *btn=j_as_str(j_get(args,"button"));
        if(!*btn) return NULL;
        safety_track_up(btn);
        return tool_mouse_up(btn);
    }
    if(!strcmp(name,"mouse_click")){
        const char *btn=j_as_str(j_get(args,"button"));
        if(!*btn) return NULL;
        JVal *jx=j_get(args,"x"),*jy=j_get(args,"y");
        return tool_mouse_click(btn,jx!=NULL,jy!=NULL,(int)j_as_num(jx),(int)j_as_num(jy));
    }
    if(!strcmp(name,"copy")) return tool_copy();
    if(!strcmp(name,"paste")){
        const char *text=j_as_str(j_get(args,"text"));
        return tool_paste(text);
    }
    return NULL;
}

/* ============================= MAIN ============================= */
static void print_help(void) {
    printf("desktop-agent  MCP server providing desktop GUI capabilities.\n");
    printf("Version: %s\n\n", VERSION);
    printf("Usage:  desktop-agent [--help|--version]\n\n");
    printf("When run without arguments, starts an MCP server over stdio.\n");
    printf("Intended for use as an MCP tool server with opencode or other MCP clients.\n\n");
    printf("Available tools (via JSON-RPC 2.0 tools/list and tools/call):\n");
    printf("  screenshot           Capture entire desktop as PNG\n");
    printf("  mouse_move           Move cursor to (x, y)\n");
    printf("  mouse_down           Press and hold button (left/right)\n");
    printf("  mouse_up             Release button (left/right)\n");
    printf("  mouse_click          Click button at optional (x, y)\n");
    printf("  get_cursor_position  Report current cursor coordinates\n");
    printf("  copy                 Send Ctrl+C, return clipboard text\n");
    printf("  paste                Send Ctrl+V, optionally set clipboard text\n\n");
    printf("Flags:\n");
    printf("  --help   Show this help\n");
    printf("  --version  Show version\n");
}

int main(int argc, char **argv) {
    if (argc > 1) {
        if (!strcmp(argv[1], "--help") || !strcmp(argv[1], "-h")) { print_help(); return 0; }
        if (!strcmp(argv[1], "--version") || !strcmp(argv[1], "-v")) { printf("%s\n", VERSION); return 0; }
        fprintf(stderr, "Unknown argument: %s. Use --help for usage.\n", argv[1]);
        return 1;
    }
    setvbuf(stdout,NULL,_IONBF,0);
    FILE *ef=freopen("desktop-agent-err.log","w",stderr);
    (void)ef;

    GdiplusStartupInput gsi;
    gsi.GdiplusVersion = 1;
    gsi.DebugEventCallback = NULL;
    gsi.SuppressBackgroundThread = FALSE;
    gsi.SuppressExternalCodecs = FALSE;
    ULONG_PTR gtoken; GdiplusStartup(&gtoken,&gsi,NULL);
    init_display();
    safety_init();
    fprintf(stderr,"Desktop Agent MCP v%s starting (virtual desktop: %dx%d)\n",VERSION,virt_w,virt_h);

    while(1){
        char *msg=read_msg();
        if(!msg) break;

        JVal *req=j_parse(msg); free(msg);
        if(!req){fprintf(stderr,"Parse error\n");continue;}

        JVal *jid=j_get(req,"id"), *jmeth=j_get(req,"method");
        if(!jmeth){j_free(req);continue;}

        const char *method=j_as_str(jmeth);
        int id=jid?(int)j_as_num(jid):0;

        if(!jid){
            if(!strcmp(method,"notifications/initialized")) log_event("MCP","Client initialized");
            else if(!strcmp(method,"notifications/cancelled")) log_event("MCP","Cancelled");
            j_free(req); continue;
        }

        if(!strcmp(method,"initialize")){
            JVal *res=j_obj();
            j_set_str(res,"protocolVersion","2024-11-05");
            JVal *cap=j_obj(); j_set(cap,"tools",j_obj()); j_set(res,"capabilities",cap);
            JVal *info=j_obj(); j_set_str(info,"name","desktop-agent"); j_set_str(info,"version",VERSION); j_set(res,"serverInfo",info);
            send_resp(id,res);
            log_event("MCP","Initialized (id=%d)",id);
        } else if(!strcmp(method,"tools/list")){
            JVal *res=j_obj(); j_set(res,"tools",build_tool_list());
            send_resp(id,res);
            log_event("MCP","tools/list (id=%d)",id);
        } else if(!strcmp(method,"tools/call")){
            JVal *jp=j_get(req,"params");
            const char *tn=j_as_str(j_get(jp,"name"));
            JVal *ja=j_get(jp,"arguments"); if(!ja)ja=j_obj();
            fprintf(stderr,"Tool call: %s\n",tn);
            log_event("TOOL","Call: %s",tn);
            JVal *result=dispatch(tn,ja);
            if(result){
                send_resp(id,result);
                log_event("TOOL","Success: %s",tn);
            } else {
                char em[256]; sprintf(em,"Invalid tool or args: %s",tn);
                send_err(id,-32602,em);
                log_event("TOOL","Error: %s",em);
            }
        } else {
            char em[256]; sprintf(em,"Method not found: %s",method);
            send_err(id,-32601,em);
        }
        j_free(req);
    }

    fprintf(stderr,"Stdin closed, exiting.\n");
    safety_shutdown();
    GdiplusShutdown(gtoken);
    fclose(stderr);
    return 0;
}
