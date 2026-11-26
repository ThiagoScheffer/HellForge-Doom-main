#include "platform/storm_rules.h"
#include <cassert>
#include <cmath>
#include <cstdio>

static void testBoltEnvelope() {
    using namespace doom_storm;

    constexpr float expected[] = {
        .26f, 1.f, .55f, .26f, .20f,
        .72f, .42f, .24f, .13f, .06f
    };

    constexpr float originalFlash[] = {
        .26f, 1.f, .38f, .07f, .04f,
        .72f, .34f, .06f, .13f
    };

    static_assert(boltLifetimeTics == 10);

    assert(boltPulse(-1) == 0.f);

    for (int age = 0; age < boltLifetimeTics; ++age) {
        const float value = boltPulse(age);

        assert(value == expected[age]);
        assert(value > 0.f);
        assert(value <= 1.f);
    }

    assert(boltPulse(10) == 0.f);
    assert(boltPulse(100) == 0.f);

    // Lock the original diffuse flash curve.
    for (int age = 0; age < 9; ++age) {
        assert(pulse(age) == originalFlash[age]);
    }

    assert(pulse(-1) == 0.f);
    assert(pulse(9) == 0.f);
    assert(pulse(10) == 0.f);

    // Original primary and secondary peaks are retained.
    assert(boltPulse(1) == pulse(1));
    assert(boltPulse(5) == pulse(5));

    // Improved visibility between pulses.
    assert(boltPulse(3) > pulse(3));
    assert(boltPulse(4) > pulse(4));
}

static void testExtendedBoltLifetime() {
    using namespace doom_storm;

    constexpr int rate = 35;
    constexpr int start = 100;
    constexpr int mode = 2;
    constexpr std::uint32_t seed = 4242u;

    constexpr Kind kinds[] = {
        Kind::Close,
        Kind::Medium,
        Kind::Distant
    };

    constexpr float strengths[] = {
        .70f, .53f, .32f
    };

    for (int k = 0; k < 3; ++k) {
        for (int strong = 0; strong < 2; ++strong) {
            State initial{};

            initial.seed = seed;
            initial.mode = mode;
            initial.event = 1;
            initial.lastTic = start - 1;
            initial.flashTic = start;

            // Prevent a new scheduled event from interfering
            // with this controlled envelope test.
            initial.nextFlash = start + 1000;
            initial.thunderTic = start + 20;

            initial.kind = kinds[k];
            initial.hasBolt = true;
            initial.boltAzimuth = 1.25f;
            initial.boltSeed = .42f;

            State visible = initial;
            State hidden = initial;
            State noBolt = initial;

            noBolt.hasBolt = false;

            const float strength =
                strong ? strengths[k] : .19f;

            int visibleFrames = 0;
            int thunderEvents = 0;

            for (int age = 0; age <= 21; ++age) {
                const int tic = start + age;

                const auto x = advance(
                    visible, tic, rate, mode,
                    true, strong != 0, seed, true);

                const auto y = advance(
                    hidden, tic, rate, mode,
                    true, strong != 0, seed, false);

                const auto z = advance(
                    noBolt, tic, rate, mode,
                    true, strong != 0, seed, true);

                // Diffuse lighting remains unchanged.
                assert(x.flash == pulse(age) * strength);
                assert(x.flash == y.flash);
                assert(x.flash == z.flash);

                // Visibility has no influence on audio.
                assert(x.thunder == y.thunder);
                assert(x.thunder == z.thunder);

                assert(y.bolt == 0.f);
                assert(z.bolt == 0.f);

                // Exact bolt envelope and intensity cap.
                assert(x.bolt == boltPulse(age) * strength);
                assert(x.bolt >= 0.f);
                assert(x.bolt <= strength);

                if (age < boltLifetimeTics) {
                    assert(x.bolt > 0.f);
                    ++visibleFrames;

                    assert(x.azimuth == initial.boltAzimuth);
                    assert(x.boltSeed == initial.boltSeed);
                } else {
                    assert(x.bolt == 0.f);
                }

                // Critical regression: the final bolt tail
                // must exist without a diffuse flash.
                if (age == 9) {
                    assert(x.flash == 0.f);
                    assert(x.bolt > 0.f);
                }

                if (x.thunder != Kind::None) {
                    assert(age == 20);
                    assert(x.thunder == kinds[k]);
                    ++thunderEvents;
                }

                // No event rescheduling or RNG divergence.
                assert(visible.nextFlash == initial.nextFlash);
                assert(visible.event == initial.event);
                assert(visible.nextFlash == hidden.nextFlash);
                assert(visible.event == hidden.event);
                assert(visible.thunderTic == hidden.thunderTic);
            }

            assert(visibleFrames == 10);
            assert(thunderEvents == 1);
        }
    }
}

