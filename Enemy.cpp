// Enemy.cpp
#include "pch.h"
#include "Enemy.h"
#include "Player.h"
#include "Projectile.h"
#include "SpriteManager.h"
#include "AudioManager.h"
#include <math.h>

// ============== 基类 ==============
CEnemy::CEnemy(EnemyType t, float x, float y, float diffMul)
    : m_type(t), m_pos(x,y), m_curCD(0),
      m_hurtFlash(0), m_freezeLeft(0), m_weakened(FALSE), m_id(0),
      m_spawnFrames(ENEMY_SPAWN_FRAMES),   // 1 秒出生预警
      m_hitstun(0)
{
    // 基础属性
    switch (t) {
    case ENEMY_GHOST:    m_maxHP =  35;  m_radius = 18; m_attackCD = 45; break;
    case ENEMY_TREE:     m_maxHP =  90;  m_radius = 24; m_attackCD = 70; break;
    case ENEMY_BAT:      m_maxHP =  45;  m_radius = 16; m_attackCD = 75; break;
    case ENEMY_FOX:      m_maxHP =  70;  m_radius = 18; m_attackCD = 35; break;
    case ENEMY_WOLF:     m_maxHP = 110;  m_radius = 22; m_attackCD = 60; break;
    case ENEMY_ASSASSIN: m_maxHP =  85;  m_radius = 18; m_attackCD = 80; break;
    case ENEMY_WRAITH:   m_maxHP = 130;  m_radius = 22; m_attackCD = 65; break;
    case ENEMY_BOSS:     m_maxHP = 900;  m_radius = 40; m_attackCD = 90; break;
    }
    m_maxHP = (int)(m_maxHP * diffMul);
    m_hp = m_maxHP;
}

// 在场景中画召唤圆+粒子(基类工具)
void CEnemy::DrawSpawnEffect(CDC* pDC, int x, int y, int radius)
{
    if (m_spawnFrames <= 0) return;
    float t = 1.0f - (float)m_spawnFrames / ENEMY_SPAWN_FRAMES;  // 0 → 1
    // 1. 旋转法阵(双层椭圆)
    int r1 = (int)(radius * (0.6f + t*0.5f));
    int r2 = (int)(radius * (0.4f + t*0.3f));
    CPen pen(PS_SOLID, 2, RGB(180, 80, 220));
    CPen* po = pDC->SelectObject(&pen);
    CBrush* pb = (CBrush*)pDC->SelectStockObject(NULL_BRUSH);
    pDC->Ellipse(x-r1, y-r1/2, x+r1, y+r1/2);
    pDC->Ellipse(x-r1/2, y-r1, x+r1/2, y+r1);

    CPen pen2(PS_SOLID, 1, RGB(255, 150, 255));
    pDC->SelectObject(&pen2);
    pDC->Ellipse(x-r2, y-r2, x+r2, y+r2);

    // 2. 粒子从外向内汇聚
    int particleCount = 8;
    for (int i = 0; i < particleCount; ++i) {
        float ang = (float)(i * 360.0f / particleCount + m_spawnFrames * 6) * 3.1415926f / 180;
        float dist = radius * 1.5f * (1.0f - t);
        int px = x + (int)(cos(ang) * dist);
        int py = y + (int)(sin(ang) * dist);
        CBrush b(RGB(220, 150, 255));
        CBrush* pob = pDC->SelectObject(&b);
        pDC->Ellipse(px-3, py-3, px+3, py+3);
        pDC->SelectObject(pob);
    }
    pDC->SelectObject(po);
    pDC->SelectObject(pb);
}

int CEnemy::GetCoinDrop() const
{
    switch (m_type) {
    case ENEMY_GHOST:    return 12 + rand()%5;
    case ENEMY_TREE:     return 30;
    case ENEMY_BAT:      return 24;
    case ENEMY_FOX:      return 42;
    case ENEMY_WOLF:     return 50;
    case ENEMY_ASSASSIN: return 48;
    case ENEMY_WRAITH:   return 58;
    case ENEMY_BOSS:     return 150;
    }
    return 0;
}

void CEnemy::TakeDamage(int dmg)
{
    m_hp -= dmg;
    if (m_hp < 0) m_hp = 0;
    m_hurtFlash = 8;     // 闪红 8 帧(更明显)
    m_hitstun = 5;       // 硬直 5 帧(不能移动)
    g_audio.PlaySfx(SFX_HIT);
}

void CEnemy::WeakenDefense()
{
    m_weakened = TRUE;
    m_maxHP = (m_maxHP + m_hp)/2;
    if (m_hp > m_maxHP) m_hp = m_maxHP;
}

Vec2 CEnemy::DirTo(const Vec2& t) const
{
    float dx = t.x - m_pos.x;
    float dy = t.y - m_pos.y;
    float d = sqrt(dx*dx + dy*dy);
    if (d < 0.1f) return Vec2(0,0);
    return Vec2(dx/d, dy/d);
}

BOOL CEnemy::CanReach(const Vec2& t, float r) const
{
    if (m_curCD > 0) return FALSE;
    float dx = t.x - m_pos.x, dy = t.y - m_pos.y;
    return dx*dx + dy*dy <= r*r;
}

void CEnemy::ClampInLevel()
{
    if (m_pos.x < m_radius)          m_pos.x = (float)m_radius;
    if (m_pos.x > LEVEL_W - m_radius) m_pos.x = (float)(LEVEL_W - m_radius);
    if (m_pos.y < GROUND_TOP_Y)      m_pos.y = (float)GROUND_TOP_Y;
    if (m_pos.y > WIN_H - m_radius)  m_pos.y = (float)(WIN_H - m_radius);
}

// 工具：通用 HP 条
static void DrawHpBar(CDC* pDC, int x, int y, int w, int hp, int mx,
                      COLORREF c = RGB(220,80,80))
{
    pDC->FillSolidRect(x-w/2, y, w, 4, RGB(60,20,20));
    pDC->FillSolidRect(x-w/2, y, w*hp/mx, 4, c);
}

// =============================================
// 1. 游荡幽影
// =============================================
CGhost::CGhost(float x, float y, float dm) : CEnemy(ENEMY_GHOST, x, y, dm)
{ m_pA = Vec2(x-100, y); m_pB = Vec2(x+100, y); m_t = 0; m_dir = 1; }

