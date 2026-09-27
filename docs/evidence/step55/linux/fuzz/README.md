# voxo_fuzz on the Linux box — the SIGFPE (DECISIONS_6 #38)

`ctest` (Release, GCC 15.2): `voxo_fuzz ... ***Exception: Numerical` after 2.76 s
(the fuzzer seeds its RNG from the clock; two earlier runs had passed).
`crash.dspreset` + `crash_tone.wav` are the work folder's files as the crash
left them (the mutated sample case restores `tone.wav` only after the load):
`midi-sink --dev --voxo-load crash.dspreset` beside a `Samples/tone.wav` copy of
`crash_tone.wav` exited 136 (SIGFPE). Under gdb:

    Program received signal SIGFPE, Arithmetic exception.
    #0  voxo_ds::estimate_decoded_bytes(voxo_ds::Instrument const&, voxo_ds::Reader&)
    #1  voxo_ds::load(...)
    #2  voxo_load_preset ()
    #3  main ()

The fmt chunk of the mutated WAV: format 0xFFFE, 1 channel, 8000 Hz, block
align 3, **6 bits per sample** — `data / (bits / 8)` with `bits / 8 == 0`
(`decoded_size_from_head`, the gate's header estimate of #20). Fixed by
reading such a header as unreadable (`bits >= 8`), covered by
`tests/fixtures/dspresets/malformed/bits6.{dspreset,Samples/bits6.wav}` in
`voxo_preset_tests`; the reproducer then loads; `voxo_fuzz --seconds 60`:
1 815 091 loads, no crash.
