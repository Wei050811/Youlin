// AudioManager.h
// 简单 wav 音效 / BGM 管理器(基于 mciSendString 实现多通道)
#pragma once
#include "GameTypes.h"

enum SfxID {
    SFX_CLICK = 0,
    SFX_HIT,
    SFX_FIREBALL,
    SFX_SKILL_ICE,
    SFX_SKILL_LIGHTNING,
    SFX_SKILL_STORM,
    SFX_SKILL_HEAL,
    SFX_DASH,
    SFX_BOSS_SKILL,
    SFX_BOSS_PHASE,
    SFX_TYPING,
    SFX_COUNT
};

enum BgmID {
    BGM_MENU = 0,
    BGM_CH1, BGM_CH2, BGM_CH3, BGM_CH4,
    BGM_COUNT
};

class CAudioManager
{
public:
    CAudioManager();
    ~CAudioManager();

    void Init();              // 启动时调用,扫描 audio/ 目录
    void Shutdown();          // 退出时释放

    void PlaySfx(SfxID id);   // 播放音效(不阻塞)
    void PlayBgm(BgmID id);   // 播放 BGM 循环
    void StopBgm();

    void SetEnabled(BOOL on) { m_enabled = on; }
    BOOL IsEnabled() const { return m_enabled; }

private:
    BOOL m_enabled;
    BgmID m_curBgm;
    BOOL m_sfxLoaded[SFX_COUNT];
    BOOL m_bgmLoaded[BGM_COUNT];
    CString m_sfxAlias[SFX_COUNT];
    CString m_bgmAlias[BGM_COUNT];
    int m_sfxChannel;  // 轮转通道编号
};

extern CAudioManager g_audio;
