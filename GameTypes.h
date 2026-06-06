// GameTypes.h
// 《幽林守夜人》全局类型、常量、剧情文本
#pragma once
#include <vector>
#include <afxwin.h>

// ===================== 窗口 =====================
const int WIN_W = 1280;   // 16:9 宽屏(匹配现代显示器,消除黑边/变形)
const int WIN_H = 720;
// 地面带顶部 Y(失落城堡式: 角色在地面带内活动,上方是背景)
const int GROUND_TOP_Y = (int)(720 * 0.62f);  // = 446

// ===================== 关卡 =====================
const int  LEVEL_W = 3000;     // 关卡总宽度(像素),约 2.34 屏
const int  CAMERA_DEADZONE = 80;

// ===================== 波次配置 =====================
// 每章总波次数(最后一波是 BOSS 时其中)
const int CHAPTER_WAVES[4] = { 3, 3, 4, 3 };
// 两波之间间隔(帧)
const int WAVE_INTERVAL_FRAMES = 90;

// ===================== 游戏状态 =====================
enum GameState {
    STATE_MENU,             // 主菜单
    STATE_DIFFICULTY,       // 难度选择
    STATE_LOAD,             // 存档列表
    STATE_STORY_BEGIN,      // 章节开头剧情
    STATE_PLAYING,          // 战斗
    STATE_STORY_MID,        // 关卡内剧情触发(对话)
    STATE_REWARD,           // 通关翻牌奖励
    STATE_STORY_END,        // 章节结尾剧情
    STATE_SHOP,             // 商店
    STATE_PAUSE,            // 暂停菜单
    STATE_VICTORY,          // 通关
    STATE_GAMEOVER,         // 死亡
    STATE_ENDING_TRUE,
    STATE_ENDING_NORMAL
};

// ===================== 章节 =====================
enum Chapter {
    CHAP_1 = 0,  // 迷途雾影
    CHAP_2 = 1,  // 古树拦道
    CHAP_3 = 2,  // 月下逐影
    CHAP_4 = 3,  // 雾落终章
    CHAP_COUNT = 4
};

// ===================== 难度 =====================
enum Difficulty {
    DIFF_EASY = 0,    // 敌人 HP/伤害 70%
    DIFF_NORMAL = 1,
    DIFF_HARD = 2     // 敌人 HP/伤害 140%
};

// ===================== 敌人类型(扩展到8种) =====================
enum EnemyType {
    ENEMY_GHOST,       // 1章 游荡幽影  - 巡逻
    ENEMY_TREE,        // 2章 路障树精  - 镇守
    ENEMY_BAT,         // 2章 暗夜蝙蝠  - 飞行远程
    ENEMY_FOX,         // 3章 追月灵狐  - 追踪
    ENEMY_WOLF,        // 3章 狂暴狼影  - 冲刺
    ENEMY_ASSASSIN,    // 3章 影刺客    - 突袭
    ENEMY_WRAITH,      // 4章 雾魔     - 传送
    ENEMY_BOSS         // 4章 雾核守卫  - BOSS
};

// ===================== 投射物类型 =====================
enum ProjectileType {
    PROJ_FIREBALL,     // 玩家普攻火球
    PROJ_ICE,          // 技能1 冰锥
    PROJ_LIGHTNING,    // 技能2 闪电
    PROJ_POISON,       // 蝙蝠毒液
    PROJ_SEED,         // 树妖种子
    PROJ_LASER         // 幽灵蓝色激光
};

// ===================== 技能类型 =====================
enum SkillType {
    SKILL_ICE = 0,        // 冰锥术：单体伤害+冻结
    SKILL_LIGHTNING = 1,  // 闪电链：穿透直线
    SKILL_STORM = 2,      // 烈焰风暴：以自身为中心 AOE
    SKILL_NOVA = 3,       // 奥术爆发：超大范围高伤害(取代治疗)
    SKILL_COUNT = 4
};

