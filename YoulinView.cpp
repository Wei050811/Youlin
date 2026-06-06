// YoulinView.cpp
#include "pch.h"
#include "Youlin.h"
#include "YoulinDoc.h"
#include "YoulinView.h"
#include "SaveManager.h"
#include "SpriteManager.h"
#include "AudioManager.h"
#include <math.h>
#include <time.h>
#include <imm.h>
#pragma comment(lib, "imm32.lib")

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

IMPLEMENT_DYNCREATE(CYoulinView, CView)

BEGIN_MESSAGE_MAP(CYoulinView, CView)
    ON_WM_CREATE()
    ON_WM_TIMER()
    ON_WM_KEYDOWN()
    ON_WM_KEYUP()
    ON_WM_LBUTTONDOWN()
    ON_WM_MOUSEMOVE()
    ON_WM_MOUSEWHEEL()
    ON_WM_SIZE()
    ON_WM_ERASEBKGND()
END_MESSAGE_MAP()

#define TIMER_TICK 1
#define FRAME_MS   16

CYoulinView::CYoulinView()
    : m_state(STATE_MENU), m_chapter(CHAP_1), m_diff(DIFF_NORMAL), m_frame(0),
      m_menuSel(0), m_loadSel(0), m_diffSel(1),
      m_camX(0.0f),
      m_currentWave(0), m_waveTransitionLeft(0),
      m_rewardSel(-1),
      m_clientW(WIN_W), m_clientH(WIN_H),
      m_viewX(0), m_viewY(0), m_viewW(WIN_W), m_viewH(WIN_H),
      m_player(NULL),
      m_keyUp(FALSE), m_keyDown(FALSE), m_keyLeft(FALSE), m_keyRight(FALSE),
      m_dcReady(FALSE), m_mouseHover(0,0),
      m_shopOpen(FALSE), m_shopTab(0), m_shopSel(0)
{
    srand((unsigned)time(NULL));
    m_player = new CPlayer();
    for (int i = 0; i < 3; ++i) m_menuItemRect[i].SetRectEmpty();
    m_menuItemRect[3].SetRectEmpty();
    for (int i = 0; i < 3; ++i) {
        m_diffItemRect[i].SetRectEmpty();
        m_loadItemRect[i].SetRectEmpty();
        m_rewardRect[i].SetRectEmpty();
        m_shopTabRect[i].SetRectEmpty();
        m_rewardRevealed[i] = FALSE;
        m_rewardRevealAnim[i] = 0;
        m_rewardCards[i].type = REWARD_HEAL;
        m_rewardCards[i].value = 0;
    }
    m_rewardConfirmRect.SetRectEmpty();
    for (int i = 0; i < 7; ++i) m_shopItemRect[i].SetRectEmpty();
    m_shopCloseRect.SetRectEmpty();
    m_shopScroll = 0;
    // 新成员初始化
    m_allWavesCleared = FALSE;
    m_midStoryTriggered = FALSE;
    m_bossStoryTriggered = FALSE;
    m_dialogLines = NULL;
    m_dialogCount = m_dialogIndex = m_dialogCharShown = m_dialogCharTick = 0;
    m_dialogWarmup = 0;
    m_pendingDialog = NULL;
    m_pendingDialogCount = 0;
    m_pendingAfterBoss = FALSE;
    m_pendingWaitFrames = 0;
    m_dialogAfterIsBoss = FALSE;
    m_pauseSel = 0;
    m_cheatOpen = FALSE;
    for (int i = 0; i < 4; ++i) m_pauseItemRect[i].SetRectEmpty();
    for (int i = 0; i < 8; ++i) m_cheatRect[i].SetRectEmpty();
}
CYoulinView::~CYoulinView()
{
    delete m_player;
    ClearEnemies();
}

BOOL CYoulinView::PreCreateWindow(CREATESTRUCT& cs)
{
    // 允许最大化(全屏)
    cs.style |= WS_MAXIMIZEBOX;
    cs.cx = WIN_W;
    cs.cy = WIN_H;
    return CView::PreCreateWindow(cs);
}

BOOL CYoulinView::OnEraseBkgnd(CDC*) { return TRUE; }

int CYoulinView::OnCreate(LPCREATESTRUCT lpcs)
{
    if (CView::OnCreate(lpcs) == -1) return -1;
    SetTimer(TIMER_TICK, FRAME_MS, NULL);
    g_sprites.LoadAll();
    g_audio.Init();
    g_audio.PlayBgm(BGM_MENU);
    // 禁用输入法: 防止 Shift/Ctrl 切换输入法干扰游戏按键
    ImmAssociateContext(m_hWnd, NULL);
    return 0;
}

void CYoulinView::OnSize(UINT nType, int cx, int cy)
{
    CView::OnSize(nType, cx, cy);
    if (cx <= 0 || cy <= 0) return;
    m_clientW = cx;
    m_clientH = cy;
    // 铺满整个窗口,不留黑边(允许宽高比变形)
    m_viewX = 0;
    m_viewY = 0;
    m_viewW = cx;
    m_viewH = cy;
}

void CYoulinView::EnsureBackBuffer()
{
    if (m_dcReady) return;
    CDC* pDC = GetDC();
    m_backDC.CreateCompatibleDC(pDC);
    m_backBmp.CreateCompatibleBitmap(pDC, WIN_W, WIN_H);
    m_backDC.SelectObject(&m_backBmp);
    ReleaseDC(pDC);
    m_dcReady = TRUE;
}

float CYoulinView::DiffMul() const
{
    return m_diff == DIFF_EASY ? 0.7f : m_diff == DIFF_HARD ? 1.4f : 1.0f;
}

CPoint CYoulinView::ScreenToGame(CPoint sp) const
{
    // sp 是相对客户区的坐标 → 减去黑边偏移 → 反向 StretchBlt
    if (m_viewW <= 0 || m_viewH <= 0) return CPoint(0,0);
    int x = (sp.x - m_viewX) * WIN_W / m_viewW;
    int y = (sp.y - m_viewY) * WIN_H / m_viewH;
    if (x < 0) x = 0; if (x > WIN_W) x = WIN_W;
    if (y < 0) y = 0; if (y > WIN_H) y = WIN_H;
    return CPoint(x, y);
}

void CYoulinView::UpdateCamera()
{
    if (!m_player) return;
    // 目标 camX: 玩家位于屏幕中心
    float target = m_player->GetPos().x - WIN_W/2.0f;
    if (target < 0) target = 0;
    if (target > LEVEL_W - WIN_W) target = (float)(LEVEL_W - WIN_W);
    // 平滑
    m_camX += (target - m_camX) * 0.15f;
    if (m_camX < 0) m_camX = 0;
    if (m_camX > LEVEL_W - WIN_W) m_camX = (float)(LEVEL_W - WIN_W);
}

// =============================================
// 主循环
// =============================================
void CYoulinView::OnTimer(UINT_PTR id)
{
    m_frame++;
    g_sprites.TickAnim();  // 推进所有 GIF 动画帧

    switch (m_state) {
    case STATE_STORY_BEGIN:
    case STATE_STORY_END:
    case STATE_ENDING_TRUE:
    case STATE_ENDING_NORMAL:
        m_story.Update();
        break;

    case STATE_STORY_MID:
        UpdateDialog();
        break;

    case STATE_PAUSE:
        // 暂停: 不更新游戏逻辑
        break;

    case STATE_PLAYING:
        if (!m_shopOpen) {
            // 处理待触发对话: 先刷的怪出生完成后再真正开始对话
            if (m_pendingDialog) {
                m_player->Update(m_keyUp, m_keyDown, m_keyLeft, m_keyRight);
                for (auto* e : m_enemies)
                    if (e->IsAlive()) e->Update(m_player, &m_proj);
                m_proj.Update(m_player, m_enemies);
                UpdateCamera();
                m_pendingWaitFrames--;
                // 怪都出生完了(或等待超时) → 真正开始对话
                if (!AnyEnemySpawning() || m_pendingWaitFrames <= 0) {
                    const DialogLine* dl = m_pendingDialog;
                    int n = m_pendingDialogCount;
                    m_pendingDialog = NULL;
                    StartDialog(dl, n, FALSE);
                }
                break;
            }

            m_player->Update(m_keyUp, m_keyDown, m_keyLeft, m_keyRight);
            for (auto* e : m_enemies)
                if (e->IsAlive()) e->Update(m_player, &m_proj);
            m_proj.Update(m_player, m_enemies);
            UpdateCamera();

            // 检测 BOSS 阶段切换 → 触发对应剧情(2=阶段2, 3=阶段3/第二条命)
            for (auto* e : m_enemies) {
                CFogCore* boss = dynamic_cast<CFogCore*>(e);
                if (boss && boss->IsAlive()) {
                    int pt = boss->ConsumePhaseTrigger();
                    if (pt == 2) {
                        StartDialog(Story::CH4_BOSS_PHASE2,
                                    sizeof(Story::CH4_BOSS_PHASE2)/sizeof(DialogLine), FALSE);
                        break;
                    } else if (pt == 3) {
                        StartDialog(Story::CH4_BOSS_PHASE3,
                                    sizeof(Story::CH4_BOSS_PHASE3)/sizeof(DialogLine), FALSE);
                        break;
                    }
                }
            }
            if (m_state == STATE_STORY_MID) break;  // 已触发剧情,本帧结束

            float px = m_player->GetPos().x;

            // 中段剧情触发: 玩家走到地图中部(约 45%)
            if (!m_midStoryTriggered && px > LEVEL_W * 0.42f) {
                m_midStoryTriggered = TRUE;
                const DialogLine* dl = NULL; int n = 0;
                switch (m_chapter) {
                case CHAP_1: dl = Story::CH1_MID; n = sizeof(Story::CH1_MID)/sizeof(DialogLine); break;
                case CHAP_2: dl = Story::CH2_MID; n = sizeof(Story::CH2_MID)/sizeof(DialogLine); break;
                case CHAP_3: dl = Story::CH3_MID; n = sizeof(Story::CH3_MID)/sizeof(DialogLine); break;
                case CHAP_4: dl = Story::CH4_MID; n = sizeof(Story::CH4_MID)/sizeof(DialogLine); break;
                }
                if (dl) { StartDialog(dl, n, FALSE); break; }
            }

            if (!m_player->IsAlive()) m_state = STATE_GAMEOVER;
            else if (AllEnemiesDead()) {
                if (m_currentWave + 1 < CHAPTER_WAVES[m_chapter]) {
                    int nextWave = m_currentWave + 1;
                    // 各章最后一波前先触发 BOSS 战前剧情(先刷怪,镜头会移到怪身上)
                    if (!m_bossStoryTriggered && nextWave + 1 == CHAPTER_WAVES[m_chapter]) {
                        m_bossStoryTriggered = TRUE;
                        // 先把最后一波(含 BOSS/精英)生成出来
                        m_currentWave++;
                        SpawnWave(m_currentWave);
                        const DialogLine* dl = NULL; int n = 0;
                        switch (m_chapter) {
                        case CHAP_1: dl = Story::CH1_BOSS; n = sizeof(Story::CH1_BOSS)/sizeof(DialogLine); break;
                        case CHAP_2: dl = Story::CH2_BOSS; n = sizeof(Story::CH2_BOSS)/sizeof(DialogLine); break;
                        case CHAP_3: dl = Story::CH3_BOSS; n = sizeof(Story::CH3_BOSS)/sizeof(DialogLine); break;
                        case CHAP_4: dl = Story::CH4_BOSS; n = sizeof(Story::CH4_BOSS)/sizeof(DialogLine); break;
                        }
                        // 等怪出生完成后再对话(不对着召唤特效说话)
                        if (dl) { QueueDialogAfterSpawn(dl, n); break; }
                    }
                    m_currentWave++;
                    SpawnWave(m_currentWave);
                } else {
                    if (m_chapter == CHAP_4) {
                        // BOSS 关: 清完(BOSS死)直接进入翻牌,不用走到最右
                        EndChapterFight();
                    } else {
                        // 其他关: 标记,等玩家走到地图最右才过关
                        m_allWavesCleared = TRUE;
                    }
                }
            }

            // 非 BOSS 关: 所有波清完且玩家走到地图最右 → 进入翻牌
            if (m_allWavesCleared && px > LEVEL_W - 120) {
                EndChapterFight();
            }
        }
        break;

    default: break;
    }
    // 翻牌动画推进
    if (m_state == STATE_REWARD) {
        for (int i = 0; i < 3; ++i)
            if (m_rewardRevealAnim[i] > 0) m_rewardRevealAnim[i]--;
    }
    // 用 RedrawWindow 直接重绘,跳过 WM_ERASEBKGND,消除闪烁
    RedrawWindow(NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW | RDW_NOERASE);
    CView::OnTimer(id);
}

