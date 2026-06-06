// Projectile.cpp
#include "pch.h"
#include "Projectile.h"
#include "Player.h"
#include "Enemy.h"
#include "SpriteManager.h"
#include <math.h>

CProjectileMgr::CProjectileMgr() {}
void CProjectileMgr::Clear() { m_list.clear(); }

Vec2 CProjectileMgr::FaceToDir(int face)
{
    switch (face) {
    case 0: return Vec2(0, 1);   // 下
    case 1: return Vec2(0,-1);   // 上
    case 2: return Vec2(-1, 0);  // 左
    default: return Vec2(1, 0);  // 右
    }
}

void CProjectileMgr::SpawnFireball(Vec2 from, int face, int dmg)
{
    Vec2 d = FaceToDir(face);
    Projectile p;
    p.type = PROJ_FIREBALL;
    p.pos  = from;
    p.vel  = Vec2(d.x*9, d.y*9);
    p.damage = dmg;
    p.life = 42;   // 缩短飞行距离(约 380 像素)
    p.fromPlayer = TRUE;
    p.pierce = FALSE;
    p.hitMask = 0;
    p.alive = TRUE;
    p.freezeFrames = 0;
    m_list.push_back(p);
}

void CProjectileMgr::SpawnIce(Vec2 from, int face, int dmg)
{
    Vec2 d = FaceToDir(face);
    Projectile p;
    p.type = PROJ_ICE;
    p.pos  = from;
    p.vel  = Vec2(d.x*8, d.y*8);
    p.damage = dmg;
    p.life = 90;
    p.fromPlayer = TRUE;
    p.pierce = FALSE;
    p.hitMask = 0;
    p.alive = TRUE;
    p.freezeFrames = 90;   // 冻 1.5 秒
    m_list.push_back(p);
}

void CProjectileMgr::SpawnLightning(Vec2 from, int face, int dmg)
{
    Vec2 d = FaceToDir(face);
    Projectile p;
    p.type = PROJ_LIGHTNING;
    p.pos  = from;
    p.vel  = Vec2(d.x*12, d.y*12);
    p.damage = dmg;
    p.life = 60;
    p.fromPlayer = TRUE;
    p.pierce = TRUE;
    p.hitMask = 0;
    p.alive = TRUE;
    p.freezeFrames = 0;
    m_list.push_back(p);
}

void CProjectileMgr::SpawnPoison(Vec2 from, Vec2 dir, int dmg)
{
    float len = sqrt(dir.x*dir.x + dir.y*dir.y);
    if (len < 0.1f) { dir.x = 1; dir.y = 0; len = 1; }
    Projectile p;
    p.type = PROJ_POISON;
    p.pos  = from;
    p.vel  = Vec2(dir.x/len*5, dir.y/len*5);
    p.damage = dmg;
    p.life = 100;
    p.fromPlayer = FALSE;
    p.pierce = FALSE;
    p.hitMask = 0;
    p.alive = TRUE;
    p.freezeFrames = 0;
    m_list.push_back(p);
}

void CProjectileMgr::SpawnSeed(Vec2 from, Vec2 dir, int dmg)
{
    float len = sqrt(dir.x*dir.x + dir.y*dir.y);
    if (len < 0.1f) { dir.x = 1; dir.y = 0; len = 1; }
    Projectile p;
    p.type = PROJ_SEED;
    p.pos  = from;
    p.vel  = Vec2(dir.x/len*4.5f, dir.y/len*4.5f);
    p.damage = dmg;
    p.life = 130;
    p.fromPlayer = FALSE;
    p.pierce = FALSE;
    p.hitMask = 0;
    p.alive = TRUE;
    p.freezeFrames = 0;
    m_list.push_back(p);
}

void CProjectileMgr::SpawnLaser(Vec2 from, Vec2 dir, int dmg)
{
    float len = sqrt(dir.x*dir.x + dir.y*dir.y);
    if (len < 0.1f) { dir.x = 1; dir.y = 0; len = 1; }
    Projectile p;
    p.type = PROJ_LASER;
    p.pos  = from;
    p.vel  = Vec2(dir.x/len, dir.y/len);  // 方向(归一化),激光不移动
    p.damage = dmg;
    p.life = 18;       // 激光持续 18 帧
    p.fromPlayer = FALSE;
    p.pierce = TRUE;   // 贯穿
    p.hitMask = 0;
    p.alive = TRUE;
    p.freezeFrames = 0;
    m_list.push_back(p);
}