// ===================== 被动类型 =====================
enum PassiveType {
    PASS_HP = 0,       // 最大生命
    PASS_MP = 1,       // 最大法力
    PASS_ATK = 2,      // 普攻伤害
    PASS_REGEN = 3,    // 法力回复速度
    PASS_DODGE = 4,    // 闪避(被攻击有几率免伤)
    PASS_LIFESTEAL = 5,// 吸血(普攻伤害转化为 HP)
    PASS_DEFENSE = 6,  // 防御力(减少受到的伤害)
    PASS_COUNT = 7
};

// ===================== 奖励类型 =====================
enum RewardType {
    REWARD_HEAL = 0,     // 立即回 HP (随机 40~80)
    REWARD_MANA,         // 立即回 MP (随机 30~60)
    REWARD_COIN,         // 金币 (随机 80~180)
    REWARD_PERMA_ATK,    // 永久 ATK + (随机 4~10)
    REWARD_PERMA_HP,     // 永久最大 HP + (随机 20~40)
    REWARD_PERMA_MP,     // 永久最大 MP + (随机 15~30)
    REWARD_PERMA_DEF,    // 永久防御 + (随机 2~5)
    REWARD_POTION,       // 红药水 ×N (随机 1~3)
    REWARD_MANAPOTION,   // 蓝药水 ×N (随机 1~3)
    REWARD_REVIVE,       // 复活卷轴 ×1
    REWARD_KIND_COUNT
};

struct RewardItem {
    RewardType type;
    int value;
};

// ===================== 暴击 / 吸血 =====================
const int CRIT_CHANCE_PCT = 10;   // 普攻 10% 暴击率
const float CRIT_MULTIPLIER = 2.0f;
// 吸血等级带来的回血百分比(0=未升级,1-3级)
const int PASS_LIFESTEAL_PCT[4] = { 0, 5, 10, 15 };

// ===================== 道具类型 =====================
enum ItemType {
    ITEM_POTION = 0,   // 红药水 30 金币 回 50 HP
    ITEM_MANA   = 1,   // 蓝药水 25 金币 回 40 MP
    ITEM_AMULET = 2,   // 护符   50 金币 8s 免追
    ITEM_SCROLL = 3,   // 卷轴   80 金币 弱化 BOSS
    ITEM_REVIVE = 4,   // 复活卷轴 150 金币 死亡时自动复活(50% HP)
    ITEM_COUNT
};

// ===================== 价格 =====================
const int PRICE_POTION = 30;
const int PRICE_MANA   = 25;
const int PRICE_AMULET = 50;
const int PRICE_SCROLL = 80;
const int PRICE_REVIVE = 150;

// 技能升级价格表：第 1/2/3 级
const int PRICE_SKILL_LV[3] = { 80, 150, 250 };
// 被动升级价格表：第 1/2/3 级
const int PRICE_PASS_LV[3]  = { 60, 120, 200 };

// ===================== 玩家初始数据 =====================
const int  BASE_HP        = 100;
const int  BASE_MP        = 80;
const int  BASE_ATK       = 18;
const int  PLAYER_SPEED   = 4;
const int  PLAYER_ATK_CD  = 18;       // 普攻冷却(帧)

// ===================== 技能配置 =====================
// 索引对应 SkillType
const int SKILL_MP_COST[4] = { 15, 25, 40, 30 };
const int SKILL_CD[4]      = { 30, 60, 120, 90 };  // 冷却(帧)
// 等级 0/1/2/3 的伤害
const int SKILL_VAL_LV[4][4] = {
    { 25, 35, 50, 70 },     // 冰锥伤害
    { 20, 28, 40, 55 },     // 闪电伤害(每个敌人)
    { 35, 50, 70, 95 },     // 风暴伤害
    { 60, 85, 120, 160 }    // 奥术爆发(超大范围高伤害)
};

// 被动升级带来的提升 (0=未升级, 1-3级)
const int PASS_HP_BONUS[4]    = { 0, 30, 70, 120 };
const int PASS_MP_BONUS[4]    = { 0, 20, 50, 90 };
const int PASS_ATK_BONUS[4]   = { 0, 5,  12, 22 };
const int PASS_REGEN_BONUS[4] = { 0, 1,  2,  4  }; // 每秒额外回 MP
const int PASS_DODGE_PCT[4]   = { 0, 8, 16, 25 }; // 闪避几率(%)
const int PASS_DEFENSE_VAL[4] = { 0, 3,  7, 12 }; // 防御力(每次受击减伤固定值)
// 吸血百分比改在 PASS_LIFESTEAL_PCT (上面已定义)