void CGhost::Update(CPlayer* p, CProjectileMgr*)
{
    if (m_spawnFrames > 0) { m_spawnFrames--; return; }
    if (m_hitstun > 0) { m_hitstun--; if (m_hurtFlash>0) m_hurtFlash--; return; }
    if (m_hurtFlash > 0) m_hurtFlash--;
    if (m_curCD > 0) m_curCD--;
    if (m_freezeLeft > 0) { m_freezeLeft--; return; }
    m_faceLeft = (p->GetPos().x < m_pos.x);  // 朝向玩家

    Vec2 pp = p->GetPos();
    float dx = pp.x - m_pos.x, dy = pp.y - m_pos.y;
    float dist = sqrt(dx*dx + dy*dy);

    if (dist < 280) {
        // 察觉玩家: 飘向玩家,带轻微正弦摆动(幽灵漂浮感)
        Vec2 d = DirTo(pp);
        m_t += 0.08f;
        float perpX = -d.y, perpY = d.x;  // 垂直方向
        float wobble = sin(m_t * 3) * 0.6f;
        m_pos.x += d.x * 1.6f + perpX * wobble;
        m_pos.y += d.y * 1.6f + perpY * wobble;
    } else {
        // 未察觉: 缓慢游荡(随机微调方向)
        m_t += 0.02f;
        m_pos.x += cos(m_t) * 0.8f;
        m_pos.y += sin(m_t * 1.3f) * 0.6f;
    }
    if (CanReach(pp, 35)) { p->TakeDamage(6); m_curCD = m_attackCD; }
    ClampInLevel();
}

void CGhost::Draw(CDC* pDC, int)
{
    if (m_spawnFrames > 0) {
        DrawSpawnEffect(pDC, (int)m_pos.x, (int)m_pos.y, m_radius + 8);
        return;
    }
    int x = (int)m_pos.x, y = (int)m_pos.y;
    if (g_sprites.Has(SPR_GOBLIN)) g_sprites.DrawScaled(pDC, SPR_GOBLIN, x, y, DrawScale(), m_faceLeft);
    else if (g_sprites.Has(SPR_GHOST)) g_sprites.DrawScaled(pDC, SPR_GHOST, x, y, DrawScale(), m_faceLeft);
    else {
        CBrush b((m_hurtFlash>0)?RGB(255,255,255):RGB(180,200,230));
        CBrush* po = pDC->SelectObject(&b);
        pDC->Ellipse(x-18,y-18,x+18,y+18);
        pDC->SelectObject(po);
    }
    DrawHpBar(pDC, x, y-26, 30, m_hp, m_maxHP);
    if (m_freezeLeft > 0) {
        CPen pen(PS_SOLID, 1, RGB(100,220,255));
        CPen* po = pDC->SelectObject(&pen);
        CBrush* pn = (CBrush*)pDC->SelectStockObject(NULL_BRUSH);
        pDC->Ellipse(x-22,y-22,x+22,y+22);
        pDC->SelectObject(po); pDC->SelectObject(pn);
    }
}

// =============================================
// 2. 路障树精
// =============================================
CTreeGuard::CTreeGuard(float x, float y, float dm) : CEnemy(ENEMY_TREE, x, y, dm), m_sway(0) {}

void CTreeGuard::Update(CPlayer* p, CProjectileMgr* proj)
{
    if (m_spawnFrames > 0) { m_spawnFrames--; return; }
    if (m_hitstun > 0) { m_hitstun--; if (m_hurtFlash>0) m_hurtFlash--; return; }
    if (m_hurtFlash > 0) m_hurtFlash--;
    if (m_curCD > 0) m_curCD--;
    if (m_freezeLeft > 0) { m_freezeLeft--; return; }
    m_faceLeft = (p->GetPos().x < m_pos.x);  // 朝向玩家
    m_sway++;

    Vec2 pp = p->GetPos();
    float dx = pp.x - m_pos.x, dy = pp.y - m_pos.y;
    float dist = sqrt(dx*dx + dy*dy);

    // 近战
    if (CanReach(pp, 60)) { p->TakeDamage(14); m_curCD = m_attackCD; }

    // 远程: 360 度环形齐射种子(树精扎根不动,但范围压制)
    m_seedTimer--;
    if (m_seedTimer <= 0 && dist < 550) {
        const int N = 12;  // 12 颗种子均匀环形
        for (int i = 0; i < N; ++i) {
            float ang = (float)i / N * 6.2831853f;
            Vec2 d(cos(ang), sin(ang));
            proj->SpawnSeed(m_pos, d, 8);
        }
        m_seedTimer = 150;  // 间隔(留出反应时间)
    }
}

void CTreeGuard::Draw(CDC* pDC, int)
{
    if (m_spawnFrames > 0) {
        DrawSpawnEffect(pDC, (int)m_pos.x, (int)m_pos.y, m_radius + 8);
        return;
    }
    int x = (int)m_pos.x, y = (int)m_pos.y;
    if (g_sprites.Has(SPR_TREE)) g_sprites.DrawScaled(pDC, SPR_TREE, x, y, DrawScale(), m_faceLeft);
    else {
        CBrush trunk(RGB(90,60,40)), leaf((m_hurtFlash>0)?RGB(255,255,255):RGB(60,120,70));
        CBrush* po = pDC->SelectObject(&trunk);
        pDC->Rectangle(x-12, y-5, x+12, y+30);
        pDC->SelectObject(&leaf);
        pDC->Ellipse(x-28, y-35, x+28, y+5);
        pDC->SelectObject(po);
    }
    DrawHpBar(pDC, x, y-44, 50, m_hp, m_maxHP, RGB(220,160,80));
}

// =============================================
// 3. 暗夜蝙蝠
// =============================================
CBat::CBat(float x, float y, float dm) : CEnemy(ENEMY_BAT, x, y, dm)
{ m_baseX = x; m_baseY = y; m_phase = 0; m_shootTimer = 60; }

void CBat::Update(CPlayer* p, CProjectileMgr* proj)
{
    if (m_spawnFrames > 0) { m_spawnFrames--; return; }
    if (m_hitstun > 0) { m_hitstun--; if (m_hurtFlash>0) m_hurtFlash--; return; }
    if (m_hurtFlash > 0) m_hurtFlash--;
    if (m_curCD > 0) m_curCD--;
    if (m_freezeLeft > 0) { m_freezeLeft--; return; }
    m_faceLeft = (p->GetPos().x < m_pos.x);  // 朝向玩家

    m_phase += 0.08f;
    Vec2 pp = p->GetPos();
    float dx = pp.x - m_pos.x, dy = pp.y - m_pos.y;
    float dist = sqrt(dx*dx + dy*dy);
    Vec2 d = DirTo(pp);

    // 风筝走位: 与玩家保持 180~250 的理想距离
    float ideal = 210;
    if (dist < ideal - 40) {
        // 太近,后撤 + 侧移
        m_pos.x -= d.x * 2.2f;
        m_pos.y -= d.y * 2.2f;
        m_pos.x += -d.y * 1.5f;  // 绕圈
        m_pos.y += d.x * 1.5f;
    } else if (dist > ideal + 40) {
        // 太远,接近
        m_pos.x += d.x * 1.8f;
        m_pos.y += d.y * 1.8f;
    } else {
        // 理想距离,绕玩家盘旋
        m_pos.x += -d.y * 2.0f;
        m_pos.y += d.x * 2.0f;
    }
    // 上下扇翅抖动
    m_pos.y += sin(m_phase) * 0.8f;

    m_shootTimer--;
    if (m_shootTimer <= 0 && dist < 400) {
        // 预判: 朝玩家略微提前量射击
        proj->SpawnPoison(m_pos, d, 10);
        m_shootTimer = 70;
    }
    ClampInLevel();
}

