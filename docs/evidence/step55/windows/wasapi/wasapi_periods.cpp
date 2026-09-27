// wasapi_periods: for every active render endpoint, the mix format and the
// shared-mode engine periods IAudioClient3 offers (default / fundamental /
// min / max, in frames at the mix rate) plus IAudioClient::GetDevicePeriod.
// Step 55 evidence (the Windows row of DECISIONS_6 #9). Standalone, no miniaudio.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <functiondiscoverykeys_devpkey.h>
#include <cstdio>
#include <cwchar>
int main() {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    IMMDeviceEnumerator* en = nullptr;
    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&en))) return 1;
    IMMDevice* def = nullptr; LPWSTR defId = nullptr;
    if (SUCCEEDED(en->GetDefaultAudioEndpoint(eRender, eConsole, &def))) def->GetId(&defId);
    IMMDeviceCollection* col = nullptr;
    en->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &col);
    UINT n = 0; col->GetCount(&n);
    for (UINT i = 0; i < n; i++) {
        IMMDevice* d = nullptr; col->Item(i, &d);
        LPWSTR id = nullptr; d->GetId(&id);
        IPropertyStore* ps = nullptr; d->OpenPropertyStore(STGM_READ, &ps);
        PROPVARIANT name; PropVariantInit(&name); ps->GetValue(PKEY_Device_FriendlyName, &name);
        IAudioClient3* ac = nullptr;
        HRESULT hr = d->Activate(__uuidof(IAudioClient3), CLSCTX_ALL, nullptr, (void**)&ac);
        std::printf("%s%ls\n", (defId && wcscmp(id, defId) == 0) ? "* (default) " : "  ", name.pwszVal);
        if (SUCCEEDED(hr)) {
            WAVEFORMATEX* mix = nullptr; ac->GetMixFormat(&mix);
            REFERENCE_TIME defP = 0, minP = 0; ac->GetDevicePeriod(&defP, &minP);
            UINT32 dflt = 0, fund = 0, mn = 0, mx = 0;
            HRESULT h3 = ac->GetSharedModeEnginePeriod(mix, &dflt, &fund, &mn, &mx);
            std::printf("    mix format: %u Hz, %u ch, %u bits (tag %u)\n", mix->nSamplesPerSec, mix->nChannels, mix->wBitsPerSample, mix->wFormatTag);
            std::printf("    GetDevicePeriod: default %.3f ms, minimum %.3f ms\n", defP / 10000.0, minP / 10000.0);
            if (SUCCEEDED(h3))
                std::printf("    IAudioClient3 shared-mode engine period: default %u, fundamental %u, min %u, max %u frames (min %.3f ms)\n", dflt, fund, mn, mx, 1000.0 * mn / mix->nSamplesPerSec);
            else std::printf("    GetSharedModeEnginePeriod failed: 0x%08lx\n", (unsigned long)h3);
            CoTaskMemFree(mix); ac->Release();
        } else std::printf("    IAudioClient3 not available: 0x%08lx\n", (unsigned long)hr);
        PropVariantClear(&name); ps->Release(); CoTaskMemFree(id); d->Release();
    }
    return 0;
}
