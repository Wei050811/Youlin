// Youlin.h
#pragma once

#ifndef __AFXWIN_H__
    #error "include 'stdafx.h' before including this file for PCH"
#endif

#include "resource.h"

class CYoulinApp : public CWinAppEx
{
public:
    CYoulinApp();
    virtual BOOL InitInstance();
    virtual int  ExitInstance();
    DECLARE_MESSAGE_MAP()
};

extern CYoulinApp theApp;