void CBat::Draw(CDC* pDC, int frame)
{
    if (m_spawnFrames > 0) {
        DrawSpawnEffect(pDC, (int)m_pos.x, (int)m_pos.y, m_radius + 8);
        return;
    }
    int x = (int)m_pos.x, y = (int)m_pos.y;
    if (g_sprites.Has(SPR_BAT)) g_sprites.DrawScaled(pDC, SPR_BAT, x, y, DrawScale(), m_faceLeft);
    else {
        // 蝙蝠：紫黑身 + 翅膀
        int wingFlap = (int)(sin(frame*0.5) * 6);
        CBrush body((m_hurtFlash>0)?RGB(255,255,255):RGB(70,40,90));
        CBrush eye(RGB(255,40,40));
        CBrush* po = pDC->SelectObject(&body);
        pDC->Ellipse(x-8, y-6, x+8, y+8);
        // 翅膀（多边形）
        POINT wL[3] = { {x-7,y-2}, {x-22, y-8+wingFlap}, {x-7, y+4} };
        POINT wR[3] = { {x+7,y-2}, {x+22, y-8+wingFlap}, {x+7, y+4} };
        pDC->Polygon(wL, 3); pDC->Polygon(wR, 3);
        pDC->SelectObject(&eye);
        pDC->Ellipse(x-4, y-3, x-2, y-1);
        pDC->Ellipse(x+2, y-3, x+4, y-1);
        pDC->SelectObject(po);
    }
    DrawHpBar(pDC, x, y-18, 30, m_hp, m_maxHP, RGB(180,100,200));
}

// =============================================
// 4. 追月灵狐
// =============================================
CFox::CFox(float x, float y, float dm) : CEnemy(ENEMY_FOX, x, y, dm), m_speed(3.6f) {}

void CFox::Update(CPlayer* p, CProjectileMgr*)
{
    if (m_spawnFrames > 0) { m_spawnFrames--; return; }
    if (m_hitstun > 0) { m_hitstun--; if (m_hurtFlash>0) m_hurtFlash--; return; }
    if (m_hurtFlash > 0) m_hurtFlash--;
    if (m_curCD > 0) m_curCD--;
    if (m_freezeLeft > 0) { m_freezeLeft--; return; }
    m_faceLeft = (p->GetPos().x < m_pos.x);  // 朝向玩家

    if (p->IsAmuletActive()) { ClampInLevel(); return; }

    Vec2 pp = p->GetPos();
    float dx = pp.x - m_pos.x, dy = pp.y - m_pos.y;
    float dist = sqrt(dx*dx + dy*dy);
    Vec2 d = DirTo(pp);

    // m_speed 复用为"扑击计时器"逻辑: 用 m_curCD 之外的相位
    // 行为: 远处绕圈接近 → 距离合适时蓄力 → 猛扑
    m_pounceTimer--;
    if (m_pounceTimer > 0 && m_pounceTimer < 14) {
        // 猛扑中: 高速直冲(更快)
        m_pos.x += d.x * 9.5f;
        m_pos.y += d.y * 9.5f;
    } else if (dist > 160) {
        // 远: 螺旋接近(更快)
        m_pos.x += d.x * m_speed + (-d.y) * 2.2f;
        m_pos.y += d.y * m_speed + (d.x) * 2.2f;
    } else {
        // 中距离: 绕玩家转圈伺机(更快进入下次扑击)
        m_pos.x += -d.y * 3.8f + d.x * 0.8f;
        m_pos.y += d.x * 3.8f + d.y * 0.8f;
        if (m_pounceTimer <= 0) m_pounceTimer = 48;  // 更频繁扑击(原70)
    }
    if (CanReach(pp, 32)) { p->TakeDamage(10); m_curCD = m_attackCD; }
    ClampInLevel();
}

void CFox::Draw(CDC* pDC, int)
{
    if (m_spawnFrames > 0) {
        DrawSpawnEffect(pDC, (int)m_pos.x, (int)m_pos.y, m_radius + 8);
        return;
    }
    int x = (int)m_pos.x, y = (int)m_pos.y;
    if (g_sprites.Has(SPR_FOX)) g_sprites.DrawScaled(pDC, SPR_FOX, x, y, DrawScale(), m_faceLeft);
    else {
        CBrush body((m_hurtFlash>0)?RGB(255,255,255):RGB(220,130,60));
        CBrush* po = pDC->SelectObject(&body);
        pDC->Ellipse(x-18, y-10, x+18, y+15);
        pDC->Ellipse(x+8, y-18, x+28, y);
        pDC->SelectObject(po);
    }
    DrawHpBar(pDC, x, y-22, 36, m_hp, m_maxHP, RGB(255,160,80));
}

// =============================================
// 5. 狂暴狼影 (会冲刺)
// =============================================
CWolf::CWolf(float x, float y, float dm)
    : CEnemy(ENEMY_WOLF, x, y, dm), m_dashCD(120), m_dashing(0), m_dashDir(0,0) {}

void CWolf::Update(CPlayer* p, CProjectileMgr*)
{
    if (m_spawnFrames > 0) { m_spawnFrames--; return; }
    if (m_hitstun > 0) { m_hitstun--; if (m_hurtFlash>0) m_hurtFlash--; return; }
    if (m_hurtFlash > 0) m_hurtFlash--;
    if (m_curCD > 0) m_curCD--;
    if (m_freezeLeft > 0) { m_freezeLeft--; return; }
    m_faceLeft = (p->GetPos().x < m_pos.x);  // 朝向玩家

    if (m_dashing > 0) {
        // 冲刺中
        m_pos.x += m_dashDir.x * 12;
        m_pos.y += m_dashDir.y * 12;
        m_dashing--;
        if (CanReach(p->GetPos(), 40)) { p->TakeDamage(18); m_curCD = m_attackCD; m_dashing = 0; }
    } else {
        m_dashCD--;
        if (m_dashCD <= 0) {
            m_dashDir = DirTo(p->GetPos());
            m_dashing = 22;     // 冲刺帧
            m_dashCD  = 110;    // 更频繁冲刺(原180)
        } else {
            // 接近更快
            Vec2 d = DirTo(p->GetPos());
            m_pos.x += d.x * 2.6f;
            m_pos.y += d.y * 2.6f;
            if (CanReach(p->GetPos(), 35)) { p->TakeDamage(12); m_curCD = m_attackCD; }
        }
    }
    ClampInLevel();
}

