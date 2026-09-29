// suzu.cpp — the one platform-specific piece of Suzu: the rendering thread's
// floating-point mode (SYNTH §2.4, §4). Everything else in suzu.h is inline.
#include "suzu.h"

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
  #include <immintrin.h>
  #define SUZU_X86 1
#elif defined(__aarch64__) || defined(_M_ARM64)
  #define SUZU_ARM64 1
#endif

namespace suzu {

#if defined(SUZU_X86)
void ftz_enable()  { _mm_setcsr(_mm_getcsr() | 0x8040u); }         // FTZ (bit 15) | DAZ (bit 6)
void ftz_disable() { _mm_setcsr(_mm_getcsr() & ~0x8040u); }
bool ftz_enabled() { return (_mm_getcsr() & 0x8040u) == 0x8040u; }
#elif defined(SUZU_ARM64) && !defined(_MSC_VER)
static inline uint64_t read_fpcr() { uint64_t v; __asm__ volatile("mrs %0, fpcr" : "=r"(v)); return v; }
static inline void write_fpcr(uint64_t v) { __asm__ volatile("msr fpcr, %0" : : "r"(v)); }
void ftz_enable()  { write_fpcr(read_fpcr() | (1ull << 24)); }    // FZ: flush-to-zero (inputs and outputs)
void ftz_disable() { write_fpcr(read_fpcr() & ~(1ull << 24)); }
bool ftz_enabled() { return (read_fpcr() & (1ull << 24)) != 0; }
#elif defined(SUZU_ARM64) && defined(_MSC_VER)
  #include <intrin.h>
void ftz_enable()  { _WriteStatusReg(ARM64_FPCR, _ReadStatusReg(ARM64_FPCR) | (1ull << 24)); }
void ftz_disable() { _WriteStatusReg(ARM64_FPCR, _ReadStatusReg(ARM64_FPCR) & ~(1ull << 24)); }
bool ftz_enabled() { return (_ReadStatusReg(ARM64_FPCR) & (1ull << 24)) != 0; }
#else
void ftz_enable()  {}
void ftz_disable() {}
bool ftz_enabled() { return false; }
#endif

} // namespace suzu