void CProjectileMgr::Update(CPlayer* player, std::vector<CEnemy*>& enemies)
{
    Vec2 pp = player->GetPos();
    for (auto& p : m_list) {
        if (!p.alive) continue;
        if (p.type == PROJ_LASER) {
            // 激光不移动,只递减寿命; 命中判定用点到射线距离
            p.life--;
            if (p.life <= 0) { p.alive = FALSE; continue; }
            // 激光长度缩短到 550(约半屏,玩家可躲); 每束激光整个生命周期只造成 1 次伤害
            float relx = pp.x - p.pos.x, rely = pp.y - p.pos.y;
            float proj = relx * p.vel.x + rely * p.vel.y;
            if (proj > 0 && proj < 550 && p.hitMask == 0) {
                float perpx = relx - proj * p.vel.x;
                float perpy = rely - proj * p.vel.y;
                float perp = sqrt(perpx*perpx + perpy*perpy);
                if (perp < 18) {
                    player->TakeDamage(p.damage);
                    p.hitMask = 1;  // 标记已伤,本束激光不再重复伤害
                }
            }
            continue;
        }
        p.pos.x += p.vel.x;
        p.pos.y += p.vel.y;
        p.life--;
        if (p.life <= 0 ||
            p.pos.x < -20 || p.pos.x > LEVEL_W+20 ||
            p.pos.y < 50  || p.pos.y > WIN_H+20) {
            p.alive = FALSE;
            continue;
        }
        // 碰撞
        if (p.fromPlayer) {
            // 击中敌人
            for (auto* e : enemies) {
                if (!e->IsAlive()) continue;
                int id = e->GetId();
                if (p.pierce && (p.hitMask & (1<<id))) continue;
                Vec2 ep = e->GetPos();
                float dx = ep.x - p.pos.x;
                float dy = ep.y - p.pos.y;
                float r  = (float)(e->GetRadius() + 8);
                if (dx*dx + dy*dy <= r*r) {
                    // 出生预警期间敌人完全免疫
                    if (e->IsSpawning()) continue;
                    // BOSS 用自定义命中逻辑(无敌期反弹)
                    CFogCore* boss = dynamic_cast<CFogCore*>(e);
                    if (boss) {
                        boss->OnHit(p.damage);
                    } else {
                        e->TakeDamage(p.damage);
                    }
                    // 火球(普攻)命中时吸血
                    if (p.type == PROJ_FIREBALL) player->DoLifesteal(p.damage);
                    if (p.freezeFrames > 0)
                        e->ApplyFreeze(p.freezeFrames);
                    if (!e->IsAlive())
                        player->AddCoinByMul(e->GetCoinDrop());
                    if (p.pierce) {
                        p.hitMask |= (1<<id);
                    } else {
                        p.alive = FALSE;
                        break;
                    }
                }
            }
        } else {
            // 击中玩家
            float dx = pp.x - p.pos.x;
            float dy = pp.y - p.pos.y;
            if (dx*dx + dy*dy <= 20*20) {
                player->TakeDamage(p.damage);
                p.alive = FALSE;
            }
        }
    }
    // 清理
    m_list.erase(std::remove_if(m_list.begin(), m_list.end(),
        [](const Projectile& p){ return !p.alive; }), m_list.end());
}

void CProjectileMgr::Draw(CDC* pDC)
{
    for (auto& p : m_list) {
        if (!p.alive) continue;
        int x = (int)p.pos.x, y = (int)p.pos.y;

        // 蓝色激光: 沿方向画一条发光粗线
        if (p.type == PROJ_LASER) {
            int ex = x + (int)(p.vel.x * 550);
            int ey = y + (int)(p.vel.y * 550);
            // 外层光晕(淡蓝粗)
            CPen glow(PS_SOLID, 16, RGB(80, 140, 255));
            CPen* pg = pDC->SelectObject(&glow);
            pDC->MoveTo(x, y); pDC->LineTo(ex, ey);
            // 中层
            CPen mid(PS_SOLID, 8, RGB(60, 180, 255));
            pDC->SelectObject(&mid);
            pDC->MoveTo(x, y); pDC->LineTo(ex, ey);
            // 核心(亮白蓝)
            CPen core(PS_SOLID, 3, RGB(220, 245, 255));
            pDC->SelectObject(&core);
            pDC->MoveTo(x, y); pDC->LineTo(ex, ey);
            pDC->SelectObject(pg);
            continue;
        }

        SpriteID sid = SPR_FIREBALL;
        COLORREF fallback = RGB(255,140,40);
        int radius = 8;
        switch (p.type) {
        case PROJ_FIREBALL:  sid = SPR_FIREBALL;  fallback = RGB(255,140,40); radius = 8; break;
        case PROJ_ICE:       sid = SPR_ICESHARD;  fallback = RGB(140,220,255); radius = 8; break;
        case PROJ_LIGHTNING: sid = SPR_LIGHTNING; fallback = RGB(200,160,255); radius = 8; break;
        case PROJ_POISON:    sid = SPR_POISON;    fallback = RGB(80,200,80);  radius = 6; break;
        case PROJ_SEED:      sid = SPR_SEED;      fallback = RGB(120,80,40);  radius = 6; break;
        }
        if (g_sprites.Has(sid)) {
            g_sprites.Draw(pDC, sid, x, y);
        } else {
            CBrush b(fallback);
            CBrush* pOld = pDC->SelectObject(&b);
            pDC->Ellipse(x-radius, y-radius, x+radius, y+radius);
            // 高光
            CBrush hl(RGB(255,255,200));
            pDC->SelectObject(&hl);
            pDC->Ellipse(x-3, y-3, x+1, y+1);
            pDC->SelectObject(pOld);
        }
        // 闪电拖尾
        if (p.type == PROJ_LIGHTNING) {
            CPen pen(PS_SOLID, 2, RGB(200,160,255));
            CPen* pOldP = pDC->SelectObject(&pen);
            int tx = x - (int)p.vel.x*2;
            int ty = y - (int)p.vel.y*2;
            pDC->MoveTo(x, y); pDC->LineTo(tx, ty);
            pDC->SelectObject(pOldP);
        }
    }
}
