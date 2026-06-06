// Player.cpp
#include "pch.h"
#include "Player.h"
#include "Projectile.h"
#include "Enemy.h"
#include "SpriteManager.h"
#include <math.h>

CPlayer::CPlayer() { FullReset(); }

void CPlayer::FullReset()
{
    m_pos = Vec2(WIN_W*0.15f, WIN_H*0.80f);
    m_coin = 0;
    ZeroMemory(m_items,   sizeof(m_items));
    ZeroMemory(m_skillLv, sizeof(m_skillLv));
    ZeroMemory(m_passLv,  sizeof(m_passLv));
    m_extraHP = m_extraMP = m_extraATK = 0;
    m_extraDEF = 0;
    m_godMode = FALSE;
    Reset();
}

// 进入下一关:保留 HP/MP/金币/被动等级,只重置位置/冷却/冲刺/拖尾
void CPlayer::ResetForNextChapter()
{
    m_pos = Vec2(WIN_W*0.15f, WIN_H*0.80f);
    m_atkCD = 0;
    m_attackAnim = 0;
    m_amuletFrames = 0;
    m_mpRegenTick = 0;
    m_stormFrames = 0;
    m_novaFrames = 0;
    m_face = 3;
    for (int i = 0; i < SKILL_COUNT; ++i) m_skillCDLeft[i] = 0;
    m_dashFrames = 0;
    m_dashCD = 0;
    m_dashVX = m_dashVY = 0;
    for (int i = 0; i < 8; ++i) m_trail[i] = m_pos;
    m_trailIdx = 0;
}

void CPlayer::Reset(){
    m_pos = Vec2(WIN_W*0.15f, WIN_H*0.80f);
    m_hp = GetMaxHP();
    m_mp = GetMaxMP();
    m_atkCD = 0;
    m_attackAnim = 0;
    m_amuletFrames = 0;
    m_mpRegenTick = 0;
    m_stormFrames = 0;
    m_novaFrames = 0;
    m_face = 3;
    for (int i = 0; i < SKILL_COUNT; ++i) m_skillCDLeft[i] = 0;
    // 冲刺
    m_dashFrames = 0;
    m_dashCD = 0;
    m_dashVX = m_dashVY = 0;
    for (int i = 0; i < 8; ++i) m_trail[i] = m_pos;
    m_trailIdx = 0;
}

void CPlayer::Update(BOOL up, BOOL dn, BOOL lf, BOOL rt)
{
    // 冲刺中: 不受输入控制,沿锁定方向高速移动
    if (m_dashFrames > 0) {
        m_pos.x += m_dashVX * DASH_DISTANCE;
        m_pos.y += m_dashVY * DASH_DISTANCE;
        if (m_pos.x < 20)         m_pos.x = 20;
        if (m_pos.x > LEVEL_W-20) m_pos.x = LEVEL_W-20;
        if (m_pos.y < GROUND_TOP_Y) m_pos.y = (float)GROUND_TOP_Y;
        if (m_pos.y > WIN_H-30)   m_pos.y = (float)(WIN_H-30);
        m_dashFrames--;
        // 记录拖尾
        m_trailIdx = (m_trailIdx + 1) % 8;
        m_trail[m_trailIdx] = m_pos;
        // 冷却递减
        if (m_dashCD > 0) m_dashCD--;
        if (m_attackAnim > 0) m_attackAnim--;
        if (m_amuletFrames > 0) m_amuletFrames--;
        if (m_stormFrames > 0) m_stormFrames--;
        if (m_novaFrames > 0) m_novaFrames--;
        for (int i = 0; i < SKILL_COUNT; ++i)
            if (m_skillCDLeft[i] > 0) m_skillCDLeft[i]--;
        return;
    }
    if (m_dashCD > 0) m_dashCD--;

    float dx = 0, dy = 0;
    if (up) dy -= PLAYER_SPEED;
    if (dn) dy += PLAYER_SPEED;
    if (lf) dx -= PLAYER_SPEED;
    if (rt) dx += PLAYER_SPEED;
    if (dx && dy) { dx *= 0.707f; dy *= 0.707f; }
    m_pos.x += dx;
    m_pos.y += dy;
    if (m_pos.x < 20)         m_pos.x = 20;
    if (m_pos.x > LEVEL_W-20) m_pos.x = LEVEL_W-20;
    if (m_pos.y < GROUND_TOP_Y) m_pos.y = (float)GROUND_TOP_Y;
    if (m_pos.y > WIN_H-30)   m_pos.y = (float)(WIN_H-30);

    // 新动画布局: 只跟随左右方向(忽略上下),保留最后一次左右朝向
    if (dx < 0) m_face = 2;       // 朝左
    else if (dx > 0) m_face = 3;  // 朝右
    // dy 变化不改变 face

    if (m_atkCD > 0)        m_atkCD--;
    if (m_attackAnim > 0)   m_attackAnim--;
    if (m_amuletFrames > 0) m_amuletFrames--;
    if (m_stormFrames > 0)  m_stormFrames--;
    if (m_novaFrames > 0)   m_novaFrames--;
    for (int i = 0; i < SKILL_COUNT; ++i)
        if (m_skillCDLeft[i] > 0) m_skillCDLeft[i]--;

    // MP 自动回复：基础每秒 +2，被动等级加成
    m_mpRegenTick++;
    int interval = 30;  // 0.5 秒一跳
    if (m_mpRegenTick >= interval) {
        m_mpRegenTick = 0;
        int amt = 1 + PASS_REGEN_BONUS[m_passLv[PASS_REGEN]];
        RegenMP(amt);
        // 吸血改在普攻命中时触发,不在这里自动回血
    }
}

