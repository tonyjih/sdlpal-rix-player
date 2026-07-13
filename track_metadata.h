#pragma once

struct TrackMetadata {
    int id;
    const char *name_zh_hant;
    const char *source_zh_hant;
};

inline const TrackMetadata *find_track_metadata(int id) {
    // palresearch maps raw ID 0x56 (decimal 86) to 酒劍仙. Its table says
    // primary names are based on the official PAL memorial collection, so the
    // UI describes this as official-source-derived rather than claiming the
    // community ID mapping itself is an official publication.
    static constexpr TrackMetadata metadata[] = {
        {86, u8"酒劍仙", u8"曲名來源：官方資料整理"},
    };
    for (const TrackMetadata &entry : metadata) {
        if (entry.id == id) return &entry;
    }
    return nullptr;
}