static void testBoltClockAndReset() {
    using namespace doom_storm;

    State s{};

    constexpr std::uint32_t seed = 4242u;
    constexpr int mode = 2;
    constexpr int start = 100;

    s.seed = seed;
    s.mode = mode;
    s.event = 1;
    s.lastTic = start;
    s.flashTic = start;
    s.nextFlash = start + 1000;
    s.thunderTic = start + 20;
    s.kind = Kind::Close;
    s.hasBolt = true;
    s.boltAzimuth = .75f;
    s.boltSeed = .35f;

    // Repeated evaluation at the same simulation tic
    // cannot extend or consume the bolt lifetime.
    const auto a = advance(
        s, start + 9, 35, mode,
        true, false, seed, true);

    const auto b = advance(
        s, start + 9, 35, mode,
        true, false, seed, true);

    assert(a.bolt > 0.f);
    assert(a.flash == 0.f);
    assert(a.bolt == b.bolt);
    assert(a.azimuth == b.azimuth);
    assert(a.boltSeed == b.boltSeed);

    // Advancing to age 10 expires the visible bolt.
    const auto expired = advance(
        s, start + 10, 35, mode,
        true, false, seed, true);

    assert(expired.bolt == 0.f);

    // Test disabling a storm during its active tail.
    State active{};
    active.seed = seed;
    active.mode = mode;
    active.lastTic = start;
    active.flashTic = start;
    active.nextFlash = start + 1000;
    active.kind = Kind::Close;
    active.hasBolt = true;

    State dryState = active;
    State offState = active;
    State rewindState = active;

    const auto dry = advance(
        dryState, start + 9, 35, mode,
        false, false, seed, true);

    assert(dry.bolt == 0.f);
    assert(dry.flash == 0.f);
    assert(!dryState.hasBolt);

    const auto off = advance(
        offState, start + 9, 35, 0,
        true, false, seed, true);

    assert(off.bolt == 0.f);
    assert(off.flash == 0.f);
    assert(!offState.hasBolt);

    const auto rewind = advance(
        rewindState, 0, 35, mode,
        true, false, seed, true);

    assert(rewind.bolt == 0.f);
    assert(rewind.flash == 0.f);
    assert(!rewindState.hasBolt);
}

int main() {
  testBoltEnvelope();
  testExtendedBoltLifetime();
  testBoltClockAndReset();
  doom_storm::State s;
  int events=0,close=0,medium=0,far=0,anyFlash=0;
  for(int t=0;t<35*600;++t) {
    auto f=doom_storm::advance(s,t,35,3,true,true,0x11345u);
    assert(f.flash>=0&&f.flash<=.70f);
    if(f.flash>0)++anyFlash;
    if(f.thunder!=doom_storm::Kind::None) {
       ++events;
       if(f.thunder==doom_storm::Kind::Close)++close;
       if(f.thunder==doom_storm::Kind::Medium)++medium;
       if(f.thunder==doom_storm::Kind::Distant)++far;
    }
  }
  assert(events>10&&anyFlash>30&&far>0&&medium>0&&close>0);
  auto off=doom_storm::advance(s,35*600,35,3,false,true,0x11345u);
  assert(off.thunder==doom_storm::Kind::None&&off.flash==0&&s.nextFlash==-1);
  doom_storm::advance(s,0,35,3,true,false,0x11345u);
  for(int t=1;t<300;++t) {auto f=doom_storm::advance(s,t,35,3,true,false,0x11345u);assert(f.flash<=.19f);}
  // Same level seed produces repeatable flashes and audio events.
  doom_storm::State a,b;
  for(int t=0;t<10000;++t) {
    auto x=doom_storm::advance(a,t,35,2,true,true,4242);
    auto y=doom_storm::advance(b,t,35,2,true,true,4242);
    assert(x.thunder==y.thunder&&std::abs(x.flash-y.flash)<1e-7);
  }
  // Bolts stay fixed for each event; their visibility cannot change audio or flashes.
  doom_storm::State visible,hidden;
  int boltFrames=0;
  float azimuth=0,branchSeed=0;
  unsigned event=0;
  for(int t=0;t<35*600;++t) {
    auto x=doom_storm::advance(visible,t,35,3,true,false,4242,true);
    auto y=doom_storm::advance(hidden,t,35,3,true,false,4242,false);
    assert(x.thunder==y.thunder&&x.flash==y.flash&&y.bolt==0);
    assert(x.bolt>=0&&x.bolt<=.19f);
    if(visible.event!=event) {
      event=visible.event;azimuth=x.azimuth;branchSeed=x.boltSeed;
    }
    if(x.bolt>0) {
      ++boltFrames;
      assert(x.azimuth==azimuth&&x.boltSeed==branchSeed);
      assert(t - visible.flashTic < doom_storm::boltLifetimeTics);
      assert(x.azimuth>=-3.141593f&&x.azimuth<=3.141593f);
      assert(x.boltSeed>=0&&x.boltSeed<1);
    }
  }
  assert(boltFrames>0);
  auto dry=doom_storm::advance(visible,35*600,35,3,false,false,4242,true);
  assert(dry.bolt==0&&!visible.hasBolt);
  // Reset after level time moves backwards.
  auto rewind=doom_storm::advance(a,0,35,2,true,true,4242);
  assert(a.event==0&&rewind.thunder==doom_storm::Kind::None);
  std::puts("PASS: 10-tic bolt lifetime, unchanged diffuse flash, and bolt intensity limits");
  std::printf("PASS: %d thunder events (%d close, %d medium, %d distant), %d flash frames; resets and seed reproducibility\n",events,close,medium,far,anyFlash);
}
