// SaveManager.cpp
#include "pch.h"
#include "SaveManager.h"
#include "Player.h"

const DWORD SAVE_MAGIC = 0x594F554C;   // 'YOUL'
const DWORD SAVE_VER   = 4;

CString CSaveManager::GetPath(int slot)
{
    CString s;
    s.Format(_T("save%d.dat"), slot + 1);
    return s;
}

SaveSlotInfo CSaveManager::Query(int slot)
{
    SaveSlotInfo info = { FALSE, 0, 1, 0, 0, _T("") };
    CString path = GetPath(slot);

    CFile f;
    if (!f.Open(path, CFile::modeRead)) return info;

    DWORD magic = 0, ver = 0;
    f.Read(&magic, sizeof(magic));
    f.Read(&ver,   sizeof(ver));
    if (magic != SAVE_MAGIC || ver != SAVE_VER) { f.Close(); return info; }

    int chapter = 0, diff = 1;
    f.Read(&chapter, sizeof(chapter));
    f.Read(&diff,    sizeof(diff));
    info.chapter = chapter;
    info.difficulty = diff;

    // 跳过 Player 头部 32 字节读 hp/coin
    Vec2 pos; int hp, mp, coin;
    f.Read(&pos,  sizeof(pos));
    f.Read(&hp,   sizeof(hp));
    f.Read(&mp,   sizeof(mp));
    f.Read(&coin, sizeof(coin));
    info.hp = hp;
    info.coin = coin;
    info.exists = TRUE;

    // 文件时间
    CFileStatus st;
    if (f.GetStatus(st)) {
        info.timeStr = st.m_mtime.Format(_T("%Y-%m-%d %H:%M"));
    }
    f.Close();
    return info;
}

BOOL CSaveManager::Save(int slot, int chapter, int difficulty, const CPlayer& p)
{
    CString path = GetPath(slot);
    CFile f;
    if (!f.Open(path, CFile::modeCreate | CFile::modeWrite)) return FALSE;

    DWORD magic = SAVE_MAGIC, ver = SAVE_VER;
    f.Write(&magic, sizeof(magic));
    f.Write(&ver,   sizeof(ver));
    f.Write(&chapter,    sizeof(chapter));
    f.Write(&difficulty, sizeof(difficulty));
    p.SaveTo(f);
    f.Close();
    return TRUE;
}

BOOL CSaveManager::Load(int slot, int& chapter, int& difficulty, CPlayer& p)
{
    CString path = GetPath(slot);
    CFile f;
    if (!f.Open(path, CFile::modeRead)) return FALSE;

    DWORD magic = 0, ver = 0;
    f.Read(&magic, sizeof(magic));
    f.Read(&ver,   sizeof(ver));
    if (magic != SAVE_MAGIC || ver != SAVE_VER) { f.Close(); return FALSE; }

    f.Read(&chapter,    sizeof(chapter));
    f.Read(&difficulty, sizeof(difficulty));
    p.LoadFrom(f);
    f.Close();
    return TRUE;
}

BOOL CSaveManager::Delete(int slot)
{
    try {
        CFile::Remove(GetPath(slot));
        return TRUE;
    } catch (...) { return FALSE; }
}
