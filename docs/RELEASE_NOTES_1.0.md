# Geiger Generator 1.0.0

A polished native CLAP instrument for Windows x64 and Linux x86_64. The 0.1 preview was confirmed working by the user in Bitwig; this release's new behaviour has automated coverage and still benefits from listening and project testing in the actual host.

## Sound

- Density balance now acts before circuit saturation, follows accepted detector counts and changes smoothly. Dense fields preserve more dynamic space instead of merely attenuating already flattened audio. New patches default to 45%; saved values, including zero, are respected.
- Fractional arrival times are distributed across adjacent audio samples. Crackle bursts add energy rather than correlated envelopes. Stereo noise decorrelates with Width while zero Width remains exactly mono.
- Smoothed drive and antiderivative-antialiased soft saturation in the drive and output-protection stages reduce abruptness and nonlinear aliasing. They are not an alias-free or true-peak-limiter guarantee.
- Eight click models: the original six plus Ceramic tap and Wire ping. Transducers have individual enclosure-decay profiles.

## Presets and interaction

- 20 factory presets, including Velvet reactor, Ion storm, Ceramic drizzle, Wire constellation, Deep containment, Distant fallout, Faulty quench, Pulse engine, Glass tide, Night survey and Critical mass.
- The selected preset name remains visible. An asterisk marks a modified sound. The name and edited status survive editor reopening and project save/load.
- Previous/next preset buttons and descriptive tooltips. Presets keep Output, Power, Run mode and Seed so browsing does not unexpectedly change performance setup or master level.

## Visual explanations

Hover or adjust a control to see its effect. Circuit controls show a rendered pulse; speaker controls show its spectrum; field controls explain timing, losses, recovery and changing intensity; behaviour controls explain gate envelopes, MIDI pitch and seeded repeats. The gray comparison uses the focused parameter's default while preserving the other settings.

These are explicitly labelled explanatory previews, separate from the real detector meter. The live pulse trace uses signed output minima/maxima with a sample-rate-aware time window. The rate/distance Probe playground remains available on the Field tab.

## Compatibility and checks

All 37 parameter IDs, the plug-in ID and port/remote-page IDs remain unchanged. Preview project states load without the new preset metadata. Sound refinements mean old patches are not promised to be bit-identical. Keep the old preview ZIP for exact old renders, and save a copy of important projects before upgrading.

Release publication requires Windows and Linux compilation, DSP and host/state regression tests, an allocation-check test, real editor snapshots and clap-validator. The workflow verifies the uploaded release files before publishing. Consult `docs/TESTING.md` for coverage and the manual Bitwig checklist.

Extract `GeigerGenerator.clap` from your platform ZIP to your host's CLAP folder and rescan. The ZIP includes documentation and licence notices. Audio examples and actual UI snapshots are separate assets. This is a sound instrument, not a dosimeter.