// =============================================
// 输入 - 键盘
// =============================================
void CYoulinView::OnKeyDown(UINT nChar, UINT, UINT)
{
    // 全局: F11 切换最大化
    if (nChar == VK_F11) {
        CWnd* pFrame = GetParentFrame();
        if (pFrame) {
            WINDOWPLACEMENT wp; wp.length = sizeof(wp);
            pFrame->GetWindowPlacement(&wp);
            wp.showCmd = (wp.showCmd == SW_MAXIMIZE) ? SW_RESTORE : SW_MAXIMIZE;
            pFrame->SetWindowPlacement(&wp);
        }
        return;
    }

    // 状态分发
    switch (m_state) {
    // ------ 主菜单 ------
    case STATE_MENU:
        if (nChar == VK_UP || nChar == 'W')   m_menuSel = (m_menuSel + 3) % 4;
        else if (nChar == VK_DOWN || nChar == 'S') m_menuSel = (m_menuSel + 1) % 4;
        else if (nChar == VK_RETURN || nChar == VK_SPACE) {
            switch (m_menuSel) {
            case 0: StartGame(); break;
            case 1: m_state = STATE_LOAD;       m_loadSel = 0; break;
            case 2: m_state = STATE_DIFFICULTY; m_diffSel = m_diff; break;
            case 3: AfxGetMainWnd()->PostMessage(WM_CLOSE); break;
            }
        }
        break;

    // ------ 难度 ------
    case STATE_DIFFICULTY:
        if (nChar == VK_UP || nChar == 'W')   m_diffSel = (m_diffSel + 2) % 3;
        else if (nChar == VK_DOWN || nChar == 'S') m_diffSel = (m_diffSel + 1) % 3;
        else if (nChar == VK_RETURN || nChar == VK_SPACE) {
            m_diff = (Difficulty)m_diffSel; m_state = STATE_MENU;
        }
        else if (nChar == VK_ESCAPE) m_state = STATE_MENU;
        break;

    // ------ 读档 ------
    case STATE_LOAD:
        if (nChar == VK_UP || nChar == 'W')   m_loadSel = (m_loadSel + 2) % 3;
        else if (nChar == VK_DOWN || nChar == 'S') m_loadSel = (m_loadSel + 1) % 3;
        else if (nChar == VK_RETURN || nChar == VK_SPACE) {
            int ch = 0, df = 1;
            if (CSaveManager::Load(m_loadSel, ch, df, *m_player)) {
                m_chapter = (Chapter)ch; m_diff = (Difficulty)df;
                m_state = STATE_STORY_BEGIN;
                m_story.LoadBegin(m_chapter);
            }
        }
        else if (nChar == VK_ESCAPE) m_state = STATE_MENU;
        break;

    // ------ 剧情 ------
    case STATE_STORY_BEGIN:
        if (nChar == VK_RETURN || nChar == VK_SPACE) {
            if (m_story.Next()) EnterChapterFight();
        }
        else if (nChar == VK_ESCAPE) {
            while (m_story.Next()) {}
            EnterChapterFight();
        }
        break;
    case STATE_STORY_END:
        if (nChar == VK_RETURN || nChar == VK_SPACE) {
            if (m_story.Next()) NextChapter();
        }
        break;
    case STATE_ENDING_TRUE:
    case STATE_ENDING_NORMAL:
        if (nChar == VK_RETURN || nChar == VK_SPACE) {
            if (m_story.Next()) GoMenu();
        }
        break;

    // ------ 战斗 ------
    case STATE_PLAYING:
        if (m_shopOpen) {
            if (nChar == VK_ESCAPE || nChar == 'B') CloseShop();
            else if (nChar == VK_UP || nChar == 'W') ShopMove(-1);
            else if (nChar == VK_DOWN || nChar == 'S') ShopMove(1);
            else if (nChar == VK_LEFT || nChar == 'A') ShopTab((m_shopTab+2)%3);
            else if (nChar == VK_RIGHT|| nChar == 'D') ShopTab((m_shopTab+1)%3);
            else if (nChar == VK_RETURN || nChar == VK_SPACE) ShopBuy();
            break;
        }
        if (nChar == 'W' || nChar == VK_UP)    m_keyUp = TRUE;
        else if (nChar == 'S' || nChar == VK_DOWN)  m_keyDown = TRUE;
        else if (nChar == 'A' || nChar == VK_LEFT)  m_keyLeft = TRUE;
        else if (nChar == 'D' || nChar == VK_RIGHT) m_keyRight = TRUE;
        else if (nChar == VK_SPACE) { if (m_player->TryFireball(&m_proj)) g_audio.PlaySfx(SFX_FIREBALL); }
        else if (nChar == 'J') { if (m_player->TrySkill(SKILL_ICE,       &m_proj, m_enemies)) g_audio.PlaySfx(SFX_SKILL_ICE); }
        else if (nChar == 'K') { if (m_player->TrySkill(SKILL_LIGHTNING, &m_proj, m_enemies)) g_audio.PlaySfx(SFX_SKILL_LIGHTNING); }
        else if (nChar == 'L') { if (m_player->TrySkill(SKILL_STORM,     &m_proj, m_enemies)) g_audio.PlaySfx(SFX_SKILL_STORM); }
        else if (nChar == 'I') { if (m_player->TrySkill(SKILL_NOVA,      &m_proj, m_enemies)) g_audio.PlaySfx(SFX_SKILL_STORM); }
        else if (nChar == VK_SHIFT) { if (m_player->TryDash()) g_audio.PlaySfx(SFX_DASH); }
        else if (nChar == '1') m_player->UseItem(ITEM_POTION);
        else if (nChar == '2') m_player->UseItem(ITEM_MANA);
        else if (nChar == '3') m_player->UseItem(ITEM_AMULET);
        else if (nChar == '4') m_player->UseItem(ITEM_SCROLL);
        else if (nChar == 'B') OpenShop();
        else if (nChar == 'F') { CSaveManager::Save(0, m_chapter, m_diff, *m_player); }
        else if (nChar == VK_ESCAPE) {
            // ESC = 暂停菜单(不直接退出)
            m_pauseSel = 0;
            m_cheatOpen = FALSE;
            m_state = STATE_PAUSE;
        }
        break;

    // ------ 关卡内对话 ------
    case STATE_STORY_MID:
        if (nChar == VK_RETURN || nChar == VK_SPACE || nChar == VK_ESCAPE)
            AdvanceDialog();
        break;

    // ------ 暂停菜单 ------
    case STATE_PAUSE:
        if (m_cheatOpen) {
            if (nChar == VK_ESCAPE) m_cheatOpen = FALSE;
            // 数字键 1-8 触发修改器
            else if (nChar >= '1' && nChar <= '8') CheatAction(nChar - '1');
            break;
        }
        if (nChar == VK_UP || nChar == 'W') m_pauseSel = (m_pauseSel+3)%4;
        else if (nChar == VK_DOWN || nChar == 'S') m_pauseSel = (m_pauseSel+1)%4;
        else if (nChar == VK_ESCAPE) m_state = STATE_PLAYING;  // ESC 再按 = 继续
        else if (nChar == VK_RETURN || nChar == VK_SPACE) {
            switch (m_pauseSel) {
            case 0: m_state = STATE_PLAYING; break;
            case 1: CSaveManager::Save(0, m_chapter, m_diff, *m_player); break;
            case 2: m_cheatOpen = TRUE; break;
            case 3: ClearEnemies(); m_proj.Clear(); GoMenu(); break;
            }
        }
        break;

    // ------ 翻牌奖励 ------
    case STATE_REWARD: {
        // 1/2/3 数字键翻对应卡片
        if (nChar >= '1' && nChar <= '3') {
            int i = nChar - '1';
            if (!m_rewardRevealed[i] && m_rewardSel < 0) {
                m_rewardRevealed[i] = TRUE;
                m_rewardRevealAnim[i] = 15;
                m_rewardSel = i;
                g_audio.PlaySfx(SFX_CLICK);
            }
        }
        else if (nChar == VK_RETURN || nChar == VK_SPACE) {
            // 已选中:确认应用
            if (m_rewardSel >= 0 && m_rewardRevealAnim[m_rewardSel] == 0) {
                ApplyReward();
            }
        }
        break;
    }

    // ------ 结算 ------
    case STATE_VICTORY:
    case STATE_GAMEOVER:
        if (nChar == VK_RETURN || nChar == VK_SPACE) GoMenu();
        break;
    default: break;
    }
}

void CYoulinView::OnKeyUp(UINT nChar, UINT, UINT)
{
    if (nChar == 'W' || nChar == VK_UP)    m_keyUp = FALSE;
    else if (nChar == 'S' || nChar == VK_DOWN)  m_keyDown = FALSE;
    else if (nChar == 'A' || nChar == VK_LEFT)  m_keyLeft = FALSE;
    else if (nChar == 'D' || nChar == VK_RIGHT) m_keyRight = FALSE;
}

// =============================================
// 输入 - 鼠标
// =============================================
void CYoulinView::OnLButtonDown(UINT, CPoint pt)
{
    SetFocus();
    CPoint gp = ScreenToGame(pt);

    // UI 状态点击播音效(战斗/对话中点击不播)
    if (m_state != STATE_PLAYING && m_state != STATE_STORY_MID) {
        g_audio.PlaySfx(SFX_CLICK);
    }

    switch (m_state) {
    case STATE_MENU:
        for (int i = 0; i < 4; ++i) {
            if (m_menuItemRect[i].PtInRect(gp)) {
                m_menuSel = i;
                switch (i) {
                case 0: StartGame(); break;
                case 1: m_state = STATE_LOAD; m_loadSel = 0; break;
                case 2: m_state = STATE_DIFFICULTY; m_diffSel = m_diff; break;
                case 3: AfxGetMainWnd()->PostMessage(WM_CLOSE); break;
                }
                return;
            }
        }
        break;

    case STATE_DIFFICULTY:
        for (int i = 0; i < 3; ++i) {
            if (m_diffItemRect[i].PtInRect(gp)) {
                m_diff = (Difficulty)i; m_state = STATE_MENU; return;
            }
        }
        break;

    case STATE_LOAD:
        for (int i = 0; i < 3; ++i) {
            if (m_loadItemRect[i].PtInRect(gp)) {
                m_loadSel = i;
                int ch = 0, df = 1;
                if (CSaveManager::Load(i, ch, df, *m_player)) {
                    m_chapter = (Chapter)ch; m_diff = (Difficulty)df;
                    m_state = STATE_STORY_BEGIN;
                    m_story.LoadBegin(m_chapter);
                }
                return;
            }
        }
        break;

    case STATE_PLAYING:
        if (m_shopOpen) { ShopClickAt(gp); return; }
        // 战斗中点击 = 发射火球
        if (m_player->TryFireball(&m_proj)) g_audio.PlaySfx(SFX_FIREBALL);
        break;

    case STATE_STORY_MID:
        AdvanceDialog();
        break;

    case STATE_PAUSE:
        if (m_cheatOpen) {
            for (int i = 0; i < 8; ++i) {
                if (m_cheatRect[i].PtInRect(gp)) { CheatAction(i); return; }
            }
            return;
        }
        for (int i = 0; i < 4; ++i) {
            if (m_pauseItemRect[i].PtInRect(gp)) {
                m_pauseSel = i;
                switch (i) {
                case 0: m_state = STATE_PLAYING; break;
                case 1: CSaveManager::Save(0, m_chapter, m_diff, *m_player); break;
                case 2: m_cheatOpen = TRUE; break;
                case 3: ClearEnemies(); m_proj.Clear(); GoMenu(); break;
                }
                return;
            }
        }
        break;

    case STATE_REWARD:
        RewardClickAt(gp);
        break;

    case STATE_STORY_BEGIN:
    case STATE_STORY_END:
    case STATE_ENDING_TRUE:
    case STATE_ENDING_NORMAL:
        if (m_story.Next()) {
            if (m_state == STATE_STORY_BEGIN) EnterChapterFight();
            else if (m_state == STATE_STORY_END) NextChapter();
            else GoMenu();
        }
        break;

    case STATE_VICTORY:
    case STATE_GAMEOVER:
        GoMenu();
        break;
    default: break;
    }
}

void CYoulinView::OnMouseMove(UINT, CPoint pt)
{
    m_mouseHover = ScreenToGame(pt);
}

BOOL CYoulinView::OnMouseWheel(UINT, short zDelta, CPoint)
{
    if (m_state == STATE_PLAYING && m_shopOpen) {
        // 商店内滚轮: 向下滚 -> 列表下移
        m_shopScroll -= (zDelta > 0 ? 1 : -1) * 60;
        if (m_shopScroll < 0) m_shopScroll = 0;
        // 上限在 Draw 里钳制
    }
    return TRUE;
}

// =============================================
// 流程控制
// =============================================
void CYoulinView::StartGame()
{
    m_player->FullReset();
    m_chapter = CHAP_1;
    EnterChapterBegin();
}

void CYoulinView::GoMenu()
{
    ClearEnemies();
    m_proj.Clear();
    m_camX = 0;
    m_shopOpen = FALSE;
    m_state = STATE_MENU;
    m_menuSel = 0;
    g_audio.PlayBgm(BGM_MENU);
}

