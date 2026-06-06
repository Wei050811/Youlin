// MainFrm.h
#pragma once

class CMainFrame : public CFrameWndEx
{
protected:
    CMainFrame();
    DECLARE_DYNCREATE(CMainFrame)
public:
    virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
public:
    virtual ~CMainFrame();
    DECLARE_MESSAGE_MAP()
    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
};
