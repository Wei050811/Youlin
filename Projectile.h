// Projectile.h
// 投射物管理：玩家火球/冰锥/闪电；敌人毒液
#pragma once
#include "GameTypes.h"

class CPlayer;
class CEnemy;

struct Projectile {
    ProjectileType type;
    Vec2 pos;
    Vec2 vel;        // 速度向量
    int  damage;
    int  life;       // 剩余存活帧
    BOOL fromPlayer; // TRUE=玩家发的，可伤敌；FALSE=敌人发的，可伤玩家
    BOOL pierce;     // 是否穿透(闪电链)
    int  hitMask;    // 已击中的敌人位标记 (避免穿透时重复打)
    BOOL alive;
    int  freezeFrames; // 冰锥的冻结附加效果
};

class CProjectileMgr
{
public:
    CProjectileMgr();
    void Clear();

    // 添加：玩家普攻火球
    void SpawnFireball(Vec2 from, int face, int dmg);
    // 添加：技能 冰锥
    void SpawnIce(Vec2 from, int face, int dmg);
    // 添加：技能 闪电链
    void SpawnLightning(Vec2 from, int face, int dmg);
    // 添加：敌人毒液
    void SpawnPoison(Vec2 from, Vec2 dir, int dmg);
    void SpawnSeed(Vec2 from, Vec2 dir, int dmg);   // 树精种子(用 seed.png)
    void SpawnLaser(Vec2 from, Vec2 dir, int dmg);  // 幽灵蓝色激光

    // 更新：所有投射物移动 + 碰撞
    void Update(CPlayer* player, std::vector<CEnemy*>& enemies);
    void Draw(CDC* pDC);

private:
    std::vector<Projectile> m_list;

    static Vec2 FaceToDir(int face);
};
