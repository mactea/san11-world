// Exercise the actual hook with a simulated playback state, without graphics drivers.
#define MUSIC_UNIT_TEST
#include "music.cpp"
#include <cassert>

static int played, looping;
static void Current(int id) {
    testCurrentTrack = id;
}
static int __fastcall FakePlay(void*, void*, int id, int loop, int, int, float) {
    played = id;
    looping = loop;
    Current(id);
    return id;
}
static void CompleteAndRequest(int id, int expected) {
    Current(-1);
    started = GetTickCount() - 2000;
    PlayHook(manager, nullptr, id, 1, 500, 0, -1.0f);
    assert(played == expected && last == expected && looping == 0);
}
int main() {
    original = reinterpret_cast<Play>(FakePlay);
    customCount = 2;
    Current(-1);
    PlayHook(manager, nullptr, 3, 1, 500, 0, -1.0f);
    assert(played == 3 && looping == 0 && active);
    CompleteAndRequest(3, 30);
    PlayHook(manager, nullptr, 6, 1, 500, 0, -1.0f);
    assert(played == 30 && season == 6);
    CompleteAndRequest(6, 31);
    PlayHook(manager, nullptr, 5, 1, 500, 0, -1.0f);
    assert(played == 31 && season == 5);
    CompleteAndRequest(5, 5);
    CompleteAndRequest(5, 30);
    PlayHook(manager, nullptr, 10, 1, 500, 0, -1.0f);
    assert(played == 30 && season == 10 && active);
    CompleteAndRequest(10, 31);
    CompleteAndRequest(10, 10);
    DWORD previousStart = started = GetTickCount() - 1000;
    for (int repeat = 0; repeat < 20; ++repeat)
        PlayHook(manager, nullptr, 10, 1, 500, 0, -1.0f);
    assert(played == 10 && started == previousStart && active);
    CompleteAndRequest(10, 30);
    PlayHook(manager, nullptr, 16, 1, 500, 0, -1.0f);
    assert(played == 16 && looping == 1 && !active);
    PlayHook(manager, nullptr, 1, 1, 500, 0, -1.0f);
    assert(played == 1 && looping == 1 && !active);
    PlayHook(manager, nullptr, 31, 1, 500, 0, -1.0f);
    assert(played == 31 && looping == 0 && active);
    assert(ResourceHook(nullptr, nullptr, 0, 2267) == &records[0]);
    assert(ResourceHook(nullptr, nullptr, 0, 2268) == &records[1]);
    // Existing one-custom-track installations still alternate as before.
    customCount = 1;
    last = 30;
    season = 4;
    assert(NextTrack() == 4);
    customCount = 2;
    heroOnly = true;
    for (int id = 3; id <= 10; ++id) {
        Current(-1);
        PlayHook(manager, nullptr, id, 1, 500, 0, -1.0f);
        assert(played == 31 && looping == 1 && !active);
    }
    for (int id = 30; id <= 31; ++id) {
        Current(-1);
        PlayHook(manager, nullptr, id, 0, 500, 0, -1.0f);
        assert(played == 31 && looping == 1 && !active);
    }
    for (int id : {1, 16}) {
        PlayHook(manager, nullptr, id, 1, 500, 0, -1.0f);
        assert(played == id && looping == 1 && !active);
        PlayHook(manager, nullptr, 10, 1, 500, 0, -1.0f);
        assert(played == 31 && looping == 1 && !active);
    }
    puts("PASS: fixed Hero's Dawn map loop, all map cues, custom restores, menu/event cues and map return");
    puts("PASS: original / medley / Hero's Dawn rotation, wartime map cue 10, repeated requests, native cues, both resources, one-track compatibility");
}
