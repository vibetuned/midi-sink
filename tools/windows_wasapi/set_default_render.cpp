// set_default_render <name substring>: makes the matching active render
// endpoint the default (console, multimedia and communications roles) through
// the undocumented IPolicyConfig — what the Sound control panel does. Step 55
// hotplug item: "switch the default output in Windows" while Voxo plays.
// With no argument it lists the active render endpoints and marks the default.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmdeviceapi.h>
#include <mmreg.h>
#include <functiondiscoverykeys_devpkey.h>
#include <cstdio>
#include <cwchar>
#include <string>

struct __declspec(uuid("f8679f50-850a-41cf-9c72-430f290290c8")) IPolicyConfig : IUnknown {
    virtual HRESULT STDMETHODCALLTYPE GetMixFormat(PCWSTR, WAVEFORMATEX**) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetDeviceFormat(PCWSTR, INT, WAVEFORMATEX**) = 0;
    virtual HRESULT STDMETHODCALLTYPE ResetDeviceFormat(PCWSTR) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetDeviceFormat(PCWSTR, WAVEFORMATEX*, WAVEFORMATEX*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetProcessingPeriod(PCWSTR, INT, PINT64, PINT64) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetProcessingPeriod(PCWSTR, PINT64) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetShareMode(PCWSTR, struct DeviceShareMode*) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetShareMode(PCWSTR, struct DeviceShareMode*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetPropertyValue(PCWSTR, const PROPERTYKEY&, PROPVARIANT*) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetPropertyValue(PCWSTR, const PROPERTYKEY&, PROPVARIANT*) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetDefaultEndpoint(PCWSTR wszDeviceId, ERole eRole) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetEndpointVisibility(PCWSTR, INT) = 0;
};
struct __declspec(uuid("870af99c-171d-4f9e-af0d-e63df40c2bc9")) CPolicyConfigClient;

int wmain(int argc, wchar_t** argv) {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    IMMDeviceEnumerator* en = nullptr;
    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&en))) return 1;
    IMMDevice* def = nullptr; LPWSTR defId = nullptr;
    if (SUCCEEDED(en->GetDefaultAudioEndpoint(eRender, eConsole, &def))) def->GetId(&defId);
    IMMDeviceCollection* col = nullptr; en->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &col);
    UINT n = 0; col->GetCount(&n);
    std::wstring targetId;
    for (UINT i = 0; i < n; i++) {
        IMMDevice* d = nullptr; col->Item(i, &d);
        LPWSTR id = nullptr; d->GetId(&id);
        IPropertyStore* ps = nullptr; d->OpenPropertyStore(STGM_READ, &ps);
        PROPVARIANT name; PropVariantInit(&name); ps->GetValue(PKEY_Device_FriendlyName, &name);
        const bool isDef = defId && wcscmp(id, defId) == 0;
        if (argc < 2) std::printf("%s %ls\n", isDef ? "*" : " ", name.pwszVal);
        else if (targetId.empty() && wcsstr(name.pwszVal, argv[1])) { targetId = id; std::printf("target: %ls%s\n", name.pwszVal, isDef ? " (already default)" : ""); }
        PropVariantClear(&name); ps->Release(); CoTaskMemFree(id); d->Release();
    }
    if (argc < 2) return 0;
    if (targetId.empty()) { std::printf("no active render endpoint matches\n"); return 2; }
    IPolicyConfig* pc = nullptr;
    HRESULT hr = CoCreateInstance(__uuidof(CPolicyConfigClient), nullptr, CLSCTX_ALL, __uuidof(IPolicyConfig), (void**)&pc);
    if (FAILED(hr)) { std::printf("IPolicyConfig unavailable: 0x%08lx\n", (unsigned long)hr); return 3; }
    for (ERole r : {eConsole, eMultimedia, eCommunications}) {
        hr = pc->SetDefaultEndpoint(targetId.c_str(), r);
        if (FAILED(hr)) { std::printf("SetDefaultEndpoint(role %d) failed: 0x%08lx\n", (int)r, (unsigned long)hr); return 4; }
    }
    std::printf("default render endpoint switched\n");
    return 0;
}
