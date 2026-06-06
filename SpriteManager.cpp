// SpriteManager.cpp
// 加载 PNG/JPG/GIF; PNG/JPG 用 CImage, GIF 用 GDI+ Image 多帧
#include "pch.h"
#include "SpriteManager.h"
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "msimg32.lib")

using namespace Gdiplus;

CSpriteManager g_sprites;

// 资源路径表(顺序对应 SpriteID 枚举)
// 一个 sprite 可能有两个候选: .png 优先,失败则尝试 .gif
struct SpritePath {
    const TCHAR* primary;
    const TCHAR* alt;  // NULL 表示无备选
};

static const SpritePath kPaths[SPR_COUNT] = {
    { _T("res\\player.png"),    NULL },
    { _T("res\\ghost.gif"),     _T("res\\ghost.png") },
    { _T("res\\tree.png"),      NULL },
    { _T("res\\bat.gif"),       _T("res\\bat.png") },
    { _T("res\\fox.gif"),       _T("res\\fox.png") },
    { _T("res\\wolf.gif"),      _T("res\\wolf.png") },
    { _T("res\\assassin.gif"),  _T("res\\assassin.png") },
    { _T("res\\wraith.png"),    NULL },
    { _T("res\\boss.png"),      NULL },
    { _T("res\\goblin.gif"),    _T("res\\goblin.png") },

    { _T("res\\boss1.png"),     _T("res\\boss.png") },   // 阶段1(备选旧 boss.png)
    { _T("res\\boss2.png"),     _T("res\\boss.png") },   // 阶段2
    { _T("res\\boss3.png"),     _T("res\\boss.png") },   // 阶段3(第二条命)

    { _T("res\\fireball.png"),  NULL },
    { _T("res\\iceshard.png"),  NULL },
    { _T("res\\lightning.png"), NULL },
    { _T("res\\poison.png"),    NULL },
    { _T("res\\seed.png"),      NULL },

    { _T("res\\item_potion.png"), NULL },
    { _T("res\\item_mana.png"),   NULL },
    { _T("res\\item_amulet.png"), NULL },
    { _T("res\\item_scroll.png"), NULL },
    { _T("res\\phoenix.gif"),   _T("res\\item_scroll.png") },  // 复活卷轴(备选镇灵卷轴图)

    { _T("res\\skill_ice.png"),   NULL },
    { _T("res\\skill_lightning.png"), NULL },
    { _T("res\\skill_storm.png"), NULL },
    { _T("res\\skill_heal.png"),  NULL },
    { _T("res\\arcane.png"),    _T("res\\skill_heal.png") },   // 奥术爆发(备选旧 heal 图)

    { _T("res\\menu_alter.png"), _T("res\\menu_bg.jpg") },
    { _T("res\\bg_ch1.png"),    NULL },
    { _T("res\\bg_ch2.png"),    NULL },
    { _T("res\\bg_ch3.png"),    NULL },
    { _T("res\\bg_ch4.png"),    NULL },
};

