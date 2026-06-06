// MainFrm.cpp
#include "pch.h"
#include "Youlin.h"
#include "MainFrm.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

IMPLEMENT_DYNCREATE(CMainFrame, CFrameWndEx)

BEGIN_MESSAGE_MAP(CMainFrame, CFrameWndEx)
    ON_WM_CREATE()
END_MESSAGE_MAP()

CMainFrame::CMainFrame() {}
CMainFrame::~CMainFrame() {}

int CMainFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
    if (CFrameWndEx::OnCreate(lpCreateStruct) == -1)
        return -1;

    // 隐藏菜单栏(游戏不需要)
    SetMenu(NULL);
    return 0;
}

BOOL CMainFrame::PreCreateWindow(CREATESTRUCT& cs)
{
    if (!CFrameWndEx::PreCreateWindow(cs))
        return FALSE;

    // 允许调整大小和最大化(全屏)
    cs.style |= WS_MAXIMIZEBOX | WS_THICKFRAME;
    // 初始窗口大小 16:9 (客户区约 1280x720 + 边框)
    cs.cx = 1296;
    cs.cy = 759;
    return TRUE;
}
