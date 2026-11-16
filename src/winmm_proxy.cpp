/*
 * winmm.dll proxy — tum cagrilari gercek system winmm.dll'e yonlendirir
 * DllMain dllmain.cpp'de, burasi sadece export forwarding
 */
#include <Windows.h>
#include <mmsystem.h>

static HMODULE RealWinmm() {
    static HMODULE h = nullptr;
    if (!h) {
        char path[MAX_PATH];
        GetSystemDirectoryA(path, sizeof(path));
        lstrcatA(path, "\\winmm.dll");
        h = LoadLibraryA(path);
    }
    return h;
}
static FARPROC Sym(const char* name) {
    return GetProcAddress(RealWinmm(), name);
}

// ---- Timer ------------------------------------------------------------------
extern "C" __declspec(dllexport)
DWORD WINAPI timeGetTime() {
    static auto fn = (DWORD(WINAPI*)())Sym("timeGetTime");
    return fn ? fn() : 0;
}
extern "C" __declspec(dllexport)
MMRESULT WINAPI timeBeginPeriod(UINT u) {
    static auto fn = (MMRESULT(WINAPI*)(UINT))Sym("timeBeginPeriod");
    return fn ? fn(u) : TIMERR_NOERROR;
}
extern "C" __declspec(dllexport)
MMRESULT WINAPI timeEndPeriod(UINT u) {
    static auto fn = (MMRESULT(WINAPI*)(UINT))Sym("timeEndPeriod");
    return fn ? fn(u) : TIMERR_NOERROR;
}
extern "C" __declspec(dllexport)
MMRESULT WINAPI timeGetDevCaps(LPTIMECAPS ptc, UINT cbtc) {
    static auto fn = (MMRESULT(WINAPI*)(LPTIMECAPS,UINT))Sym("timeGetDevCaps");
    return fn ? fn(ptc, cbtc) : TIMERR_NOERROR;
}
extern "C" __declspec(dllexport)
MMRESULT WINAPI timeSetEvent(UINT uDelay, UINT uRes, LPTIMECALLBACK cb, DWORD_PTR dwUser, UINT fu) {
    static auto fn = (MMRESULT(WINAPI*)(UINT,UINT,LPTIMECALLBACK,DWORD_PTR,UINT))Sym("timeSetEvent");
    return fn ? fn(uDelay, uRes, cb, dwUser, fu) : 0;
}
extern "C" __declspec(dllexport)
MMRESULT WINAPI timeKillEvent(UINT id) {
    static auto fn = (MMRESULT(WINAPI*)(UINT))Sym("timeKillEvent");
    return fn ? fn(id) : TIMERR_NOERROR;
}

// ---- Sound ------------------------------------------------------------------
extern "C" __declspec(dllexport)
BOOL WINAPI PlaySoundA(LPCSTR p, HMODULE h, DWORD f) {
    static auto fn = (BOOL(WINAPI*)(LPCSTR,HMODULE,DWORD))Sym("PlaySoundA");
    return fn ? fn(p, h, f) : FALSE;
}
extern "C" __declspec(dllexport)
BOOL WINAPI PlaySoundW(LPCWSTR p, HMODULE h, DWORD f) {
    static auto fn = (BOOL(WINAPI*)(LPCWSTR,HMODULE,DWORD))Sym("PlaySoundW");
    return fn ? fn(p, h, f) : FALSE;
}
extern "C" __declspec(dllexport)
BOOL WINAPI sndPlaySoundA(LPCSTR p, UINT f) {
    static auto fn = (BOOL(WINAPI*)(LPCSTR,UINT))Sym("sndPlaySoundA");
    return fn ? fn(p, f) : FALSE;
}
extern "C" __declspec(dllexport)
BOOL WINAPI sndPlaySoundW(LPCWSTR p, UINT f) {
    static auto fn = (BOOL(WINAPI*)(LPCWSTR,UINT))Sym("sndPlaySoundW");
    return fn ? fn(p, f) : FALSE;
}