// ===================== 难度配置 =====================
struct DifficultyConfig {
    float enemyHP;       // 敌人血量系数
    float enemyDmg;      // 敌人伤害系数
    float enemySpeed;    // 敌人移动速度系数
    float enemyAtkFreq;  // 敌人攻击频率系数(越大攻击越频繁,CD越短)
    float coinMul;       // 金币掉落系数
    int   extraWave;     // 每章额外波次数量
};
// 简单 / 普通 / 困难
const DifficultyConfig DIFF_CONFIG[3] = {
    { 0.65f, 0.60f, 0.85f, 0.80f, 1.6f, 0 },  // 简单:敌人弱/慢/攻击少,金币最多
    { 1.00f, 1.00f, 1.00f, 1.00f, 1.5f, 0 },  // 普通
    { 1.45f, 1.40f, 1.25f, 1.40f, 2.0f, 1 },  // 困难:敌人强/快/攻击频繁/多刷一波,金币翻倍
};

// ===================== 出生特效 =====================
const int ENEMY_SPAWN_FRAMES = 60; // 1 秒预警:不能移动,不能受击,显示召唤圆+粒子

// ===================== BOSS =====================
const int BOSS_INVINCIBLE_TRIGGER = 8;   // 命中 8 次进入 3 秒无敌
const int BOSS_INVINCIBLE_FRAMES  = 180; // 3 秒
const int BOSS_PHASE2_HP_PCT      = 50;  // 50% 血量进入二阶段
const int BOSS_PHASE2_ANIM_FRAMES = 120; // 二阶段切换动画 2 秒
const int BOSS_BERSERK_HP_PCT     = 30;  // 30% 血量进入狂暴态(攻速提升)

// ===================== 玩家冲刺 =====================
const int DASH_FRAMES   = 15;   // 冲刺持续 0.25 秒(15 帧)
const int DASH_CD       = 180;  // 冲刺冷却 3 秒
const int DASH_DISTANCE = 8;    // 冲刺每帧移动距离(15帧 × 8 = 120 像素)

// ===================== 工具结构 =====================
struct Vec2 {
    float x, y;
    Vec2(float a=0,float b=0):x(a),y(b){}
};

// ===================== 剧情幻灯片 =====================
enum StorySpeaker {
    SPEAKER_NARRATION = 0,  // 旁白(不移动镜头)
    SPEAKER_PLAYER,         // 主角说话(镜头移到主角)
    SPEAKER_ENEMY,          // 小怪说话(镜头移到最近的怪)
    SPEAKER_BOSS            // BOSS 说话(镜头移到 BOSS)
};

struct StorySlide {
    CString title;
    CString content;
    int bgIndex;
};

// 关卡内对话(轻量,只有标题+内容+说话者),不带背景
struct DialogLine {
    StorySpeaker speaker;
    CString name;       // 显示的说话者名字
    CString content;    // 对话内容
};

namespace Story {

    static const StorySlide CH1_BEGIN[] = {
        { _T("序章 · 月落幽林"),
          _T("古老的月落幽林，原本四季安宁。\n\n直到近期……夜雾泛滥，\n沉睡的灵怪被惊扰苏醒。"), 0 },
        { _T("守夜法师"),
          _T("你是村里最后的守夜法师，\n手握月辉法杖，掌控四系秘咒。\n\n你的使命：\n净化迷雾、安抚灵怪、重启长夜。"), 0 },
        { _T("第一章 · 迷途雾影"),
          _T("浅雾弥漫的林间小径上，\n无数游荡幽影正漫无目的地徘徊。\n\n它们并非邪恶，\n只是被迷雾迷惑了心智。"), 0 },
        { _T("操作提示"),
          _T("WASD 移动   空格/鼠标 攻击\nJ/K/L/I 释放四技能   B 商店\n\n按 [回车] 进入战斗"), 0 },
    };
    static const StorySlide CH1_END[] = {
        { _T("第一章 · 收尾"),
          _T("迷途的雾气归于平静。\n\n林间小径重新显现，\n你攒下了第一份守夜酬劳。"), 0 },
    };

