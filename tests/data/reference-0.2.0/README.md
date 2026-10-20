# Reference renders of version 0.2.0

Rendered with the released 0.2.0 code (the `main` branch at the start of version 0.3, before any
0.3 change): every factory preset and two test projects, one note each (note 36, velocity 100,
44.1 kHz, left channel, 0.3 s, note off at 0.15 s). The format is the same as in
`../reference-0.1.1`: an int32 latency in samples, an int32 sample count and the float32 samples
(little endian). `tests/core/ReferenceRenderTests.cpp` renders the same cases with the current code
and requires every sample to be within 1e-6 of these (1e-4 on Windows and macOS, as explained in the
0.1.1 README), aligned by the reported latency.

The test projects (defined in `cases020()` in the test):

- `buses-tail`: Classic Punch with Osc 2 on Bus A (Apply To Tail), the Noise on Bus B, the Tock and
  Osc 1 on Main; Bus A holds Filter, Transient and a multiband Compressor, Bus B a Reverb and a
  second Distortion, Main an EQ with an Extreme band and Key Track, a single-band Compressor (copy 2)
  and Stereo.
- `main-chain`: Gabber Fold with Osc 2 on Bus B (Apply To Punch, holding an EQ copy and a Distortion
  copy) and a long Main rack with a high-pass Filter copy, a Transient, a multiband Compressor, a
  Distortion, a Reverb, an EQ with an Extreme band and Stereo.

To make them again (only ever from the 0.2.0 code): `FCM_WRITE_REFERENCES=0.2.0` and run the
`fcm_core_tests` executable. Never regenerate them from newer code.
