// Enemy.h
// 8 种敌人
#pragma once
#include "GameTypes.h"

class CPlayer;
class CProjectileMgr;

class CEnemy
{
public:
    CEnemy(EnemyType t, float x, float y, float diffMul);
    virtual ~CEnemy() {}

    virtual void Update(CPlayer* p, CProjectileMgr* proj) = 0;
    virtual void Draw(CDC* pDC, int frame) = 0;

    EnemyType GetType()  const { return m_type; }
    Vec2      GetPos()   const { return m_pos; }
    int       GetHP()    const { return m_hp; }
    int       GetMaxHP() const { return m_maxHP; }
    BOOL      IsAlive()  const { return m_hp > 0; }
    int       GetRadius()const { return m_radius; }

    int       GetCoinDrop() const;
    void      TakeDamage(int dmg);
    virtual void ApplyFreeze(int frames) { m_freezeLeft = max(m_freezeLeft, frames); }
    BOOL      IsFrozen() const { return m_freezeLeft > 0; }
    void      WeakenDefense();   // 卷轴

    // 出生预警: > 0 表示还在召唤动画中,不可被攻击/不能造成伤害
    BOOL      IsSpawning() const { return m_spawnFrames > 0; }
    int       GetSpawnFrames() const { return m_spawnFrames; }

    // 用于穿透判定的唯一 id
    int  GetId() const { return m_id; }
    void SetId(int id)  { m_id = id; }

    // 应用难度配置(速度/攻击频率系数) - public,供 View 调用
    void ApplyDifficulty(float spd, float atkFreq) {
        m_diffSpeed = spd;
        if (atkFreq > 0.01f) m_attackCD = (int)(m_attackCD / atkFreq);
    }

protected:
    EnemyType m_type;
    Vec2  m_pos;
    int   m_hp, m_maxHP;
    int   m_radius;
    int   m_attackCD, m_curCD;
    int   m_hurtFlash;
    int   m_freezeLeft;
    BOOL  m_weakened;
    int   m_id;
    int   m_spawnFrames;   // 出生预警倒计时
    int   m_hitstun;       // 受击硬直(>0 时不能移动)
    float m_diffSpeed = 1.0f;  // 难度速度系数
    BOOL  m_faceLeft = FALSE;  // 朝向(GIF 翻转用): TRUE=朝左
    float m_lastX = 0;         // 上一帧 X(判断移动方向)

public:
    // 根据当前位置与玩家位置更新朝向(在各子类 Update 末尾可调用)
    void UpdateFacing(const Vec2& playerPos) {
        // 朝向玩家
        m_faceLeft = (playerPos.x < m_pos.x);
    }
    BOOL IsFaceLeft() const { return m_faceLeft; }
    // 放大倍数(让怪物更大,失落城堡手感)
    float DrawScale() const { return 2.0f; }

    // 子类辅助: 在 Draw 中调用,在指定位置画召唤圆+粒子
    void DrawSpawnEffect(CDC* pDC, int x, int y, int radius);
    // 限制怪物在关卡范围内(防止跑出界)
    void ClampInLevel();

    // 工具：朝向玩家的归一化方向
    Vec2 DirTo(const Vec2& target) const;
    BOOL CanReach(const Vec2& target, float range) const;
};

// 1. 游荡幽影 (1章 巡逻)
class CGhost : public CEnemy {
public:
    CGhost(float x, float y, float dm);
    void Update(CPlayer*, CProjectileMgr*) override;
    void Draw(CDC*, int) override;
private:
    Vec2 m_pA, m_pB;
    float m_t; int m_dir;
};

// 2. 路障树精 (2章 镇守)
class CTreeGuard : public CEnemy {
public:
    CTreeGuard(float x, float y, float dm);
    void Update(CPlayer*, CProjectileMgr*) override;
    void Draw(CDC*, int) override;
private:
    int m_sway;
    int m_seedTimer = 90;
};

// 3. 暗夜蝙蝠 (2章 飞行远程)
class CBat : public CEnemy {
public:
    CBat(float x, float y, float dm);
    void Update(CPlayer*, CProjectileMgr*) override;
    void Draw(CDC*, int) override;
private:
    float m_baseX, m_baseY;   // 巡飞中心
    float m_phase;
    int   m_shootTimer;
};

// 4. 追月灵狐 (3章 追踪)
class CFox : public CEnemy {
public:
    CFox(float x, float y, float dm);
    void Update(CPlayer*, CProjectileMgr*) override;
    void Draw(CDC*, int) override;
private:
    float m_speed;
    int   m_pounceTimer = 70;
};