    static const StorySlide CH2_BEGIN[] = {
        { _T("第二章 · 古树拦道"),
          _T("深入森林后，古老树精扎根要道。\n暗夜蝙蝠从树冠盘旋俯冲。\n\n这里不再是单一威胁——\n你需要同时应对近战与远程。"), 1 },
        { _T("战斗提示"),
          _T("蝙蝠会发射毒液，记得走位躲避。\n树精血厚但不动，先杀蝙蝠再清树精。\n\n按 [回车] 进入战斗"), 1 },
    };
    static const StorySlide CH2_END[] = {
        { _T("第二章 · 收尾"),
          _T("固执的古树放下戒备，\n蝙蝠遁回林间深处。\n\n尘封的深林通路向你敞开。"), 1 },
    };

    static const StorySlide CH3_BEGIN[] = {
        { _T("第三章 · 月下逐影"),
          _T("森林深处的猎手们苏醒了——\n机敏的灵狐、狂暴的狼影、潜伏的影刺客。\n\n这是整片森林最凶险的猎场。"), 2 },
        { _T("战斗提示"),
          _T("· 灵狐追踪你\n· 狼影会突进冲刺\n· 影刺客随机突袭\n\n善用静心护符，注意 MP 回复。\n按 [回车] 进入战斗"), 2 },
    };
    static const StorySlide CH3_END[] = {
        { _T("第三章 · 收尾"),
          _T("月下残影散去，林间躁动平息。\n\n迷雾源头，已然临近。"), 2 },
    };

    static const StorySlide CH4_BEGIN[] = {
        { _T("第四章 · 雾落终章"),
          _T("森林最中心，矗立着迷雾的本体——\n雾核守卫，以及它召唤的雾魔。\n\n所有混乱的源头都在此处。"), 3 },
        { _T("最终战提示"),
          _T("BOSS 会释放范围 AOE，看到预警圈立刻远离。\n雾魔会传送，注意背后。\n\n卷轴可弱化 BOSS 防御。\n按 [回车] 进入战斗"), 3 },
    };
    static const StorySlide ENDING_TRUE[] = {
        { _T("真结局 · 幽林永明"),
          _T("你以守夜人的意志，\n净化了雾核本源。\n\n漫天夜雾尽数消散，\n幽林恢复百年安守。\n\n生灵归静，山林归序。"), 3 },
    };
    static const StorySlide ENDING_NORMAL[] = {
        { _T("普通结局 · 守夜未终"),
          _T("迷雾被暂时驱散，\n但力量并未完全根除。\n\n森林暂时归于平静，\n夜色依旧暗藏浮动。"), 3 },
    };


