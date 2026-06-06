// Youlin.cpp
#include "pch.h"
#include "Youlin.h"
#include "MainFrm.h"
#include "YoulinDoc.h"
#include "YoulinView.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

BEGIN_MESSAGE_MAP(CYoulinApp, CWinAppEx)
END_MESSAGE_MAP()

CYoulinApp::CYoulinApp() {}
CYoulinApp theApp;

BOOL CYoulinApp::InitInstance()
{
    CWinAppEx::InitInstance();

    // CWinAppEx 必需的管理器初始化(否则 CFrameWndEx 内部会因 NULL 指针崩溃)
    EnableTaskbarInteraction(FALSE);
    InitContextMenuManager();
    InitKeyboardManager();
    InitTooltipManager();
    CMFCToolTipInfo ttParams;
    ttParams.m_bVislManagerTheme = TRUE;
    GetTooltipManager()->SetTooltipParams(
        AFX_TOOLTIP_TYPE_ALL, RUNTIME_CLASS(CMFCToolTipCtrl), &ttParams);

    SetRegistryKey(_T("Youlin"));

    CSingleDocTemplate* pDocTemplate;
    pDocTemplate = new CSingleDocTemplate(
        IDR_MAINFRAME,
        RUNTIME_CLASS(CYoulinDoc),
        RUNTIME_CLASS(CMainFrame),
        RUNTIME_CLASS(CYoulinView));
    if (!pDocTemplate) return FALSE;
    AddDocTemplate(pDocTemplate);

    CCommandLineInfo info;
    ParseCommandLine(info);
    // 不要设 FileNothing,否则 ProcessShellCommand 不会创建主窗口
    if (!ProcessShellCommand(info)) return FALSE;

    if (!m_pMainWnd) {
        AfxMessageBox(_T("主窗口创建失败"));
        return FALSE;
    }

    m_pMainWnd->ShowWindow(SW_SHOW);
    m_pMainWnd->UpdateWindow();
    return TRUE;
}

int CYoulinApp::ExitInstance()
{
    return CWinAppEx::ExitInstance();
}
