#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ole2.h>
#include <gdiplus.h>
#include <stdio.h>
#pragma comment(lib, "gdiplus.lib")

using namespace Gdiplus;

static void add_round_rect(GraphicsPath *p, Rect r, int rad) {
    if (rad > r.Width/2) rad = r.Width/2;
    if (rad > r.Height/2) rad = r.Height/2;
    p->AddArc(r.X, r.Y, rad*2, rad*2, 180, 90);
    p->AddLine(r.X+rad, r.Y, r.X+r.Width-rad, r.Y);
    p->AddArc(r.X+r.Width-rad*2, r.Y, rad*2, rad*2, 270, 90);
    p->AddLine(r.X+r.Width, r.Y+rad, r.X+r.Width, r.Y+r.Height-rad);
    p->AddArc(r.X+r.Width-rad*2, r.Y+r.Height-rad*2, rad*2, rad*2, 0, 90);
    p->AddLine(r.X+r.Width-rad, r.Y+r.Height, r.X+rad, r.Y+r.Height);
    p->AddArc(r.X, r.Y+r.Height-rad*2, rad*2, rad*2, 90, 90);
    p->AddLine(r.X, r.Y+r.Height-rad, r.X, r.Y+rad);
    p->CloseFigure();
}

static int put_ico(const char *path, const unsigned char *entries[], const DWORD entry_sizes[], int display_sizes[], int count) {
    FILE *f = fopen(path, "wb"); if (!f) return -1;
    unsigned short hdr[] = { 0, 1, (unsigned short)count };
    fwrite(hdr, sizeof(hdr), 1, f);
    DWORD offset = 6 + count * 16;
    for (int i = 0; i < count; i++) {
        unsigned char ent[16] = { 0 };
        int ds = display_sizes[i];
        ent[0] = (ds >= 256) ? 0 : (unsigned char)ds;
        ent[1] = (ds >= 256) ? 0 : (unsigned char)ds;
        ent[4] = 1; ent[5] = 0;
        ent[6] = 32; ent[7] = 0;
        memcpy(ent+8, &entry_sizes[i], 4);
        memcpy(ent+12, &offset, 4);
        fwrite(ent, 16, 1, f);
        offset += entry_sizes[i];
    }
    for (int i = 0; i < count; i++)
        fwrite(entries[i], 1, entry_sizes[i], f);
    fclose(f); return 0;
}

// Resize bitmap to target size, save as PNG into memory, return size
static DWORD save_png_to_mem(Bitmap *src, int size, unsigned char **out) {
    Bitmap small(size, size, PixelFormat32bppARGB);
    Graphics g(&small);
    g.SetSmoothingMode(SmoothingModeHighQuality);
    g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
    g.DrawImage(src, 0, 0, size, size);

    IStream *st = NULL;
    if (CreateStreamOnHGlobal(NULL, TRUE, &st) != S_OK) return 0;
    CLSID pngId; CLSIDFromString(L"{557cf406-1a04-11d3-9a73-0000f81ef32e}", &pngId);
    small.Save(st, &pngId, NULL);
    HGLOBAL hg; GetHGlobalFromStream(st, &hg);
    DWORD sz = (DWORD)GlobalSize(hg);
    *out = (unsigned char*)malloc(sz);
    memcpy(*out, GlobalLock(hg), sz);
    GlobalUnlock(hg); st->Release();
    return sz;
}

