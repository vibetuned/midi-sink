# WASAPI helpers (Phase 7 step 55, the Windows box)

Three small programs the Windows agent wrote to verify hotplug and the engine
period (DECISIONS_6 #39, #41): `wasapi_periods.cpp` asks every render
endpoint its shared-mode engine period through `IAudioClient3` (the 480 of
#39); `set_default_render.cpp` switches the default
render endpoint through `IPolicyConfig` (what the Sound panel does) so a
device change can be scripted under a running storm; `audio_sessions.cpp`
lists every endpoint's audio sessions through `IAudioSessionManager2`, which
is how "the app's session moved to the new default within a second" was
read. Build with MSVC (`cl /EHsc <file> ole32.lib`); Windows only.
