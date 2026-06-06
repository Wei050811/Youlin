// SaveManager.h
// 三槽存档，文件：save1.dat save2.dat save3.dat
#pragma once
#include "GameTypes.h"

class CPlayer;

struct SaveSlotInfo {
    BOOL exists;
    int  chapter;
    int  difficulty;
    int  hp;
    int  coin;
    CString timeStr;
};

class CSaveManager
{
public:
    static const int SLOT_COUNT = 3;

    // 获取槽位文件信息（用于读档界面显示）
    static SaveSlotInfo Query(int slot);

    // 存档：写入 chapter / difficulty + Player 数据
    static BOOL Save(int slot, int chapter, int difficulty, const CPlayer& p);

    // 读档：读出 chapter / difficulty / Player
    static BOOL Load(int slot, int& chapter, int& difficulty, CPlayer& p);

    // 删除存档
    static BOOL Delete(int slot);

private:
    static CString GetPath(int slot);
};
