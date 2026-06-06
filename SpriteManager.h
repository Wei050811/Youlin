// SpriteManager.h
// 加载 PNG/JPG/GIF 贴图(GIF 支持多帧动画); 缺失时回退到几何图占位
#pragma once
#include <atlimage.h>

namespace Gdiplus { class Image; }

enum SpriteID {
    SPR_PLAYER = 0,
    SPR_GHOST,  SPR_TREE,  SPR_BAT,
    SPR_FOX,    SPR_WOLF,  SPR_ASSASSIN,
    SPR_WRAITH, SPR_BOSS,
    SPR_GOBLIN,
    SPR_BOSS1, SPR_BOSS2, SPR_BOSS3,   // BOSS 三阶段贴图
    SPR_FIREBALL, SPR_ICESHARD, SPR_LIGHTNING,
    SPR_POISON,   SPR_SEED,
    SPR_ITEM_POTION, SPR_ITEM_MANA,
    SPR_ITEM_AMULET, SPR_ITEM_SCROLL,
    SPR_ITEM_REVIVE,                   // 复活卷轴(phoenix.gif)
    SPR_SKILL_ICE,   SPR_SKILL_LIGHTNING,
    SPR_SKILL_STORM, SPR_SKILL_HEAL,
    SPR_SKILL_ARCANE,                  // 奥术爆发 I 技能(arcane.png)
    SPR_MENU_BG,
    SPR_BG_CH1, SPR_BG_CH2, SPR_BG_CH3, SPR_BG_CH4,
    SPR_COUNT
};

class CSpriteManager
{
public:
    CSpriteManager();
    ~CSpriteManager();

    void LoadAll();
    void Shutdown();
    BOOL Has(SpriteID id) const;
    BOOL IsGif(SpriteID id) const;

    void TickAnim();

    void Draw(CDC* pDC, SpriteID id, int x, int y, BOOL flipH = FALSE);
    void DrawStretch(CDC* pDC, SpriteID id, const CRect& dst);
    void DrawFrame(CDC* pDC, SpriteID id, int x, int y,
                   int frameW, int frameH, int frameIndex);
    // 分帧 + 缩放 + 翻转(玩家放大用)
    void DrawFrameScaled(CDC* pDC, SpriteID id, int x, int y,
                         int frameW, int frameH, int frameIndex,
                         float scale, BOOL flipH = FALSE);
    void DrawSized(CDC* pDC, SpriteID id, int x, int y, int w, int h);
    // 居中绘制并可指定缩放倍数 + 水平翻转(怪物放大+朝向用)
    void DrawScaled(CDC* pDC, SpriteID id, int x, int y, float scale, BOOL flipH = FALSE);

private:
    CImage*          m_images[SPR_COUNT];
    BOOL             m_loaded[SPR_COUNT];

    Gdiplus::Image*  m_gifs[SPR_COUNT];
    UINT             m_gifFrameCount[SPR_COUNT];
    UINT             m_gifCurFrame[SPR_COUNT];
    DWORD            m_gifLastTick[SPR_COUNT];
    UINT*            m_gifDelays[SPR_COUNT];
    int              m_gifW[SPR_COUNT];
    int              m_gifH[SPR_COUNT];

    ULONG_PTR        m_gdiplusToken;
    BOOL             m_gdiplusOn;
};

extern CSpriteManager g_sprites;