void CPlayer::TakeDamage(int dmg)
{
    // 修改器无敌
    if (m_godMode) return;
    // 冲刺中完全无敌
    if (m_dashFrames > 0) return;
    // 闪避判定
    int dodge = PASS_DODGE_PCT[m_passLv[PASS_DODGE]];
    if (dodge > 0 && (rand() % 100) < dodge) {
        m_attackAnim = 6;
        return;
    }
    // 防御减伤(至少受 1 点) + 难度倍率
    dmg = (int)(dmg * m_dmgTakenMul);
    dmg -= GetDefense();
    if (dmg < 1) dmg = 1;
    m_hp -= dmg;
    if (m_hp <= 0) {
        // 复活卷轴: 死亡时自动消耗,恢复 50% HP
        if (m_items[ITEM_REVIVE] > 0) {
            m_items[ITEM_REVIVE]--;
            m_hp = GetMaxHP() / 2;
            m_dashFrames = 0;
            m_dashCD = 0;
        } else {
            m_hp = 0;
        }
    }
}
void CPlayer::Heal(int hp) { m_hp = min(GetMaxHP(), m_hp + hp); }
void CPlayer::RegenMP(int mp) { m_mp = min(GetMaxMP(), m_mp + mp); }

BOOL CPlayer::SpendCoin(int c)
{
    if (m_coin < c) return FALSE;
    m_coin -= c;
    return TRUE;
}

BOOL CPlayer::UseItem(ItemType t)
{
    if (t == ITEM_REVIVE) return FALSE;  // 复活卷轴是死亡时自动触发,不能主动用
    if (m_items[t] <= 0) return FALSE;
    m_items[t]--;
    switch (t) {
    case ITEM_POTION: Heal(50); break;
    case ITEM_MANA:   RegenMP(40); break;
    case ITEM_AMULET: ApplyAmulet(); break;
    case ITEM_SCROLL: /* 由 View 处理 */ break;
    default: break;
    }
    return TRUE;
}

BOOL CPlayer::TryDash()
{
    if (m_dashCD > 0 || m_dashFrames > 0) return FALSE;
    // 朝当前 face 方向冲刺
    float vx = 0, vy = 0;
    switch (m_face) {
    case 0: vy = 1; break;    // 下
    case 1: vy = -1; break;   // 上
    case 2: vx = -1; break;   // 左
    case 3: vx = 1; break;    // 右
    }
    m_dashVX = vx;
    m_dashVY = vy;
    m_dashFrames = DASH_FRAMES;
    m_dashCD = DASH_CD;
    // 初始化拖尾位置
    for (int i = 0; i < 8; ++i) m_trail[i] = m_pos;
    m_trailIdx = 0;
    return TRUE;
}

BOOL CPlayer::TryFireball(CProjectileMgr* mgr)
{
    if (m_atkCD > 0) return FALSE;
    m_atkCD = PLAYER_ATK_CD;
    m_attackAnim = 8;
    int dmg = RollAtkDamage();   // 自带暴击判定
    mgr->SpawnFireball(m_pos, m_face, dmg);
    return TRUE;
}

