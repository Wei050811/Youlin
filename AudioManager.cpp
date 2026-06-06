// AudioManager.cpp
#include "pch.h"
#include "AudioManager.h"
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")

CAudioManager g_audio;

// 文件路径表
static const TCHAR* kSfxFiles[SFX_COUNT] = {
    _T("audio\\sfx_click.wav"),
    _T("audio\\sfx_hit.wav"),
    _T("audio\\sfx_fireball.wav"),
    _T("audio\\sfx_skill_ice.wav"),
    _T("audio\\sfx_skill_lightning.wav"),
    _T("audio\\sfx_skill_storm.wav"),
    _T("audio\\sfx_skill_heal.wav"),
    _T("audio\\sfx_dash.wav"),
    _T("audio\\sfx_boss_skill.wav"),
    _T("audio\\sfx_boss_phase.wav"),
    _T("audio\\sfx_typing.wav"),
};
static const TCHAR* kBgmFiles[BGM_COUNT] = {
    _T("audio\\bgm_menu.wav"),
    _T("audio\\bgm_ch1.wav"),
    _T("audio\\bgm_ch2.wav"),
    _T("audio\\bgm_ch3.wav"),
    _T("audio\\bgm_ch4.wav"),
};

CAudioManager::CAudioManager()
    : m_enabled(TRUE), m_curBgm((BgmID)-1), m_sfxChannel(0)
{
    ZeroMemory(m_sfxLoaded, sizeof(m_sfxLoaded));
    ZeroMemory(m_bgmLoaded, sizeof(m_bgmLoaded));
}

CAudioManager::~CAudioManager() { Shutdown(); }

static BOOL FileExists(const TCHAR* path)
{
    DWORD a = GetFileAttributes(path);
    return (a != INVALID_FILE_ATTRIBUTES) && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

void CAudioManager::Init()
{
    TCHAR cmd[512];
    // 每个 SFX 我们用 3 个并行通道,避免快速连发被截断
    for (int i = 0; i < SFX_COUNT; ++i) {
        if (!FileExists(kSfxFiles[i])) continue;
        for (int ch = 0; ch < 3; ++ch) {
            CString alias;
            alias.Format(_T("sfx_%d_%d"), i, ch);
            _stprintf_s(cmd, _T("open \"%s\" type waveaudio alias %s"),
                        kSfxFiles[i], (LPCTSTR)alias);
            mciSendString(cmd, NULL, 0, NULL);
        }
        m_sfxLoaded[i] = TRUE;
        m_sfxAlias[i].Format(_T("sfx_%d"), i);
    }
    // BGM 单通道,支持 repeat
    for (int i = 0; i < BGM_COUNT; ++i) {
        if (!FileExists(kBgmFiles[i])) continue;
        CString alias;
        alias.Format(_T("bgm_%d"), i);
        _stprintf_s(cmd, _T("open \"%s\" type mpegvideo alias %s"),
                    kBgmFiles[i], (LPCTSTR)alias);
        if (mciSendString(cmd, NULL, 0, NULL) != 0) {
            // mpegvideo 失败时退回 waveaudio
            _stprintf_s(cmd, _T("open \"%s\" type waveaudio alias %s"),
                        kBgmFiles[i], (LPCTSTR)alias);
            if (mciSendString(cmd, NULL, 0, NULL) != 0) continue;
        }
        m_bgmLoaded[i] = TRUE;
        m_bgmAlias[i] = alias;
    }
}

void CAudioManager::Shutdown()
{
    TCHAR cmd[256];
    for (int i = 0; i < SFX_COUNT; ++i) {
        if (!m_sfxLoaded[i]) continue;
        for (int ch = 0; ch < 3; ++ch) {
            _stprintf_s(cmd, _T("close sfx_%d_%d"), i, ch);
            mciSendString(cmd, NULL, 0, NULL);
        }
        m_sfxLoaded[i] = FALSE;
    }
    for (int i = 0; i < BGM_COUNT; ++i) {
        if (!m_bgmLoaded[i]) continue;
        _stprintf_s(cmd, _T("close bgm_%d"), i);
        mciSendString(cmd, NULL, 0, NULL);
        m_bgmLoaded[i] = FALSE;
    }
}

void CAudioManager::PlaySfx(SfxID id)
{
    if (!m_enabled || id < 0 || id >= SFX_COUNT || !m_sfxLoaded[id]) return;
    // 轮转通道
    m_sfxChannel = (m_sfxChannel + 1) % 3;
    TCHAR cmd[256];
    // 从头播放
    _stprintf_s(cmd, _T("seek sfx_%d_%d to start"), id, m_sfxChannel);
    mciSendString(cmd, NULL, 0, NULL);
    _stprintf_s(cmd, _T("play sfx_%d_%d"), id, m_sfxChannel);
    mciSendString(cmd, NULL, 0, NULL);
}

void CAudioManager::PlayBgm(BgmID id)
{
    if (!m_enabled || id < 0 || id >= BGM_COUNT) return;
    if (m_curBgm == id) return; // 已经在播
    StopBgm();
    if (!m_bgmLoaded[id]) { m_curBgm = id; return; }
    TCHAR cmd[256];
    _stprintf_s(cmd, _T("seek bgm_%d to start"), id);
    mciSendString(cmd, NULL, 0, NULL);
    _stprintf_s(cmd, _T("play bgm_%d repeat"), id);
    mciSendString(cmd, NULL, 0, NULL);
    m_curBgm = id;
}

void CAudioManager::StopBgm()
{
    if (m_curBgm < 0) return;
    TCHAR cmd[256];
    _stprintf_s(cmd, _T("stop bgm_%d"), (int)m_curBgm);
    mciSendString(cmd, NULL, 0, NULL);
    m_curBgm = (BgmID)-1;
}
