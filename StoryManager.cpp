// StoryManager.cpp
#include "pch.h"
#include "StoryManager.h"
#include "SpriteManager.h"
#include "AudioManager.h"

CStoryManager::CStoryManager()
    : m_slides(NULL), m_count(0), m_index(0),
      m_charShown(0), m_charDelay(6), m_frame(0),
      m_fadeIn(0), m_finished(FALSE) {}

void CStoryManager::Load(const StorySlide* s, int n)
{
    m_slides = s; m_count = n; m_index = 0;
    m_finished = FALSE;
    RestartAnim();
}

void CStoryManager::RestartAnim() { m_charShown = 0; m_frame = 0; m_fadeIn = 30; }

void CStoryManager::Update()
{
    m_frame++;
    if (m_fadeIn > 0) m_fadeIn--;
    if (m_slides && m_index < m_count) {
        int total = m_slides[m_index].content.GetLength();
        if (m_frame % m_charDelay == 0 && m_charShown < total) {
            m_charShown++;
            // 播打字音效(跳过空格,避免太吵)
            TCHAR c = m_slides[m_index].content.GetAt(m_charShown - 1);
            if (c != _T(' ') && c != _T('\n') && c != _T('\r'))
                g_audio.PlaySfx(SFX_TYPING);
        }
    }
}

BOOL CStoryManager::NextSlide()
{
    if (!m_slides) return TRUE;
    int total = m_slides[m_index].content.GetLength();
    if (m_charShown < total) { m_charShown = total; return FALSE; }
    m_index++;
    if (m_index >= m_count) { m_finished = TRUE; return TRUE; }
    RestartAnim();
    return FALSE;
}