void CYoulinView::EnterChapterBegin()
{
    m_state = STATE_STORY_BEGIN;
    m_story.LoadBegin(m_chapter);
}

void CYoulinView::EnterChapterFight()
{
    if (m_chapter == CHAP_1) {
        m_player->Reset();
    } else {
        m_player->ResetForNextChapter();
    }
    m_player->SetDamageTakenMul(DIFF_CONFIG[m_diff].enemyDmg);
    m_player->SetCoinMul(DIFF_CONFIG[m_diff].coinMul);
    ClearEnemies();
    m_proj.Clear();
    m_currentWave = 0;
    m_camX = 0;
    m_allWavesCleared = FALSE;
    m_midStoryTriggered = FALSE;
    m_bossStoryTriggered = FALSE;
    SpawnWave(0);
    m_state = STATE_PLAYING;
    g_audio.PlayBgm((BgmID)(BGM_CH1 + m_chapter));
}

// 修改器动作
void CYoulinView::CheatAction(int idx)
{
    g_audio.PlaySfx(SFX_CLICK);
    switch (idx) {
    case 0: m_player->DebugSetHP(m_player->GetHP() + 100); break;
    case 1: m_player->DebugSetMP(m_player->GetMP() + 100); break;
    case 2: m_player->DebugAddATK(50); break;
    case 3: m_player->DebugAddCoin(1000); break;
    case 4: m_player->DebugFullHeal(); break;
    case 5: m_player->DebugGodMode(!m_player->IsGodMode()); break;
    case 6:
        for (int i = 0; i < SKILL_COUNT; ++i) m_player->UpgradeSkillFree();
        break;
    case 7: m_cheatOpen = FALSE; break;
    default: break;
    }
}

void CYoulinView::EnterChapterEnd()
{
    m_state = STATE_STORY_END;
    m_story.LoadEnd(m_chapter);
}

void CYoulinView::EndChapterFight()
{
    // 章末固定奖励金币(乘难度系数,困难翻倍)
    int baseCoin[4] = {80, 120, 180, 250};
    int coin = (int)(baseCoin[m_chapter] * DIFF_CONFIG[m_diff].coinMul);
    m_player->AddCoin(coin);
    EnterReward();
}

void CYoulinView::EnterReward()
{
    m_state = STATE_REWARD;
    m_rewardSel = -1;
    for (int i = 0; i < 3; ++i) {
        m_rewardRevealed[i] = FALSE;
        m_rewardRevealAnim[i] = 0;
    }
    RollNewRewards();
}

// 随机生成 3 个奖励(可以重复)
void CYoulinView::RollNewRewards()
{
    for (int i = 0; i < 3; ++i) {
        RewardType t = (RewardType)(rand() % REWARD_KIND_COUNT);
        int val = 0;
        switch (t) {
        case REWARD_HEAL:        val = 40 + rand() % 41; break;  // 40~80
        case REWARD_MANA:        val = 30 + rand() % 31; break;  // 30~60
        case REWARD_COIN:        val = 80 + rand() % 101; break; // 80~180
        case REWARD_PERMA_ATK:   val =  4 + rand() % 7;  break;  // 4~10
        case REWARD_PERMA_HP:    val = 20 + rand() % 21; break;  // 20~40
        case REWARD_PERMA_MP:    val = 15 + rand() % 16; break;  // 15~30
        case REWARD_PERMA_DEF:   val =  2 + rand() % 4;  break;  // 2~5
        case REWARD_POTION:      val =  1 + rand() % 3;  break;  // 1~3
        case REWARD_MANAPOTION:  val =  1 + rand() % 3;  break;  // 1~3
        case REWARD_REVIVE:      val =  1; break;                // 1 个复活卷轴
        default: val = 1; break;
        }
        m_rewardCards[i].type = t;
        m_rewardCards[i].value = val;
    }
}

void CYoulinView::ApplyReward()
{
    if (m_rewardSel < 0 || m_rewardSel > 2) return;
    const RewardItem& r = m_rewardCards[m_rewardSel];
    switch (r.type) {
    case REWARD_HEAL:       m_player->Heal(r.value); break;
    case REWARD_MANA:       m_player->RegenMP(r.value); break;
    case REWARD_COIN:       m_player->AddCoin(r.value); break;
    case REWARD_PERMA_ATK:  m_player->AddPermaATK(r.value); break;
    case REWARD_PERMA_HP:   m_player->AddPermaHP(r.value); break;
    case REWARD_PERMA_MP:   m_player->AddPermaMP(r.value); break;
    case REWARD_PERMA_DEF:  m_player->AddPermaDEF(r.value); break;
    case REWARD_POTION:     for (int i=0;i<r.value;++i) m_player->AddItem(ITEM_POTION); break;
    case REWARD_MANAPOTION: for (int i=0;i<r.value;++i) m_player->AddItem(ITEM_MANA); break;
    case REWARD_REVIVE:     m_player->AddItem(ITEM_REVIVE); break;
    default: break;
    }
    EnterChapterEnd();
}

void CYoulinView::NextChapter()
{
    if (m_chapter == CHAP_4) {
        // 通关判定: HP >= 60% 为真结局
        if (m_player->GetHP() * 100 / m_player->GetMaxHP() >= 60) {
            m_state = STATE_ENDING_TRUE;
            m_story.LoadEndingTrue();
        } else {
            m_state = STATE_ENDING_NORMAL;
            m_story.LoadEndingNormal();
        }
    } else {
        m_chapter = (Chapter)(m_chapter + 1);
        EnterChapterBegin();
    }
}

// =============================================
// 敌人波次
// =============================================
void CYoulinView::ClearEnemies()
{
    for (auto* e : m_enemies) delete e;
    m_enemies.clear();
}

BOOL CYoulinView::AllEnemiesDead()
{
    for (auto* e : m_enemies) if (e->IsAlive()) return FALSE;
    return TRUE;
}

// =============================================
// 关卡内对话系统(镜头跟随说话者)
// =============================================
void CYoulinView::StartDialog(const DialogLine* lines, int count, BOOL afterBoss)
{
    m_dialogLines = lines;
    m_dialogCount = count;
    m_dialogIndex = 0;
    m_dialogCharShown = 0;
    m_dialogCharTick = 0;
    m_dialogAfterIsBoss = afterBoss;
    m_dialogWarmup = 30;   // 30 帧预热: 镜头平滑移动到位再开始打字
    m_state = STATE_STORY_MID;
}

// 是否有怪还在出生预警(召唤圈特效中)
BOOL CYoulinView::AnyEnemySpawning()
{
    for (auto* e : m_enemies)
        if (e->IsAlive() && e->IsSpawning()) return TRUE;
    return FALSE;
}

// 先刷怪,记录待触发对话,等怪出生完成后(OnTimer 轮询)再真正 StartDialog
void CYoulinView::QueueDialogAfterSpawn(const DialogLine* lines, int count)
{
    m_pendingDialog = lines;
    m_pendingDialogCount = count;
    m_pendingWaitFrames = 120;  // 最多等 2 秒(兜底,防止卡住)
    // 镜头先开始移向怪(在 STATE_PLAYING 下 UpdateCamera 会跟随,但我们手动推一把)
}

Vec2 CYoulinView::GetDialogFocusPos()
{
    // 根据当前对话说话者返回世界坐标
    if (!m_dialogLines || m_dialogIndex >= m_dialogCount)
        return m_player->GetPos();
    StorySpeaker sp = m_dialogLines[m_dialogIndex].speaker;
    if (sp == SPEAKER_PLAYER || sp == SPEAKER_NARRATION)
        return m_player->GetPos();
    if (sp == SPEAKER_BOSS) {
        for (auto* e : m_enemies) {
            if (dynamic_cast<CFogCore*>(e) && e->IsAlive())
                return e->GetPos();
        }
    }
    // SPEAKER_ENEMY: 最近的存活怪
    CEnemy* nearest = NULL; float best = 1e18f;
    Vec2 pp = m_player->GetPos();
    for (auto* e : m_enemies) {
        if (!e->IsAlive()) continue;
        float dx = e->GetPos().x - pp.x, dy = e->GetPos().y - pp.y;
        float d = dx*dx + dy*dy;
        if (d < best) { best = d; nearest = e; }
    }
    if (nearest) return nearest->GetPos();
    return m_player->GetPos();
}

void CYoulinView::UpdateDialog()
{
    // 镜头平滑移到说话者
    Vec2 focus = GetDialogFocusPos();
    float target = focus.x - WIN_W/2.0f;
    if (target < 0) target = 0;
    if (target > LEVEL_W - WIN_W) target = (float)(LEVEL_W - WIN_W);
    m_camX += (target - m_camX) * 0.12f;

    // 预热阶段: 等镜头到位 + 等说话的怪出生预警结束,再开始打字
    if (m_dialogWarmup > 0) {
        m_dialogWarmup--;
        return;
    }
    // 若当前说话者是怪/BOSS 且仍在出生预警中,继续等待(不打字),避免对着召唤特效说话
    if (m_dialogIndex < m_dialogCount) {
        StorySpeaker sp = m_dialogLines[m_dialogIndex].speaker;
        if (sp == SPEAKER_ENEMY || sp == SPEAKER_BOSS) {
            BOOL speakerSpawning = FALSE;
            if (sp == SPEAKER_BOSS) {
                for (auto* e : m_enemies)
                    if (dynamic_cast<CFogCore*>(e) && e->IsAlive() && e->IsSpawning())
                        speakerSpawning = TRUE;
            } else {
                // 最近的怪是否在出生
                CEnemy* nearest = NULL; float best = 1e18f;
                Vec2 pp = m_player->GetPos();
                for (auto* e : m_enemies) {
                    if (!e->IsAlive()) continue;
                    float dx = e->GetPos().x - pp.x, dy = e->GetPos().y - pp.y;
                    float d = dx*dx + dy*dy;
                    if (d < best) { best = d; nearest = e; }
                }
                if (nearest && nearest->IsSpawning()) speakerSpawning = TRUE;
            }
            if (speakerSpawning) return;  // 等怪出生完再说话
        }
    }

    // 打字机
    if (m_dialogIndex < m_dialogCount) {
        int total = m_dialogLines[m_dialogIndex].content.GetLength();
        m_dialogCharTick++;
        if (m_dialogCharTick % 4 == 0 && m_dialogCharShown < total) {
            m_dialogCharShown++;
            TCHAR c = m_dialogLines[m_dialogIndex].content.GetAt(m_dialogCharShown-1);
            if (c != _T(' ') && c != _T('\n')) g_audio.PlaySfx(SFX_TYPING);
        }
    }
}

void CYoulinView::AdvanceDialog()
{
    if (m_dialogWarmup > 0) return;  // 预热期间不可推进
    if (m_dialogIndex >= m_dialogCount) return;
    int total = m_dialogLines[m_dialogIndex].content.GetLength();
    if (m_dialogCharShown < total) {
        // 未显示完: 一次显示全部
        m_dialogCharShown = total;
        return;
    }
    // 进入下一句
    m_dialogIndex++;
    m_dialogCharShown = 0;
    m_dialogCharTick = 0;
    if (m_dialogIndex >= m_dialogCount) {
        // 对话结束
        if (m_dialogAfterIsBoss) {
            m_currentWave++;
            SpawnWave(m_currentWave);
        }
        m_state = STATE_PLAYING;
    }
}