void CWolf::Draw(CDC* pDC, int)
{
    if (m_spawnFrames > 0) {
        DrawSpawnEffect(pDC, (int)m_pos.x, (int)m_pos.y, m_radius + 8);
        return;
    }
    int x = (int)m_pos.x, y = (int)m_pos.y;
    if (g_sprites.Has(SPR_WOLF)) g_sprites.DrawScaled(pDC, SPR_WOLF, x, y, DrawScale(), m_faceLeft);
    else {
        COLORREF c = (m_hurtFlash>0)?RGB(255,255,255):
                     (m_dashing>0)?RGB(180,80,80):RGB(80,80,90);
        CBrush body(c), eye(RGB(255,60,60));
        CBrush* po = pDC->SelectObject(&body);
        pDC->Ellipse(x-22, y-12, x+18, y+15);
        pDC->Ellipse(x+10, y-20, x+30, y-2);
        // 耳朵
        POINT ear[3] = { {x+13,y-15}, {x+16,y-26}, {x+19,y-15} };
        pDC->Polygon(ear, 3);
        pDC->SelectObject(&eye);
        pDC->Ellipse(x+20, y-14, x+24, y-10);
        pDC->SelectObject(po);
    }
    DrawHpBar(pDC, x, y-28, 48, m_hp, m_maxHP, RGB(220,100,100));
    // 冲刺前预警
    if (m_dashing > 0) {
        CPen pen(PS_SOLID, 2, RGB(255,80,80));
        CPen* po = pDC->SelectObject(&pen);
        pDC->MoveTo(x, y);
        pDC->LineTo(x + (int)(m_dashDir.x*60), y + (int)(m_dashDir.y*60));
        pDC->SelectObject(po);
    }
}

// =============================================
// 6. 影刺客 (会隐身突袭)
// =============================================
CAssassin::CAssassin(float x, float y, float dm)
    : CEnemy(ENEMY_ASSASSIN, x, y, dm), m_stealth(0), m_attackWindup(0) {}

void CAssassin::Update(CPlayer* p, CProjectileMgr*)
{
    if (m_spawnFrames > 0) { m_spawnFrames--; return; }
    if (m_hitstun > 0) { m_hitstun--; if (m_hurtFlash>0) m_hurtFlash--; return; }
    if (m_hurtFlash > 0) m_hurtFlash--;
    if (m_curCD > 0) m_curCD--;
    if (m_freezeLeft > 0) { m_freezeLeft--; return; }
    m_faceLeft = (p->GetPos().x < m_pos.x);  // 朝向玩家

    if (m_stealth > 0) {
        m_stealth--;
        if (m_stealth == 0) {
            // 出现在玩家附近
            float ang = (float)(rand()%360) * 3.1415926f/180;
            m_pos.x = p->GetPos().x + cos(ang) * 80;
            m_pos.y = p->GetPos().y + sin(ang) * 80;
            ClampInLevel();
            m_attackWindup = 30;   // 出手前摇 0.5s
        }
        return;
    }
    if (m_attackWindup > 0) {
        m_attackWindup--;
        if (m_attackWindup == 0) {
            if (CanReach(p->GetPos(), 60)) { p->TakeDamage(20); }
            m_curCD = m_attackCD;
            m_stealth = 90;   // 进入下一轮隐身
        }
        return;
    }
    // 慢慢逼近 + 偶尔隐身
    Vec2 d = DirTo(p->GetPos());
    m_pos.x += d.x * 1.8f;
    m_pos.y += d.y * 1.8f;
    if (m_curCD == 0 && (rand()%240) == 0) {
        m_stealth = 60;
    }
    ClampInLevel();
}

void CAssassin::Draw(CDC* pDC, int frame)
{
    if (m_spawnFrames > 0) {
        DrawSpawnEffect(pDC, (int)m_pos.x, (int)m_pos.y, m_radius + 8);
        return;
    }
    int x = (int)m_pos.x, y = (int)m_pos.y;
    if (m_stealth > 0) {
        // 隐身时只画一个半透明轮廓
        CPen pen(PS_DOT, 1, RGB(120,80,160));
        CPen* po = pDC->SelectObject(&pen);
        CBrush* pn = (CBrush*)pDC->SelectStockObject(NULL_BRUSH);
        pDC->Ellipse(x-12, y-14, x+12, y+14);
        pDC->SelectObject(po); pDC->SelectObject(pn);
        return;
    }
    if (g_sprites.Has(SPR_ASSASSIN)) g_sprites.DrawScaled(pDC, SPR_ASSASSIN, x, y, DrawScale(), m_faceLeft);
    else {
        // 黑披风 + 紫光
        CBrush cloak((m_hurtFlash>0)?RGB(255,255,255):RGB(30,15,45));
        CBrush blade(RGB(180,120,220));
        CBrush eye(RGB(180,80,255));
        CBrush* po = pDC->SelectObject(&cloak);
        pDC->Ellipse(x-12, y-14, x+12, y+14);
        pDC->SelectObject(&eye);
        pDC->Ellipse(x-4, y-6, x-1, y-3);
        pDC->Ellipse(x+1, y-6, x+4, y-3);
        // 双刀
        pDC->SelectObject(&blade);
        pDC->Rectangle(x-15, y, x-13, y+8);
        pDC->Rectangle(x+13, y, x+15, y+8);
        pDC->SelectObject(po);
    }
    if (m_attackWindup > 0) {
        // 前摇红圈
        CPen pen(PS_SOLID, 2, RGB(255,60,60));
        CPen* po = pDC->SelectObject(&pen);
        CBrush* pn = (CBrush*)pDC->SelectStockObject(NULL_BRUSH);
        int r = 60;
        pDC->Ellipse(x-r, y-r, x+r, y+r);
        pDC->SelectObject(po); pDC->SelectObject(pn);
    }
    DrawHpBar(pDC, x, y-20, 36, m_hp, m_maxHP, RGB(180,80,200));
}

// =============================================
// 7. 雾魔 (会传送)
// =============================================
CWraith::CWraith(float x, float y, float dm)
    : CEnemy(ENEMY_WRAITH, x, y, dm), m_teleCD(200), m_teleAnim(0) {}

void CWraith::Update(CPlayer* p, CProjectileMgr* proj)
{
    if (m_spawnFrames > 0) { m_spawnFrames--; return; }
    if (m_hitstun > 0) { m_hitstun--; if (m_hurtFlash>0) m_hurtFlash--; return; }
    if (m_hurtFlash > 0) m_hurtFlash--;
    if (m_curCD > 0) m_curCD--;
    if (m_freezeLeft > 0) { m_freezeLeft--; return; }
    m_faceLeft = (p->GetPos().x < m_pos.x);  // 朝向玩家
    if (m_teleAnim > 0) { m_teleAnim--; return; }
    m_teleCD--;
    if (m_teleCD <= 0) {
        // 传送到玩家身边
        float ang = (float)(rand()%360) * 3.1415926f/180;
        m_pos.x = p->GetPos().x + cos(ang) * 100;
        m_pos.y = p->GetPos().y + sin(ang) * 100;
        if (m_pos.x < 20) m_pos.x = 20;
        if (m_pos.x > LEVEL_W-20) m_pos.x = LEVEL_W-20;
        if (m_pos.y < GROUND_TOP_Y) m_pos.y = (float)GROUND_TOP_Y;
        if (m_pos.y > WIN_H-30) m_pos.y = WIN_H-30;
        m_teleCD = 240;
        m_teleAnim = 15;
    }

    // === 三束蓝色激光齐射: 朝玩家方向 + 上下各偏一束 ===
    m_shootTimer--;
    if (m_shootTimer <= 0) {
        Vec2 d = DirTo(p->GetPos());
        float ang0 = atan2(d.y, d.x);
        // 中束
        proj->SpawnLaser(m_pos, d, 6);
        // 上下偏束(±18°)
        for (int s = -1; s <= 1; s += 2) {
            float a = ang0 + s * 0.32f;
            Vec2 dd(cos(a), sin(a));
            proj->SpawnLaser(m_pos, dd, 6);
        }
        g_audio.PlaySfx(SFX_BOSS_SKILL);  // 激光音效
        m_shootTimer = 160;  // 间隔拉长,给玩家反应/输出时间
    }

    // 缓慢漂向玩家
    Vec2 d = DirTo(p->GetPos());
    m_pos.x += d.x * 1.5f;
    m_pos.y += d.y * 1.5f;
    if (CanReach(p->GetPos(), 50)) { p->TakeDamage(15); m_curCD = m_attackCD; }
    ClampInLevel();
}