int main(void) {
    GdiplusStartupInput gsi; gsi.GdiplusVersion = 1; gsi.DebugEventCallback = NULL;
    gsi.SuppressBackgroundThread = FALSE; gsi.SuppressExternalCodecs = FALSE;
    ULONG_PTR gtoken; GdiplusStartup(&gtoken, &gsi, NULL);

    Bitmap bmp(256, 256, PixelFormat32bppARGB);
    Graphics g(&bmp);
    g.SetSmoothingMode(SmoothingModeHighQuality);
    g.Clear(Color(0,0,0,0));

    // BG circle
    SolidBrush bgBr(Color(0x14,0x1D,0x35,0x57));
    g.FillEllipse(&bgBr, 14,14, 228,228);

    // Toolbox
    GraphicsPath bd; add_round_rect(&bd, Rect(64,80,128,96), 8);
    LinearGradientBrush bGr(Rect(64,80,128,96), Color(0xFF,0xE6,0x39,0x46), Color(0xFF,0xC1,0x12,0x1F), LinearGradientModeForwardDiagonal);
    g.FillPath(&bGr, &bd);
    SolidBrush sh(Color(0x1F,0,0,0));
    g.FillRectangle(&sh, 68,84,120,88);

    // Lid
    GraphicsPath ld; add_round_rect(&ld, Rect(58,70,140,20), 6);
    LinearGradientBrush lGr(Rect(58,70,140,20), Color(0xFF,0xD6,0x28,0x28), Color(0xFF,0x9B,0x22,0x26), LinearGradientModeVertical);
    g.FillPath(&lGr, &ld);

    // Handle
    GraphicsPath hn; add_round_rect(&hn, Rect(96,56,64,14), 7);
    SolidBrush hB(Color(0xFF,0x1D,0x35,0x57));
    g.FillPath(&hB, &hn);
    GraphicsPath lt; add_round_rect(&lt, Rect(114,64,28,12), 4);
    SolidBrush lB(Color(0xFF,0x78,0x00,0x00));
    g.FillPath(&lB, &lt);

    // Tools
    SolidBrush tB(Color(0xFF,0xA8,0xDA,0xDC));
    g.TranslateTransform(82,90); g.RotateTransform(-15);
    g.FillRectangle(&tB, -3,-14,6,28);
    g.FillEllipse(&tB, -6,-20,12,12);
    g.ResetTransform();
    g.TranslateTransform(160,90); g.RotateTransform(15);
    g.FillRectangle(&tB, -3,-12,6,24);
    SolidBrush hh(Color(0xFF,0xE0,0xC9,0xA6));
    g.FillRectangle(&hh, -7,-20,14,12);
    g.ResetTransform();

    // Details
    SolidBrush d1(Color(0x80,0x9B,0x22,0x26));
    g.FillRectangle(&d1, 74,94,108,4);
    SolidBrush d2(Color(0x40,0x9B,0x22,0x26));
    g.FillRectangle(&d2, 74,104,108,2);

    // Wrench emblem
    Pen wp(Color(0x99,0xFD,0xF0,0xD5), 2);
    g.DrawEllipse(&wp, 114,114,16,16);
    SolidBrush wb(Color(0x99,0xFD,0xF0,0xD5));
    g.FillRectangle(&wb, 120,128,4,14);

    // Hand palm
    SolidBrush hdB(Color(0xFF,0xFD,0xF0,0xD5));
    Pen hdP(Color(0xFF,0xC9,0xA9,0x6E), 1.5f);
    GraphicsPath pl;
    pl.AddBezier(176,128, 184,122, 204,126, 202,142);
    pl.AddBezier(202,142, 200,158, 196,168, 186,176);
    pl.AddBezier(186,176, 178,182, 162,186, 150,180);
    pl.AddBezier(150,180, 142,176, 136,164, 134,156);
    pl.AddBezier(134,156, 132,148, 134,138, 140,132);
    pl.AddBezier(140,132, 146,126, 158,128, 162,134);
    pl.AddBezier(162,134, 164,138, 162,144, 158,148);
    pl.AddBezier(158,148, 154,152, 148,152, 146,148);
    pl.CloseFigure();
    g.FillPath(&hdB, &pl); g.DrawPath(&hdP, &pl);

    // Wrist
    GraphicsPath wr;
    wr.StartFigure(); wr.AddBezier(150,180, 156,188, 166,204, 164,216);
    wr.AddBezier(164,216, 162,226, 146,224, 140,216);
    wr.AddBezier(140,216, 134,208, 132,196, 134,186);
    wr.CloseFigure();
    g.FillPath(&hdB, &wr);

    // Fingers
    struct { float x,y,rx,ry,rot; } ff[] = {
        {180,72,14,20,20}, {98,64,8,14,-10}, {116,62,8,14,-5},
        {134,62,8,14,5}, {152,64,7,12,15}
    };
    for (int i=0;i<5;i++) {
        GraphicsPath fp;
        fp.AddEllipse(int(ff[i].x-ff[i].rx), int(ff[i].y-ff[i].ry), int(ff[i].rx*2), int(ff[i].ry*2));
        Matrix m; m.RotateAt(ff[i].rot, PointF(ff[i].x, ff[i].y));
        fp.Transform(&m);
        g.FillPath(&hdB, &fp); g.DrawPath(&hdP, &fp);
    }

    // Save .png
    CLSID pngId;
    CLSIDFromString(L"{557cf406-1a04-11d3-9a73-0000f81ef32e}", &pngId);
    bmp.Save(L"manus.png", &pngId, NULL);
    printf("manus.png\n");

    // Save .ico with multiple resolutions
    int sizes[] = { 256, 64, 48, 32, 24, 16 };
    int count = sizeof(sizes)/sizeof(sizes[0]);
    const unsigned char *entries[8];
    DWORD entry_sizes[8];
    for (int i = 0; i < count; i++) {
        entry_sizes[i] = save_png_to_mem(&bmp, sizes[i], (unsigned char**)&entries[i]);
        printf("  %dx%d: %lu bytes\n", sizes[i], sizes[i], entry_sizes[i]);
    }
    put_ico("manus.ico", entries, entry_sizes, sizes, count);
    for (int i = 0; i < count; i++) free((void*)entries[i]);

    GdiplusShutdown(gtoken);
    return 0;
}
