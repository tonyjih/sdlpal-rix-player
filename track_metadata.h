#pragma once

struct TrackMetadata {
    int id;
    const char *name_zh_hant;
    const char *source_zh_hant;
};

inline const TrackMetadata *find_track_metadata(int id) {
    static constexpr const char kSourceMixed[] =
        u8"曲名來源：palresearch 對照（主要依《永恆回憶錄》，部分為社群補名）";
    static constexpr const char kSourceCommunity[] =
        u8"曲名來源：palresearch 社群描述性命名";
    static constexpr const char kSourceNutcracker[] =
        u8"曲名來源：《胡桃夾子解密錄》整理";
    static constexpr const char kSourceComposer[] =
        u8"曲名來源：作曲者林坤信公開資料";
    static constexpr const char kSourceSystem[] = u8"系統／空白曲目";

    static constexpr TrackMetadata metadata[] = {
        {0, u8"停止播放", kSourceSystem}, {1, u8"勝敗乃兵家常事……", kSourceMixed},
        {2, u8"戰鬥勝利1", kSourceMixed}, {3, u8"戰鬥勝利2", kSourceMixed},
        {4, u8"雲谷鶴峰2", kSourceMixed}, {5, u8"雲谷鶴峰1", kSourceMixed},
        {6, u8"蝶戀1", kSourceMixed}, {7, u8"蝶戀2", kSourceMixed},
        {8, u8"餘杭春日", kSourceMixed}, {9, u8"蝶戀2", kSourceMixed},
        {10, u8"憂", kSourceMixed}, {11, u8"驚", kSourceMixed},
        {12, u8"小橋流水", kSourceMixed}, {13, u8"來世再續未了緣", kSourceMixed},
        {14, u8"比武招親", kSourceMixed}, {15, u8"蝶戀3", kSourceMixed},
        {16, u8"情怨1", kSourceMixed}, {17, u8"情怨2", kSourceMixed},
        {18, u8"逆天而行", kSourceMixed}, {19, u8"鬼陰山", kSourceMixed},
        {20, u8"兵凶戰危", kSourceMixed}, {21, u8"救黎民", kSourceMixed},
        {22, u8"魂縈夢牽", kSourceMixed}, {23, u8"蒙難", kSourceMixed},
        {24, u8"盟誓", kSourceMixed}, {25, u8"遇襲", kSourceMixed},
        {26, u8"十面埋伏", kSourceMixed}, {27, u8"蝶戀4", kSourceMixed},
        {28, u8"紅塵路緲", kSourceMixed}, {29, u8"無", kSourceSystem},
        {30, u8"未收錄曲目－隱龍窟", kSourceCommunity}, {31, u8"樂逍遙", kSourceMixed},
        {32, u8"宿命", kSourceMixed}, {33, u8"身陷囹圄", kSourceNutcracker},
        {34, u8"危機", kSourceMixed}, {35, u8"神木林", kSourceMixed},
        {36, u8"神木林變奏－夜色", kSourceCommunity}, {37, u8"風起雲湧", kSourceMixed},
        {38, u8"勢如破竹", kSourceMixed}, {39, u8"心急如焚", kSourceMixed},
        {40, u8"戰意昂", kSourceMixed}, {41, u8"飛沙走石", kSourceNutcracker},
        {42, u8"俠客行", kSourceMixed}, {43, u8"羅漢陣", kSourceMixed},
        {44, u8"腥風血雨", kSourceMixed}, {45, u8"無所遁形", kSourceComposer},
        {46, u8"兵凶戰危變奏2", kSourceCommunity}, {47, u8"御劍伏魔2", kSourceMixed},
        {48, u8"御劍伏魔1", kSourceMixed}, {49, u8"晨光", kSourceMixed},
        {50, u8"風光", kSourceMixed}, {51, u8"富甲一方", kSourceMixed},
        {52, u8"蝶舞春園", kSourceMixed}, {53, u8"蝶舞春園2", kSourceMixed},
        {54, u8"大開眼界", kSourceMixed}, {55, u8"白河寒秋", kSourceMixed},
        {56, u8"桃花幻夢", kSourceMixed}, {57, u8"心忐忑", kSourceMixed},
        {58, u8"頹城", kSourceMixed}, {59, u8"回夢", kSourceMixed},
        {60, u8"歷險", kSourceMixed}, {61, u8"窺春", kSourceMixed},
        {62, u8"夢醒變奏", kSourceCommunity}, {63, u8"終曲", kSourceMixed},
        {64, u8"醉仙驅魔", kSourceMixed}, {65, u8"戲仙", kSourceMixed},
        {66, u8"春色無邊", kSourceMixed}, {67, u8"神佑", kSourceMixed},
        {68, u8"雨2", kSourceMixed}, {69, u8"看盡前塵", kSourceMixed},
        {70, u8"靈山", kSourceMixed}, {71, u8"繁華看盡", kSourceMixed},
        {72, u8"凌雲壯志", kSourceMixed}, {73, u8"春風戀牡丹", kSourceMixed},
        {74, u8"美景", kSourceMixed}, {75, u8"情牽", kSourceMixed},
        {76, u8"今生情不悔", kSourceMixed}, {77, u8"雲谷鶴峰3", kSourceMixed},
        {78, u8"步步為營", kSourceMixed}, {79, u8"靈怨", kSourceMixed},
        {80, u8"救佳人", kSourceMixed}, {81, u8"險境", kSourceMixed},
        {82, u8"血海餘生", kSourceMixed}, {83, u8"鬼影幢幢", kSourceMixed},
        {84, u8"雨1", kSourceMixed}, {85, u8"夢醒", kSourceMixed},
        {86, u8"酒劍仙", kSourceMixed}, {87, u8"孤雀無棲", kSourceMixed},
    };

    for (const TrackMetadata &entry : metadata) {
        if (entry.id == id) return &entry;
    }
    return nullptr;
}