void CWraith::Draw(CDC* pDC, int frame)
{
    if (m_spawnFrames > 0) {
        DrawSpawnEffect(pDC, (int)m_pos.x, (int)m_pos.y, m_radius + 8);
        return;
    }
    int x = (int)m_pos.x, y = (int)m_pos.y;
    if (m_teleAnim > 0) {
        CPen pen(PS_SOLID, 2, RGB(180,120,220));
        CPen* po = pDC->SelectObject(&pen);
        CBrush* pn = (CBrush*)pDC->SelectStockObject(NULL_BRUSH);
        int r = (15 - m_teleAnim) * 4;
        pDC->Ellipse(x-r, y-r, x+r, y+r);
        pDC->SelectObject(po); pDC->SelectObject(pn);
        return;
    }
    if (g_sprites.Has(SPR_WRAITH)) g_sprites.DrawScaled(pDC, SPR_WRAITH, x, y, DrawScale(), m_faceLeft);
    else {
        int sway = (int)(sin(frame*0.1) * 3);
        CBrush body((m_hurtFlash>0)?RGB(255,255,255):RGB(110,70,140));
        CBrush eye(RGB(255,255,255));
        CBrush scythe(RGB(160,160,170));
        CBrush* po = pDC->SelectObject(&body);
        // 长袍
        POINT robe[5] = {
            {x-14+sway,y-15}, {x+14+sway,y-15},
            {x+18, y+18}, {x-18, y+22}, {x-14+sway,y-15}
        };
        pDC->Polygon(robe, 5);
        // 眼睛
        pDC->SelectObject(&eye);
        pDC->Ellipse(x-6+sway, y-10, x-2+sway, y-6);
        pDC->Ellipse(x+2+sway, y-10, x+6+sway, y-6);
        // 镰刀
        pDC->SelectObject(&scythe);
        pDC->Rectangle(x+14, y-25, x+16, y+5);
        POINT blade[3] = { {x+16,y-25}, {x+30,y-22}, {x+20,y-12} };
        pDC->Polygon(blade, 3);
        pDC->SelectObject(po);
    }
    DrawHpBar(pDC, x, y-30, 44, m_hp, m_maxHP, RGB(180,80,220));
}

// =============================================
// 8. BOSS 雾核守卫(完整重塑 - 多技能 + 镰刀近战)
// =============================================
CFogCore::CFogCore(float x, float y, float dm)
    : CEnemy(ENEMY_BOSS, x, y, dm),
      m_meleeCD(40), m_sweepCD(120), m_blinkCD(150), m_aoeCD(180),
      m_meleeAnim(0), m_sweepWarn(0), m_sweepActive(0),
      m_blinkAnim(0), m_blinkPhase(0),
      m_aoeWarn(0), m_aoeActive(0),
      m_crossCD(240), m_summonCD(360), m_abyssCD(420), m_rainCD(180),
      m_abyssCenter(0, 0), m_abyssFrames(0),
      m_phase2(FALSE), m_phaseAnim(0), m_berserk(FALSE),
      m_hitsToInvinc(0), m_invincibleLeft(0),
      m_cloakCD(360), m_cloakLeft(0),
      m_strikeTarget(0, 0),
      m_scytheAngle(0), m_floatPhase(0), m_diffMul(dm) {}

// BOSS 冰冻抗性: 阶段1 缩减60%, 阶段2及第二条命完全免疫
void CFogCore::ApplyFreeze(int frames)
{
    if (m_phase >= 2) return;        // 阶段2/3 免疫所有冰冻
    int reduced = frames * 40 / 100; // 阶段1 只受 40% 冻结时间
    m_freezeLeft = max(m_freezeLeft, reduced);
}

void CFogCore::OnHit(int dmg)
{
    // 隐身/无敌/动画期间不掉血
    if (m_invincibleLeft > 0 || m_cloakLeft > 0 || m_phaseAnim > 0) {
        m_hurtFlash = 3;
        return;
    }
    m_hp -= dmg;
    if (m_hp < 0) m_hp = 0;
    m_hurtFlash = 5;
    m_hitsToInvinc++;
    g_audio.PlaySfx(SFX_HIT);

    if (m_hitsToInvinc >= BOSS_INVINCIBLE_TRIGGER) {
        m_hitsToInvinc = 0;
        m_invincibleLeft = BOSS_INVINCIBLE_FRAMES;
    }

    if (m_lifeStage == 1) {
        // ===== 第一条命: 阶段1 → (50%) 阶段2 =====
        if (m_phase == 1 && m_hp * 100 / m_maxHP <= BOSS_PHASE2_HP_PCT) {
            m_phase = 2;
            m_phase2 = TRUE;
            m_phaseTrigger = 2;               // 通知 View 触发阶段2剧情
            m_freezeLeft = 0;
            m_phaseAnim = BOSS_PHASE2_ANIM_FRAMES;
            m_invincibleLeft = BOSS_PHASE2_ANIM_FRAMES;
            m_meleeCD = 35; m_sweepCD = 70; m_blinkCD = 100;
            g_audio.PlaySfx(SFX_BOSS_PHASE);
        }
        // 第一条命 HP 耗尽 → 不死,转入第二条命(阶段3)
        if (m_hp <= 0) {
            m_lifeStage = 2;
            m_phase = 3;
            m_phaseTrigger = 3;               // 通知 View 触发阶段3剧情
            // 第二条命满血(全面加强,但血量适中保证能打过)
            m_maxHP = 800;
            m_hp = m_maxHP;
            m_freezeLeft = 0;
            m_berserk = TRUE;                 // 第二条命直接狂暴
            m_phaseAnim = BOSS_PHASE2_ANIM_FRAMES;
            m_invincibleLeft = BOSS_PHASE2_ANIM_FRAMES;
            // 第三阶段 CD 全面加快
            m_meleeCD = 25; m_sweepCD = 50; m_blinkCD = 70; m_aoeCD = 120;
            m_crossCD = 150; m_summonCD = 240; m_abyssCD = 300; m_rainCD = 120;
            g_audio.PlaySfx(SFX_BOSS_PHASE);
        }
    }
    // 第二条命(阶段3)血到 0 → 真正死亡(基类 IsAlive 判定)
}

