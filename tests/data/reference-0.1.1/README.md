# Reference renders of version 0.1.1

Rendered with the released 0.1.1 code: every factory preset and two test projects with the Tock on,
one note each (note 36, velocity 100, 44.1 kHz, left channel, 0.3 s, note off at 0.15 s).
Each `.f32` file is an int32 latency in samples, an int32 sample count and the float32 samples
(little endian). `tests/core/ReferenceRenderTests.cpp` renders the same cases with the current code
and requires every sample to be within 1e-6 of these, aligned by the reported latency
(docs/specs/mixer-and-buses.md section 5). Never regenerate them from newer code.