// 5. 狂暴狼影 (3章 冲刺)
class CWolf : public CEnemy {
public:
    CWolf(float x, float y, float dm);
    void Update(CPlayer*, CProjectileMgr*) override;
    void Draw(CDC*, int) override;
private:
    int   m_dashCD;         // 距离下次冲刺
    int   m_dashing;        // 冲刺持续帧
    Vec2  m_dashDir;
};

// 6. 影刺客 (3章 突袭)
class CAssassin : public CEnemy {
public:
    CAssassin(float x, float y, float dm);
    void Update(CPlayer*, CProjectileMgr*) override;
    void Draw(CDC*, int) override;
private:
    int  m_stealth;         // 隐身帧
    int  m_attackWindup;    // 出手前摇
};

// 7. 雾魔 (4章 传送杂兵)
class CWraith : public CEnemy {
public:
    CWraith(float x, float y, float dm);
    void Update(CPlayer*, CProjectileMgr*) override;
    void Draw(CDC*, int) override;
private:
    int  m_teleCD;
    int  m_teleAnim;        // 传送动画帧
    int  m_shootTimer = 100; // 三线射手齐射计时
};

// 8. 雾核守卫 BOSS (4章) - 重塑版
class CFogCore : public CEnemy {
public:
    CFogCore(float x, float y, float dm);
    void Update(CPlayer*, CProjectileMgr*) override;
    void Draw(CDC*, int) override;
    void OnHit(int dmg);
    void ApplyFreeze(int frames) override;  // 冰冻抗性
    BOOL IsInvincible() const { return m_invincibleLeft > 0; }
    BOOL IsPhase2() const     { return m_phase2; }
    BOOL IsCloaked() const    { return m_cloakLeft > 0; }
    int  GetPhase() const     { return m_phase; }       // 当前阶段 1/2/3
    int  GetLifeStage() const { return m_lifeStage; }   // 第几条命 1/2
    int  GetPhaseAnim() const { return m_phaseAnim; }
    // 阶段切换触发: 0=无, 2=进阶段2, 3=进阶段3/第二条命(供 View 触发剧情)
    int  ConsumePhaseTrigger() { int t = m_phaseTrigger; m_phaseTrigger = 0; return t; }
private:
    // === 一阶段技能冷却 ===
    int  m_meleeCD;          // 镰刀近战平 A
    int  m_sweepCD;          // 镰刀大横扫
    int  m_blinkCD;          // 短距传送斩
    int  m_aoeCD;            // AOE 雾爆
    // === 技能动画状态 ===
    int  m_meleeAnim;        // 平A 挥砍动画帧
    int  m_sweepWarn;        // 大横扫预警帧
    int  m_sweepActive;      // 大横扫攻击窗口
    int  m_blinkAnim;        // 传送动画(消失/出现)
    int  m_blinkPhase;       // 0=未激活 1=消失中 2=出现挥砍中
    int  m_aoeWarn;          // AOE 预警
    int  m_aoeActive;        // AOE 爆炸窗口
    // === 二阶段独有 ===
    int  m_crossCD;          // 十字镰刃 CD
    int  m_summonCD;         // 暗影分身 CD
    int  m_abyssCD;          // 黑暗深渊 CD
    int  m_rainCD;           // 暗影箭雨 CD
    Vec2 m_abyssCenter;      // 黑暗深渊中心
    int  m_abyssFrames;      // 黑暗深渊剩余帧
    // === 阶段控制 ===
    BOOL m_phase2;
    BOOL m_phase2TriggerFlag = FALSE;  // (兼容旧)
    int  m_phase = 1;                  // 当前阶段 1/2/3
    int  m_lifeStage = 1;              // 第几条命 1/2
    int  m_phaseTrigger = 0;           // 阶段切换待触发剧情标志
    int  m_phaseAnim;
    BOOL m_berserk;          // 30% HP 触发狂暴态
    // === 无敌机制 ===
    int  m_hitsToInvinc;
    int  m_invincibleLeft;
    // === 隐身突袭(二阶段) ===
    int  m_cloakCD;
    int  m_cloakLeft;
    Vec2 m_strikeTarget;
    // === 镰刀朝向(渲染用) ===
    float m_scytheAngle;     // 当前镰刀朝向(用于动画)
    float m_floatPhase;
    float m_diffMul;
    // 攻击系数(狂暴态时 1.5)
    float SpeedMul() const { return m_berserk ? 1.5f : 1.0f; }
};