void CFogCore::Update(CPlayer* p, CProjectileMgr* proj)
{
    if (m_spawnFrames > 0) { m_spawnFrames--; return; }
    if (m_hitstun > 0) { m_hitstun--; if (m_hurtFlash>0) m_hurtFlash--; return; }
    if (m_hurtFlash > 0) m_hurtFlash--;
    if (m_invincibleLeft > 0) m_invincibleLeft--;
    if (m_phaseAnim > 0) { m_phaseAnim--; return; }
    if (m_freezeLeft > 0) { m_freezeLeft--; return; }
    m_faceLeft = (p->GetPos().x < m_pos.x);  // 朝向玩家

    m_floatPhase += 0.05f;

    // 所有 CD 推进(狂暴态时减得更快)
    float mul = SpeedMul();
    int dec = (int)mul;
    if (mul - dec > (rand() % 100) / 100.0f) dec++;  // 概率多减 1
    m_meleeCD = max(0, m_meleeCD - dec);
    m_sweepCD = max(0, m_sweepCD - dec);
    m_blinkCD = max(0, m_blinkCD - dec);
    m_aoeCD   = max(0, m_aoeCD - dec);
    m_crossCD = max(0, m_crossCD - dec);
    m_summonCD= max(0, m_summonCD- dec);
    m_abyssCD = max(0, m_abyssCD - dec);
    m_rainCD  = max(0, m_rainCD - dec);
    m_cloakCD = max(0, m_cloakCD - dec);

    Vec2 pp = p->GetPos();
    float ddx = pp.x - m_pos.x, ddy = pp.y - m_pos.y;
    float dist = sqrt(ddx*ddx + ddy*ddy);

    // 更新镰刀朝向(始终指向玩家)
    if (dist > 0.5f) m_scytheAngle = atan2(ddy, ddx);

    // === 黑暗深渊持续伤害 ===
    if (m_abyssFrames > 0) {
        m_abyssFrames--;
        float adx = pp.x - m_abyssCenter.x;
        float ady = pp.y - m_abyssCenter.y;
        if (adx*adx + ady*ady < 90*90 && (m_abyssFrames % 30) == 0) {
            p->TakeDamage((int)(8 * m_diffMul));
        }
    }

    // === 二阶段隐身突袭 ===
    if (m_phase2 && m_cloakLeft > 0) {
        m_cloakLeft--;
        float dx2 = m_strikeTarget.x - m_pos.x;
        float dy2 = m_strikeTarget.y - m_pos.y;
        float d2 = sqrt(dx2*dx2 + dy2*dy2);
        if (d2 > 5) {
            m_pos.x += dx2/d2 * 9.0f;
            m_pos.y += dy2/d2 * 9.0f;
        }
        if (m_cloakLeft == 0) {
            // 隐身结束: 范围爆发伤害
            float ex = pp.x - m_pos.x, ey = pp.y - m_pos.y;
            if (ex*ex + ey*ey < 110*110) {
                int dmg = (int)(32 * m_diffMul);
                p->TakeDamage(dmg);
            }
            m_meleeAnim = 15;  // 顺势挥砍
        }
        return;  // 隐身期间不释放别的
    }
    // 进入二阶段后才考虑隐身突袭
    if (m_phase2 && m_cloakCD == 0 && m_blinkAnim == 0 && m_sweepActive == 0) {
        m_strikeTarget = pp;
        m_cloakLeft = 50;
        m_cloakCD = 480;
        g_audio.PlaySfx(SFX_BOSS_SKILL);
        return;
    }

    // === 传送斩(短距闪现) ===
    if (m_blinkAnim > 0) {
        m_blinkAnim--;
        if (m_blinkPhase == 1 && m_blinkAnim == 0) {
            // 消失结束 → 出现在玩家身后
            float ang = m_scytheAngle + 3.1415926f; // 玩家身后
            m_pos.x = pp.x + cos(ang) * 70;
            m_pos.y = pp.y + sin(ang) * 70;
            if (m_pos.x < 30) m_pos.x = 30;
            if (m_pos.x > LEVEL_W-30) m_pos.x = LEVEL_W-30;
            if (m_pos.y < 80) m_pos.y = 80;
            if (m_pos.y > WIN_H-30) m_pos.y = WIN_H-30;
            m_blinkPhase = 2;
            m_blinkAnim = 20;  // 出现后立刻挥砍 0.33 秒
        } else if (m_blinkPhase == 2 && m_blinkAnim == 0) {
            // 挥砍命中检测
            float ex = pp.x - m_pos.x, ey = pp.y - m_pos.y;
            if (ex*ex + ey*ey < 100*100) {
                int dmg = m_phase2 ? 28 : 22;
                p->TakeDamage((int)(dmg * m_diffMul));
            }
            m_blinkPhase = 0;
        }
        return;
    }
    if (m_blinkCD == 0 && dist > 80) {
        m_blinkPhase = 1;
        m_blinkAnim = 25;
        m_blinkCD = m_phase2 ? 300 : 360;
        g_audio.PlaySfx(SFX_BOSS_SKILL);
        return;
    }

    // === 镰刀大横扫(预警+爆发) ===
    if (m_sweepWarn > 0) {
        m_sweepWarn--;
        if (m_sweepWarn == 0) m_sweepActive = 15;
    }
    if (m_sweepActive > 0) {
        m_sweepActive--;
        if (m_sweepActive == 14) {
            // 360° 圆形大范围
            float ex = pp.x - m_pos.x, ey = pp.y - m_pos.y;
            if (ex*ex + ey*ey < 150*150) {
                int dmg = m_phase2 ? 38 : 28;
                p->TakeDamage((int)(dmg * m_diffMul));
            }
        }
        return;
    }
    if (m_sweepCD == 0 && dist < 250) {
        m_sweepWarn = 30;
        m_sweepCD = m_phase2 ? 240 : 280;
        g_audio.PlaySfx(SFX_BOSS_SKILL);
        return;
    }

    // === AOE 雾爆 ===
    int aoeRadius = m_phase2 ? 260 : 220;
    if (m_aoeWarn > 0) {
        m_aoeWarn--;
        if (m_aoeWarn == 0) m_aoeActive = 12;
    }
    if (m_aoeActive > 0) {
        m_aoeActive--;
        if (m_aoeActive == 11) {
            float ex = pp.x - m_pos.x, ey = pp.y - m_pos.y;
            if (ex*ex + ey*ey < aoeRadius*aoeRadius) {
                int dmg = m_weakened ? 18 : (m_phase2 ? 42 : 30);
                p->TakeDamage((int)(dmg * m_diffMul));
            }
        }
    }
    if (m_aoeCD == 0 && m_aoeWarn == 0 && m_aoeActive == 0) {
        m_aoeWarn = m_phase2 ? 45 : 60;
        m_aoeCD = m_phase2 ? 300 : 340;
        g_audio.PlaySfx(SFX_BOSS_SKILL);
    }

    // === 镰刀近战平A ===
    if (m_meleeAnim > 0) m_meleeAnim--;
    if (m_meleeCD == 0 && dist < 90) {
        m_meleeAnim = 12;
        // 命中检测: 前方扇形(简化为前方圆形)
        float ax = m_pos.x + cos(m_scytheAngle) * 50;
        float ay = m_pos.y + sin(m_scytheAngle) * 50;
        float ex = pp.x - ax, ey = pp.y - ay;
        if (ex*ex + ey*ey < 70*70) {
            int dmg = m_phase2 ? 18 : 14;
            p->TakeDamage((int)(dmg * m_diffMul));
        }
        m_meleeCD = m_berserk ? 30 : 45;
    }

    // === BOSS 主动移动: 远离玩家时缓慢逼近,保持压迫 ===
    {
        Vec2 d = DirTo(pp);
        float moveSpeed = m_berserk ? 2.4f : (m_phase2 ? 1.8f : 1.3f);
        if (dist > 70) {
            m_pos.x += d.x * moveSpeed;
            m_pos.y += d.y * moveSpeed;
        }
        ClampInLevel();
    }

    // === 二阶段独有: 十字镰刃 ===
    if (m_phase2 && m_crossCD == 0) {
        // 朝玩家方向 + 上下左右各一发(共5发,穿透型)
        Vec2 dirs[5];
        dirs[0] = Vec2(cos(m_scytheAngle), sin(m_scytheAngle));
        dirs[1] = Vec2(1, 0); dirs[2] = Vec2(-1, 0);
        dirs[3] = Vec2(0, 1); dirs[4] = Vec2(0, -1);
        for (int i = 0; i < 5; ++i) {
            proj->SpawnPoison(m_pos, dirs[i], (int)(18 * m_diffMul));
        }
        m_crossCD = 360;
    }

    // === 二阶段独有: 暗影箭雨 ===
    if (m_phase2 && m_rainCD == 0) {
        for (int i = -2; i <= 2; ++i) {
            float ang = m_scytheAngle + i * 0.18f;
            Vec2 dir(cos(ang), sin(ang));
            proj->SpawnPoison(m_pos, dir, (int)(12 * m_diffMul));
        }
        m_rainCD = 180;
    }

    // === 二阶段独有: 黑暗深渊(玩家脚下) ===
    if (m_phase2 && m_abyssCD == 0 && m_abyssFrames == 0) {
        m_abyssCenter = pp;
        m_abyssFrames = 180;  // 持续 3 秒
        m_abyssCD = 480;
    }
}