BOOL CPlayer::TrySkill(SkillType s, CProjectileMgr* mgr, std::vector<CEnemy*>& enemies)
{
    if (m_skillCDLeft[s] > 0) return FALSE;
    if (m_mp < SKILL_MP_COST[s]) return FALSE;
    int lv = m_skillLv[s];
    int val = SKILL_VAL_LV[s][lv];

    switch (s) {
    case SKILL_ICE:
        mgr->SpawnIce(m_pos, m_face, val);
        break;
    case SKILL_LIGHTNING:
        mgr->SpawnLightning(m_pos, m_face, val);
        break;
    case SKILL_STORM: {
        // 直接对附近敌人造成伤害
        for (auto* e : enemies) {
            if (!e->IsAlive()) continue;
            if (e->IsSpawning()) continue;   // 出生期免疫
            float dx = e->GetPos().x - m_pos.x;
            float dy = e->GetPos().y - m_pos.y;
            if (dx*dx + dy*dy <= 180*180) {
                CFogCore* boss = dynamic_cast<CFogCore*>(e);
                if (boss) boss->OnHit(val);
                else e->TakeDamage(val);
            }
        }
        m_stormFrames = 25;
        break;
    }
    case SKILL_NOVA: {
        // 奥术爆发: 超大范围(280)高伤害
        for (auto* e : enemies) {
            if (!e->IsAlive()) continue;
            if (e->IsSpawning()) continue;
            float dx = e->GetPos().x - m_pos.x;
            float dy = e->GetPos().y - m_pos.y;
            if (dx*dx + dy*dy <= 280*280) {
                CFogCore* boss = dynamic_cast<CFogCore*>(e);
                if (boss) boss->OnHit(val);
                else e->TakeDamage(val);
            }
        }
        m_novaFrames = 30;  // 爆发动画
        break;
    }
    }
    m_mp -= SKILL_MP_COST[s];
    m_skillCDLeft[s] = SKILL_CD[s];
    return TRUE;
}

BOOL CPlayer::UpgradeSkill(SkillType s)
{
    int lv = m_skillLv[s];
    if (lv >= 3) return FALSE;
    int price = PRICE_SKILL_LV[lv];
    if (!SpendCoin(price)) return FALSE;
    m_skillLv[s]++;
    return TRUE;
}

BOOL CPlayer::UpgradePassive(PassiveType p)
{
    int lv = m_passLv[p];
    if (lv >= 3) return FALSE;
    int price = PRICE_PASS_LV[lv];
    if (!SpendCoin(price)) return FALSE;
    m_passLv[p]++;
    // 升级 HP/MP 时即时补满
    if (p == PASS_HP) m_hp = GetMaxHP();
    if (p == PASS_MP) m_mp = GetMaxMP();
    return TRUE;
}