static BOOL FileExists2(const TCHAR* p)
{
    DWORD a = GetFileAttributes(p);
    return (a != INVALID_FILE_ATTRIBUTES) && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

static BOOL EndsWith(const TCHAR* s, const TCHAR* suf)
{
    int ls = (int)_tcslen(s), lf = (int)_tcslen(suf);
    if (ls < lf) return FALSE;
    return _tcsicmp(s + ls - lf, suf) == 0;
}

CSpriteManager::CSpriteManager()
    : m_gdiplusToken(0), m_gdiplusOn(FALSE)
{
    for (int i = 0; i < SPR_COUNT; ++i) {
        m_images[i] = NULL;
        m_loaded[i] = FALSE;
        m_gifs[i]   = NULL;
        m_gifFrameCount[i] = 0;
        m_gifCurFrame[i] = 0;
        m_gifLastTick[i] = 0;
        m_gifDelays[i] = NULL;
        m_gifW[i] = m_gifH[i] = 0;
    }
}

CSpriteManager::~CSpriteManager()
{
    Shutdown();
}

void CSpriteManager::Shutdown()
{
    for (int i = 0; i < SPR_COUNT; ++i) {
        if (m_images[i]) { delete m_images[i]; m_images[i] = NULL; }
        if (m_gifs[i])   { delete m_gifs[i];   m_gifs[i] = NULL; }
        if (m_gifDelays[i]) { delete[] m_gifDelays[i]; m_gifDelays[i] = NULL; }
        m_loaded[i] = FALSE;
    }
    if (m_gdiplusOn) {
        GdiplusShutdown(m_gdiplusToken);
        m_gdiplusOn = FALSE;
    }
}

// 加载一张 GIF 到 GDI+ Image,读出每帧延迟
static BOOL LoadGifAsAnim(const TCHAR* path, Gdiplus::Image*& outImg,
                          UINT& outFrameCount, UINT*& outDelays,
                          int& outW, int& outH)
{
    // GDI+ 需要 wchar_t* 路径
#ifdef _UNICODE
    Image* img = Image::FromFile(path);
#else
    int len = MultiByteToWideChar(CP_ACP, 0, path, -1, NULL, 0);
    WCHAR* wp = new WCHAR[len+1];
    MultiByteToWideChar(CP_ACP, 0, path, -1, wp, len);
    Image* img = Image::FromFile(wp);
    delete[] wp;
#endif
    if (!img || img->GetLastStatus() != Ok) {
        if (img) delete img;
        return FALSE;
    }
    outW = (int)img->GetWidth();
    outH = (int)img->GetHeight();
    UINT dimCount = img->GetFrameDimensionsCount();
    if (dimCount == 0) {
        outImg = img;
        outFrameCount = 1;
        outDelays = new UINT[1]; outDelays[0] = 100;
        return TRUE;
    }
    GUID dimIds[8] = {};
    if (dimCount > 8) dimCount = 8;
    img->GetFrameDimensionsList(dimIds, dimCount);
    UINT count = img->GetFrameCount(&dimIds[0]);
    if (count == 0) count = 1;

    outImg = img;
    outFrameCount = count;
    outDelays = new UINT[count];

    // 读每帧延迟(PropertyTagFrameDelay = 0x5100)
    UINT delaySize = img->GetPropertyItemSize(PropertyTagFrameDelay);
    if (delaySize > 0) {
        PropertyItem* propItem = (PropertyItem*)malloc(delaySize);
        if (propItem) {
            if (img->GetPropertyItem(PropertyTagFrameDelay, delaySize, propItem) == Ok
                && propItem->type == PropertyTagTypeLong) {
                UINT* delays = (UINT*)propItem->value;
                UINT n = propItem->length / sizeof(UINT);
                for (UINT k = 0; k < count; ++k) {
                    UINT d = (k < n) ? delays[k] * 10 : 100;  // GIF 单位是 1/100s
                    if (d < 20) d = 100;  // 太小给个默认
                    outDelays[k] = d;
                }
            } else {
                for (UINT k = 0; k < count; ++k) outDelays[k] = 100;
            }
            free(propItem);
        } else {
            for (UINT k = 0; k < count; ++k) outDelays[k] = 100;
        }
    } else {
        for (UINT k = 0; k < count; ++k) outDelays[k] = 100;
    }
    return TRUE;
}

static void MakeWhiteTransparent(CImage* img);  // 前置声明(定义在下方)

void CSpriteManager::LoadAll()
{
    // 初始化 GDI+(只一次)
    if (!m_gdiplusOn) {
        GdiplusStartupInput input;
        GdiplusStartup(&m_gdiplusToken, &input, NULL);
        m_gdiplusOn = TRUE;
    }

    for (int i = 0; i < SPR_COUNT; ++i) {
        const TCHAR* tryPaths[2] = { kPaths[i].primary, kPaths[i].alt };
        for (int p = 0; p < 2; ++p) {
            const TCHAR* path = tryPaths[p];
            if (!path || !FileExists2(path)) continue;

            if (EndsWith(path, _T(".gif"))) {
                // GIF: 用 GDI+
                Image* gimg = NULL;
                UINT cnt = 0, *delays = NULL;
                int gw = 0, gh = 0;
                if (LoadGifAsAnim(path, gimg, cnt, delays, gw, gh)) {
                    m_gifs[i] = gimg;
                    m_gifFrameCount[i] = cnt;
                    m_gifDelays[i] = delays;
                    m_gifW[i] = gw;
                    m_gifH[i] = gh;
                    m_gifCurFrame[i] = 0;
                    m_gifLastTick[i] = GetTickCount();
                    m_loaded[i] = TRUE;
                    break;
                }
            } else {
                // 怪物类 sprite(需要左右翻转)的 PNG 也用 GDI+ 加载,当单帧动画
                // 这样翻转走 GDI+ 路径,完美支持 alpha
                BOOL needFlip = (i == SPR_TREE || i == SPR_WRAITH || i == SPR_BOSS ||
                                 i == SPR_BOSS1 || i == SPR_BOSS2 || i == SPR_BOSS3);
                if (needFlip) {
                    Image* gimg = NULL; UINT cnt = 0, *delays = NULL; int gw = 0, gh = 0;
                    if (LoadGifAsAnim(path, gimg, cnt, delays, gw, gh)) {
                        m_gifs[i] = gimg;
                        m_gifFrameCount[i] = 1;  // PNG 当单帧,不切换
                        m_gifDelays[i] = delays;
                        m_gifW[i] = gw; m_gifH[i] = gh;
                        m_gifCurFrame[i] = 0;
                        m_gifLastTick[i] = GetTickCount();
                        m_loaded[i] = TRUE;
                        break;
                    }
                }
                // 其他(背景/UI图标): 用 CImage
                CImage* img = new CImage();
                if (SUCCEEDED(img->Load(path))) {
                    m_images[i] = img;
                    m_loaded[i] = TRUE;
                    // 玩家精灵图: 预处理把白色背景转透明(一次性,避免运行时开销)
                    if (i == SPR_PLAYER) MakeWhiteTransparent(img);
                    break;
                }
                delete img;
            }
        }
    }
}

BOOL CSpriteManager::Has(SpriteID id) const
{
    return id >= 0 && id < SPR_COUNT && m_loaded[id];
}

BOOL CSpriteManager::IsGif(SpriteID id) const
{
    return Has(id) && m_gifs[id] != NULL;
}

void CSpriteManager::TickAnim()
{
    DWORD now = GetTickCount();
    for (int i = 0; i < SPR_COUNT; ++i) {
        if (!m_gifs[i] || m_gifFrameCount[i] <= 1) continue;
        UINT cur = m_gifCurFrame[i];
        DWORD elapsed = now - m_gifLastTick[i];
        UINT delay = m_gifDelays[i][cur];
        if (elapsed >= delay) {
            cur = (cur + 1) % m_gifFrameCount[i];
            // 切到下一帧
            GUID dimId = FrameDimensionTime;
            m_gifs[i]->SelectActiveFrame(&dimId, cur);
            m_gifCurFrame[i] = cur;
            m_gifLastTick[i] = now;
        }
    }
}

// 每个 sprite 的原图朝向: TRUE=原图朝左, FALSE=原图朝右
// (用于翻转判断: 当怪需要朝向与原图相反时才镜像)
static BOOL SpriteArtFacesLeft(SpriteID id)
{
    switch (id) {
    case SPR_FOX:      return TRUE;   // 狐狸原图朝左
    case SPR_WOLF:     return TRUE;   // 狼原图朝左
    case SPR_WRAITH:   return FALSE;  // 雾魇原图朝右
    case SPR_ASSASSIN: return FALSE;  // 刺客原图朝右
    case SPR_GHOST:    return FALSE;
    case SPR_GOBLIN:   return FALSE;
    case SPR_BAT:      return FALSE;
    default:           return FALSE;
    }
}

// 把 CImage 中接近白色的像素设为透明(预处理玩家精灵图,避免运行时 TransparentBlt 开销)
static void MakeWhiteTransparent(CImage* img)
{
    if (!img || img->GetBPP() < 32) return;  // 需要 32 位带 alpha
    int w = img->GetWidth(), h = img->GetHeight();
    if (w <= 0 || h <= 0) return;

    // 自动检测背景色: 取左上角像素(BGRA)
    BYTE* corner = (BYTE*)img->GetPixelAddress(0, 0);
    int bgB = corner[0], bgG = corner[1], bgR = corner[2];

    for (int y = 0; y < h; ++y) {
        BYTE* row = (BYTE*)img->GetPixelAddress(0, y);
        for (int x = 0; x < w; ++x) {
            BYTE* px = row + x * 4;  // BGRA
            int b = px[0], g = px[1], r = px[2];
            // 与背景色相近(各通道差 < 40)则透明
            int db = b - bgB, dg = g - bgG, dr = r - bgR;
            if (db*db + dg*dg + dr*dr < 40*40) {
                px[0] = px[1] = px[2] = px[3] = 0;  // 全透明
            } else {
                px[3] = 255;  // 其余不透明
            }
        }
    }
}

// GDI+ 绘制 GIF 当前帧到指定区域(支持水平翻转)
static void DrawGifAt(Gdiplus::Image* img, CDC* pDC, int x, int y, int w, int h, BOOL flipH = FALSE)
{
    if (!img) return;
    Graphics g(pDC->GetSafeHdc());
    // 全部用最快模式,关闭抗锯齿/高质量混合,提升帧率
    g.SetInterpolationMode(InterpolationModeNearestNeighbor);
    g.SetSmoothingMode(SmoothingModeNone);
    g.SetPixelOffsetMode(PixelOffsetModeNone);
    g.SetCompositingQuality(CompositingQualityHighSpeed);
    if (flipH) {
        Point destPts[3] = { Point(x+w, y), Point(x, y), Point(x+w, y+h) };
        g.DrawImage(img, destPts, 3);
    } else {
        g.DrawImage(img, x, y, w, h);
    }
}

// CImage 水平翻转绘制
static void DrawCImageFlip(CImage* img, CDC* pDC, int x, int y, int w, int h, BOOL flipH)
{
    if (w <= 0 || h <= 0) return;
    // PNG(CImage)翻转会破坏 alpha 透明,故 PNG 怪不翻转(tree/wraith 用对称图)
    // GIF 怪的翻转走 GDI+ 路径,见 DrawGifAt
    (void)flipH;
    img->Draw(pDC->GetSafeHdc(), x, y, w, h);
}

void CSpriteManager::Draw(CDC* pDC, SpriteID id, int x, int y, BOOL flipH)
{
    if (!Has(id)) return;
    if (m_gifs[id]) {
        int w = m_gifW[id], h = m_gifH[id];
        DrawGifAt(m_gifs[id], pDC, x - w/2, y - h/2, w, h, flipH);
    } else {
        CImage* img = m_images[id];
        int w = img->GetWidth(), h = img->GetHeight();
        DrawCImageFlip(img, pDC, x - w/2, y - h/2, w, h, flipH);
    }
}

void CSpriteManager::DrawScaled(CDC* pDC, SpriteID id, int x, int y, float scale, BOOL flipH)
{
    if (!Has(id)) return;
    // flipH 语义: 怪物想朝左=TRUE。实际是否镜像 = (想朝左 != 原图朝左)
    BOOL wantLeft = flipH;
    BOOL artLeft = SpriteArtFacesLeft(id);
    BOOL mirror = (wantLeft != artLeft);
    if (m_gifs[id]) {
        int w = (int)(m_gifW[id]*scale), h = (int)(m_gifH[id]*scale);
        DrawGifAt(m_gifs[id], pDC, x - w/2, y - h/2, w, h, mirror);
    } else {
        CImage* img = m_images[id];
        int w = (int)(img->GetWidth()*scale), h = (int)(img->GetHeight()*scale);
        DrawCImageFlip(img, pDC, x - w/2, y - h/2, w, h, mirror);
    }
}

void CSpriteManager::DrawStretch(CDC* pDC, SpriteID id, const CRect& dst)
{
    if (!Has(id)) return;
    if (m_gifs[id]) {
        DrawGifAt(m_gifs[id], pDC, dst.left, dst.top, dst.Width(), dst.Height());
    } else {
        m_images[id]->Draw(pDC->GetSafeHdc(),
                           dst.left, dst.top, dst.Width(), dst.Height());
    }
}

void CSpriteManager::DrawFrame(CDC* pDC, SpriteID id, int x, int y,
                               int frameW, int frameH, int frameIndex)
{
    if (!Has(id)) return;
    if (m_gifs[id]) {
        // GIF 不支持精灵图分帧,直接画当前 GIF 帧
        int w = m_gifW[id], h = m_gifH[id];
        DrawGifAt(m_gifs[id], pDC, x - frameW/2, y - frameH/2, frameW, frameH);
        return;
    }
    CImage* img = m_images[id];
    int cols = img->GetWidth() / frameW; if (cols <= 0) cols = 1;
    int idx = frameIndex % cols;
    img->Draw(pDC->GetSafeHdc(),
              x - frameW/2, y - frameH/2, frameW, frameH,
              idx * frameW, 0, frameW, frameH);
}

void CSpriteManager::DrawFrameScaled(CDC* pDC, SpriteID id, int x, int y,
                                     int frameW, int frameH, int frameIndex,
                                     float scale, BOOL flipH)
{
    if (!Has(id)) return;
    int dw = (int)(frameW * scale), dh = (int)(frameH * scale);
    if (m_gifs[id]) {
        int w = m_gifW[id], h = m_gifH[id];
        DrawGifAt(m_gifs[id], pDC, x - dw/2, y - dh/2, dw, dh, flipH);
        return;
    }
    CImage* img = m_images[id];
    int cols = img->GetWidth() / frameW; if (cols <= 0) cols = 1;
    int idx = frameIndex % cols;
    (void)flipH;
    // 白色背景已在加载时预处理为透明,直接 Draw(alpha 混合,快)
    int om = pDC->SetStretchBltMode(COLORONCOLOR);
    img->Draw(pDC->GetSafeHdc(), x - dw/2, y - dh/2, dw, dh,
              idx * frameW, 0, frameW, frameH);
    pDC->SetStretchBltMode(om);
}

void CSpriteManager::DrawSized(CDC* pDC, SpriteID id, int x, int y, int w, int h){
    if (!Has(id)) return;
    if (m_gifs[id]) {
        DrawGifAt(m_gifs[id], pDC, x, y, w, h);
    } else {
        m_images[id]->Draw(pDC->GetSafeHdc(), x, y, w, h);
    }
}
