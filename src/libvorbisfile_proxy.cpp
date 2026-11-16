#include <Windows.h>
#include <stdio.h>

typedef __int64        ogg_int64_t;
typedef void           OVF;
typedef void           vorbis_info;
typedef void           vorbis_comment;

typedef struct {
    void* read_func;
    void* seek_func;
    void* close_func;
    void* tell_func;
} ov_cb;

static HMODULE RealLib() {
    static HMODULE h = nullptr;
    if (!h) {
        char path[MAX_PATH];
        GetModuleFileNameA(GetModuleHandleA("libvorbisfile.dll"), path, MAX_PATH);
        char* s = strrchr(path, '\\');
        if (s) lstrcpyA(s + 1, "libvorbisfile_real.dll");
        h = LoadLibraryA(path);
    }
    return h;
}
static void* Sym(const char* n) { return (void*)GetProcAddress(RealLib(), n); }

extern "C" {

__declspec(dllexport)
long ov_bitrate(OVF* vf, int i) {
    static auto f = (long(__cdecl*)(OVF*, int))Sym("ov_bitrate");
    return f ? f(vf, i) : -1;
}
__declspec(dllexport)
long ov_bitrate_instant(OVF* vf) {
    static auto f = (long(__cdecl*)(OVF*))Sym("ov_bitrate_instant");
    return f ? f(vf) : -1;
}
__declspec(dllexport)
int ov_clear(OVF* vf) {
    static auto f = (int(__cdecl*)(OVF*))Sym("ov_clear");
    return f ? f(vf) : -1;
}
__declspec(dllexport)
vorbis_comment* ov_comment(OVF* vf, int link) {
    static auto f = (vorbis_comment*(__cdecl*)(OVF*, int))Sym("ov_comment");
    return f ? f(vf, link) : nullptr;
}
__declspec(dllexport)
int ov_crosslap(OVF* vf1, OVF* vf2) {
    static auto f = (int(__cdecl*)(OVF*, OVF*))Sym("ov_crosslap");
    return f ? f(vf1, vf2) : -1;
}
__declspec(dllexport)
int ov_halfrate(OVF* vf, int flag) {
    static auto f = (int(__cdecl*)(OVF*, int))Sym("ov_halfrate");
    return f ? f(vf, flag) : -1;
}
__declspec(dllexport)
int ov_halfrate_p(OVF* vf) {
    static auto f = (int(__cdecl*)(OVF*))Sym("ov_halfrate_p");
    return f ? f(vf) : -1;
}
__declspec(dllexport)
vorbis_info* ov_info(OVF* vf, int link) {
    static auto f = (vorbis_info*(__cdecl*)(OVF*, int))Sym("ov_info");
    return f ? f(vf, link) : nullptr;
}
__declspec(dllexport)
int ov_open(FILE* fp, OVF* vf, const char* initial, long ibytes) {
    static auto f = (int(__cdecl*)(FILE*, OVF*, const char*, long))Sym("ov_open");
    return f ? f(fp, vf, initial, ibytes) : -1;
}
__declspec(dllexport)
int ov_open_callbacks(void* ds, OVF* vf, const char* initial, long ibytes, ov_cb cb) {
    static auto f = (int(__cdecl*)(void*, OVF*, const char*, long, ov_cb))Sym("ov_open_callbacks");
    return f ? f(ds, vf, initial, ibytes, cb) : -1;
}
__declspec(dllexport)
int ov_pcm_seek(OVF* vf, ogg_int64_t pos) {
    static auto f = (int(__cdecl*)(OVF*, ogg_int64_t))Sym("ov_pcm_seek");
    return f ? f(vf, pos) : -1;
}
__declspec(dllexport)
int ov_pcm_seek_lap(OVF* vf, ogg_int64_t pos) {
    static auto f = (int(__cdecl*)(OVF*, ogg_int64_t))Sym("ov_pcm_seek_lap");
    return f ? f(vf, pos) : -1;
}
__declspec(dllexport)
int ov_pcm_seek_page(OVF* vf, ogg_int64_t pos) {
    static auto f = (int(__cdecl*)(OVF*, ogg_int64_t))Sym("ov_pcm_seek_page");
    return f ? f(vf, pos) : -1;
}
__declspec(dllexport)
int ov_pcm_seek_page_lap(OVF* vf, ogg_int64_t pos) {
    static auto f = (int(__cdecl*)(OVF*, ogg_int64_t))Sym("ov_pcm_seek_page_lap");
    return f ? f(vf, pos) : -1;
}
__declspec(dllexport)
ogg_int64_t ov_pcm_tell(OVF* vf) {
    static auto f = (ogg_int64_t(__cdecl*)(OVF*))Sym("ov_pcm_tell");
    return f ? f(vf) : -1;
}
__declspec(dllexport)
ogg_int64_t ov_pcm_total(OVF* vf, int i) {
    static auto f = (ogg_int64_t(__cdecl*)(OVF*, int))Sym("ov_pcm_total");
    return f ? f(vf, i) : -1;
}
__declspec(dllexport)
int ov_raw_seek(OVF* vf, ogg_int64_t pos) {
    static auto f = (int(__cdecl*)(OVF*, ogg_int64_t))Sym("ov_raw_seek");
    return f ? f(vf, pos) : -1;
}
__declspec(dllexport)
int ov_raw_seek_lap(OVF* vf, ogg_int64_t pos) {
    static auto f = (int(__cdecl*)(OVF*, ogg_int64_t))Sym("ov_raw_seek_lap");
    return f ? f(vf, pos) : -1;
}
__declspec(dllexport)
ogg_int64_t ov_raw_tell(OVF* vf) {
    static auto f = (ogg_int64_t(__cdecl*)(OVF*))Sym("ov_raw_tell");
    return f ? f(vf) : -1;
}
__declspec(dllexport)
ogg_int64_t ov_raw_total(OVF* vf, int i) {
    static auto f = (ogg_int64_t(__cdecl*)(OVF*, int))Sym("ov_raw_total");
    return f ? f(vf, i) : -1;
}
__declspec(dllexport)
long ov_read(OVF* vf, char* buf, int len, int bigend, int word, int sgned, int* bs) {
    static auto f = (long(__cdecl*)(OVF*, char*, int, int, int, int, int*))Sym("ov_read");
    return f ? f(vf, buf, len, bigend, word, sgned, bs) : -1;
}
__declspec(dllexport)
long ov_read_filter(OVF* vf, char* buf, int len, int bigend, int word, int sgned,
                    int* bs, void* filter, void* filter_param) {
    static auto f = (long(__cdecl*)(OVF*, char*, int, int, int, int, int*, void*, void*))Sym("ov_read_filter");
    return f ? f(vf, buf, len, bigend, word, sgned, bs, filter, filter_param) : -1;
}
__declspec(dllexport)
long ov_read_float(OVF* vf, float*** pcm, int samples, int* bs) {
    static auto f = (long(__cdecl*)(OVF*, float***, int, int*))Sym("ov_read_float");
    return f ? f(vf, pcm, samples, bs) : -1;
}
__declspec(dllexport)
int ov_serialnumber(OVF* vf, int i) {
    static auto f = (int(__cdecl*)(OVF*, int))Sym("ov_serialnumber");
    return f ? f(vf, i) : -1;
}
__declspec(dllexport)
long ov_streams(OVF* vf) {
    static auto f = (long(__cdecl*)(OVF*))Sym("ov_streams");
    return f ? f(vf) : -1;
}
__declspec(dllexport)
int ov_test(FILE* fp, OVF* vf, const char* initial, long ibytes) {
    static auto f = (int(__cdecl*)(FILE*, OVF*, const char*, long))Sym("ov_test");
    return f ? f(fp, vf, initial, ibytes) : -1;
}
__declspec(dllexport)
int ov_test_callbacks(void* ds, OVF* vf, const char* initial, long ibytes, ov_cb cb) {
    static auto f = (int(__cdecl*)(void*, OVF*, const char*, long, ov_cb))Sym("ov_test_callbacks");
    return f ? f(ds, vf, initial, ibytes, cb) : -1;
}
__declspec(dllexport)
int ov_test_open(OVF* vf) {
    static auto f = (int(__cdecl*)(OVF*))Sym("ov_test_open");
    return f ? f(vf) : -1;
}
__declspec(dllexport)
int ov_time_seek(OVF* vf, double pos) {
    static auto f = (int(__cdecl*)(OVF*, double))Sym("ov_time_seek");
    return f ? f(vf, pos) : -1;
}
__declspec(dllexport)
int ov_time_seek_lap(OVF* vf, double pos) {
    static auto f = (int(__cdecl*)(OVF*, double))Sym("ov_time_seek_lap");
    return f ? f(vf, pos) : -1;
}
__declspec(dllexport)
int ov_time_seek_page(OVF* vf, double pos) {
    static auto f = (int(__cdecl*)(OVF*, double))Sym("ov_time_seek_page");
    return f ? f(vf, pos) : -1;
}
__declspec(dllexport)
int ov_time_seek_page_lap(OVF* vf, double pos) {
    static auto f = (int(__cdecl*)(OVF*, double))Sym("ov_time_seek_page_lap");
    return f ? f(vf, pos) : -1;
}
__declspec(dllexport)
double ov_time_tell(OVF* vf) {
    static auto f = (double(__cdecl*)(OVF*))Sym("ov_time_tell");
    return f ? f(vf) : -1.0;
}
__declspec(dllexport)
double ov_time_total(OVF* vf, int i) {
    static auto f = (double(__cdecl*)(OVF*, int))Sym("ov_time_total");
    return f ? f(vf, i) : -1.0;
}

} // extern "C"