void CYoulinView::SpawnWave(int waveIdx)
{
    ClearEnemies();
    const DifficultyConfig& dc = DIFF_CONFIG[m_diff];
    float mul = dc.enemyHP;
    int extra = dc.extraWave;

    // 出生范围: 玩家右侧屏幕外延到关卡末端
    auto randX = [&]() {
        float minX = m_player->GetPos().x + 250;
        float maxX = (float)LEVEL_W - 60;
        if (minX > maxX - 100) minX = maxX - 100;
        return minX + (rand() % (int)(maxX - minX));
    };
    auto randY = [&]() { return (float)GROUND_TOP_Y + (rand() % (WIN_H - GROUND_TOP_Y - 40)); };

    // 统一创建 helper: 创建怪物 + 应用难度速度/攻击频率
    auto spawn = [&](CEnemy* e) {
        e->ApplyDifficulty(dc.enemySpeed, dc.enemyAtkFreq);
        m_enemies.push_back(e);
    };

    switch (m_chapter) {
    case CHAP_1: {
        int n = 3 + waveIdx + extra;
        for (int i = 0; i < n; ++i) spawn(new CGhost(randX(), randY(), mul));
        break;
    }
    case CHAP_2: {
        if (waveIdx == 0) {
            for (int i = 0; i < 2 + extra; ++i) spawn(new CTreeGuard(randX(), randY(), mul));
            for (int i = 0; i < 2; ++i) spawn(new CBat(randX(), randY(), mul));
        } else if (waveIdx == 1) {
            for (int i = 0; i < 3; ++i) spawn(new CBat(randX(), randY(), mul));
            spawn(new CTreeGuard(randX(), randY(), mul));
        } else {
            for (int i = 0; i < 3 + extra; ++i) spawn(new CTreeGuard(randX(), randY(), mul));
            for (int i = 0; i < 3; ++i) spawn(new CBat(randX(), randY(), mul));
        }
        break;
    }
    case CHAP_3: {
        if (waveIdx == 0) {
            for (int i = 0; i < 3; ++i) spawn(new CFox(randX(), randY(), mul));
        } else if (waveIdx == 1) {
            for (int i = 0; i < 2; ++i) spawn(new CWolf(randX(), randY(), mul));
            for (int i = 0; i < 2; ++i) spawn(new CFox(randX(), randY(), mul));
        } else if (waveIdx == 2) {
            for (int i = 0; i < 2; ++i) spawn(new CAssassin(randX(), randY(), mul));
            for (int i = 0; i < 2; ++i) spawn(new CWolf(randX(), randY(), mul));
        } else {
            for (int i = 0; i < 2 + extra; ++i) spawn(new CAssassin(randX(), randY(), mul));
            spawn(new CWolf(randX(), randY(), mul));
            spawn(new CFox(randX(), randY(), mul));
        }
        break;
    }
    case CHAP_4: {
        if (waveIdx + 1 < CHAPTER_WAVES[CHAP_4]) {
            for (int i = 0; i < 3 + waveIdx; ++i) spawn(new CWraith(randX(), randY(), mul));
        } else {
            spawn(new CFogCore(LEVEL_W * 0.75f, WIN_H * 0.78f, mul));
            for (int i = 0; i < 2; ++i) spawn(new CWraith(randX(), randY(), mul));
        }
        break;
    }
    }
}

// =============================================
// 渲染主入口
// =============================================
void CYoulinView::OnDraw(CDC* pDC)
{
    EnsureBackBuffer();
    CDC* b = &m_backDC;
    b->FillSolidRect(0, 0, WIN_W, WIN_H, RGB(0,0,0));

    switch (m_state) {
    case STATE_MENU:          DrawMenu(b); break;
    case STATE_DIFFICULTY:    DrawDifficulty(b); break;
    case STATE_LOAD:          DrawLoadList(b); break;
    case STATE_STORY_BEGIN:
    case STATE_STORY_END:
    case STATE_ENDING_TRUE:
    case STATE_ENDING_NORMAL: m_story.Draw(b); break;
    case STATE_PLAYING:
        DrawBackground(b);
        DrawPlaying(b);
        DrawHUD(b);
        if (m_shopOpen) DrawShop(b);
        break;
    case STATE_STORY_MID:
        // 关卡内对话: 场景作背景 + 对话框
        DrawBackground(b);
        DrawPlaying(b);
        DrawDialog(b);
        break;
    case STATE_PAUSE:
        DrawBackground(b);
        DrawPlaying(b);
        DrawHUD(b);
        if (m_cheatOpen) DrawCheat(b);
        else DrawPause(b);
        break;
    case STATE_REWARD:
        DrawBackground(b);
        DrawPlaying(b);  // 仍然画着场景作背景
        DrawReward(b);
        break;
    case STATE_VICTORY:  DrawVictory(b); break;
    case STATE_GAMEOVER: DrawGameOver(b); break;
    }
    // 全屏拉伸输出(16:9 画布匹配宽屏,直接铺满,无需填黑→消除闪烁)
    CRect cr; GetClientRect(&cr);
    pDC->SetStretchBltMode(COLORONCOLOR);
    pDC->StretchBlt(0, 0, cr.Width(), cr.Height(),
                    b, 0, 0, WIN_W, WIN_H, SRCCOPY);
}

// =============================================
// 背景(滚动)
// =============================================
void CYoulinView::DrawBackground(CDC* pDC)
{
    SpriteID bg[4] = { SPR_BG_CH1, SPR_BG_CH2, SPR_BG_CH3, SPR_BG_CH4 };
    // 地面带从 GROUND_Y 开始(屏幕下方 ~38%)
    const int GROUND_Y = (int)(WIN_H * 0.62f);

    if (g_sprites.Has(bg[m_chapter])) {
        int parallaxCam = (int)(m_camX * 0.6f);
        // 只把背景图拉伸到屏幕可见区(WIN_W 宽),而不是整个 LEVEL_W(3000)
        // 背景随视差缓慢滚动: 用源图横向偏移模拟。这里简化为整图缩放铺满一屏 + 视差位移
        // 为保留滚动感,把背景画成约 1.5 屏宽,跟随视差移动
        int bgW = (int)(WIN_W * 1.6f);
        int ox = -(parallaxCam % bgW);
        // 画两段保证铺满(滚动衔接)
        g_sprites.DrawStretch(pDC, bg[m_chapter], CRect(ox, 0, ox + bgW, WIN_H));
        if (ox + bgW < WIN_W)
            g_sprites.DrawStretch(pDC, bg[m_chapter], CRect(ox + bgW, 0, ox + 2*bgW, WIN_H));
    } else {
        // 回退纯色天空
        COLORREF sky;
        switch (m_chapter) {
        case CHAP_1: sky = RGB(70,90,80);  break;
        case CHAP_2: sky = RGB(60,80,50);  break;
        case CHAP_3: sky = RGB(45,45,75);  break;
        default:     sky = RGB(65,35,55);  break;
        }
        pDC->FillSolidRect(0, 0, WIN_W, WIN_H, sky);
    }

    // === 地面带(失落城堡式行走路面) ===
    // 地面用深色渐变带 + 纹理线,营造"路"的感觉
    COLORREF groundTop, groundMain;
    switch (m_chapter) {
    case CHAP_1: groundTop = RGB(46,54,40); groundMain = RGB(28,32,24); break;
    case CHAP_2: groundTop = RGB(40,48,28); groundMain = RGB(22,28,16); break;
    case CHAP_3: groundTop = RGB(34,32,52); groundMain = RGB(18,18,30); break;
    default:     groundTop = RGB(44,26,40); groundMain = RGB(24,14,22); break;
    }
    // 地面上沿高光线
    pDC->FillSolidRect(0, GROUND_Y, WIN_W, 3, groundTop);
    // 地面主体
    pDC->FillSolidRect(0, GROUND_Y+3, WIN_W, WIN_H-GROUND_Y-3, groundMain);
    // 路面纹理: 随 camX 滚动的横向短线
    CPen tex(PS_SOLID, 1, groundTop);
    CPen* po = pDC->SelectObject(&tex);
    int camOff = (int)m_camX % 80;
    for (int gx = -camOff; gx < WIN_W; gx += 80) {
        pDC->MoveTo(gx, GROUND_Y + 20);
        pDC->LineTo(gx + 40, GROUND_Y + 20);
        pDC->MoveTo(gx + 20, GROUND_Y + 50);
        pDC->LineTo(gx + 60, GROUND_Y + 50);
    }
    pDC->SelectObject(po);
}

// =============================================
// 战斗世界绘制(用 SetViewportOrg 平移)
// =============================================
void CYoulinView::DrawPlaying(CDC* pDC)
{
    // 平移视口: 所有世界坐标减 camX
    int icam = (int)m_camX;
    pDC->SetViewportOrg(-icam, 0);

    // 关卡右边界提示线
    pDC->FillSolidRect(LEVEL_W - 2, 60, 4, WIN_H - 80, RGB(255, 220, 80));

    // 敌人
    for (auto* e : m_enemies)
        if (e->IsAlive()) e->Draw(pDC, m_frame);
    // 玩家
    m_player->Draw(pDC, m_frame);
    // 投射物
    m_proj.Draw(pDC);

    // 还原 viewport,后续 HUD 在屏幕坐标
    pDC->SetViewportOrg(0, 0);
}