// ---- MCI --------------------------------------------------------------------
extern "C" __declspec(dllexport)
MCIERROR WINAPI mciSendCommandA(MCIDEVICEID id, UINT msg, DWORD_PTR p1, DWORD_PTR p2) {
    static auto fn = (MCIERROR(WINAPI*)(MCIDEVICEID,UINT,DWORD_PTR,DWORD_PTR))Sym("mciSendCommandA");
    return fn ? fn(id, msg, p1, p2) : MCIERR_UNSUPPORTED_FUNCTION;
}
extern "C" __declspec(dllexport)
MCIERROR WINAPI mciSendCommandW(MCIDEVICEID id, UINT msg, DWORD_PTR p1, DWORD_PTR p2) {
    static auto fn = (MCIERROR(WINAPI*)(MCIDEVICEID,UINT,DWORD_PTR,DWORD_PTR))Sym("mciSendCommandW");
    return fn ? fn(id, msg, p1, p2) : MCIERR_UNSUPPORTED_FUNCTION;
}
extern "C" __declspec(dllexport)
MCIERROR WINAPI mciSendStringA(LPCSTR cmd, LPSTR ret, UINT retLen, HWND cb) {
    static auto fn = (MCIERROR(WINAPI*)(LPCSTR,LPSTR,UINT,HWND))Sym("mciSendStringA");
    return fn ? fn(cmd, ret, retLen, cb) : MCIERR_UNSUPPORTED_FUNCTION;
}
extern "C" __declspec(dllexport)
MCIERROR WINAPI mciSendStringW(LPCWSTR cmd, LPWSTR ret, UINT retLen, HWND cb) {
    static auto fn = (MCIERROR(WINAPI*)(LPCWSTR,LPWSTR,UINT,HWND))Sym("mciSendStringW");
    return fn ? fn(cmd, ret, retLen, cb) : MCIERR_UNSUPPORTED_FUNCTION;
}
extern "C" __declspec(dllexport)
BOOL WINAPI mciGetErrorStringA(MCIERROR err, LPSTR text, UINT len) {
    static auto fn = (BOOL(WINAPI*)(MCIERROR,LPSTR,UINT))Sym("mciGetErrorStringA");
    return fn ? fn(err, text, len) : FALSE;
}
extern "C" __declspec(dllexport)
BOOL WINAPI mciGetErrorStringW(MCIERROR err, LPWSTR text, UINT len) {
    static auto fn = (BOOL(WINAPI*)(MCIERROR,LPWSTR,UINT))Sym("mciGetErrorStringW");
    return fn ? fn(err, text, len) : FALSE;
}