void CPlayer::Draw(CDC* pDC, int frame)
{
    int x = (int)m_pos.x, y = (int)m_pos.y;

    // 冲刺拖尾: 画 5 个渐淡的残影
    if (m_dashFrames > 0) {
        for (int i = 1; i < 6; ++i) {
            int idx = (m_trailIdx - i + 8) % 8;
            int tx = (int)m_trail[idx].x;
            int ty = (int)m_trail[idx].y;
            int alpha = 200 - i * 35;
            COLORREF col = RGB(120 + i*10, 180 + i*5, 255);
            CBrush b(col);
            CBrush* po = pDC->SelectObject(&b);
            CPen* pe = (CPen*)pDC->SelectStockObject(NULL_PEN);
            pDC->Ellipse(tx-10, ty-10, tx+10, ty+10);
            pDC->SelectObject(po);
            pDC->SelectObject(pe);
        }
    }

    // 优先用贴图
    if (g_sprites.Has(SPR_PLAYER)) {
        // 新精灵图: 128x32, 4 帧 = 0:右攻击 1:左攻击 2:朝左 3:朝右
        int frameIdx;
        if (m_attackAnim > 0) {
            frameIdx = (m_face == 2) ? 1 : 0;
        } else {
            frameIdx = (m_face == 2) ? 2 : 3;
        }
        g_sprites.DrawFrameScaled(pDC, SPR_PLAYER, x, y, 32, 32, frameIdx, 2.0f, FALSE);
    } else {
        // 回退绘制：法师斗篷
        CBrush cloak(RGB(80, 60, 130)), face(RGB(245,220,180));
        CBrush hat(RGB(60, 40, 100)), staff(RGB(120,80,50));
        CBrush gem(RGB(80,200,255));
        CBrush* pOld = pDC->SelectObject(&cloak);
        pDC->Ellipse(x-16, y-6, x+16, y+24);
        pDC->SelectObject(&face);
        pDC->Ellipse(x-9, y-18, x+9, y-2);
        // 法师尖帽
        pDC->SelectObject(&hat);
        POINT hatp[3] = { {x-12, y-15}, {x, y-30}, {x+12, y-15} };
        pDC->Polygon(hatp, 3);
        // 法杖
        pDC->SelectObject(&staff);
        pDC->Rectangle(x+12, y-25, x+15, y+20);
        pDC->SelectObject(&gem);
        pDC->Ellipse(x+8, y-32, x+20, y-22);
        pDC->SelectObject(pOld);
    }

    // 护符光圈
    if (IsAmuletActive()) {
        CPen pen(PS_SOLID, 2, RGB(100,200,255));
        CPen* pOldP = pDC->SelectObject(&pen);
        CBrush* pNullB = (CBrush*)pDC->SelectStockObject(NULL_BRUSH);
        pDC->Ellipse(x-28, y-28, x+28, y+28);
        pDC->SelectObject(pOldP);
        pDC->SelectObject(pNullB);
    }

    // 烈焰风暴特效
    if (m_stormFrames > 0) {
        int r = 180 - (25 - m_stormFrames) * 4;
        CPen pen(PS_SOLID, 3, RGB(255, 120, 40));
        CPen* pOldP = pDC->SelectObject(&pen);
        CBrush* pNullB = (CBrush*)pDC->SelectStockObject(NULL_BRUSH);
        pDC->Ellipse(x-r, y-r, x+r, y+r);
        CPen pen2(PS_SOLID, 2, RGB(255, 200, 80));
        pDC->SelectObject(&pen2);
        pDC->Ellipse(x-r+10, y-r+10, x+r-10, y+r-10);
        pDC->SelectObject(pOldP);
        pDC->SelectObject(pNullB);
    }

    // 奥术爆发特效(紫白超大圆)
    if (m_novaFrames > 0) {
        int prog = 30 - m_novaFrames;
        int rN = 60 + prog * 8;
        if (rN > 280) rN = 280;
        CPen penN(PS_SOLID, 4, RGB(180, 80, 255));
        CPen* pOldPN = pDC->SelectObject(&penN);
        CBrush* pNullBN = (CBrush*)pDC->SelectStockObject(NULL_BRUSH);
        pDC->Ellipse(x-rN, y-rN, x+rN, y+rN);
        CPen penN2(PS_SOLID, 2, RGB(230, 180, 255));
        pDC->SelectObject(&penN2);
        pDC->Ellipse(x-rN+14, y-rN+14, x+rN-14, y+rN-14);
        int rc2 = rN / 3;
        CPen penN3(PS_SOLID, 3, RGB(255, 255, 255));
        pDC->SelectObject(&penN3);
        pDC->Ellipse(x-rc2, y-rc2, x+rc2, y+rc2);
        pDC->SelectObject(pOldPN);
        pDC->SelectObject(pNullBN);
    }
}

void CPlayer::SaveTo(CFile& f) const
{
    f.Write(&m_pos,  sizeof(m_pos));
    f.Write(&m_hp,   sizeof(m_hp));
    f.Write(&m_mp,   sizeof(m_mp));
    f.Write(&m_coin, sizeof(m_coin));
    f.Write(m_items,   sizeof(m_items));
    f.Write(m_skillLv, sizeof(m_skillLv));
    f.Write(m_passLv,  sizeof(m_passLv));
    f.Write(&m_extraHP,  sizeof(m_extraHP));
    f.Write(&m_extraMP,  sizeof(m_extraMP));
    f.Write(&m_extraATK, sizeof(m_extraATK));
    f.Write(&m_extraDEF, sizeof(m_extraDEF));
}
void CPlayer::LoadFrom(CFile& f)
{
    f.Read(&m_pos,  sizeof(m_pos));
    f.Read(&m_hp,   sizeof(m_hp));
    f.Read(&m_mp,   sizeof(m_mp));
    f.Read(&m_coin, sizeof(m_coin));
    f.Read(m_items,   sizeof(m_items));
    f.Read(m_skillLv, sizeof(m_skillLv));
    f.Read(m_passLv,  sizeof(m_passLv));
    f.Read(&m_extraHP,  sizeof(m_extraHP));
    f.Read(&m_extraMP,  sizeof(m_extraMP));
    f.Read(&m_extraATK, sizeof(m_extraATK));
    f.Read(&m_extraDEF, sizeof(m_extraDEF));
    m_atkCD = m_attackAnim = m_amuletFrames = m_stormFrames = m_novaFrames = 0;
    m_godMode = FALSE;
    for (int i = 0; i < SKILL_COUNT; ++i) m_skillCDLeft[i] = 0;
}
