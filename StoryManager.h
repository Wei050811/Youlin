// StoryManager.h
#pragma once
#include "GameTypes.h"

class CStoryManager
{
public:
    CStoryManager();
    void Load(const StorySlide* slides, int count);
    void Update();
    BOOL NextSlide();        // 返回 TRUE 表示已结束(无下一页)
    void RestartAnim();
    BOOL IsFinished() const { return m_finished; }
    void Draw(CDC* pDC);

    // 便捷加载 + 推进接口
    void LoadBegin(Chapter c);
    void LoadEnd(Chapter c);
    void LoadEndingTrue();
    void LoadEndingNormal();
    BOOL Next() { return NextSlide(); }  // 兼容旧调用

private:
    const StorySlide* m_slides;
    int  m_count, m_index;
    int  m_charShown, m_charDelay, m_frame, m_fadeIn;
    BOOL m_finished;
};
