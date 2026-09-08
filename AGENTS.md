# Geiger Generator: agent guide

Build a native CLAP sound generator on null-clap. JUCE is only the window/drawing toolkit: do not add an AudioProcessor or another plug-in wrapper.

- Preserve stable parameter, audio-port, note-port and remote-page IDs. Existing projects and automation must keep working.
- Keep the deterministic, allocation-free DSP independent of JUCE/CLAP and test it directly. No allocation, locks, logging, files or host-main-thread callbacks in audio processing.
- Model random detector arrivals separately from tube dead time, electrical pulse shaping and acoustic transducer response. Do not imply that click pitch/loudness encodes particle energy. Artistic deviations must be named clearly.
- Read effective parameters for every event-delimited audio span. Use null-clap GUI gesture/value APIs, not direct parameter writes from controls.
- Keep UI telemetry atomic and bounded. UI animation must reflect actual DSP events, not invent detector activity.
- Keep dependencies pinned. Do not modify the shared framework just to implement this instrument.
- Build and test Windows and Linux; run clap-validator without hiding failures. Publish usable binaries to GitHub Releases only after checks pass. Never describe a queued build as a verified release.
- Maintain readable high-contrast labels, generous targets, scalable layout and concise help. Avoid unsupported symbol fonts.
- Update README and docs when changing behavior. Expand sound models and tests when warranted; do not treat the initial architecture as a feature ceiling.
