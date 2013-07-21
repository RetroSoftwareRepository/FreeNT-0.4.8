
#include "DibLib.h"

/* This is a fast xlateless memmove version that can be used for R2L as well */
VOID
FASTCALL
Dib_BitBlt_SRCCOPY_EqSurf(PBLTDATA pBltData)
{
    ULONG cLines, cjWidth;
    PBYTE pjDestBase = pBltData->siDst.pjBase;
    PBYTE pjSrcBase = pBltData->siSrc.pjBase;

    /* Calculate the width in bytes */
    cjWidth = pBltData->ulWidth * pBltData->siDst.jBpp / 8;

    /* Loop all lines */
    cLines = pBltData->ulHeight;
    while (cLines--)
    {
        /* Use memmove to handle copying right-to-left with overlap */
        memmove(pjDestBase, pjSrcBase, cjWidth);
        pjDestBase += pBltData->siDst.cjAdvanceY;
        pjSrcBase += pBltData->siSrc.cjAdvanceY;
    }
}

VOID
FASTCALL
Dib_BitBlt_SRCCOPY_EqSurfR2L(PBLTDATA pBltData)
{
    ULONG cLines, cjWidth;
    PBYTE pjDestBase = pBltData->siDst.pjBase;
    PBYTE pjSrcBase = pBltData->siSrc.pjBase;

    /* Calculate the width in bytes */
    cjWidth = pBltData->ulWidth * pBltData->siDst.jBpp / 8;

    pjDestBase -= (cjWidth - 1);
    pjSrcBase -= (cjWidth - 1);

    /* Loop all lines */
    cLines = pBltData->ulHeight;
    while (cLines--)
    {
        /* Use memmove to handle copying right-to-left with overlap */
        memmove(pjDestBase, pjSrcBase, cjWidth);
        pjDestBase += pBltData->siDst.cjAdvanceY;
        pjSrcBase += pBltData->siSrc.cjAdvanceY;
    }
}

#define Dib_BitBlt_SRCCOPY_S8_D8_EqSurf Dib_BitBlt_SRCCOPY_EqSurf
#define Dib_BitBlt_SRCCOPY_S16_D16_EqSurf Dib_BitBlt_SRCCOPY_EqSurf
#define Dib_BitBlt_SRCCOPY_S24_D24_EqSurf Dib_BitBlt_SRCCOPY_EqSurf
#define Dib_BitBlt_SRCCOPY_S8_D8_EqSurfR2L Dib_BitBlt_SRCCOPY_EqSurfR2L
#define Dib_BitBlt_SRCCOPY_S16_D16_EqSurfR2L Dib_BitBlt_SRCCOPY_EqSurfR2L
#define Dib_BitBlt_SRCCOPY_S24_D24_EqSurfR2L Dib_BitBlt_SRCCOPY_EqSurfR2L
#define Dib_BitBlt_SRCCOPY_S32_D32_EqSurfR2L Dib_BitBlt_SRCCOPY_EqSurfR2L

/* special movsd optimization on x86, only for left-to-right */
#if defined(_M_IX86) || defined(_M_AMD64)
VOID
FASTCALL
Dib_BitBlt_SRCCOPY_S32_D32_EqSurf(PBLTDATA pBltData)
{
    ULONG cLines, cRows;
    PBYTE pjDestBase = pBltData->siDst.pjBase;
    PBYTE pjSrcBase = pBltData->siSrc.pjBase;

    /* Loop all lines */
    cRows = pBltData->ulWidth;
    cLines = pBltData->ulHeight;
    while (cLines--)
    {
        __movsd((PULONG)pjDestBase, (PULONG)pjSrcBase, cRows);
        pjDestBase += pBltData->siDst.cjAdvanceY;
        pjSrcBase += pBltData->siSrc.cjAdvanceY;
    }
}
#define Dib_BitBlt_SRCCOPY_S32_D32_EqSurf_manual 1
#else
#define Dib_BitBlt_SRCCOPY_S32_D32_EqSurf Dib_BitBlt_SRCCOPY_EqSurf
#endif

/* This definition will be checked against in DibLib_BitBlt.h
   for all "redirected" functions */
#define Dib_BitBlt_SRCCOPY_EqSurf_manual 1
#define Dib_BitBlt_SRCCOPY_EqSurfR2L_manual 1

#define __USES_SOURCE 1
#define __USES_PATTERN 0
#define __USES_DEST 0
#define __USES_MASK 0

#define __FUNCTIONNAME BitBlt_SRCCOPY

#define _DibDoRop(pBltData, M, D, S, P) ROP_SRCCOPY(D,S,P)

#include "DibLib_AllSrcBPP.h"

VOID
FASTCALL
Dib_BitBlt_SRCCOPY(PBLTDATA pBltData)
{
    gapfnBitBlt_SRCCOPY[pBltData->siDst.iFormat][pBltData->siSrc.iFormat](pBltData);
}

