# Sound model

## Sources and scope

The physical starting points are manufacturer explanations, not the assumption that radiation itself makes an audible click:

1. Mirion, [Gamma and X-Ray Detection](https://www.mirion.com/discover/knowledge-hub/articles/education/nuclear-measurement-fundamental-principle-gamma-and-x-ray-detection): gas ionization, avalanche, GM pulses independent of initial ionization magnitude, quenching and dead time.
2. Mirion, [Introduction to Radiation Detectors](https://www.mirion.com/discover/knowledge-hub/articles/education/introduction-to-radiation-detectors): detector operating regions and event-counting principles.
3. Vernier, [What is the dead time of the radiation monitor?](https://www.vernier.com/til/23711): an example manufacturer's detector-specific dead-time specification. It is not a universal value for all tubes.

The 190 microsecond default is a representative sound-design setting, not a calibration to a named meter. Click and enclosure recipes are original synthesized approximations, not sampled commercial devices or circuit-component simulations.

## Signal path

`source field -> random arrivals -> tube dead time / recovery -> electrical pulse + damped modes -> transducer / case -> smoothed density balance -> smoothed antialiased drive -> DC removal -> antialiased bounded output`

For a steady independent source of intensity lambda, waiting times at 100% Randomness are `-ln(U)/lambda`, with uniform U strictly between zero and one. The engine integrates hazard per sample and retains its pending threshold across automation spans. Setting intensity to zero stops future primary events; it does not erase an already sounding click. Event times are sub-sample for detector acceptance; acoustic excitations are linearly distributed between adjacent samples with a bounded two-tap fractional delay. This reduces arrival quantization without changing the detector event stream or adding extra random draws.

The tube uses a **non-paralyzable (non-extendable) dead-time** model. Arrivals inside the dead interval are rejected but do not extend that interval. The expected steady measured rate is `m = lambda / (1 + lambda * dead_time)`. Immediately after the dead interval, a tunable exponential recovery scales the pulse. This approximates circuit/tube recovery, not incident particle energy. The count meter is a 0.75-second exponential estimate; its total and loss ratio include accepted/rejected simulated secondary discharges when enabled.

The pulse is a difference of decaying exponentials combined with two damped modal oscillators and an optional short noise-energy envelope. A shared linear state accumulates overlapping pulses rather than allocating individual voices, so high count rates form a continuous texture naturally. Crackle bursts add squared amplitudes and use the square root of decaying energy for their noise envelope, avoiding the former correlated-envelope buildup. Each event has independently configurable amplitude/modal-balance variation and stereo position. A separate noise stream decorrelates the stereo crackle/hiss as Width rises; Width zero remains sample-identical mono. Primary timing and acoustic variation use independent PRNG streams; changing click colour alone does not reshuffle primary arrival times.

Speaker recipes combine high-pass and two-pole low-pass filtering with a damped enclosure resonance. Enclosure-decay times depend on the speaker profile. Drive is a nonlinear artistic stage. Density gain precedes it: `1 / sqrt(1 + measured_cps * 0.005 * balance_fraction)`, using the 0.75-second detector estimate and a further 35 ms smoothing stage. Drive changes are smoothed over 12 ms. The static soft-clipping law is `f(x) = x / sqrt(1 + x*x)`; its antiderivative-antialiased form is `(x + previous) / (sqrt(1+x*x) + sqrt(1+previous*previous))`. Rationalizing the difference avoids cancellation for nearly equal inputs. DC is removed after drive; a second antialiased bounded stage keeps output samples below full scale. First-order ADAA introduces a small high-frequency smoothing/delay; it is not oversampling. This is not a true-peak limiter and does not guarantee a comfortable acoustic monitoring level. The filters and nonlinear stage are not claimed to be alias-free.

## Deliberate creative departures

- Randomness below 100% mixes exponential and fixed waiting thresholds; zero gives regular pulses, not radioactive decay statistics.
- Clusters, slow wandering and source motion vary the simulated incident rate. Motion is a free-running Hz modulation, not host-tempo sync.
- Afterpulse probability creates bounded delayed secondary events without recursive cascades. It is an exaggerated adjustable quenching artefact, not a calibrated tube fault model.
- Relay, soft, glass, ceramic and wire voices are alternate acoustic characters, not distinct particle types.
- Stereo scatter spatializes individual clicks. A single physical speaker would normally be mono.
- Listening distance attenuates/darkens the speaker. It is intentionally separate from source intensity: moving a microphone and moving a radioactive source are not the same operation.
- Density balance optionally reduces pre-drive gain using high accepted count rates; real meter speakers do not necessarily behave this way. This acoustic control does not change detector timing, count totals or tube loss.

## Reproducibility, limits and realtime behaviour

The seed is persistent. Resetting the engine with the same seed/settings and sample rate gives the same render. Transport mode can restart the sequence on Play. Saving a project stores controls, not the exact in-flight random-generator/filter state; loading mid-pattern is not promised to resume the identical event position.

There are no heap allocations, locks, network/file access or UI calls in the engine's render path. Work per sample is bounded, including at most 64 event operations and a fixed 16-slot secondary-event schedule. Artistic combined incident intensity is capped at 120,000 counts/s. Extreme overload can therefore discard events rather than perform unlimited work. Detector timing has sub-sample resolution but acoustic output remains sample-rate limited. Values are finite-checked/clamped; supported sample rates are 1-768 kHz, including fractional sample rates.

Output, power, listening attenuation, drive, density balance and noise levels are smoothed; discrete circuit changes can still alter an already sounding resonance. The instrument keeps processing while active, even when quiet. GUI telemetry uses fixed-size atomics and never calls the audio engine from its paint routine.

## Visualization contract

The count meter, loss total, running light and signed output trace use bounded atomic telemetry from the real audio engine. Parameter previews run a separate deterministic engine on the GUI thread, at most ten refreshes per second when settings change. They never render or reset the live engine.

Circuit previews render one isolated 48 kHz click, suppress hiss/hum and use unity master level except while explaining Output. Both the current and default-reference waveform share one vertical scale and time window. Speaker previews take a fixed 16,384-sample transform of that isolated click plus enabled noise/hum, with an end taper. Their -60 to +30 dB FFT-magnitude scale is fixed, not a calibrated steady-state frequency-response measurement or loudness reading. A silent/off layer cannot acquire a visible response merely because its frequency was selected.

Timing histograms, non-extendable dead-time throughput, electrical recovery, secondary-event probability, pan range and field-density envelopes are labelled models. Seed/repeat previews clamp their illustrative rate to 2..80 cps for legibility. A gray overlay substitutes the focused control's default, not an entire default patch. The live detector remains the authority for actual output activity.