// ---- WaveOut ----------------------------------------------------------------
extern "C" __declspec(dllexport)
UINT WINAPI waveOutGetNumDevs() {
    static auto fn = (UINT(WINAPI*)())Sym("waveOutGetNumDevs");
    return fn ? fn() : 0;
}
extern "C" __declspec(dllexport)
MMRESULT WINAPI waveOutOpen(LPHWAVEOUT phwo, UINT dev, LPCWAVEFORMATEX fmt,
                            DWORD_PTR cb, DWORD_PTR inst, DWORD flags) {
    static auto fn = (MMRESULT(WINAPI*)(LPHWAVEOUT,UINT,LPCWAVEFORMATEX,DWORD_PTR,DWORD_PTR,DWORD))Sym("waveOutOpen");
    return fn ? fn(phwo, dev, fmt, cb, inst, flags) : MMSYSERR_NODRIVER;
}
extern "C" __declspec(dllexport)
MMRESULT WINAPI waveOutClose(HWAVEOUT hwo) {
    static auto fn = (MMRESULT(WINAPI*)(HWAVEOUT))Sym("waveOutClose");
    return fn ? fn(hwo) : MMSYSERR_NODRIVER;
}
extern "C" __declspec(dllexport)
MMRESULT WINAPI waveOutWrite(HWAVEOUT hwo, LPWAVEHDR pwh, UINT cbwh) {
    static auto fn = (MMRESULT(WINAPI*)(HWAVEOUT,LPWAVEHDR,UINT))Sym("waveOutWrite");
    return fn ? fn(hwo, pwh, cbwh) : MMSYSERR_NODRIVER;
}
extern "C" __declspec(dllexport)
MMRESULT WINAPI waveOutPrepareHeader(HWAVEOUT hwo, LPWAVEHDR pwh, UINT cbwh) {
    static auto fn = (MMRESULT(WINAPI*)(HWAVEOUT,LPWAVEHDR,UINT))Sym("waveOutPrepareHeader");
    return fn ? fn(hwo, pwh, cbwh) : MMSYSERR_NODRIVER;
}
extern "C" __declspec(dllexport)
MMRESULT WINAPI waveOutUnprepareHeader(HWAVEOUT hwo, LPWAVEHDR pwh, UINT cbwh) {
    static auto fn = (MMRESULT(WINAPI*)(HWAVEOUT,LPWAVEHDR,UINT))Sym("waveOutUnprepareHeader");
    return fn ? fn(hwo, pwh, cbwh) : MMSYSERR_NODRIVER;
}
extern "C" __declspec(dllexport)
MMRESULT WINAPI waveOutGetVolume(HWAVEOUT hwo, LPDWORD pdwVolume) {
    static auto fn = (MMRESULT(WINAPI*)(HWAVEOUT,LPDWORD))Sym("waveOutGetVolume");
    return fn ? fn(hwo, pdwVolume) : MMSYSERR_NODRIVER;
}
extern "C" __declspec(dllexport)
MMRESULT WINAPI waveOutSetVolume(HWAVEOUT hwo, DWORD dwVolume) {
    static auto fn = (MMRESULT(WINAPI*)(HWAVEOUT,DWORD))Sym("waveOutSetVolume");
    return fn ? fn(hwo, dwVolume) : MMSYSERR_NODRIVER;
}
extern "C" __declspec(dllexport)
MMRESULT WINAPI waveOutGetDevCapsA(UINT_PTR dev, LPWAVEOUTCAPSA caps, UINT size) {
    static auto fn = (MMRESULT(WINAPI*)(UINT_PTR,LPWAVEOUTCAPSA,UINT))Sym("waveOutGetDevCapsA");
    return fn ? fn(dev, caps, size) : MMSYSERR_NODRIVER;
}
extern "C" __declspec(dllexport)
MMRESULT WINAPI waveOutGetDevCapsW(UINT_PTR dev, LPWAVEOUTCAPSW caps, UINT size) {
    static auto fn = (MMRESULT(WINAPI*)(UINT_PTR,LPWAVEOUTCAPSW,UINT))Sym("waveOutGetDevCapsW");
    return fn ? fn(dev, caps, size) : MMSYSERR_NODRIVER;
}
extern "C" __declspec(dllexport)
MMRESULT WINAPI waveOutMessage(HWAVEOUT hwo, UINT msg, DWORD_PTR p1, DWORD_PTR p2) {
    static auto fn = (MMRESULT(WINAPI*)(HWAVEOUT,UINT,DWORD_PTR,DWORD_PTR))Sym("waveOutMessage");
    return fn ? fn(hwo, msg, p1, p2) : MMSYSERR_NODRIVER;
}

// ---- WaveIn -----------------------------------------------------------------
extern "C" __declspec(dllexport)
UINT WINAPI waveInGetNumDevs() {
    static auto fn = (UINT(WINAPI*)())Sym("waveInGetNumDevs");
    return fn ? fn() : 0;
}
extern "C" __declspec(dllexport)
MMRESULT WINAPI waveInOpen(LPHWAVEIN phwi, UINT dev, LPCWAVEFORMATEX fmt,
                           DWORD_PTR cb, DWORD_PTR inst, DWORD flags) {
    static auto fn = (MMRESULT(WINAPI*)(LPHWAVEIN,UINT,LPCWAVEFORMATEX,DWORD_PTR,DWORD_PTR,DWORD))Sym("waveInOpen");
    return fn ? fn(phwi, dev, fmt, cb, inst, flags) : MMSYSERR_NODRIVER;
}
extern "C" __declspec(dllexport)
MMRESULT WINAPI waveInClose(HWAVEIN hwi) {
    static auto fn = (MMRESULT(WINAPI*)(HWAVEIN))Sym("waveInClose");
    return fn ? fn(hwi) : MMSYSERR_NODRIVER;
}
extern "C" __declspec(dllexport)
MMRESULT WINAPI waveInStart(HWAVEIN hwi) {
    static auto fn = (MMRESULT(WINAPI*)(HWAVEIN))Sym("waveInStart");
    return fn ? fn(hwi) : MMSYSERR_NODRIVER;
}
extern "C" __declspec(dllexport)
MMRESULT WINAPI waveInStop(HWAVEIN hwi) {
    static auto fn = (MMRESULT(WINAPI*)(HWAVEIN))Sym("waveInStop");
    return fn ? fn(hwi) : MMSYSERR_NODRIVER;
}
extern "C" __declspec(dllexport)
MMRESULT WINAPI waveInReset(HWAVEIN hwi) {
    static auto fn = (MMRESULT(WINAPI*)(HWAVEIN))Sym("waveInReset");
    return fn ? fn(hwi) : MMSYSERR_NODRIVER;
}
