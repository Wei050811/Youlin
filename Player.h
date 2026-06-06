// Player.h
// 法师玩家：HP/MP/普攻火球/4 技能/被动等级/道具
#pragma once
#include "GameTypes.h"

class CProjectileMgr;
class CEnemy;

class CPlayer
{
public:
    CPlayer();

    void Reset();    // 战斗开始时重置 HP/MP/位置/冷却
    void FullReset();// 新游戏完全重置
    void ResetForNextChapter();  // 进入下一关:保留 HP/MP,只重置位置/CD/冲刺

    void Update(BOOL up, BOOL down, BOOL left, BOOL right);
    void Draw(CDC* pDC, int frame);

    // 受伤/恢复
    void TakeDamage(int dmg);
    void Heal(int hp);
    void RegenMP(int mp);
    BOOL IsAlive() const { return m_hp > 0; }

    // 普攻：发射火球
    BOOL TryFireball(CProjectileMgr* mgr);
    // 技能 (返回是否成功)
    BOOL TrySkill(SkillType s, CProjectileMgr* mgr, std::vector<CEnemy*>& enemies);

    // 冲刺(无敌闪避)
    BOOL TryDash();
    BOOL IsDashing() const { return m_dashFrames > 0; }
    int  GetDashCD() const { return m_dashCD; }
    BOOL IsInvincible() const { return m_dashFrames > 0; } // 冲刺中无敌

    // 护符 (灵狐免追)
    void ApplyAmulet() { m_amuletFrames = 60 * 8; }
    BOOL IsAmuletActive() const { return m_amuletFrames > 0; }

    // 货币
    int  GetCoin() const { return m_coin; }
    void AddCoin(int c) { m_coin += c; }
    BOOL SpendCoin(int c);

    // 道具
    int  GetItem(ItemType t) const { return m_items[t]; }
    void AddItem(ItemType t)       { m_items[t]++; }
    BOOL UseItem(ItemType t);

    // 技能等级 (0=未升 1/2/3)
    int  GetSkillLv(SkillType s) const { return m_skillLv[s]; }
    BOOL UpgradeSkill(SkillType s);    // 商店升级
    void UpgradeSkillFree(SkillType s) { if (m_skillLv[s] < 3) m_skillLv[s]++; }

    // 被动等级
    int  GetPassLv(PassiveType p) const { return m_passLv[p]; }
    BOOL UpgradePassive(PassiveType p);

    // 数值
    int  GetHP()    const { return m_hp; }
    int  GetMaxHP() const { return BASE_HP + PASS_HP_BONUS[m_passLv[PASS_HP]] + m_extraHP; }
    int  GetMP()    const { return m_mp; }
    int  GetMaxMP() const { return BASE_MP + PASS_MP_BONUS[m_passLv[PASS_MP]] + m_extraMP; }
    int  GetATK()   const { return BASE_ATK + PASS_ATK_BONUS[m_passLv[PASS_ATK]] + m_extraATK; }

    // 永久加成(奖励获得,跨章节保留)
    void AddPermaHP(int v)  { m_extraHP  += v; m_hp = min(GetMaxHP(), m_hp + v); }
    void AddPermaMP(int v)  { m_extraMP  += v; m_mp = min(GetMaxMP(), m_mp + v); }
    void AddPermaATK(int v) { m_extraATK += v; }

    // 吸血(普攻命中怪物时调用)
    void DoLifesteal(int dmg) {
        int pct = PASS_LIFESTEAL_PCT[m_passLv[PASS_LIFESTEAL]];
        if (pct > 0) Heal(max(1, dmg * pct / 100));
    }
    // 暴击判定: 返回最终伤害(可能 ×2)
    int RollAtkDamage(BOOL* outCrit = NULL) {
        int dmg = GetATK();
        BOOL crit = (rand() % 100) < CRIT_CHANCE_PCT;
        if (crit) dmg = (int)(dmg * CRIT_MULTIPLIER);
        if (outCrit) *outCrit = crit;
        return dmg;
    }

    Vec2 GetPos()   const { return m_pos; }
    int  GetFace()  const { return m_face; }
    int  GetAttackAnim() const { return m_attackAnim; }
    int  GetSkillCD(SkillType s) const { return m_skillCDLeft[s]; }
    int  GetStormFrames() const { return m_stormFrames; }
    int  GetNovaFrames()  const { return m_novaFrames; }

    // 防御力
    int  GetDefense() const { return PASS_DEFENSE_VAL[m_passLv[PASS_DEFENSE]] + m_extraDEF; }
    void AddPermaDEF(int v) { m_extraDEF += v; }

    // 修改器(展示用): 直接设置数值
    void DebugSetHP(int v)  { m_hp = v; if (m_hp > GetMaxHP()) m_extraHP += m_hp - GetMaxHP(); }
    void DebugSetMP(int v)  { m_mp = v; if (m_mp > GetMaxMP()) m_extraMP += m_mp - GetMaxMP(); }
    void DebugAddCoin(int v){ m_coin += v; if (m_coin < 0) m_coin = 0; }
    void DebugAddATK(int v) { m_extraATK += v; if (GetATK() < 1) m_extraATK = 1 - (GetATK()-m_extraATK); }
    void DebugFullHeal()    { m_hp = GetMaxHP(); m_mp = GetMaxMP(); }
    void DebugGodMode(BOOL on) { m_godMode = on; }
    BOOL IsGodMode() const  { return m_godMode; }
    // 修改器: 免费升级一级所有技能(到上限为止)
    void UpgradeSkillFree() {
        for (int i = 0; i < SKILL_COUNT; ++i)
            if (m_skillLv[i] < 3) m_skillLv[i]++;
    }

    // 受伤难度倍率(由 View 按难度设置)
    void SetDamageTakenMul(float m) { m_dmgTakenMul = m; }
    // 金币倍率(由 View 按难度设置)
    void SetCoinMul(float m) { m_coinMul = m; }
    void AddCoinByMul(int base) { m_coin += (int)(base * m_coinMul); }

    // 存档/读档接口
    void SaveTo(CFile& f) const;
    void LoadFrom(CFile& f);

private:
    Vec2 m_pos;
    int  m_hp, m_mp;
    int  m_coin;
    int  m_atkCD;
    int  m_attackAnim;
    int  m_amuletFrames;
    int  m_mpRegenTick;
    int  m_stormFrames;     // 烈焰风暴持续视觉帧
    int  m_novaFrames;      // 奥术爆发视觉帧
    int  m_face;            // 0下 1上 2左 3右

    int  m_items[ITEM_COUNT];
    int  m_skillLv[SKILL_COUNT];
    int  m_skillCDLeft[SKILL_COUNT];
    int  m_passLv[PASS_COUNT];

    // 永久属性加成(奖励获得,FullReset 才清零)
    int  m_extraHP;
    int  m_extraMP;
    int  m_extraATK;
    int  m_extraDEF;
    BOOL m_godMode;
    float m_dmgTakenMul = 1.0f;
    float m_coinMul = 1.0f;

    // 冲刺
    int  m_dashFrames;   // 剩余冲刺帧
    int  m_dashCD;       // 冷却剩余帧
    float m_dashVX, m_dashVY; // 冲刺方向(冲刺开始时锁定)
    // 拖尾位置历史(画拖尾用)
    Vec2 m_trail[8];
    int  m_trailIdx;
};
