// audio_sessions: for every active render endpoint, the audio sessions on it
// (process id, state) — shows which endpoint carries midi-sink's WASAPI stream
// before and after the default output changes (step 55 hotplug evidence).
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmdeviceapi.h>
#include <mmreg.h>
#include <audiopolicy.h>
#include <functiondiscoverykeys_devpkey.h>
#include <psapi.h>
#include <cstdio>
#include <cwchar>
int main() {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    IMMDeviceEnumerator* en = nullptr;
    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&en))) return 1;
    IMMDevice* def = nullptr; LPWSTR defId = nullptr;
    if (SUCCEEDED(en->GetDefaultAudioEndpoint(eRender, eConsole, &def))) def->GetId(&defId);
    IMMDeviceCollection* col = nullptr; en->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &col);
    UINT n = 0; col->GetCount(&n);
    for (UINT i = 0; i < n; i++) {
        IMMDevice* d = nullptr; col->Item(i, &d);
        LPWSTR id = nullptr; d->GetId(&id);
        IPropertyStore* ps = nullptr; d->OpenPropertyStore(STGM_READ, &ps);
        PROPVARIANT name; PropVariantInit(&name); ps->GetValue(PKEY_Device_FriendlyName, &name);
        std::printf("%s%ls\n", (defId && wcscmp(id, defId) == 0) ? "* (default) " : "  ", name.pwszVal);
        IAudioSessionManager2* sm = nullptr;
        if (SUCCEEDED(d->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr, (void**)&sm))) {
            IAudioSessionEnumerator* se = nullptr;
            if (SUCCEEDED(sm->GetSessionEnumerator(&se))) {
                int count = 0; se->GetCount(&count);
                for (int s = 0; s < count; s++) {
                    IAudioSessionControl* sc = nullptr; se->GetSession(s, &sc);
                    IAudioSessionControl2* sc2 = nullptr; sc->QueryInterface(__uuidof(IAudioSessionControl2), (void**)&sc2);
                    DWORD pid = 0; AudioSessionState st = AudioSessionStateInactive;
                    if (sc2) sc2->GetProcessId(&pid);
                    sc->GetState(&st);
                    wchar_t exe[MAX_PATH] = L"?";
                    if (pid) { HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid); if (h) { DWORD sz = MAX_PATH; QueryFullProcessImageNameW(h, 0, exe, &sz); CloseHandle(h); } }
                    const wchar_t* leaf = wcsrchr(exe, L'\\'); leaf = leaf ? leaf + 1 : exe;
                    if (pid) std::printf("      session pid %lu %ls: %s\n", (unsigned long)pid, leaf, st == AudioSessionStateActive ? "ACTIVE" : st == AudioSessionStateInactive ? "inactive" : "expired");
                    if (sc2) sc2->Release(); sc->Release();
                }
                se->Release();
            }
            sm->Release();
        }
        PropVariantClear(&name); ps->Release(); CoTaskMemFree(id); d->Release();
    }
    return 0;
}
