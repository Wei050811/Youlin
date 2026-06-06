// YoulinView.h
// 主视图：状态机、输入、渲染、菜单/商店
#pragma once
#include "GameTypes.h"
#include "Player.h"
#include "Enemy.h"
#include "Projectile.h"
#include "StoryManager.h"

class CYoulinDoc;

class CYoulinView : public CView
{
protected:
    CYoulinView();
    DECLARE_DYNCREATE(CYoulinView)
public:
    CYoulinDoc* GetDocument() const;
    virtual void OnDraw(CDC* pDC);
    virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
protected:
    virtual ~CYoulinView();
#ifdef _DEBUG
    virtual void AssertValid() const;
    virtual void Dump(CDumpContext& dc) const;
#endif

    DECLARE_MESSAGE_MAP()
    afx_msg int  OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg void OnTimer(UINT_PTR nIDEvent);
    afx_msg void OnKeyDown(UINT nChar, UINT, UINT);
    afx_msg void OnKeyUp(UINT nChar, UINT, UINT);
    afx_msg void OnLButtonDown(UINT nFlags, CPoint pt);
    afx_msg void OnMouseMove(UINT nFlags, CPoint pt);
    afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);

private:
    // 状态
    GameState m_state;
    Chapter   m_chapter;
    Difficulty m_diff;
    int       m_frame;

    // 主菜单
    int m_menuSel;     // 0..3
    int m_loadSel;     // 0..2
    int m_diffSel;     // 0..2

    // 摄像机(横向滚动)
    float m_camX;

    // 波次
    int m_currentWave;          // 0-based
    int m_waveTransitionLeft;   // 波次间倒计时(帧),0 表示不在过渡
    BOOL m_allWavesCleared;     // 所有波次清完(等待玩家走到最右)
    BOOL m_midStoryTriggered;   // 本章中段剧情已触发
    BOOL m_bossStoryTriggered;  // 本章 BOSS 战前剧情已触发

    // 关卡内对话(STATE_STORY_MID)
    const DialogLine* m_dialogLines;
    int  m_dialogCount;
    int  m_dialogIndex;
    int  m_dialogCharShown;
    int  m_dialogCharTick;
    int  m_dialogWarmup;        // 对话开始前的镜头预热帧
    // 待触发对话(先刷怪,等怪出生完成再真正开始对话)
    const DialogLine* m_pendingDialog;
    int  m_pendingDialogCount;
    BOOL m_pendingAfterBoss;
    int  m_pendingWaitFrames;   // 最多等待帧数(兜底)
    BOOL m_dialogAfterIsBoss;   // 对话结束后是否生成 BOSS 波
    void StartDialog(const DialogLine* lines, int count, BOOL afterBoss);
    void QueueDialogAfterSpawn(const DialogLine* lines, int count);  // 先等怪出生再对话
    BOOL AnyEnemySpawning();  // 是否有怪还在出生预警
    void UpdateDialog();
    void DrawDialog(CDC*);
    void AdvanceDialog();
    Vec2 GetDialogFocusPos();   // 当前说话者的世界坐标(镜头跟随)

    // 暂停菜单
    int  m_pauseSel;
    CRect m_pauseItemRect[4];   // 继续/保存/修改器/退出
    // 修改器
    BOOL m_cheatOpen;
    CRect m_cheatRect[8];
    void CheatAction(int idx);

    // 通关奖励 - 翻牌系统
    int m_rewardSel;            // 已选中卡片(-1 表示没选)
    BOOL m_rewardRevealed[3];   // 哪几张已翻开
    RewardItem m_rewardCards[3];// 3 张牌的内容
    CRect m_rewardRect[3];      // 卡片点击区
    CRect m_rewardConfirmRect;  // "确认" 按钮区
    int m_rewardRevealAnim[3];  // 翻牌动画帧(0=完成翻,>0=正在翻)

    // 全屏
    int m_clientW, m_clientH;   // 实际客户区
    int m_viewX, m_viewY;       // 缩放后内容左上角
    int m_viewW, m_viewH;       // 缩放后内容大小

    // 游戏对象
    CPlayer*       m_player;
    std::vector<CEnemy*> m_enemies;
    CProjectileMgr m_proj;
    CStoryManager  m_story;

    // 输入
    BOOL m_keyUp, m_keyDown, m_keyLeft, m_keyRight;

    // 双缓冲(内部恒为 1024x720)
    CBitmap m_backBmp;
    CDC     m_backDC;
    BOOL    m_dcReady;
    CPoint  m_mouseHover;
    void EnsureBackBuffer();

    // 鼠标坐标 屏幕→游戏内部坐标(撤销 StretchBlt 缩放)
    CPoint ScreenToGame(CPoint sp) const;

    // 流程
    void StartGame();
    void GoMenu();
    void EnterChapterBegin();
    void EnterChapterFight();
    void EnterChapterEnd();
    void NextChapter();
    void EndChapterFight();
    void EnterReward();
    void ApplyReward();         // 根据 m_rewardSel 应用奖励
    void RollNewRewards();      // 抽 3 张随机奖励填进 m_rewardCards

    // 战斗
    void ClearEnemies();
    void SpawnWave(int waveIndex);  // 刷一波(根据章节+波次)
    BOOL AllEnemiesDead();
    void UpdateCamera();

    // 商店(可点击)
    BOOL  m_shopOpen;
    int   m_shopTab;       // 0=道具 1=技能 2=被动
    int   m_shopSel;
    CRect m_shopTabRect[3];
    CRect m_shopItemRect[7];     // 当前选项卡可见项的矩形(最多 6 项,被动用)
    int   m_shopScroll;          // 商店滚动偏移(像素)
    CRect m_shopCloseRect;
    void OpenShop();
    void CloseShop();
    void ShopBuy();
    void ShopMove(int dy);
    void ShopTab(int t);
    void ShopClickAt(CPoint pt);

    // 奖励界面命中
    void  RewardClickAt(CPoint pt);

    // 主菜单点击
    CRect m_menuItemRect[4];

    // 难度/读档点击
    CRect m_diffItemRect[3];
    CRect m_loadItemRect[3];

    // 绘制分支
    void DrawMenu(CDC*);
    void DrawDifficulty(CDC*);
    void DrawLoadList(CDC*);
    void DrawPlaying(CDC*);
    void DrawBackground(CDC*);
    void DrawHUD(CDC*);
    void DrawShop(CDC*);
    void DrawReward(CDC*);
    void DrawPause(CDC*);
    void DrawCheat(CDC*);
    void DrawVictory(CDC*);
    void DrawGameOver(CDC*);
    void DrawWaveTransition(CDC*);

    // 工具
    float DiffMul() const;
};

#ifndef _DEBUG
inline CYoulinDoc* CYoulinView::GetDocument() const
   { return reinterpret_cast<CYoulinDoc*>(m_pDocument); }
#endif
