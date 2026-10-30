#include "DetectorTypes.h"
#if defined(__SSE__) || defined(__x86_64__) || defined(_M_X64)
#include <xmmintrin.h>
#endif
namespace dawchecker::dsp {
void enableFlushDenormalsForThisThread() noexcept
{
#if defined(__SSE__) || defined(__x86_64__) || defined(_M_X64)
    _mm_setcsr(_mm_getcsr() | 0x8040u);   // FTZ (bit 15) | DAZ (bit 6); affects the calling thread only
#endif
}
} // namespace dawchecker::dsp
