#pragma once

// Diagnostic zones are absent from normal builds, including their arguments.
#ifdef STAR_ENABLE_TRACY
#include <tracy/Tracy.hpp>
#define STAR_TRACE_ZONE(name) ZoneScopedN(name)
#define STAR_TRACE_FRAME(name) FrameMarkNamed(name)
#else
#define STAR_TRACE_ZONE(name) ((void)0)
#define STAR_TRACE_FRAME(name) ((void)0)
#endif
