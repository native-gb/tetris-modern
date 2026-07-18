#include "audio/music.hpp"
#include "audio/sound_effects.hpp"
#include "content/catalog.hpp"
#include "content/rom.hpp"

#include <array>
#include <cstdint>
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
void hash_byte(std::uint64_t& hash, std::uint8_t value) {
    hash ^= value;
    hash *= 1'099'511'628'211ULL;
}

void hash_word(std::uint64_t& hash, std::uint16_t value) {
    hash_byte(hash, static_cast<std::uint8_t>(value & 0xFFU));
    hash_byte(hash, static_cast<std::uint8_t>(value >> 8U));
}

std::uint64_t music_trace_hash(const tetris::content::AudioCatalog& catalog,
                               std::size_t song) {
    tetris::audio::MusicPlayer music;
    if (!music.play(catalog, song))
        return 0;
    std::uint64_t hash = 1'469'598'103'934'665'603ULL;
    for (int tick = 0; tick < 1'024; ++tick) {
        music.tick();
        hash_byte(hash, music.playing() ? 1 : 0);
        for (const tetris::audio::MusicChannel& channel : music.channels()) {
            hash_byte(hash, channel.active ? 1 : 0);
            hash_byte(hash, channel.resting ? 1 : 0);
            hash_byte(hash, channel.timer);
            hash_byte(hash, channel.duration);
            hash_word(hash, channel.period);
            hash_word(hash, channel.output_period);
            hash_byte(hash, channel.vibrato_tick);
            for (const std::uint8_t value : channel.instrument) hash_byte(hash, value);
            for (const std::uint8_t value : channel.registers) hash_byte(hash, value);
        }
    }
    return hash;
}

void test_driver() {
    tetris::content::Rom rom;
    tetris::content::Catalog content;
    std::string error;
    expect(tetris::content::load_rom(TETRIS_TEST_ROM, rom, error), "audio test ROM loads");
    expect(tetris::content::extract_catalog(rom, content, error), "audio catalog extracts");
    if (!error.empty())
        return;

    tetris::audio::MusicPlayer music;
    expect(!music.play(content.audio, 99), "unknown songs are rejected");
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
    for (int tick = 0; tick < 12; ++tick)
        music.tick();
    expect(music.channels()[0].note == 0x42 && music.channels()[0].duration == 12,
           "the next duration command and note begin on their exact tick");

    const auto retained_instrument = music.channels()[1].instrument;
    expect(music.play(content.audio, 13), "the fourth Type-B jingle starts");
    music.tick();
    expect(music.channels()[1].instrument == retained_instrument,
           "instrumentless jingles retain the original channel workspace");
    music.stop();
    expect(!music.playing() && music.events().empty(),
           "explicit music stop clears deterministic driver state");
    expect(music.play(content.audio, 5), "Type-B music starts");
    music.tick();
    expect(music.channels()[2].wave_volume == 0xA0,
           "damped wave notes start at their decoded full volume");
    int damp_guard = 0;
    while (music.channels()[2].timer > 6 && damp_guard < 100) {
        music.tick();
        ++damp_guard;
    }
    expect(damp_guard < 100 && music.channels()[2].wave_volume == 0x40,
           "damped wave notes halve volume for their final six ticks");

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
    expect(!sounds.play(tetris::content::SoundChannel::pulse, 4),
           "piece movement is suppressed while the Tetris sweep owns audio");
    for (int tick = 0; tick < 32; ++tick)
        sounds.tick();
    expect(!sounds.channel(tetris::content::SoundChannel::wave).active(),
           "Tetris wave sweep stops at its decoded duration");

    expect(sounds.play(tetris::content::SoundChannel::noise, 4),
           "rocket liftoff noise starts");
    for (int tick = 0; tick < 48; ++tick)
        sounds.tick();
    expect(sounds.channel(tetris::content::SoundChannel::noise).registers[1] == 0x70 &&
               sounds.channel(tetris::content::SoundChannel::noise).registers[2] == 0x65,
           "liftoff advances through both ROM-derived register tables");
    sounds.stop();
    expect(sounds.play(tetris::content::SoundChannel::wave, 2, 0x1B),
           "divider-pitched game-over wave starts");
    sounds.tick(0x05);
    expect(sounds.channel(tetris::content::SoundChannel::wave).registers[3] == 0xE5,
           "game-over pitch combines its evolving base and random low nibble");

    for (const tetris::content::SoundEffect& effect : content.audio.effects) {
        tetris::audio::SoundEffects complete;
        complete.attach(content.audio);
        expect(complete.play(effect.channel, effect.original_id),
               "every extracted sound effect can start by semantic channel and ID");
        for (std::uint16_t tick = 0; tick < effect.duration; ++tick)
            complete.tick(7);
        expect(!complete.channel(effect.channel).active(),
               "every extracted sound effect ends at its declared duration");
    }

    // These 1,024-tick semantic hashes were generated only after the modern traces
    // matched the authenticated reference traces byte-for-byte for all 17 songs.
    constexpr std::array<std::uint64_t, 17> trace_hashes = {
        0x36BE8A4661E5D90EULL, 0x91CC8FCCD110C983ULL, 0xC54B87028C109571ULL,
        0x77420CFA915E1543ULL, 0xB47F7425F756AAC1ULL, 0xDA17FC1821E9FF9FULL,
        0x74199D0FC6694C86ULL, 0x7209BE2FB7A9A62EULL, 0x8C9F195E7EEFFA25ULL,
        0x974FF1513CBF281DULL, 0xF4C3AD2126A1B6D5ULL, 0x7690C918224B503DULL,
        0xE167E270AB5631CFULL, 0x237DEB240BD1DBDFULL, 0x1FF76FE24BCB5311ULL,
        0x73C6D498F16557A5ULL, 0x111A60EDCF4EE063ULL,
    };
    for (std::size_t song = 0; song < trace_hashes.size(); ++song) {
        expect(music_trace_hash(content.audio, song) == trace_hashes[song],
               "TETRIS-AUDIO-009 all songs retain their authenticated 1024-tick trace");
    }
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
