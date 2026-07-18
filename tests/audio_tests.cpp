#include "audio/music.hpp"
#include "audio/sound_effects.hpp"
#include "content/catalog.hpp"
#include "content/rom.hpp"

#include <array>
#include <cstdio>
#include <string>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (condition)
        return;
    std::fprintf(stderr, "FAIL: %s\n", message);
    ++failures;
}

#ifdef TETRIS_TEST_ROM
void test_driver() {
    tetris::content::Rom rom;
    tetris::content::Catalog content;
    std::string error;
    expect(tetris::content::load_rom(TETRIS_TEST_ROM, rom, error), "audio test ROM loads");
    expect(tetris::content::extract_catalog(rom, content, error), "audio catalog extracts");
    if (!error.empty())
        return;

    tetris::audio::MusicPlayer music;
    expect(music.play(content.audio, 4), "Korobeiniki starts");
    music.tick();
    const auto channels = music.channels();
    expect(channels[0].note == 0x48 && channels[0].duration == 24 &&
           channels[0].period == 0x06F7,
           "TETRIS-AUDIO-001 first pulse note matches the decoded driver");
    expect(channels[1].note == 0x52 && channels[1].duration == 24,
           "TETRIS-AUDIO-002 second pulse note starts on the same tick");
    expect(channels[2].wave == 2,
           "TETRIS-AUDIO-003 wave instruments are resolved during extraction");
    for (int tick = 0; tick < 12; ++tick)
        music.tick();
    expect(music.channels()[0].output_period == 0x06F8,
           "TETRIS-AUDIO-004 sustained notes apply decoded vibrato");

    tetris::audio::SoundEffects sounds;
    sounds.attach(content.audio);
    expect(sounds.play(tetris::content::SoundChannel::pulse, 3), "rotate sound starts");
    expect(sounds.channel(tetris::content::SoundChannel::pulse).registers ==
               std::array<std::uint8_t, 5>{0x3B, 0x80, 0xB2, 0x87, 0x87},
           "TETRIS-AUDIO-005 rotate starts with its five decoded registers");
    for (int tick = 0; tick < 3; ++tick)
        sounds.tick();
    expect(sounds.channel(tetris::content::SoundChannel::pulse).registers[2] == 0xA2,
           "TETRIS-AUDIO-006 register scripts advance on their exact tick");
    for (int tick = 3; tick < 18; ++tick)
        sounds.tick();
    expect(!sounds.channel(tetris::content::SoundChannel::pulse).active(),
           "TETRIS-AUDIO-007 effects stop at their decoded duration");
    expect(sounds.play(tetris::content::SoundChannel::pulse, 7), "Tetris cue starts");
    for (int tick = 0; tick < 24; ++tick)
        sounds.tick();
    expect(sounds.channel(tetris::content::SoundChannel::wave).active(),
           "TETRIS-AUDIO-008 the Tetris pulse hands off to its wave sweep");
}
#endif

} // namespace

int main() {
#ifdef TETRIS_TEST_ROM
    test_driver();
#endif
    if (failures != 0)
        return 1;
    std::puts("modern Native Tetris audio tests passed");
    return 0;
}
