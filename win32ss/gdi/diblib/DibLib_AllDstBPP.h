
#ifndef __DIB_FUNCTION_NAME
#define __DIB_FUNCTION_NAME __DIB_FUNCTION_NAME_DST
#endif

#ifndef _SOURCE_BPP
#define _SOURCE_BPP 0
#endif

#define _DEST_BPP 1
#define _DEST_MASK 1
#include "DibLib_BitBlt.h"
#undef _DEST_BPP

#define _DEST_BPP 4
#define _DEST_MASK 0xf
#include "DibLib_BitBlt.h"
#undef _DEST_BPP

#define _DEST_BPP 8
#define _DEST_MASK 0xff
#include "DibLib_BitBlt.h"
#undef _DEST_BPP

#define _DEST_BPP 16
#define _DEST_MASK 0xffff
#include "DibLib_BitBlt.h"
#undef _DEST_BPP

#define _DEST_BPP 24
#define _DEST_MASK 0xffffff
#include "DibLib_BitBlt.h"
#undef _DEST_BPP

#define _DEST_BPP 32
#define _DEST_MASK 0xffffffff
#include "DibLib_BitBlt.h"
#undef _DEST_BPP

#if (__USES_SOURCE == 0)
PFN_DIBFUNCTION
__PASTE(gapfn, __FUNCTIONNAME)[7] =
{
    0,
    __DIB_FUNCTION_NAME_DST(__FUNCTIONNAME, 0, 1),
    __DIB_FUNCTION_NAME_DST(__FUNCTIONNAME, 0, 4),
    __DIB_FUNCTION_NAME_DST(__FUNCTIONNAME, 0, 8),
    __DIB_FUNCTION_NAME_DST(__FUNCTIONNAME, 0, 16),
    __DIB_FUNCTION_NAME_DST(__FUNCTIONNAME, 0, 24),
    __DIB_FUNCTION_NAME_DST(__FUNCTIONNAME, 0, 32)
};
#endif
