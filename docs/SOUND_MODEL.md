# Sound model

## Sources and scope

The physical starting points are manufacturer explanations, not the assumption that radiation itself makes an audible click:

1. Mirion, [Gamma and X-Ray Detection](https://www.mirion.com/discover/knowledge-hub/articles/education/nuclear-measurement-fundamental-principle-gamma-and-x-ray-detection): gas ionization, avalanche, GM pulses independent of initial ionization magnitude, quenching and dead time.
2. Mirion, [Introduction to Radiation Detectors](https://www.mirion.com/discover/knowledge-hub/articles/education/introduction-to-radiation-detectors): detector operating regions and event-counting principles.
3. Vernier, [What is the dead time of the radiation monitor?](https://www.vernier.com/til/23711): an example manufacturer's detector-specific dead-time specification. It is not a universal value for all tubes.

The 190 microsecond default is a representative sound-design setting, not a calibration to a named meter. Click and enclosure recipes are original synthesized approximations, not sampled commercial devices or circuit-component simulations.

## Signal path

`source field -> random arrivals -> tube dead time / recovery -> electrical pulse + damped modes -> transducer / case -> drive -> DC removal -> bounded output`

For a steady independent source of intensity lambda, waiting times at 100% Randomness are `-ln(U)/lambda`, with uniform U strictly between zero and one. The engine integrates hazard per sample and retains its pending threshold across automation spans. Setting intensity to zero stops future primary events; it does not erase an already sounding click. Event times are sub-sample for detector acceptance; audible excitations are accumulated at sample resolution.

The tube uses a **non-paralyzable (non-extendable) dead-time** model. Arrivals inside the dead interval are rejected but do not extend that interval. The expected steady measured rate is `m = lambda / (1 + lambda * dead_time)`. Immediately after the dead interval, a tunable exponential recovery scales the pulse. This approximates circuit/tube recovery, not incident particle energy. The count meter is a 0.75-second exponential estimate; its total and loss ratio include accepted/rejected simulated secondary discharges when enabled.

The pulse is a difference of decaying exponentials combined with two damped modal oscillators and an optional short noise envelope. A shared linear state accumulates overlapping pulses rather than allocating individual voices, so high count rates form a continuous texture naturally. Each event has independently configurable amplitude/modal-balance variation and stereo position. Primary timing and acoustic variation use independent PRNG streams; changing click colour alone does not reshuffle primary arrival times.

Speaker recipes combine high-pass and two-pole low-pass filtering with a damped enclosure resonance. Drive is a nonlinear artistic stage. DC is removed after it and a final tanh stage bounds samples below full scale. This is not a true-peak limiter and does not guarantee a comfortable acoustic monitoring level. The filters and nonlinear stage are not claimed to be alias-free.

## Deliberate creative departures

- Randomness below 100% mixes exponential and fixed waiting thresholds; zero gives regular pulses, not radioactive decay statistics.
- Clusters, slow wandering and source motion vary the simulated incident rate. Motion is a free-running Hz modulation, not host-tempo sync.
- Afterpulse probability creates bounded delayed secondary events without recursive cascades. It is an exaggerated adjustable quenching artefact, not a calibrated tube fault model.
- Relay, soft and glass voices are alternate acoustic characters, not distinct particle types.
- Stereo scatter spatializes individual clicks. A single physical speaker would normally be mono.
- Listening distance attenuates/darkens the speaker. It is intentionally separate from source intensity: moving a microphone and moving a radioactive source are not the same operation.
- Density compensation optionally reduces output gain at high incident rates; real meter speakers do not necessarily behave this way.

## Reproducibility, limits and realtime behaviour

The seed is persistent. Resetting the engine with the same seed/settings and sample rate gives the same render. Transport mode can restart the sequence on Play. Saving a project stores controls, not the exact in-flight random-generator/filter state; loading mid-pattern is not promised to resume the identical event position.

There are no heap allocations, locks, network/file access or UI calls in the engine's render path. Work per sample is bounded, including at most 64 event operations and a fixed 16-slot secondary-event schedule. Artistic combined incident intensity is capped at 120,000 counts/s. Extreme overload can therefore discard events rather than perform unlimited work. Detector timing has sub-sample resolution but acoustic output remains sample-rate limited. Values are finite-checked/clamped; supported sample rates are 1-768 kHz, including fractional sample rates.

Output, power, listening attenuation and noise levels are smoothed; discrete circuit changes can still alter an already sounding resonance. The instrument keeps processing while active, even when quiet. GUI telemetry uses fixed-size atomics and never calls the audio engine from its paint routine.