    // ============ 关卡内对话(镜头跟随说话主体) ============
    static const DialogLine CH1_MID[] = {
        { SPEAKER_NARRATION, _T("旁白"), _T("雾气轻轻涌动，影子在地面拉长、扭曲。") },
        { SPEAKER_NARRATION, _T("旁白"), _T("暗处似乎有东西在缓慢跟随……") },
        { SPEAKER_ENEMY, _T("游荡幽影"), _T("回来…… 不要往前走……") },
        { SPEAKER_PLAYER, _T("守夜人"), _T("是百年前迷失的路人残影。它们没有恶意，只有无尽的迷茫。") },
        { SPEAKER_PLAYER, _T("守夜人"), _T("但残存的执念，会让它们攻击异类。") },
    };
    static const DialogLine CH2_MID[] = {
        { SPEAKER_NARRATION, _T("旁白"), _T("风穿过枯枝，发出细碎、类似人语的沙沙声。") },
        { SPEAKER_PLAYER, _T("守夜人"), _T("像是古树在低语。它们似乎在劝我回头。") },
        { SPEAKER_ENEMY, _T("树灵残影"), _T("离开…… 这里不属于活人……") },
        { SPEAKER_PLAYER, _T("守夜人"), _T("它们是森林的守护者，也是迷雾的囚徒。半树半魂，困死百年。") },
    };
    static const DialogLine CH3_MID[] = {
        { SPEAKER_NARRATION, _T("旁白"), _T("林间掠过一道极快的黑影。风声骤紧，寒意贴身。") },
        { SPEAKER_PLAYER, _T("守夜人"), _T("速度很快，一直在绕着我游走。它在观察、在等待、在寻找破绽。") },
        { SPEAKER_ENEMY, _T("月影猎手"), _T("抓到你了……") },
    };
    static const DialogLine CH4_MID[] = {
        { SPEAKER_NARRATION, _T("旁白"), _T("所有林木退去，浓雾汇聚中央。整片幽林的怨念全部凝聚于此。") },
        { SPEAKER_PLAYER, _T("守夜人"), _T("我走过整片幽林。所有生物皆为迷雾所困、苦难铸身。") },
        { SPEAKER_PLAYER, _T("守夜人"), _T("它们被困百年，如今，也该有个了结了。") },
    };
    static const DialogLine CH1_BOSS[] = {
        { SPEAKER_NARRATION, _T("旁白"), _T("深林更深处，雾气凝聚成团。一道庞大阴影伫立林间。") },
        { SPEAKER_PLAYER, _T("守夜人"), _T("这片雾的怨气，似乎终于面目初显。") },
    };
    static const DialogLine CH2_BOSS[] = {
        { SPEAKER_NARRATION, _T("旁白"), _T("大地震动，老树根破土翻涌。巨大的树躯缓缓睁开树眼。") },
        { SPEAKER_BOSS, _T("路障树精"), _T("外来者…… 踏碎安宁……") },
        { SPEAKER_PLAYER, _T("守夜人"), _T("它常年守护这片林地。对它而言，我才是入侵者。") },
    };
    static const DialogLine CH3_BOSS[] = {
        { SPEAKER_NARRATION, _T("旁白"), _T("月光凝聚成狐，银白虚影踏月而来。它是雾林最快的猎手。") },
        { SPEAKER_BOSS, _T("追月灵狐"), _T("你是何人，也配享受我的月光！") },
        { SPEAKER_PLAYER, _T("守夜人"), _T("原来这是灵狐的结界，专供自己修养生息！") },
    };
    static const DialogLine CH4_BOSS[] = {
        { SPEAKER_NARRATION, _T("旁白"), _T("浓雾翻涌，巨大黑影缓缓苏醒。雾核守卫睁开双眼。") },
        { SPEAKER_BOSS, _T("雾核守卫"), _T("人类…… 遗忘…… 抛弃…… 百年孤寂，皆因你们……") },
        { SPEAKER_PLAYER, _T("守夜人"), _T("我未曾遗忘。我今日至此，替人间赎罪，替长夜终章。") },
    };

    // BOSS 进入二阶段时的独白
    static const DialogLine CH4_BOSS_PHASE2[] = {
        { SPEAKER_BOSS, _T("雾核守卫"), _T("唔啊啊——！ 这点痛楚…… 不及我百年怨念之万一！") },
        { SPEAKER_BOSS, _T("雾核守卫"), _T("迷雾啊，听我号令…… 撕碎这渺小的光！") },
        { SPEAKER_PLAYER, _T("守夜人"), _T("它的力量…… 真正觉醒了。打起精神！") },
    };

    // BOSS 第一条命被击破,进入第二条命(终焉形态/阶段3)的剧情
    static const DialogLine CH4_BOSS_PHASE3[] = {
        { SPEAKER_NARRATION, _T("旁白"), _T("雾核守卫的躯体崩解，化作漫天黑雾。可是…… 它并未消散。") },
        { SPEAKER_BOSS, _T("雾核守卫"), _T("你以为…… 这就结束了吗？ 真正的终焉，现在才开始！") },
        { SPEAKER_BOSS, _T("雾核守卫"), _T("吞噬这片月落幽林吧——我的，终焉形态！") },
        { SPEAKER_PLAYER, _T("守夜人"), _T("不管你变成什么…… 长夜，终将由我来终结！") },
    };

} // namespace Story