// ====================================================
// BOSS 渲染
// ====================================================
void CFogCore::Draw(CDC* pDC, int frame)
{
    if (m_spawnFrames > 0) {
        DrawSpawnEffect(pDC, (int)m_pos.x, (int)m_pos.y, m_radius + 8);
        return;
    }
    int x = (int)m_pos.x;
    int y = (int)m_pos.y + (int)(sin(m_floatPhase) * 5);

    // 二阶段切换动画
    if (m_phaseAnim > 0) {
        float t = 1.0f - (float)m_phaseAnim / BOSS_PHASE2_ANIM_FRAMES;
        int r = (int)(t * 320);
        CPen pen(PS_SOLID, 4, RGB(255, 50, 50));
        CPen* po = pDC->SelectObject(&pen);
        CBrush* pn = (CBrush*)pDC->SelectStockObject(NULL_BRUSH);
        pDC->Ellipse(x-r, y-r, x+r, y+r);
        int r2 = (int)(t * 220);
        CPen pen2(PS_SOLID, 2, RGB(255, 160, 60));
        pDC->SelectObject(&pen2);
        pDC->Ellipse(x-r2, y-r2, x+r2, y+r2);
        if ((m_phaseAnim / 5) % 2 == 0) {
            CBrush flash(RGB(255, 255, 255));
            CBrush* pf = pDC->SelectObject(&flash);
            pDC->Ellipse(x-55, y-55, x+55, y+55);
            pDC->SelectObject(pf);
        }
        pDC->SelectObject(po);
        pDC->SelectObject(pn);
        return;
    }

    // 隐身: 仅画半透明轮廓 + 突袭路径
    if (m_cloakLeft > 0) {
        CPen pen(PS_DASH, 2, RGB(120, 60, 120));
        CPen* po = pDC->SelectObject(&pen);
        CBrush* pn = (CBrush*)pDC->SelectStockObject(NULL_BRUSH);
        pDC->Ellipse(x-50, y-50, x+50, y+50);
        int tx = (int)m_strikeTarget.x;
        int ty = (int)m_strikeTarget.y;
        CPen tp(PS_DOT, 2, RGB(255, 80, 80));
        pDC->SelectObject(&tp);
        pDC->MoveTo(x, y);
        pDC->LineTo(tx, ty);
        CBrush mb(RGB(255, 60, 60));
        CBrush* pmb = pDC->SelectObject(&mb);
        pDC->Ellipse(tx-8, ty-8, tx+8, ty+8);
        pDC->SelectObject(pmb);
        pDC->SelectObject(po);
        pDC->SelectObject(pn);
        return;
    }

    // 传送斩动画
    if (m_blinkAnim > 0) {
        if (m_blinkPhase == 1) {
            // 消失中: 缩小+变透明
            int rr = 40 * m_blinkAnim / 25;
            CPen pen(PS_DASH, 2, RGB(180, 100, 200));
            CPen* po = pDC->SelectObject(&pen);
            CBrush* pn = (CBrush*)pDC->SelectStockObject(NULL_BRUSH);
            pDC->Ellipse(x-rr, y-rr, x+rr, y+rr);
            pDC->SelectObject(po);
            pDC->SelectObject(pn);
            return;
        }
        // m_blinkPhase == 2: 出现挥砍中(继续往下画镰刀)
    }

    // === AOE 预警/爆炸 ===
    int aoeRadius = m_phase2 ? 260 : 220;
    if (m_aoeWarn > 0) {
        CPen pen(PS_DASH, 2, m_phase2 ? RGB(255,40,40) : RGB(255,80,80));
        CPen* po = pDC->SelectObject(&pen);
        CBrush* pn = (CBrush*)pDC->SelectStockObject(NULL_BRUSH);
        pDC->Ellipse(x-aoeRadius, y-aoeRadius, x+aoeRadius, y+aoeRadius);
        pDC->SelectObject(po);
        pDC->SelectObject(pn);
    }
    if (m_aoeActive > 0) {
        CBrush b(m_phase2 ? RGB(255, 100, 100) : RGB(255, 255, 255));
        CBrush* po = pDC->SelectObject(&b);
        pDC->Ellipse(x-aoeRadius, y-aoeRadius, x+aoeRadius, y+aoeRadius);
        pDC->SelectObject(po);
    }

    // === 大横扫预警(脚下旋转圆环) ===
    if (m_sweepWarn > 0) {
        CPen pen(PS_SOLID, 3, RGB(255, 200, 60));
        CPen* po = pDC->SelectObject(&pen);
        CBrush* pn = (CBrush*)pDC->SelectStockObject(NULL_BRUSH);
        pDC->Ellipse(x-150, y-150, x+150, y+150);
        pDC->SelectObject(po);
        pDC->SelectObject(pn);
        // 旋转标记
        float rotAng = (30 - m_sweepWarn) * 0.4f;
        for (int i = 0; i < 4; ++i) {
            float a = rotAng + i * 1.57f;
            int mx = x + (int)(cos(a) * 150);
            int my = y + (int)(sin(a) * 150);
            CBrush mb(RGB(255, 220, 80));
            CBrush* pmb = pDC->SelectObject(&mb);
            pDC->Ellipse(mx-6, my-6, mx+6, my+6);
            pDC->SelectObject(pmb);
        }
    }
    if (m_sweepActive > 0) {
        // 横扫爆发: 黄白圆环
        int t = 15 - m_sweepActive;
        int rr = 60 + t * 7;
        CPen pen(PS_SOLID, 5, RGB(255, 240, 120));
        CPen* po = pDC->SelectObject(&pen);
        CBrush* pn = (CBrush*)pDC->SelectStockObject(NULL_BRUSH);
        pDC->Ellipse(x-rr, y-rr, x+rr, y+rr);
        pDC->SelectObject(po);
        pDC->SelectObject(pn);
    }

    // === 黑暗深渊(玩家脚下) ===
    if (m_abyssFrames > 0) {
        int ax = (int)m_abyssCenter.x;
        int ay = (int)m_abyssCenter.y;
        // 多层圆 + 漩涡
        CBrush b1(RGB(20, 0, 30));
        CBrush* po = pDC->SelectObject(&b1);
        CPen* pe = (CPen*)pDC->SelectStockObject(NULL_PEN);
        pDC->Ellipse(ax-90, ay-90, ax+90, ay+90);
        CBrush b2(RGB(60, 0, 80));
        pDC->SelectObject(&b2);
        pDC->Ellipse(ax-60, ay-60, ax+60, ay+60);
        // 紫色光环
        CPen ring(PS_SOLID, 3, RGB(200, 80, 220));
        pDC->SelectObject(&ring);
        CBrush* pn = (CBrush*)pDC->SelectStockObject(NULL_BRUSH);
        pDC->Ellipse(ax-90, ay-90, ax+90, ay+90);
        // 漩涡粒子
        for (int i = 0; i < 6; ++i) {
            float ang = (m_abyssFrames * 0.1f) + i * 1.05f;
            int px = ax + (int)(cos(ang) * 50);
            int py = ay + (int)(sin(ang) * 50);
            CBrush bp(RGB(255, 180, 255));
            CBrush* pbp = pDC->SelectObject(&bp);
            pDC->Ellipse(px-4, py-4, px+4, py+4);
            pDC->SelectObject(pbp);
        }
        pDC->SelectObject(po);
        pDC->SelectObject(pn);
        pDC->SelectObject(pe);
    }

    // === 无敌护盾 ===
    if (m_invincibleLeft > 0) {
        int pulse = (m_invincibleLeft / 6) % 2;
        CPen pen(PS_SOLID, 4, pulse ? RGB(200, 220, 255) : RGB(120, 180, 255));
        CPen* po = pDC->SelectObject(&pen);
        CBrush* pn = (CBrush*)pDC->SelectStockObject(NULL_BRUSH);
        pDC->Ellipse(x-58, y-58, x+58, y+58);
        pDC->Ellipse(x-66, y-66, x+66, y+66);
        pDC->SelectObject(po);
        pDC->SelectObject(pn);
    }

    // === 狂暴态: 红色火焰光环 ===
    if (m_berserk) {
        for (int i = 0; i < 6; ++i) {
            float ang = (frame * 0.1f) + i * 1.05f;
            int fx = x + (int)(cos(ang) * 55);
            int fy = y + (int)(sin(ang) * 55);
            CBrush fb(RGB(255, 80, 30));
            CBrush* pfb = pDC->SelectObject(&fb);
            CPen* pe2 = (CPen*)pDC->SelectStockObject(NULL_PEN);
            pDC->Ellipse(fx-6, fy-6, fx+6, fy+6);
            pDC->SelectObject(pfb);
            pDC->SelectObject(pe2);
        }
    }

    // === BOSS 本体 ===
    if (g_sprites.Has(SPR_BOSS)) {
        // 二阶段红光晕
        if (m_phase2) {
            CBrush halo(RGB(120, 30, 30));
            CBrush* pH = pDC->SelectObject(&halo);
            CPen* ppen = (CPen*)pDC->SelectStockObject(NULL_PEN);
            pDC->Ellipse(x-56, y-56, x+56, y+56);
            pDC->SelectObject(pH);
            pDC->SelectObject(ppen);
        }
        // 按阶段选择贴图(阶段1/2/3),缺失则回退到 SPR_BOSS
        SpriteID bossSid = SPR_BOSS;
        if (m_phase == 1 && g_sprites.Has(SPR_BOSS1)) bossSid = SPR_BOSS1;
        else if (m_phase == 2 && g_sprites.Has(SPR_BOSS2)) bossSid = SPR_BOSS2;
        else if (m_phase == 3 && g_sprites.Has(SPR_BOSS3)) bossSid = SPR_BOSS3;
        // 第三阶段(第二条命)更大更威严
        float bossScale = (m_phase == 3) ? 2.0f : 1.6f;
        g_sprites.DrawScaled(pDC, bossSid, x, y, bossScale, m_faceLeft);
    } else {
        COLORREF outer = m_phase2 ? RGB(140, 30, 60)
                       : (m_weakened ? RGB(120,170,190) : RGB(60,100,140));
        CBrush ob(outer), ib((m_hurtFlash>0)?RGB(255,255,255)
                            : (m_phase2 ? RGB(220, 100, 120) : RGB(180,220,240)));
        CBrush co(m_phase2 ? RGB(255, 200, 80) : RGB(255,255,180));
        CBrush* po = pDC->SelectObject(&ob);
        pDC->Ellipse(x-50, y-50, x+50, y+50);
        pDC->SelectObject(&ib);
        pDC->Ellipse(x-32, y-32, x+32, y+32);
        pDC->SelectObject(&co);
        pDC->Ellipse(x-12, y-12, x+12, y+12);
        pDC->SelectObject(po);
    }

    // 镰刀已由 boss.png 贴图自带,不再用 GDI 绘制
    // BOSS 血条由 YoulinView::DrawHUD 在屏幕坐标绘制
}