void CStoryManager::Draw(CDC* pDC)
{
    if (!m_slides || m_index >= m_count) return;
    const StorySlide& s = m_slides[m_index];

    // 章节背景图（若有）作为半透明底；没有则用渐变
    int idx = s.bgIndex; if (idx<0||idx>3) idx = 0;
    SpriteID bgs[4] = { SPR_BG_CH1, SPR_BG_CH2, SPR_BG_CH3, SPR_BG_CH4 };
    if (g_sprites.Has(bgs[idx])) {
        g_sprites.DrawStretch(pDC, bgs[idx], CRect(0,0,WIN_W,WIN_H));
    } else {
        static const COLORREF top[4] = {
            RGB(20,30,45), RGB(25,35,25), RGB(35,25,45), RGB(45,20,30)
        };
        static const COLORREF bot[4] = {
            RGB(40,60,80), RGB(50,70,40), RGB(60,40,80), RGB(80,30,50)
        };
        for (int i = 0; i < 30; ++i) {
            int r = GetRValue(top[idx]) + (GetRValue(bot[idx])-GetRValue(top[idx]))*i/29;
            int g = GetGValue(top[idx]) + (GetGValue(bot[idx])-GetGValue(top[idx]))*i/29;
            int b = GetBValue(top[idx]) + (GetBValue(bot[idx])-GetBValue(top[idx]))*i/29;
            pDC->FillSolidRect(0, WIN_H*i/30, WIN_W, WIN_H/30+1, RGB(r,g,b));
        }
    }

    // 飘浮雾点
    {
        CBrush dot(RGB(220,230,240));
        CBrush* po = pDC->SelectObject(&dot);
        srand(123);
        for (int i = 0; i < 25; ++i) {
            int x = rand()%WIN_W, y = rand()%WIN_H;
            int off = (m_frame/2 + i*7) % WIN_W;
            int xx = (x + off) % WIN_W;
            int sz = 2 + (i%4);
            pDC->Ellipse(xx-sz, y-sz, xx+sz, y+sz);
        }
        pDC->SelectObject(po);
        srand((unsigned)time(NULL));
    }

    // 黑色框
    CRect frame(80, 120, WIN_W-80, WIN_H-120);
    pDC->FillSolidRect(frame, RGB(15,15,25));
    CPen pen(PS_SOLID, 2, RGB(180,170,120));
    CPen* po = pDC->SelectObject(&pen);
    CBrush* pn = (CBrush*)pDC->SelectStockObject(NULL_BRUSH);
    pDC->Rectangle(frame);
    pDC->SelectObject(po); pDC->SelectObject(pn);
    pDC->SetBkMode(TRANSPARENT);

    CFont tf;
    tf.CreateFont(36,0,0,0,FW_BOLD,0,0,0, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, FF_DONTCARE, _T("微软雅黑"));
    CFont* pf = pDC->SelectObject(&tf);
    pDC->SetTextColor(RGB(240,220,160));
    pDC->DrawText(s.title, CRect(frame.left, frame.top+24, frame.right, frame.top+80),
                  DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    pDC->SelectObject(pf);
    pDC->FillSolidRect(WIN_W/2-120, frame.top+88, 240, 2, RGB(180,170,120));

    CFont bf;
    bf.CreateFont(24,0,0,0,FW_NORMAL,0,0,0, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, FF_DONTCARE, _T("微软雅黑"));
    pDC->SelectObject(&bf);
    pDC->SetTextColor(RGB(225,225,230));
    CString shown = s.content.Left(m_charShown);
    pDC->DrawText(shown, CRect(frame.left+50, frame.top+110,
                  frame.right-50, frame.bottom-60), DT_LEFT|DT_TOP|DT_WORDBREAK);

    if ((m_frame/30)%2 == 0) {
        CFont tip;
        tip.CreateFont(18,0,0,0,FW_NORMAL,0,0,0, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, FF_DONTCARE, _T("微软雅黑"));
        pDC->SelectObject(&tip);
        pDC->SetTextColor(RGB(180,180,200));
        pDC->DrawText(_T("[按 Enter / 空格 / 鼠标左键 继续]"),
                      CRect(frame.left, frame.bottom-40, frame.right, frame.bottom-15),
                      DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    }
    CFont pg;
    pg.CreateFont(16,0,0,0,FW_NORMAL,0,0,0, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, FF_DONTCARE, _T("微软雅黑"));
    pDC->SelectObject(&pg);
    pDC->SetTextColor(RGB(150,150,170));
    CString pgs; pgs.Format(_T("%d / %d"), m_index+1, m_count);
    pDC->DrawText(pgs, CRect(frame.right-80, frame.bottom-30,
                  frame.right-10, frame.bottom-10),
                  DT_RIGHT|DT_VCENTER|DT_SINGLELINE);
}

// ====== 章节剧情便捷加载 ======
void CStoryManager::LoadBegin(Chapter c)
{
    using namespace Story;
    switch (c) {
    case CHAP_1: Load(CH1_BEGIN, sizeof(CH1_BEGIN)/sizeof(CH1_BEGIN[0])); break;
    case CHAP_2: Load(CH2_BEGIN, sizeof(CH2_BEGIN)/sizeof(CH2_BEGIN[0])); break;
    case CHAP_3: Load(CH3_BEGIN, sizeof(CH3_BEGIN)/sizeof(CH3_BEGIN[0])); break;
    case CHAP_4: Load(CH4_BEGIN, sizeof(CH4_BEGIN)/sizeof(CH4_BEGIN[0])); break;
    default: break;
    }
}
void CStoryManager::LoadEnd(Chapter c)
{
    using namespace Story;
    switch (c) {
    case CHAP_1: Load(CH1_END, sizeof(CH1_END)/sizeof(CH1_END[0])); break;
    case CHAP_2: Load(CH2_END, sizeof(CH2_END)/sizeof(CH2_END[0])); break;
    case CHAP_3: Load(CH3_END, sizeof(CH3_END)/sizeof(CH3_END[0])); break;
    case CHAP_4: Load(ENDING_TRUE, sizeof(ENDING_TRUE)/sizeof(ENDING_TRUE[0])); break;
    default: break;
    }
}
void CStoryManager::LoadEndingTrue()
{
    Load(Story::ENDING_TRUE, sizeof(Story::ENDING_TRUE)/sizeof(Story::ENDING_TRUE[0]));
}
void CStoryManager::LoadEndingNormal()
{
    Load(Story::ENDING_NORMAL, sizeof(Story::ENDING_NORMAL)/sizeof(Story::ENDING_NORMAL[0]));
}