// =============================================
// 主菜单
// =============================================
void CYoulinView::DrawMenu(CDC* pDC)
{
    if (g_sprites.Has(SPR_MENU_BG))
        g_sprites.DrawStretch(pDC, SPR_MENU_BG, CRect(0,0,WIN_W,WIN_H));
    else
        pDC->FillSolidRect(0, 0, WIN_W, WIN_H, RGB(20,25,45));

    // 半透明遮罩
    for (int y = 0; y < WIN_H; y += 2)
        pDC->FillSolidRect(0, y, WIN_W, 1, RGB(0, 0, 0));

    pDC->SetBkMode(TRANSPARENT);

    // 标题
    CFont titleF;
    titleF.CreateFont(80, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        FF_DONTCARE, _T("微软雅黑"));
    CFont* po = pDC->SelectObject(&titleF);
    pDC->SetTextColor(RGB(255, 200, 80));
    pDC->DrawText(_T("幽 林 守 夜 人"),
        CRect(0, 120, WIN_W, 220), DT_CENTER|DT_VCENTER|DT_SINGLELINE);

    CFont subF;
    subF.CreateFont(22, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        FF_DONTCARE, _T("微软雅黑"));
    pDC->SelectObject(&subF);
    pDC->SetTextColor(RGB(220, 220, 240));
    pDC->DrawText(_T("一名孤独法师 · 一片沉睡森林 · 一场最后的长夜"),
        CRect(0, 240, WIN_W, 270), DT_CENTER|DT_SINGLELINE);

    // 难度
    pDC->SetTextColor(RGB(200, 180, 120));
    CString diffText;
    diffText.Format(_T("当前难度: %s"),
        m_diff==DIFF_EASY?_T("简单"):m_diff==DIFF_NORMAL?_T("普通"):_T("困难"));
    pDC->DrawText(diffText,
        CRect(0, 305, WIN_W, 330), DT_CENTER|DT_SINGLELINE);

    // 4 个按钮
    const TCHAR* names[4] = { _T("开始新游戏"), _T("加载游戏"), _T("选择难度"), _T("退出游戏") };
    CFont btnF;
    btnF.CreateFont(28, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        FF_DONTCARE, _T("微软雅黑"));
    pDC->SelectObject(&btnF);
    int btnW = 280, btnH = 56;
    int bx = (WIN_W - btnW) / 2;
    int by = 360;
    for (int i = 0; i < 4; ++i) {
        CRect rc(bx, by + i*70, bx + btnW, by + i*70 + btnH);
        m_menuItemRect[i] = rc;
        BOOL hover = rc.PtInRect(m_mouseHover) || m_menuSel == i;
        pDC->FillSolidRect(rc, hover ? RGB(120, 60, 40) : RGB(40, 40, 60));
        pDC->Draw3dRect(rc, RGB(220,180,90), RGB(80,40,20));
        pDC->SetTextColor(hover ? RGB(255, 230, 120) : RGB(220, 220, 220));
        pDC->DrawText(names[i], rc, DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    }

    // 提示
    CFont tipF;
    tipF.CreateFont(16, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        FF_DONTCARE, _T("微软雅黑"));
    pDC->SelectObject(&tipF);
    pDC->SetTextColor(RGB(180,180,200));
    pDC->DrawText(_T("↑↓ 选择   Enter 确认   也可用鼠标点击   F11 全屏"),
        CRect(0, WIN_H - 50, WIN_W, WIN_H - 30), DT_CENTER|DT_SINGLELINE);
    pDC->SelectObject(po);
}

// =============================================
// 难度选择
// =============================================
void CYoulinView::DrawDifficulty(CDC* pDC)
{
    pDC->FillSolidRect(0, 0, WIN_W, WIN_H, RGB(15, 18, 30));
    pDC->SetBkMode(TRANSPARENT);

    CFont titF;
    titF.CreateFont(48, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        FF_DONTCARE, _T("微软雅黑"));
    CFont* po = pDC->SelectObject(&titF);
    pDC->SetTextColor(RGB(220, 200, 120));
    pDC->DrawText(_T("选 择 难 度"),
        CRect(0, 80, WIN_W, 160), DT_CENTER|DT_VCENTER|DT_SINGLELINE);

    const TCHAR* names[3] = { _T("简单 · 漫步幽林"), _T("普通 · 标准守夜"), _T("困难 · 雾境噩梦") };
    const TCHAR* descs[3] = {
        _T("敌人血量×0.65  伤害×0.6  速度×0.85  攻击频率×0.8\n金币×1.6   适合初次体验"),
        _T("全维度标准  金币×1.5\n推荐挑战难度"),
        _T("敌人血量×1.45  伤害×1.4  速度×1.25  攻击频率×1.4\n金币×2.0   每章多刷 1 波   极限挑战")
    };
    CFont btnF;
    btnF.CreateFont(28, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        FF_DONTCARE, _T("微软雅黑"));
    pDC->SelectObject(&btnF);
    int rx = (WIN_W - 700) / 2;
    for (int i = 0; i < 3; ++i) {
        CRect rc(rx, 200 + i*120, rx + 700, 200 + i*120 + 100);
        m_diffItemRect[i] = rc;
        BOOL hover = rc.PtInRect(m_mouseHover) || m_diffSel == i;
        pDC->FillSolidRect(rc, hover ? RGB(80, 40, 30) : RGB(40, 40, 50));
        pDC->Draw3dRect(rc, RGB(180, 140, 70), RGB(60, 30, 10));
        pDC->SetTextColor(hover ? RGB(255, 230, 120) : RGB(220, 220, 220));
        pDC->DrawText(names[i], CRect(rc.left+20, rc.top+8, rc.right-20, rc.top+40),
                      DT_LEFT|DT_VCENTER|DT_SINGLELINE);
        CFont descF;
        descF.CreateFont(18, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            FF_DONTCARE, _T("微软雅黑"));
        CFont* po2 = pDC->SelectObject(&descF);
        pDC->SetTextColor(RGB(180,180,200));
        pDC->DrawText(descs[i], CRect(rc.left+20, rc.top+42, rc.right-20, rc.bottom-8),
                      DT_LEFT|DT_TOP);
        pDC->SelectObject(po2);
        pDC->SelectObject(&btnF);
    }
    pDC->SelectObject(po);

    CFont tipF;
    tipF.CreateFont(16, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        FF_DONTCARE, _T("微软雅黑"));
    pDC->SelectObject(&tipF);
    pDC->SetTextColor(RGB(180,180,200));
    pDC->DrawText(_T("Esc 返回   Enter 确认"),
        CRect(0, WIN_H - 50, WIN_W, WIN_H - 30), DT_CENTER|DT_SINGLELINE);
}

// =============================================
// 读档列表
// =============================================
void CYoulinView::DrawLoadList(CDC* pDC)
{
    pDC->FillSolidRect(0, 0, WIN_W, WIN_H, RGB(20, 25, 35));
    pDC->SetBkMode(TRANSPARENT);

    CFont titF;
    titF.CreateFont(48, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        FF_DONTCARE, _T("微软雅黑"));
    CFont* po = pDC->SelectObject(&titF);
    pDC->SetTextColor(RGB(180, 220, 240));
    pDC->DrawText(_T("加 载 游 戏"),
        CRect(0, 80, WIN_W, 160), DT_CENTER|DT_VCENTER|DT_SINGLELINE);

    CFont slotF;
    slotF.CreateFont(24, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        FF_DONTCARE, _T("微软雅黑"));
    pDC->SelectObject(&slotF);
    int rx = (WIN_W - 700) / 2;
    for (int i = 0; i < 3; ++i) {
        CRect rc(rx, 200 + i*120, rx + 700, 200 + i*120 + 100);
        m_loadItemRect[i] = rc;
        BOOL hover = rc.PtInRect(m_mouseHover) || m_loadSel == i;
        pDC->FillSolidRect(rc, hover ? RGB(40,80,100) : RGB(35,40,55));
        pDC->Draw3dRect(rc, RGB(140, 180, 220), RGB(30, 40, 60));
        SaveSlotInfo info = CSaveManager::Query(i);
        CString txt;
        if (info.exists)
            txt.Format(_T("槽位 %d · 第 %d 章 · 难度: %s · 金币 %d"),
                i+1, info.chapter+1,
                info.difficulty==DIFF_EASY?_T("简单"):info.difficulty==DIFF_NORMAL?_T("普通"):_T("困难"),
                info.coin);
        else
            txt.Format(_T("槽位 %d · 空"), i+1);
        pDC->SetTextColor(hover ? RGB(255, 255, 200) : RGB(220, 220, 220));
        pDC->DrawText(txt, rc, DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    }
    pDC->SelectObject(po);

    CFont tipF;
    tipF.CreateFont(16, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        FF_DONTCARE, _T("微软雅黑"));
    pDC->SelectObject(&tipF);
    pDC->SetTextColor(RGB(180,180,200));
    pDC->DrawText(_T("Esc 返回   Enter 确认"),
        CRect(0, WIN_H - 50, WIN_W, WIN_H - 30), DT_CENTER|DT_SINGLELINE);
}

// =============================================
// HUD (屏幕坐标)
// =============================================
static void DrawIconSlot(CDC* pDC, int x, int y, int s,
                         SpriteID spr, COLORREF fallbackBg,
                         const CString& label, COLORREF lblCol)
{
    CRect rc(x, y, x+s, y+s);
    pDC->FillSolidRect(rc, RGB(20,20,30));
    pDC->Draw3dRect(rc, RGB(160,140,80), RGB(40,30,10));
    if (g_sprites.Has(spr))
        g_sprites.DrawSized(pDC, spr, x+4, y+4, s-8, s-8);
    else
        pDC->FillSolidRect(x+6, y+6, s-12, s-12, fallbackBg);
    pDC->SetBkMode(TRANSPARENT);
    pDC->SetTextColor(lblCol);
    pDC->DrawText(label, CRect(x, y+s, x+s, y+s+18), DT_CENTER);
}

void CYoulinView::DrawHUD(CDC* pDC)
{
    // 顶部 HUD 条
    pDC->FillSolidRect(0, 0, WIN_W, 56, RGB(10, 12, 22));
    pDC->FillSolidRect(0, 56, WIN_W, 1, RGB(200, 180, 80));

    // HP/MP 条
    int barX = 16, barY = 10;
    int barW = 250;
    pDC->FillSolidRect(barX, barY, barW, 16, RGB(70, 20, 20));
    int hpW = barW * m_player->GetHP() / m_player->GetMaxHP();
    if (hpW < 0) hpW = 0;
    pDC->FillSolidRect(barX, barY, hpW, 16, RGB(220, 60, 60));
    pDC->Draw3dRect(barX-1, barY-1, barW+2, 18, RGB(160,160,160), RGB(60,60,60));
    pDC->FillSolidRect(barX, barY+22, barW, 16, RGB(20, 30, 70));
    int mpW = barW * m_player->GetMP() / m_player->GetMaxMP();
    if (mpW < 0) mpW = 0;
    pDC->FillSolidRect(barX, barY+22, mpW, 16, RGB(60, 130, 220));
    pDC->Draw3dRect(barX-1, barY+21, barW+2, 18, RGB(160,160,160), RGB(60,60,60));

    pDC->SetBkMode(TRANSPARENT);
    CFont hudF;
    hudF.CreateFont(14, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        FF_DONTCARE, _T("微软雅黑"));
    CFont* po = pDC->SelectObject(&hudF);
    pDC->SetTextColor(RGB(255,255,255));
    CString hpTxt, mpTxt, coinTxt, chapTxt;
    hpTxt.Format(_T("HP %d/%d"), m_player->GetHP(), m_player->GetMaxHP());
    mpTxt.Format(_T("MP %d/%d"), m_player->GetMP(), m_player->GetMaxMP());
    pDC->DrawText(hpTxt, CRect(barX, barY, barX+barW, barY+16),
                  DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    pDC->DrawText(mpTxt, CRect(barX, barY+22, barX+barW, barY+38),
                  DT_CENTER|DT_VCENTER|DT_SINGLELINE);

    // 金币
    coinTxt.Format(_T("💰 %d"), m_player->GetCoin());
    pDC->SetTextColor(RGB(255, 220, 80));
    pDC->DrawText(coinTxt, CRect(barX + barW + 20, barY+4, barX + barW + 200, barY+32),
                  DT_LEFT|DT_VCENTER|DT_SINGLELINE);

    // 章节信息 + 波次
    chapTxt.Format(_T("第%d章 · 月落幽林 · 波 %d/%d"),
        m_chapter+1, m_currentWave+1, CHAPTER_WAVES[m_chapter]);
    pDC->SetTextColor(RGB(220, 220, 240));
    pDC->DrawText(chapTxt, CRect(0, 8, WIN_W, 30), DT_CENTER|DT_SINGLELINE);

    // 4 个技能图标(右上)
    SpriteID skillIcons[4] = { SPR_SKILL_ICE, SPR_SKILL_LIGHTNING, SPR_SKILL_STORM, SPR_SKILL_ARCANE };
    COLORREF skillColors[4] = { RGB(120,200,255), RGB(255,220,80), RGB(255,80,40), RGB(60,220,100) };
    const TCHAR* skey[4] = { _T("J"), _T("K"), _T("L"), _T("I") };
    int sx = WIN_W - 280;
    for (int i = 0; i < 4; ++i) {
        int x = sx + i*50, y = 4;
        CRect rc(x, y, x+44, y+44);
        pDC->FillSolidRect(rc, RGB(20,20,30));
        pDC->Draw3dRect(rc, RGB(160,140,80), RGB(40,30,10));
        if (g_sprites.Has(skillIcons[i]))
            g_sprites.DrawSized(pDC, skillIcons[i], x+4, y+4, 36, 36);
        else
            pDC->FillSolidRect(x+6, y+6, 32, 32, skillColors[i]);
        // 冷却灰
        int cd = m_player->GetSkillCD((SkillType)i);
        if (cd > 0) {
            int cdH = 36 * cd / 120;
            if (cdH > 36) cdH = 36;
            pDC->FillSolidRect(x+4, y+4 + 36 - cdH, 36, cdH, RGB(60,60,60));
        }
        // 技能等级
        int lv = m_player->GetSkillLv((SkillType)i);
        if (lv > 0) {
            CString lvTxt;
            lvTxt.Format(_T("Lv%d"), lv);
            pDC->SetTextColor(RGB(255, 220, 80));
            pDC->DrawText(lvTxt, CRect(x, y+24, x+44, y+44), DT_RIGHT|DT_BOTTOM|DT_SINGLELINE);
        }
        pDC->SetTextColor(RGB(255,255,255));
        pDC->DrawText(skey[i], CRect(x, y, x+18, y+16),
                      DT_LEFT|DT_TOP|DT_SINGLELINE);
    }

    // 冲刺图标(技能图标右边)
    {
        int x = sx + 4*50 + 10, y = 4;
        CRect rc(x, y, x+44, y+44);
        pDC->FillSolidRect(rc, RGB(20,20,30));
        pDC->Draw3dRect(rc, RGB(160,220,255), RGB(40,80,160));
        // 冲刺图形: 三条向右的快速线
        CPen pen(PS_SOLID, 3, RGB(150, 220, 255));
        CPen* pp = pDC->SelectObject(&pen);
        pDC->MoveTo(x+8,  y+14); pDC->LineTo(x+34, y+14);
        pDC->MoveTo(x+12, y+22); pDC->LineTo(x+38, y+22);
        pDC->MoveTo(x+8,  y+30); pDC->LineTo(x+34, y+30);
        pDC->SelectObject(pp);
        // 冷却灰罩
        int dcd = m_player->GetDashCD();
        if (dcd > 0) {
            int cdH = 36 * dcd / DASH_CD;
            if (cdH > 36) cdH = 36;
            pDC->FillSolidRect(x+4, y+4 + 36 - cdH, 36, cdH, RGB(60,60,60));
        }
        pDC->SetBkMode(TRANSPARENT);
        pDC->SetTextColor(RGB(255,255,255));
        pDC->DrawText(_T("Shift"), CRect(x, y, x+44, y+18),
                      DT_LEFT|DT_TOP|DT_SINGLELINE);
    }

    // 4 个道具图标(左下)
    SpriteID itemIcons[4] = { SPR_ITEM_POTION, SPR_ITEM_MANA, SPR_ITEM_AMULET, SPR_ITEM_SCROLL };
    COLORREF itemColors[4] = { RGB(220,30,60), RGB(60,100,220), RGB(220,180,40), RGB(240,220,160) };
    const TCHAR* ikey[4] = { _T("1"), _T("2"), _T("3"), _T("4") };
    int iy = WIN_H - 78;
    for (int i = 0; i < 4; ++i) {
        int x = 16 + i*54;
        int cnt = m_player->GetItem((ItemType)i);
        CString label;
        label.Format(_T("%d %s"), cnt, ikey[i]);
        DrawIconSlot(pDC, x, iy, 48, itemIcons[i], itemColors[i],
                     label, cnt > 0 ? RGB(240,240,240) : RGB(120,120,120));
    }

    // BOSS 血条(第4章最后一波)
    if (m_chapter == CHAP_4 && m_currentWave + 1 == CHAPTER_WAVES[CHAP_4]) {
        CFogCore* boss = NULL;
        for (auto* e : m_enemies) {
            CFogCore* fc = dynamic_cast<CFogCore*>(e);
            if (fc) { boss = fc; break; }
        }
        if (boss && boss->IsAlive()) {
            int barW2 = 600, bx = (WIN_W - barW2)/2;
            pDC->FillSolidRect(bx-2, 60, barW2+4, 24, RGB(20,20,30));
            pDC->FillSolidRect(bx,   62, barW2,   20, RGB(60,20,20));
            int bhp = boss->GetHP(), bmaxhp = boss->GetMaxHP();
            // 第二条命血条用紫红色,第一条命用红色
            COLORREF hpCol = (boss->GetLifeStage() == 2) ? RGB(180, 60, 220) : RGB(200,80,100);
            pDC->FillSolidRect(bx, 62, barW2*bhp/bmaxhp, 20, hpCol);
            CFont bossF;
            bossF.CreateFont(18, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET,
                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                FF_DONTCARE, _T("微软雅黑"));
            CFont* pBossF = pDC->SelectObject(&bossF);
            pDC->SetTextColor(RGB(240,240,240));
            // 名字 + 形态标识
            CString bossName;
            if (boss->GetLifeStage() == 2)
                bossName = _T("雾核守卫 · 终焉形态");
            else
                bossName.Format(_T("雾核守卫 · %s"), boss->GetPhase() == 2 ? _T("觉醒") : _T("守势"));
            pDC->DrawText(bossName, CRect(bx, 88, bx+barW2, 110), DT_CENTER);
            // 命数标记(两颗心,代表两条命)
            int hearts = (boss->GetLifeStage() == 1) ? 2 : 1;
            for (int hh = 0; hh < 2; ++hh) {
                COLORREF hc = (hh < hearts) ? RGB(255,80,80) : RGB(70,70,70);
                pDC->FillSolidRect(bx + barW2 - 50 + hh*22, 64, 16, 16, hc);
            }
            pDC->SelectObject(pBossF);
        }
    }

    // 非 BOSS 关清完所有波: 提示玩家往右走
    if (m_allWavesCleared && m_chapter != CHAP_4) {
        CFont tipF;
        tipF.CreateFont(34, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            FF_DONTCARE, _T("微软雅黑"));
        CFont* ptf = pDC->SelectObject(&tipF);
        pDC->SetTextColor(RGB(255, 230, 120));
        // 闪烁
        if ((m_frame / 20) % 2 == 0) {
            pDC->DrawText(_T("本区域已清空！ 向右前进 → 继续探索"),
                CRect(0, 130, WIN_W, 175), DT_CENTER|DT_SINGLELINE);
        }
        // 右侧大箭头
        int ax = WIN_W - 90, ay = WIN_H/2;
        int off = (m_frame/6) % 20;
        CBrush arr(RGB(255, 220, 100));
        CBrush* pab = pDC->SelectObject(&arr);
        CPen* pap = (CPen*)pDC->SelectStockObject(NULL_PEN);
        POINT tri[3] = { {ax+off, ay-30}, {ax+off, ay+30}, {ax+40+off, ay} };
        pDC->Polygon(tri, 3);
        pDC->SelectObject(pap);
        pDC->SelectObject(pab);
        pDC->SelectObject(ptf);
    }

    pDC->SelectObject(po);
}

// =============================================
// 波次过渡提示
// =============================================
void CYoulinView::DrawWaveTransition(CDC* pDC)
{
    // 半透明遮罩
    for (int y = 64; y < WIN_H - 60; y += 3)
        pDC->FillSolidRect(0, y, WIN_W, 1, RGB(0, 0, 0));

    pDC->SetBkMode(TRANSPARENT);
    CFont tf;
    tf.CreateFont(60, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        FF_DONTCARE, _T("微软雅黑"));
    CFont* po = pDC->SelectObject(&tf);
    pDC->SetTextColor(RGB(255, 220, 100));
    CString wave;
    wave.Format(_T("第 %d 波 来 袭"), m_currentWave + 2);
    pDC->DrawText(wave, CRect(0, 280, WIN_W, 360), DT_CENTER|DT_VCENTER|DT_SINGLELINE);

    CFont sf;
    sf.CreateFont(22, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        FF_DONTCARE, _T("微软雅黑"));
    pDC->SelectObject(&sf);
    pDC->SetTextColor(RGB(220, 220, 220));
    CString sub;
    sub.Format(_T("准备战斗  %.1f s"), m_waveTransitionLeft / 60.0f);
    pDC->DrawText(sub, CRect(0, 380, WIN_W, 410), DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    pDC->SelectObject(po);
}

// =============================================
// 奖励三选一
// =============================================
// 工具:奖励类型 → 显示名/描述/颜色
static void GetRewardLabel(const RewardItem& r, CString& title, CString& desc, COLORREF& col)
{
    switch (r.type) {
    case REWARD_HEAL:
        title.Format(_T("回 复 生 命")); desc.Format(_T("立即恢复 %d HP"), r.value);
        col = RGB(180, 40, 60); break;
    case REWARD_MANA:
        title.Format(_T("回 复 法 力")); desc.Format(_T("立即恢复 %d MP"), r.value);
        col = RGB(40, 80, 200); break;
    case REWARD_COIN:
        title.Format(_T("守 夜 金 币")); desc.Format(_T("获得 %d 金币"), r.value);
        col = RGB(200, 160, 40); break;
    case REWARD_PERMA_ATK:
        title.Format(_T("攻 击 印 记")); desc.Format(_T("永久 +%d ATK"), r.value);
        col = RGB(180, 80, 30); break;
    case REWARD_PERMA_HP:
        title.Format(_T("生 命 印 记")); desc.Format(_T("永久最大 HP +%d"), r.value);
        col = RGB(160, 30, 80); break;
    case REWARD_PERMA_MP:
        title.Format(_T("法 力 印 记")); desc.Format(_T("永久最大 MP +%d"), r.value);
        col = RGB(60, 60, 180); break;
    case REWARD_PERMA_DEF:
        title.Format(_T("守 卫 印 记")); desc.Format(_T("永久防御 +%d"), r.value);
        col = RGB(80, 130, 60); break;
    case REWARD_POTION:
        title.Format(_T("红 药 ×%d"), r.value); desc.Format(_T("生命药水 ×%d"), r.value);
        col = RGB(200, 60, 60); break;
    case REWARD_MANAPOTION:
        title.Format(_T("蓝 药 ×%d"), r.value); desc.Format(_T("法力药水 ×%d"), r.value);
        col = RGB(60, 100, 220); break;
    case REWARD_REVIVE:
        title.Format(_T("复 活 卷 轴")); desc.Format(_T("死亡时自动复活"));
        col = RGB(180, 120, 200); break;
    default:
        title = _T("???"); desc = _T(""); col = RGB(80,80,80); break;
    }
}

// =============================================
// 关卡内对话框渲染
// =============================================
void CYoulinView::DrawDialog(CDC* pDC)
{
    if (!m_dialogLines || m_dialogIndex >= m_dialogCount) return;
    const DialogLine& line = m_dialogLines[m_dialogIndex];

    // 高亮说话者: 在其头顶画指示箭头
    Vec2 focus = GetDialogFocusPos();
    int fx = (int)(focus.x - m_camX);
    int fy = (int)focus.y;
    if (line.speaker != SPEAKER_NARRATION && fx > 0 && fx < WIN_W) {
        // 头顶三角指示
        POINT tri[3] = { {fx, fy-50}, {fx-12, fy-72}, {fx+12, fy-72} };
        COLORREF c = (line.speaker == SPEAKER_BOSS) ? RGB(255,80,80)
                   : (line.speaker == SPEAKER_ENEMY) ? RGB(200,120,255)
                   : RGB(120,220,255);
        CBrush b(c);
        CBrush* po = pDC->SelectObject(&b);
        CPen* pe = (CPen*)pDC->SelectStockObject(NULL_PEN);
        pDC->Polygon(tri, 3);
        pDC->SelectObject(po);
        pDC->SelectObject(pe);
    }

    // 底部对话框
    int boxH = 180;
    int boxY = WIN_H - boxH - 20;
    CRect box(60, boxY, WIN_W - 60, WIN_H - 20);
    // 半透明底
    for (int y = box.top; y < box.bottom; y += 1)
        pDC->FillSolidRect(box.left, y, box.Width(), 1, RGB(8, 10, 20));
    pDC->Draw3dRect(box, RGB(200, 180, 90), RGB(60, 40, 10));
    // 内边框
    CRect inner = box; inner.DeflateRect(6, 6);
    pDC->Draw3dRect(inner, RGB(120, 100, 50), RGB(40, 30, 10));

    pDC->SetBkMode(TRANSPARENT);

    // 说话者名字牌
    COLORREF nameCol = (line.speaker == SPEAKER_BOSS) ? RGB(255,120,120)
                     : (line.speaker == SPEAKER_ENEMY) ? RGB(210,160,255)
                     : (line.speaker == SPEAKER_PLAYER) ? RGB(140,220,255)
                     : RGB(220,220,180);
    CFont nameF;
    nameF.CreateFont(28, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        FF_DONTCARE, _T("微软雅黑"));
    CFont* po = pDC->SelectObject(&nameF);
    // 名字背景条
    CRect nameRc(box.left + 30, box.top - 18, box.left + 220, box.top + 22);
    pDC->FillSolidRect(nameRc, RGB(30, 25, 50));
    pDC->Draw3dRect(nameRc, nameCol, RGB(40,30,10));
    pDC->SetTextColor(nameCol);
    pDC->DrawText(line.name, nameRc, DT_CENTER|DT_VCENTER|DT_SINGLELINE);

    // 对话内容(打字机)
    CFont txtF;
    txtF.CreateFont(26, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        FF_DONTCARE, _T("微软雅黑"));
    pDC->SelectObject(&txtF);
    pDC->SetTextColor(RGB(235, 235, 245));
    CString shown = line.content.Left(m_dialogCharShown);
    CRect txtRc(box.left + 40, box.top + 45, box.right - 40, box.bottom - 40);
    pDC->DrawText(shown, txtRc, DT_LEFT|DT_TOP|DT_WORDBREAK);

    // 提示(右下小字)
    CFont tipF;
    tipF.CreateFont(16, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        FF_DONTCARE, _T("微软雅黑"));
    pDC->SelectObject(&tipF);
    pDC->SetTextColor(RGB(150,150,170));
    int total = line.content.GetLength();
    pDC->DrawText(m_dialogCharShown < total ? _T("▼ 点击/空格 跳过") : _T("▼ 点击/空格 继续"),
        CRect(box.right - 220, box.bottom - 32, box.right - 20, box.bottom - 10),
        DT_RIGHT|DT_SINGLELINE);
    pDC->SelectObject(po);
}

// =============================================
// 暂停菜单
// =============================================
void CYoulinView::DrawPause(CDC* pDC)
{
    for (int y = 0; y < WIN_H; y += 2)
        pDC->FillSolidRect(0, y, WIN_W, 1, RGB(0, 0, 0));

    pDC->SetBkMode(TRANSPARENT);
    CFont titF;
    titF.CreateFont(56, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        FF_DONTCARE, _T("微软雅黑"));
    CFont* po = pDC->SelectObject(&titF);
    pDC->SetTextColor(RGB(255, 220, 100));
    pDC->DrawText(_T("暂  停"), CRect(0, 120, WIN_W, 200),
                  DT_CENTER|DT_VCENTER|DT_SINGLELINE);

    const TCHAR* items[4] = { _T("继续游戏"), _T("保存游戏"), _T("修改器"), _T("退出到主菜单") };
    CFont btnF;
    btnF.CreateFont(30, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        FF_DONTCARE, _T("微软雅黑"));
    pDC->SelectObject(&btnF);
    int bw = 320, bh = 60;
    int bx = (WIN_W - bw) / 2, by = 260;
    for (int i = 0; i < 4; ++i) {
        CRect rc(bx, by + i*80, bx + bw, by + i*80 + bh);
        m_pauseItemRect[i] = rc;
        BOOL hover = rc.PtInRect(m_mouseHover) || m_pauseSel == i;
        pDC->FillSolidRect(rc, hover ? RGB(120, 60, 40) : RGB(40, 40, 60));
        pDC->Draw3dRect(rc, RGB(220,180,90), RGB(80,40,20));
        pDC->SetTextColor(hover ? RGB(255, 230, 120) : RGB(220, 220, 220));
        pDC->DrawText(items[i], rc, DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    }
    pDC->SelectObject(po);
}

// =============================================
// 修改器(展示用)
// =============================================
void CYoulinView::DrawCheat(CDC* pDC)
{
    for (int y = 0; y < WIN_H; y += 2)
        pDC->FillSolidRect(0, y, WIN_W, 1, RGB(0, 0, 0));

    CRect panel(180, 80, WIN_W - 180, WIN_H - 80);
    pDC->FillSolidRect(panel, RGB(25, 28, 45));
    pDC->Draw3dRect(panel, RGB(220, 180, 90), RGB(60, 40, 10));

    pDC->SetBkMode(TRANSPARENT);
    CFont titF;
    titF.CreateFont(40, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        FF_DONTCARE, _T("微软雅黑"));
    CFont* po = pDC->SelectObject(&titF);
    pDC->SetTextColor(RGB(255, 220, 100));
    pDC->DrawText(_T("修 改 器 (展示用)"),
        CRect(panel.left, panel.top+15, panel.right, panel.top+65),
        DT_CENTER|DT_VCENTER|DT_SINGLELINE);

    // 当前数值显示
    CFont infoF;
    infoF.CreateFont(22, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        FF_DONTCARE, _T("微软雅黑"));
    pDC->SelectObject(&infoF);
    pDC->SetTextColor(RGB(220, 220, 240));
    CString info;
    info.Format(_T("HP %d/%d   MP %d/%d   ATK %d   金币 %d   无敌:%s"),
        m_player->GetHP(), m_player->GetMaxHP(),
        m_player->GetMP(), m_player->GetMaxMP(),
        m_player->GetATK(), m_player->GetCoin(),
        m_player->IsGodMode() ? _T("开") : _T("关"));
    pDC->DrawText(info, CRect(panel.left+30, panel.top+75, panel.right-30, panel.top+105),
                  DT_CENTER|DT_SINGLELINE);

    // 8 个修改按钮
    const TCHAR* btns[8] = {
        _T("+100 HP"), _T("+100 MP"), _T("+50 攻击"), _T("+1000 金币"),
        _T("满状态"), _T("切换无敌"), _T("+1 全技能等级"), _T("返回")
    };
    CFont btnF;
    btnF.CreateFont(24, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        FF_DONTCARE, _T("微软雅黑"));
    pDC->SelectObject(&btnF);
    int cols = 2, bw = (panel.Width() - 90) / cols, bh = 60;
    int sx = panel.left + 30, sy = panel.top + 130;
    for (int i = 0; i < 8; ++i) {
        int cx = i % cols, cy = i / cols;
        CRect rc(sx + cx*(bw+30), sy + cy*(bh+18),
                 sx + cx*(bw+30) + bw, sy + cy*(bh+18) + bh);
        m_cheatRect[i] = rc;
        BOOL hover = rc.PtInRect(m_mouseHover);
        COLORREF base = (i == 7) ? RGB(80,30,30) : RGB(40, 45, 65);
        pDC->FillSolidRect(rc, hover ? RGB(90, 70, 50) : base);
        pDC->Draw3dRect(rc, RGB(200,160,80), RGB(40,30,10));
        pDC->SetTextColor(hover ? RGB(255, 230, 120) : RGB(230, 230, 230));
        pDC->DrawText(btns[i], rc, DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    }
    pDC->SelectObject(po);
}

void CYoulinView::DrawReward(CDC* pDC)
{
    // 暗色遮罩
    for (int y = 0; y < WIN_H; y += 2)
        pDC->FillSolidRect(0, y, WIN_W, 1, RGB(0, 0, 0));

    pDC->SetBkMode(TRANSPARENT);
    CFont titF;
    titF.CreateFont(54, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        FF_DONTCARE, _T("微软雅黑"));
    CFont* po = pDC->SelectObject(&titF);
    pDC->SetTextColor(RGB(255, 220, 100));
    pDC->DrawText(_T("守 夜 翻 牌"),
        CRect(0, 80, WIN_W, 160), DT_CENTER|DT_VCENTER|DT_SINGLELINE);

    CFont subF;
    subF.CreateFont(22, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        FF_DONTCARE, _T("微软雅黑"));
    pDC->SelectObject(&subF);
    pDC->SetTextColor(RGB(220, 220, 220));
    if (m_rewardSel < 0)
        pDC->DrawText(_T("点击一张卡牌翻开 — 只能选择一张"),
            CRect(0, 175, WIN_W, 205), DT_CENTER|DT_SINGLELINE);
    else
        pDC->DrawText(_T("点击 \"确认\" 领取奖励, 或按 Enter"),
            CRect(0, 175, WIN_W, 205), DT_CENTER|DT_SINGLELINE);

    int cardW = 240, cardH = 320, gap = 40;
    int totalW = cardW*3 + gap*2;
    int sx = (WIN_W - totalW)/2;
    int sy = 240;

    CFont btnF;
    btnF.CreateFont(26, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        FF_DONTCARE, _T("微软雅黑"));

    for (int i = 0; i < 3; ++i) {
        CRect rc(sx + i*(cardW+gap), sy, sx + i*(cardW+gap) + cardW, sy + cardH);
        m_rewardRect[i] = rc;

        // 翻牌动画: 用宽度收缩→放大模拟翻转
        if (m_rewardRevealAnim[i] > 0) {
            float t = m_rewardRevealAnim[i] / 15.0f;
            // 翻转过程中:宽度先变 0 再恢复
            float scale = (t > 0.5f) ? (t - 0.5f) * 2 : (0.5f - t) * 2;
            int half = (int)(cardW * 0.5f * (0.1f + scale * 0.9f));
            int cx = rc.left + cardW/2;
            CRect anim(cx - half, rc.top, cx + half, rc.bottom);
            COLORREF c = (t > 0.5f) ? RGB(40, 40, 70) : RGB(180, 140, 60);
            pDC->FillSolidRect(anim, c);
            pDC->Draw3dRect(anim, RGB(230, 200, 100), RGB(60,30,10));
            continue;
        }

        if (!m_rewardRevealed[i]) {
            // 牌背:深色 + 金色花纹 + ?
            BOOL hover = rc.PtInRect(m_mouseHover) && m_rewardSel < 0;
            pDC->FillSolidRect(rc, hover ? RGB(60, 30, 80) : RGB(35, 20, 50));
            pDC->Draw3dRect(rc, RGB(230, 200, 100), RGB(60,30,10));
            // 内边框
            CRect inner = rc;
            inner.DeflateRect(12, 12);
            CPen pen(PS_SOLID, 2, RGB(180, 150, 80));
            CPen* pp = pDC->SelectObject(&pen);
            CBrush* pn = (CBrush*)pDC->SelectStockObject(NULL_BRUSH);
            pDC->Rectangle(inner);
            // 中央问号
            CFont qF;
            qF.CreateFont(120, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET,
                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                FF_DONTCARE, _T("微软雅黑"));
            CFont* po2 = pDC->SelectObject(&qF);
            pDC->SetTextColor(RGB(200, 160, 60));
            pDC->DrawText(_T("?"), rc, DT_CENTER|DT_VCENTER|DT_SINGLELINE);
            pDC->SelectObject(po2);
            // 卡片编号 1/2/3
            CFont nF;
            nF.CreateFont(20, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                FF_DONTCARE, _T("微软雅黑"));
            CFont* po3 = pDC->SelectObject(&nF);
            pDC->SetTextColor(RGB(160, 130, 90));
            CString num; num.Format(_T("[%d]"), i+1);
            pDC->DrawText(num, CRect(rc.left, rc.bottom-30, rc.right, rc.bottom-8),
                          DT_CENTER|DT_SINGLELINE);
            pDC->SelectObject(po3);
            pDC->SelectObject(pp);
            pDC->SelectObject(pn);
        } else {
            // 已翻开:显示内容
            CString title, desc;
            COLORREF col;
            GetRewardLabel(m_rewardCards[i], title, desc, col);
            BOOL selected = (m_rewardSel == i);
            pDC->FillSolidRect(rc, col);
            pDC->Draw3dRect(rc, RGB(255, 240, 160), RGB(60,30,10));
            if (selected) {
                // 选中卡片高亮金边
                CPen pen(PS_SOLID, 4, RGB(255, 240, 80));
                CPen* pp = pDC->SelectObject(&pen);
                CBrush* pn = (CBrush*)pDC->SelectStockObject(NULL_BRUSH);
                pDC->Rectangle(rc);
                pDC->SelectObject(pp);
                pDC->SelectObject(pn);
            }
            // 标题
            pDC->SelectObject(&btnF);
            pDC->SetTextColor(RGB(255, 255, 220));
            pDC->DrawText(title, CRect(rc.left, rc.top+110, rc.right, rc.top+155),
                          DT_CENTER|DT_SINGLELINE);
            // 描述
            CFont descF;
            descF.CreateFont(20, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                FF_DONTCARE, _T("微软雅黑"));
            CFont* po4 = pDC->SelectObject(&descF);
            pDC->SetTextColor(RGB(255, 255, 255));
            pDC->DrawText(desc, CRect(rc.left+10, rc.top+170, rc.right-10, rc.top+220),
                          DT_CENTER|DT_WORDBREAK);
            pDC->SelectObject(po4);
            pDC->SelectObject(&btnF);
        }
    }

    // 确认按钮(只有选中卡且翻完时显示)
    if (m_rewardSel >= 0 && m_rewardRevealAnim[m_rewardSel] == 0) {
        int bw = 200, bh = 50;
        CRect bc((WIN_W-bw)/2, sy + cardH + 30, (WIN_W+bw)/2, sy + cardH + 30 + bh);
        m_rewardConfirmRect = bc;
        BOOL hover = bc.PtInRect(m_mouseHover);
        pDC->FillSolidRect(bc, hover ? RGB(180, 100, 50) : RGB(120, 60, 30));
        pDC->Draw3dRect(bc, RGB(230, 200, 120), RGB(60,30,10));
        pDC->SelectObject(&btnF);
        pDC->SetTextColor(hover ? RGB(255, 250, 200) : RGB(240,240,220));
        pDC->DrawText(_T("确  认"), bc, DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    } else {
        m_rewardConfirmRect.SetRectEmpty();
    }
    pDC->SelectObject(po);

    CFont tipF;
    tipF.CreateFont(16, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        FF_DONTCARE, _T("微软雅黑"));
    pDC->SelectObject(&tipF);
    pDC->SetTextColor(RGB(180,180,200));
    pDC->DrawText(_T("[1/2/3] 翻牌   [Enter] 确认   或鼠标点击"),
        CRect(0, WIN_H - 50, WIN_W, WIN_H - 25), DT_CENTER|DT_SINGLELINE);
}

void CYoulinView::RewardClickAt(CPoint pt)
{
    // 1. 点确认按钮(只有选中且翻完才有效)
    if (m_rewardSel >= 0 && m_rewardRevealAnim[m_rewardSel] == 0
        && m_rewardConfirmRect.PtInRect(pt)) {
        ApplyReward();
        return;
    }
    // 2. 还没选时,点卡片翻开
    if (m_rewardSel < 0) {
        for (int i = 0; i < 3; ++i) {
            if (m_rewardRect[i].PtInRect(pt) && !m_rewardRevealed[i]) {
                m_rewardRevealed[i] = TRUE;
                m_rewardRevealAnim[i] = 15;  // 0.25s 翻牌动画
                m_rewardSel = i;
                return;
            }
        }
    }
}

// =============================================
// 商店(可点击)
// =============================================
void CYoulinView::OpenShop()  { m_shopOpen = TRUE;  m_shopTab = 0; m_shopSel = 0; m_shopScroll = 0; }
void CYoulinView::CloseShop() { m_shopOpen = FALSE; }
void CYoulinView::ShopTab(int t) { m_shopTab = t; m_shopSel = 0; m_shopScroll = 0; }
void CYoulinView::ShopMove(int dy)
{
    int maxIdx = (m_shopTab == 0) ? 5 : (m_shopTab == 2) ? 7 : 4;
    m_shopSel = (m_shopSel + dy + maxIdx) % maxIdx;
}

void CYoulinView::ShopBuy()
{
    if (m_shopTab == 0) {
        // 道具(含复活卷轴)
        int prices[5] = { PRICE_POTION, PRICE_MANA, PRICE_AMULET, PRICE_SCROLL, PRICE_REVIVE };
        if (m_player->SpendCoin(prices[m_shopSel])) {
            m_player->AddItem((ItemType)m_shopSel);
            g_audio.PlaySfx(SFX_CLICK);
        }
    } else if (m_shopTab == 1) {
        if (m_player->UpgradeSkill((SkillType)m_shopSel)) g_audio.PlaySfx(SFX_CLICK);
    } else {
        if (m_player->UpgradePassive((PassiveType)m_shopSel)) g_audio.PlaySfx(SFX_CLICK);
    }
}

void CYoulinView::ShopClickAt(CPoint pt)
{
    // 点击关闭按钮
    if (m_shopCloseRect.PtInRect(pt)) { CloseShop(); return; }
    // 选项卡
    for (int i = 0; i < 3; ++i) {
        if (m_shopTabRect[i].PtInRect(pt)) { ShopTab(i); return; }
    }
    // 商品
    int itemCount = (m_shopTab == 0) ? 5 : (m_shopTab == 2) ? 7 : 4;
    for (int i = 0; i < itemCount; ++i) {
        if (m_shopItemRect[i].PtInRect(pt)) {
            m_shopSel = i;
            ShopBuy();
            return;
        }
    }
}

void CYoulinView::DrawShop(CDC* pDC)
{
    // 半透明遮罩
    for (int y = 0; y < WIN_H; y += 2)
        pDC->FillSolidRect(0, y, WIN_W, 1, RGB(0, 0, 0));

    CRect panel(120, 80, WIN_W-120, WIN_H-80);
    pDC->FillSolidRect(panel, RGB(28, 30, 50));
    pDC->Draw3dRect(panel, RGB(220, 180, 90), RGB(60, 40, 10));

    pDC->SetBkMode(TRANSPARENT);
    CFont titF;
    titF.CreateFont(36, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        FF_DONTCARE, _T("微软雅黑"));
    CFont* po = pDC->SelectObject(&titF);
    pDC->SetTextColor(RGB(255, 220, 100));
    pDC->DrawText(_T("守 夜 商 店"),
        CRect(panel.left, panel.top+12, panel.right, panel.top+60),
        DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    CString coinTxt;
    coinTxt.Format(_T("金币: %d"), m_player->GetCoin());
    CFont coinF;
    coinF.CreateFont(22, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        FF_DONTCARE, _T("微软雅黑"));
    pDC->SelectObject(&coinF);
    pDC->SetTextColor(RGB(255, 220, 80));
    pDC->DrawText(coinTxt,
        CRect(panel.right-260, panel.top+18, panel.right-80, panel.top+44),
        DT_RIGHT|DT_VCENTER|DT_SINGLELINE);

    // 选项卡
    const TCHAR* tabs[3] = { _T("道具"), _T("技能升级"), _T("被动升级") };
    CFont tabF;
    tabF.CreateFont(22, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        FF_DONTCARE, _T("微软雅黑"));
    pDC->SelectObject(&tabF);
    int tabW = 160, tabH = 38;
    int tabSX = panel.left + 30;
    for (int i = 0; i < 3; ++i) {
        CRect rc(tabSX + i*(tabW+10), panel.top + 70, tabSX + i*(tabW+10) + tabW, panel.top + 70 + tabH);
        m_shopTabRect[i] = rc;
        BOOL act = (m_shopTab == i);
        BOOL hover = rc.PtInRect(m_mouseHover);
        pDC->FillSolidRect(rc, act ? RGB(120, 60, 40) : hover ? RGB(60, 50, 70) : RGB(40, 40, 60));
        pDC->Draw3dRect(rc, RGB(200,160,80), RGB(60,30,10));
        pDC->SetTextColor(act ? RGB(255, 230, 120) : RGB(220, 220, 220));
        pDC->DrawText(tabs[i], rc, DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    }

    // 商品列表 - 统一 itemH=78,支持滚动
    int itemCount = (m_shopTab == 0) ? 5 : (m_shopTab == 2) ? 7 : 4;
    int listY = panel.top + 130;
    int listBottom = panel.bottom - 30;
    int itemH = 78;
    int itemW = panel.Width() - 100;
    int itemX = panel.left + 50;
    int spacing = 8;
    int totalH = itemCount * (itemH + spacing);
    int visibleH = listBottom - listY;
    // 钳制滚动
    int maxScroll = max(0, totalH - visibleH);
    if (m_shopScroll > maxScroll) m_shopScroll = maxScroll;
    if (m_shopScroll < 0) m_shopScroll = 0;

    // 设置裁剪区
    CRgn clipRgn;
    clipRgn.CreateRectRgn(itemX, listY, itemX + itemW + 10, listBottom);
    pDC->SelectClipRgn(&clipRgn);

    CFont itemF;
    itemF.CreateFont(22, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        FF_DONTCARE, _T("微软雅黑"));
    pDC->SelectObject(&itemF);

    for (int i = 0; i < itemCount; ++i) {
        int rtop = listY + i*(itemH+spacing) - m_shopScroll;
        CRect rc(itemX, rtop, itemX + itemW, rtop + itemH);
        m_shopItemRect[i] = rc;
        BOOL sel = (m_shopSel == i);
        BOOL hover = rc.PtInRect(m_mouseHover);
        pDC->FillSolidRect(rc, sel ? RGB(80, 50, 40) : hover ? RGB(50, 45, 65) : RGB(35, 38, 55));
        pDC->Draw3dRect(rc, RGB(180,160,80), RGB(40,30,10));

        CString name, info, priceTxt;
        BOOL affordable = TRUE;

        if (m_shopTab == 0) {
            // 道具(5 项,含复活卷轴)
            SpriteID icons[5] = { SPR_ITEM_POTION, SPR_ITEM_MANA, SPR_ITEM_AMULET, SPR_ITEM_SCROLL, SPR_ITEM_REVIVE };
            const TCHAR* names[5] = { _T("生命药水"), _T("法力药水"), _T("静心护符"), _T("镇灵卷轴"), _T("复活卷轴") };
            const TCHAR* infos[5] = {
                _T("使用后恢复 50 HP"),
                _T("使用后恢复 40 MP"),
                _T("使用后 8s 内灵狐不追"),
                _T("使用后大幅弱化 BOSS"),
                _T("死亡时自动复活(50% HP)")
            };
            int prices[5] = { PRICE_POTION, PRICE_MANA, PRICE_AMULET, PRICE_SCROLL, PRICE_REVIVE };
            name = names[i];
            info = infos[i];
            priceTxt.Format(_T("%d 金"), prices[i]);
            affordable = m_player->GetCoin() >= prices[i];
            if (i < 4 && g_sprites.Has(icons[i]))
                g_sprites.DrawSized(pDC, icons[i], rc.left+10, rc.top+(itemH-44)/2, 44, 44);
            else
                pDC->FillSolidRect(rc.left+10, rc.top+(itemH-44)/2, 44, 44,
                    i==4 ? RGB(220,180,80) : RGB(120,120,140));
        } else if (m_shopTab == 1) {
            // 技能升级
            SpriteID icons[4] = { SPR_SKILL_ICE, SPR_SKILL_LIGHTNING, SPR_SKILL_STORM, SPR_SKILL_ARCANE };
            const TCHAR* names[4] = { _T("冰锥术"), _T("闪电链"), _T("烈焰风暴"), _T("奥术爆发") };
            int lv = m_player->GetSkillLv((SkillType)i);
            if (lv >= 3) {
                name.Format(_T("%s (已满级 Lv3)"), names[i]);
                priceTxt = _T("MAX");
                info = _T("已升满");
                affordable = FALSE;
            } else {
                name.Format(_T("%s (Lv%d → Lv%d)"), names[i], lv, lv+1);
                int p = PRICE_SKILL_LV[lv];
                priceTxt.Format(_T("%d 金"), p);
                affordable = m_player->GetCoin() >= p;
                info.Format(_T("提升伤害/治疗量"));
            }
            if (g_sprites.Has(icons[i]))
                g_sprites.DrawSized(pDC, icons[i], rc.left+10, rc.top+18, 44, 44);
        } else {
            // 被动升级 (7 项)
            const TCHAR* names[7] = {
                _T("生命强化"), _T("法力强化"), _T("攻击强化"),
                _T("回蓝强化"), _T("闪避强化"), _T("吸血强化"), _T("防御强化")
            };
            const TCHAR* descs[7] = {
                _T("最大 HP +30/70/120"),
                _T("最大 MP +20/50/90"),
                _T("普攻伤害 +5/12/22"),
                _T("每秒回蓝 +1/2/4"),
                _T("受击闪避率 +8%/16%/25%"),
                _T("普攻伤害 5%/10%/15% 转化为 HP"),
                _T("受击减伤 +3/7/12")
            };
            int lv = m_player->GetPassLv((PassiveType)i);
            if (lv >= 3) {
                name.Format(_T("%s (已满级 Lv3)"), names[i]);
                priceTxt = _T("MAX");
                info = descs[i];
                affordable = FALSE;
            } else {
                name.Format(_T("%s (Lv%d → Lv%d)"), names[i], lv, lv+1);
                int p = PRICE_PASS_LV[lv];
                priceTxt.Format(_T("%d 金"), p);
                affordable = m_player->GetCoin() >= p;
                info = descs[i];
            }
            COLORREF iconColors[7] = {
                RGB(200, 60, 60), RGB(60, 100, 200), RGB(200, 140, 60),
                RGB(60, 180, 200), RGB(180, 180, 80), RGB(60, 200, 100),
                RGB(140, 140, 200)
            };
            pDC->FillSolidRect(rc.left+10, rc.top+(itemH-44)/2, 44, 44, iconColors[i]);
        }

        pDC->SetTextColor(sel ? RGB(255, 240, 160) : RGB(240, 240, 240));
        pDC->DrawText(name, CRect(rc.left+70, rc.top+8, rc.right-150, rc.top+36),
                      DT_LEFT|DT_VCENTER|DT_SINGLELINE);
        CFont smallF;
        smallF.CreateFont(16, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            FF_DONTCARE, _T("微软雅黑"));
        CFont* po3 = pDC->SelectObject(&smallF);
        pDC->SetTextColor(RGB(180,180,200));
        pDC->DrawText(info, CRect(rc.left+70, rc.top+38, rc.right-150, rc.bottom-8),
                      DT_LEFT|DT_TOP);
        pDC->SelectObject(po3);

        pDC->SelectObject(&itemF);
        pDC->SetTextColor(affordable ? RGB(255, 220, 80) : RGB(120, 120, 120));
        pDC->DrawText(priceTxt, CRect(rc.right-140, rc.top, rc.right-20, rc.bottom),
                      DT_RIGHT|DT_VCENTER|DT_SINGLELINE);
    }

    // 取消裁剪区
    pDC->SelectClipRgn(NULL);

    // 滚动条提示(右侧)
    if (maxScroll > 0) {
        int barX = panel.right - 50;
        int barH = visibleH;
        int barTop = listY;
        // 滑轨
        pDC->FillSolidRect(barX, barTop, 8, barH, RGB(40, 40, 60));
        // 滑块
        int thumbH = max(30, visibleH * visibleH / totalH);
        int thumbY = barTop + (visibleH - thumbH) * m_shopScroll / maxScroll;
        pDC->FillSolidRect(barX, thumbY, 8, thumbH, RGB(200, 160, 80));
    }

    // 关闭按钮: 右上角小 ×
    CRect closeRc(panel.right - 46, panel.top + 12, panel.right - 12, panel.top + 46);
    m_shopCloseRect = closeRc;
    BOOL closeHover = closeRc.PtInRect(m_mouseHover);
    pDC->FillSolidRect(closeRc, closeHover ? RGB(180,50,50) : RGB(60,30,40));
    pDC->Draw3dRect(closeRc, RGB(220,140,140), RGB(60,20,20));
    {
        CFont xF;
        xF.CreateFont(26, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            FF_DONTCARE, _T("微软雅黑"));
        CFont* pXF = pDC->SelectObject(&xF);
        pDC->SetTextColor(closeHover ? RGB(255,230,230) : RGB(220,200,200));
        pDC->DrawText(_T("×"), closeRc, DT_CENTER|DT_VCENTER|DT_SINGLELINE);
        pDC->SelectObject(pXF);
    }

    pDC->SelectObject(po);
}

// =============================================
// 胜利

void CYoulinView::DrawVictory(CDC* pDC)
{
    pDC->FillSolidRect(0, 0, WIN_W, WIN_H, RGB(20, 25, 45));
    pDC->SetBkMode(TRANSPARENT);
    CFont tf;
    tf.CreateFont(60,0,0,0,FW_BOLD,0,0,0, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        FF_DONTCARE, _T("微软雅黑"));
    CFont* pf = pDC->SelectObject(&tf);
    pDC->SetTextColor(RGB(255, 220, 80));
    pDC->DrawText(_T("守  夜  完  成"),
        CRect(0, 240, WIN_W, 320), DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    CFont sf;
    sf.CreateFont(22,0,0,0,FW_NORMAL,0,0,0, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        FF_DONTCARE, _T("微软雅黑"));
    pDC->SelectObject(&sf);
    pDC->SetTextColor(RGB(220, 220, 220));
    pDC->DrawText(_T("月光重新洒落幽林。\n按 Enter 返回主菜单"),
        CRect(0, 360, WIN_W, 440), DT_CENTER|DT_TOP);
    pDC->SelectObject(pf);
}

// =============================================
// 游戏结束
// =============================================
void CYoulinView::DrawGameOver(CDC* pDC)
{
    pDC->FillSolidRect(0, 0, WIN_W, WIN_H, RGB(30, 10, 20));
    pDC->SetBkMode(TRANSPARENT);
    CFont tf;
    tf.CreateFont(60,0,0,0,FW_BOLD,0,0,0, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        FF_DONTCARE, _T("微软雅黑"));
    CFont* pf = pDC->SelectObject(&tf);
    pDC->SetTextColor(RGB(220,80,80));
    pDC->DrawText(_T("守  夜  失  败"),
        CRect(0, 240, WIN_W, 320), DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    CFont sf;
    sf.CreateFont(22,0,0,0,FW_NORMAL,0,0,0, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        FF_DONTCARE, _T("微软雅黑"));
    pDC->SelectObject(&sf);
    pDC->SetTextColor(RGB(200,200,200));
    pDC->DrawText(_T("夜雾再次笼罩了幽林……\n按 Enter 返回主菜单"),
        CRect(0, 380, WIN_W, 460), DT_CENTER|DT_TOP);
    pDC->SelectObject(pf);
}

#ifdef _DEBUG
CYoulinDoc* CYoulinView::GetDocument() const
{
    ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CYoulinDoc)));
    return (CYoulinDoc*)m_pDocument;
}
#endif

#ifdef _DEBUG
void CYoulinView::AssertValid() const { CView::AssertValid(); }
void CYoulinView::Dump(CDumpContext& dc) const { CView::Dump(dc); }
#endif
