
#include <win32k.h>
#include "../diblib/DibLib_interface.h"
DBG_DEFAULT_CHANNEL(GdiFont);

#define SURFOBJ_flags(pso) (CONTAINING_RECORD(pso, SURFACE, SurfObj)->flags)

#if 0
// FIXME this needs to be updated, once we use the new structure
XCLIPOBJ gxcoTrivial =
{
    {0, {LONG_MIN, LONG_MIN, LONG_MAX, LONG_MAX}, DC_TRIVIAL, FC_RECT, TC_RECTANGLES, 0},
    0, 0, 0
};
#endif


BOOL
APIENTRY
EngSrcBufferedBitBlt (
    _Inout_ SURFOBJ *psoTrg,
    _In_ SURFOBJ *psoSrc,
    _In_opt_ SURFOBJ *psoMask,
    _In_opt_ CLIPOBJ *pco,
    _In_opt_ XLATEOBJ *pxlo,
    _In_ RECTL *prclTrg,
    _When_(psoSrc, _In_) POINTL *pptlSrc,
    _When_(psoMask, _In_) POINTL *pptlMask,
    _In_opt_ BRUSHOBJ *pbo,
    _When_(pbo, _In_) POINTL *pptlBrush,
    _In_ ROP4 rop4)
{
    BOOL bResult;
    PSURFACE psurfTmp;
    SURFOBJ* psoTmp;
    RECTL rclTmp;
    ULONG iTmpBitmapFormat;

    /* Setup the target rect for the temp surface */
    rclTmp.left = 0;
    rclTmp.top = 0;
    rclTmp.right = prclTrg->right - prclTrg->left;
    rclTmp.bottom = prclTrg->bottom - prclTrg->top;

    /* Get the bitmap format for the temp surface */
    iTmpBitmapFormat = psoSrc->iBitmapFormat;
    if (iTmpBitmapFormat == BMF_4RLE) iTmpBitmapFormat = BMF_4BPP;
    if (iTmpBitmapFormat == BMF_8RLE) iTmpBitmapFormat = BMF_8BPP;

    /* Allocate a surface */
    psurfTmp = SURFACE_AllocSurface(STYPE_BITMAP,
                                    rclTmp.right,
                                    rclTmp.bottom,
                                    iTmpBitmapFormat,
                                    0,
                                    0,
                                    NULL);
    if (psurfTmp == NULL)
    {
        ERR("Failed to allocate a surface\n");
        return FALSE;
    }

    psoTmp = &psurfTmp->SurfObj;

    /* Check if we have an RLE compressed source */
    if ((psoSrc->iBitmapFormat == BMF_4RLE) ||
        (psoSrc->iBitmapFormat == BMF_8RLE))
    {
        SIZEL sizl;

        /* Decompress the bitmap */
        sizl.cx = rclTmp.right;
        sizl.cy = rclTmp.bottom;
        DecompressBitmap(sizl,
                         psoSrc->pvBits,
                         psoTmp->pvBits,
                         psoTmp->lDelta,
                         psoSrc->iBitmapFormat);
        bResult = TRUE;
    }
    else if ((psoSrc->iBitmapFormat > BMF_8RLE))
    {
        ERR("Bitmap format %d is not supported!\n", psoSrc->iBitmapFormat);
        return FALSE;
    }
    else
    {
        /* Copy the bits to the temp surface */
        bResult = EngCopyBits(psoTmp, psoSrc, NULL, NULL, &rclTmp, pptlSrc);
    }

    if (bResult)
    {
        /* Do the actual operation from the temp surface */
        bResult = IntEngBitBlt(psoTrg,
                               psoTmp,
                               psoMask,
                               pco,
                               pxlo,
                               prclTrg,
                               (PPOINTL)&rclTmp,
                               pptlMask,
                               pbo,
                               pptlBrush,
                               rop4);
    }

    /* Delete the temp surface */
    GDIOBJ_vDeleteObject(&psurfTmp->BaseObject);

    return bResult;
}

BOOL
APIENTRY
EngTrgBufferedBitBlt (
    _Inout_ SURFOBJ *psoTrg,
    _In_opt_ SURFOBJ *psoSrc,
    _In_opt_ SURFOBJ *psoMask,
    _In_opt_ CLIPOBJ *pco,
    _In_opt_ XLATEOBJ *pxlo,
    _In_ RECTL *prclTrg,
    _When_(psoSrc, _In_) POINTL *pptlSrc,
    _When_(psoMask, _In_) POINTL *pptlMask,
    _In_opt_ BRUSHOBJ *pbo,
    _When_(pbo, _In_) POINTL *pptlBrush,
    _In_ ROP4 rop4)
{
    BOOL bResult = TRUE;
    PSURFACE psurfTmp;
    SURFOBJ* psoTmp;
    RECTL rclTmp;
    ULONG iTmpBitmapFormat;

    /* Setup the target rect for the temp surface */
    rclTmp.left = 0;
    rclTmp.top = 0;
    rclTmp.right = prclTrg->right - prclTrg->left;
    rclTmp.bottom = prclTrg->bottom - prclTrg->top;

    /* Get the bitmap format for the temp surface */
    iTmpBitmapFormat = psoTrg->iBitmapFormat;

    /* Allocate a surface */
    psurfTmp = SURFACE_AllocSurface(STYPE_BITMAP,
                                    rclTmp.right,
                                    rclTmp.bottom,
                                    iTmpBitmapFormat,
                                    0,
                                    0,
                                    NULL);
    if (psurfTmp == NULL)
    {
        ERR("Failed to allocate a surface\n");
        return FALSE;
    }

    psoTmp = &psurfTmp->SurfObj;

    /* Check if the ROP uses the target pixels */
    if (ROP4_USES_DEST(rop4))
    {
        /* Copy the target bits to the target surface */
        bResult = EngCopyBits(psoTmp, psoSrc, NULL, NULL, &rclTmp, (PPOINTL)prclTrg);
    }

    if (bResult)
    {
        /* Do the actual operation to the temp surface */
        bResult = IntEngBitBlt(psoTmp,
                               psoSrc,
                               psoMask,
                               pco,
                               pxlo,
                               &rclTmp,
                               pptlSrc,
                               pptlMask,
                               pbo,
                               pptlBrush,
                               rop4);
    }

    if (bResult)
    {
        /* Copy the bits to the target surface */
        bResult = EngCopyBits(psoTrg, psoTmp, NULL, NULL, prclTrg, (PPOINTL)&rclTmp);
    }

    /* Delete the temp surface */
    GDIOBJ_vDeleteObject(&psurfTmp->BaseObject);

    return bResult;
}

static
void
CalculateCoordinates(
    PBLTDATA pbltdata,
    PRECTL prclClipped,
    PRECTL prclOrg,
    PPOINTL pptlSrc,
    PPOINTL pptlMask,
    PPOINTL pptlPat,
    PSIZEL psizlPat)
{
    ULONG cx, cy;

    /* Calculate width and height of this rect */
    pbltdata->ulWidth = prclClipped->right - prclClipped->left;
    pbltdata->ulHeight = prclClipped->bottom - prclClipped->top;

    /* Calculate the x offset to the origin coordinates */
    if (pbltdata->siDst.iFormat == 0)
        cx = (prclClipped->right - 1 - prclOrg->left);
    else
        cx = (prclClipped->left - prclOrg->left);

    /* Calculate the y offset to the origin coordinates */
    if (pbltdata->dy < 0)
        cy = (prclClipped->bottom - 1 - prclOrg->top);
    else
        cy = (prclClipped->top - prclOrg->top);

    /* Calculate the target start point */
    pbltdata->siDst.ptOrig.x = prclOrg->left + cx;
    pbltdata->siDst.ptOrig.y = prclOrg->top + cy;

    /* Calculate start position for target */
    pbltdata->siDst.pjBase = pbltdata->siDst.pvScan0;
    pbltdata->siDst.pjBase += pbltdata->siDst.ptOrig.y * pbltdata->siDst.lDelta;
    pbltdata->siDst.pjBase += pbltdata->siDst.ptOrig.x * pbltdata->siDst.jBpp / 8;

    if (pptlSrc)
    {
        /* Calculate start point and bitpointer for source */
        pbltdata->siSrc.ptOrig.x = pptlSrc->x + cx;
        pbltdata->siSrc.ptOrig.y = pptlSrc->y + cy;
        pbltdata->siSrc.pjBase = pbltdata->siSrc.pvScan0;
        pbltdata->siSrc.pjBase += pbltdata->siSrc.ptOrig.y * pbltdata->siSrc.lDelta;
        pbltdata->siSrc.pjBase += pbltdata->siSrc.ptOrig.x * pbltdata->siSrc.jBpp / 8;
    }

    if (pptlMask)
    {
        /* Calculate start point and bitpointer for mask */
        pbltdata->siMsk.ptOrig.x = pptlMask->x + cx;
        pbltdata->siMsk.ptOrig.y = pptlMask->y + cy;
        pbltdata->siMsk.pjBase = pbltdata->siMsk.pvScan0;
        pbltdata->siMsk.pjBase += pbltdata->siMsk.ptOrig.y * pbltdata->siMsk.lDelta;
        pbltdata->siMsk.pjBase += pbltdata->siMsk.ptOrig.x * pbltdata->siMsk.jBpp / 8;
    }

    if (pptlPat)
    {
        /* Calculate start point and bitpointer for pattern */
        pbltdata->siPat.ptOrig.x = (pptlPat->x + cx) % psizlPat->cx;
        pbltdata->siPat.ptOrig.y = (pptlPat->y + cy) % psizlPat->cy;
        pbltdata->siPat.pjBase = pbltdata->siPat.pvScan0;

        /* Check for bottom-up case */
        if (pbltdata->dy < 0)
        {
            pbltdata->siPat.pjBase += (psizlPat->cy - 1) * pbltdata->siPat.lDelta;
            pbltdata->siPat.ptOrig.y = psizlPat->cy - 1 - pbltdata->siPat.ptOrig.y;
        }

        /* Check for right-to-left case */
        if (pbltdata->siDst.iFormat == 0)
        {
            pbltdata->siPat.pjBase += (psizlPat->cx - 1) * pbltdata->siMsk.jBpp / 8;
            pbltdata->siPat.ptOrig.x = psizlPat->cx - 1 - pbltdata->siPat.ptOrig.x;
        }
    }
}

BOOL
APIENTRY
EngBitBlt (
    _Inout_ SURFOBJ *psoTrg,
    _In_opt_ SURFOBJ *psoSrc,
    _In_opt_ SURFOBJ *psoMask,
    _In_opt_ CLIPOBJ *pco,
    _In_opt_ XLATEOBJ *pxlo,
    _In_ RECTL *prclTrg,
    _When_(psoSrc, _In_) POINTL *pptlSrc,
    _When_(psoMask, _In_) POINTL *pptlMask,
    _In_opt_ BRUSHOBJ *pbo,
    _When_(pbo, _In_) POINTL *pptlBrush,
    _In_ ROP4 rop4)
{
    BLTDATA bltdata;
    ULONG i, iFunctionIndex, iDirection;
    PFN_DIBFUNCTION pfnBitBlt;
    BOOL bEnumMore;
    RECT_ENUM rcenum;
    PSIZEL psizlPat;
    SURFOBJ *psoPattern;
    PFN_DrvCopyBits pfnCopyBits;

//static int count = 0;
//if (++count >= 1230) __debugbreak();

    /* Sanity checks */
    NT_ASSERT(psoTrg);
    NT_ASSERT(psoTrg->iBitmapFormat >= BMF_1BPP);
    NT_ASSERT(psoTrg->iBitmapFormat <= BMF_32BPP);
    NT_ASSERT(prclTrg);
    NT_ASSERT(prclTrg->left >= 0);
    NT_ASSERT(prclTrg->top >= 0);
    NT_ASSERT(prclTrg->right <= psoTrg->sizlBitmap.cx);
    NT_ASSERT(prclTrg->bottom <= psoTrg->sizlBitmap.cy);
    ASSERT_DEVLOCK(psoTrg);
    ASSERT_DEVLOCK(psoSrc);
    ASSERT_DEVLOCK(psoMask);

    /* Check if the target is not a bitmap */
    if (psoTrg->iType != STYPE_BITMAP)
    {
        /* Check for special case SRCCOPY to device */
        if ((rop4 == ROP4_SRCCOPY) &&
            ((psoSrc->iType == STYPE_BITMAP) || (psoSrc->hdev == psoTrg->hdev)))
        {
            /* Use the driver's DrvCopyBits */
            pfnCopyBits = GDIDEVFUNCS(psoTrg).CopyBits;
            return pfnCopyBits(psoTrg, psoSrc, pco, pxlo, prclTrg, pptlSrc);
        }

        /* We need to do a buffered bitblt */
        return EngTrgBufferedBitBlt(psoTrg,
                                    psoSrc,
                                    psoMask,
                                    pco,
                                    pxlo,
                                    prclTrg,
                                    pptlSrc,
                                    pptlMask,
                                    pbo,
                                    pptlBrush,
                                    rop4);
    }

    if (!pxlo) pxlo = &gexloTrivial.xlo;

    /* Initialize the BLTINFO structure */
    bltdata.dy = 1;
    bltdata.rop4 = rop4;
    bltdata.apfnDoRop[0] = gapfnRop[ROP4_BKGND(rop4)];
    bltdata.apfnDoRop[1] = gapfnRop[ROP4_FGND(rop4)];
    bltdata.pxlo = pxlo;
    bltdata.pfnXlate = XLATEOBJ_pfnXlate(pxlo);

    /* Check if the ROP uses a source */
    if (ROP4_USES_SOURCE(rop4))
    {
        /* Sanity checks */
        NT_ASSERT(psoSrc);
        NT_ASSERT(psoSrc->iBitmapFormat >= BMF_1BPP);
        NT_ASSERT(psoSrc->iBitmapFormat <= BMF_8RLE);
        NT_ASSERT(pptlSrc);
        NT_ASSERT(pptlSrc->x >= 0);
        NT_ASSERT(pptlSrc->y >= 0);
        NT_ASSERT(pptlSrc->x <= psoSrc->sizlBitmap.cx);
        NT_ASSERT(pptlSrc->y <= psoSrc->sizlBitmap.cy);

        /* Check for special case SRCCOPY from device */
        if ((rop4 == ROP4_SRCCOPY) &&
            (psoSrc->iType != STYPE_BITMAP) &&
            ((psoTrg->iType == STYPE_BITMAP) || (psoSrc->hdev == psoTrg->hdev)))
        {
            /* Use the driver's DrvCopyBits */
            pfnCopyBits = GDIDEVFUNCS(psoSrc).CopyBits;
            return pfnCopyBits(psoTrg, psoSrc, pco, pxlo, prclTrg, pptlSrc);
        }

        /* Check if the source is not a normal bitmap */
        if ((psoSrc->iType != STYPE_BITMAP) ||
            (psoSrc->iBitmapFormat > BMF_32BPP))
        {
            /* We need to do a buffered bitblt */
            return EngSrcBufferedBitBlt(psoTrg,
                                        psoSrc,
                                        psoMask,
                                        pco,
                                        pxlo,
                                        prclTrg,
                                        pptlSrc,
                                        pptlMask,
                                        pbo,
                                        pptlBrush,
                                        rop4);
        }

        /* Check if source and target surface are equal */
        if (psoSrc == psoTrg)
        {
            /* Analyze the copying direction */
            if (prclTrg->top > pptlSrc->y)
            {
                /* Need to copy from bottom to top */
                iDirection = prclTrg->left < pptlSrc->x ? CD_RIGHTUP : CD_LEFTUP;
                bltdata.dy = -1;
            }
            else
            {
                /* We copy from top to bottom */
                iDirection = prclTrg->left < pptlSrc->x ? CD_RIGHTDOWN : CD_LEFTDOWN;
            }

            /* Check for special right to left case */
            if ((prclTrg->top == pptlSrc->y) && (prclTrg->left > pptlSrc->x))
            {
                /* Use 0 as target format to get special right to left versions */
                bltdata.siDst.iFormat = 0;
                bltdata.siSrc.iFormat = psoSrc->iBitmapFormat;
                //__debugbreak();
            }
            else
            {
                /* Use 0 as source format to get special equal surface versions */
                bltdata.siDst.iFormat = psoTrg->iBitmapFormat;
                bltdata.siSrc.iFormat = 0;
            }
        }
        else
        {
            iDirection = CD_ANY;
            bltdata.siDst.iFormat = psoTrg->iBitmapFormat;
            bltdata.siSrc.iFormat = psoSrc->iBitmapFormat;
        }

        /* Set the rest of the source format info */
        bltdata.siSrc.pvScan0 = psoSrc->pvScan0;
        bltdata.siSrc.lDelta = psoSrc->lDelta;
        bltdata.siSrc.cjAdvanceY = bltdata.dy * psoSrc->lDelta;
        bltdata.siSrc.jBpp = gajBitsPerFormat[psoSrc->iBitmapFormat];
    }
    else
    {
        iDirection = CD_ANY;
        bltdata.siDst.iFormat = psoTrg->iBitmapFormat;
    }

    /* Set the rest of the destination format info */
    bltdata.siDst.pvScan0 = psoTrg->pvScan0;
    bltdata.siDst.lDelta = psoTrg->lDelta;
    bltdata.siDst.cjAdvanceY = bltdata.dy * psoTrg->lDelta;
    bltdata.siDst.jBpp = gajBitsPerFormat[psoTrg->iBitmapFormat];

    /* Check if the ROP uses a pattern / brush */
    if (ROP4_USES_PATTERN(rop4))
    {
        /* Must have a brush */
        NT_ASSERT(pbo); // FIXME: test this!

        /* Copy the solid color */
        bltdata.ulSolidColor = pbo->iSolidColor;

        /* Check if this is a pattern brush */
        if (pbo->iSolidColor == 0xFFFFFFFF)
        {
            /* Get the realized pattern bitmap */
            psoPattern = BRUSHOBJ_psoPattern(pbo);
            if (!psoPattern)
            {
                __debugbreak();
                return FALSE;
            }

            /* Set the pattern format info */
            bltdata.siPat.iFormat = psoPattern->iBitmapFormat;
            bltdata.siPat.pvScan0 = psoPattern->pvScan0;
            bltdata.siPat.lDelta = psoPattern->lDelta;
            bltdata.siPat.cjAdvanceY = bltdata.dy * psoPattern->lDelta;
            bltdata.siPat.jBpp = gajBitsPerFormat[psoPattern->iBitmapFormat];

            bltdata.ulPatWidth = psoPattern->sizlBitmap.cx;
            bltdata.ulPatHeight = psoPattern->sizlBitmap.cy;

            psizlPat = &psoPattern->sizlBitmap;
        }
        else
        {
            pptlBrush = NULL;
            psizlPat = NULL;
        }
    }
    else
    {
        pptlBrush = NULL;
        psizlPat = NULL;
    }

    /* Check if the ROP uses a mask */
    if (ROP4_USES_MASK(rop4))
    {
        /* Must have a mask surface and point */
        NT_ASSERT(psoMask);
        NT_ASSERT(pptlMask);

        //__debugbreak();

        /* Set the mask format info */
        bltdata.siMsk.iFormat = psoMask->iBitmapFormat;
        bltdata.siMsk.pvScan0 = psoMask->pvScan0;
        bltdata.siMsk.lDelta = psoMask->lDelta;
        bltdata.siMsk.cjAdvanceY = bltdata.dy * psoMask->lDelta;
        bltdata.siMsk.jBpp = 1;

        /* Calculate the masking function index */
        iFunctionIndex = ROP4_USES_PATTERN(rop4) ? 1 : 0;
        iFunctionIndex |= ROP4_USES_SOURCE(rop4) ? 2 : 0;
        iFunctionIndex |= ROP4_USES_DEST(rop4) ? 4 : 0;

        /* Get the masking function */
        pfnBitBlt = gapfnMaskFunction[iFunctionIndex];
    }
    else
    {
        /* Get the function index from the foreground ROP index*/
        iFunctionIndex = gajIndexPerRop[ROP4_FGND(rop4)];

        /* Get the dib function */
        pfnBitBlt = gapfnDibFunction[iFunctionIndex];
    }

    /* If no clip object is given, use trivial one */
    if (!pco) pco = (CLIPOBJ*)&gxcoTrivial;

    /* Check if we need to enumerate rects */
    if (pco->iDComplexity == DC_COMPLEX)
    {
        /* Start the enumeration of the clip object */
        CLIPOBJ_cEnumStart(pco, FALSE, CT_RECTANGLES, iDirection, 0);
        bEnumMore = CLIPOBJ_bEnum(pco, sizeof(rcenum), (ULONG*)&rcenum);
    }
    else if (pco->iDComplexity == DC_RECT)
    {
        /* Use the clip bounds */
        rcenum.arcl[0] = pco->rclBounds;
        rcenum.c = 1;
        bEnumMore = FALSE;
    }
    else
    {
        /* Use the target rect */
        rcenum.arcl[0] = *prclTrg;
        rcenum.c = 1;
        bEnumMore = FALSE;
    }

    /* Enter enumeration loop */
    while (TRUE)
    {
        /* Loop all rectangles we got */
        for (i = 0; i < rcenum.c; i++)
        {
            /* Intersect this rect with the target rect */
            if (!RECTL_bIntersectRect(&rcenum.arcl[i], &rcenum.arcl[i], prclTrg))
            {
                /* This rect is outside the bounds, continue */
                continue;
            }

            /* Calculate coordinates and pointers */
            CalculateCoordinates(&bltdata,
                                 &rcenum.arcl[i],
                                 prclTrg,
                                 pptlSrc,
                                 pptlMask,
                                 pptlBrush,
                                 psizlPat);

            /* Call the dib function */
            pfnBitBlt(&bltdata);
        }

        /* Bail out, when there's nothing more to do */
        if(!bEnumMore) break;

        /* Enumerate more rectangles */
        bEnumMore = CLIPOBJ_bEnum(pco, sizeof(rcenum), (ULONG*)&rcenum);
    }

    return TRUE;
}


static
VOID
AdjustOffsetAndSize(
    _Out_ PPOINTL pptOffset,
    _Out_ PSIZEL psizTrg,
    _In_ PPOINTL pptlSrc,
    _In_ PSIZEL psizSrc)
{
    LONG x, y, cxMax, cyMax;

    x = pptlSrc->x + pptOffset->x;
    if (x < 0) pptOffset->x -= x, x = 0;

    cxMax = psizSrc->cx - x;
    if (psizTrg->cx > cxMax) psizTrg->cx = cxMax;

    y = pptlSrc->y + pptOffset->y;
    if (y < 0) pptOffset->y -= y, y = 0;

    cyMax = psizSrc->cy - y;
    if (psizTrg->cy > cyMax) psizTrg->cy = cyMax;
}

BOOL
APIENTRY
IntEngBitBlt(
    _Inout_ SURFOBJ *psoTrg,
    _In_opt_ SURFOBJ *psoSrc,
    _In_opt_ SURFOBJ *psoMask,
    _In_opt_ CLIPOBJ *pco,
    _In_opt_ XLATEOBJ *pxlo,
    _In_ RECTL *prclTrg,
    _When_(psoSrc, _In_) POINTL *pptlSrc,
    _When_(psoMask, _In_) POINTL *pptlMask,
    _In_opt_ BRUSHOBJ *pbo,
    _When_(pbo, _In_) POINTL *pptlBrush,
    _In_ ROP4 rop4)
{
    BOOL bResult;
    RECTL rcClipped;
    POINTL ptOffset, ptSrc, ptMask, ptBrush;
    SIZEL sizTrg;
    PFN_DrvBitBlt pfnBitBlt;

//__debugbreak();

    /* Sanity checks */
    NT_ASSERT(IS_VALID_ROP4(rop4));
    NT_ASSERT(psoTrg);
    NT_ASSERT(psoTrg->iBitmapFormat >= BMF_1BPP);
    NT_ASSERT(psoTrg->iBitmapFormat <= BMF_32BPP);
    NT_ASSERT(prclTrg);

    /* Clip the target rect to the extents of the target surface */
    rcClipped.left = max(prclTrg->left, 0);
    rcClipped.top = max(prclTrg->top, 0);
    rcClipped.right = min(prclTrg->right, psoTrg->sizlBitmap.cx);
    rcClipped.bottom = min(prclTrg->bottom, psoTrg->sizlBitmap.cy);

    /* If no clip object is given, use trivial one */
    if (pco == NULL)
        pco = (CLIPOBJ*)&gxcoTrivial;

    /* Check if there is anything to clip */
    else if (pco->iDComplexity != DC_TRIVIAL)
    {
        /* Clip the target rect to the bounds of the clipping region */
        if (!RECTL_bIntersectRect(&rcClipped, &rcClipped, &pco->rclBounds))
        {
            /* Nothing left */
            return TRUE;
        }

        /* Don't pass a clip object with a single rectangle */
        if (pco->iDComplexity == DC_RECT)
            pco = (CLIPOBJ*)&gxcoTrivial;
    }

    /* Calculate initial offset and size */
    ptOffset.x = rcClipped.left - prclTrg->left;
    ptOffset.y = rcClipped.top - prclTrg->top;
    sizTrg.cx = rcClipped.right - rcClipped.left;
    sizTrg.cy = rcClipped.bottom - rcClipped.top;

    /* Check if the ROP uses a source */
    if (ROP4_USES_SOURCE(rop4))
    {
        /* Must have a source surface and point */
        NT_ASSERT(psoSrc);
        NT_ASSERT(pptlSrc);

        /* Get the source point */
        ptSrc = *pptlSrc;

        /* Clip against the extents of the source surface */
        AdjustOffsetAndSize(&ptOffset, &sizTrg, &ptSrc, &psoSrc->sizlBitmap);
    }
    else
    {
        psoSrc = NULL;
        ptSrc.x = 0;
        ptSrc.y = 0;
    }

    /* Check if the ROP uses a mask */
    if (ROP4_USES_MASK(rop4))
    {
        /* Must have a mask surface and point */
        NT_ASSERT(psoMask);
        NT_ASSERT(pptlMask);

        /* Get the mask point */
        ptMask = *pptlMask;

        /* Clip against the extents of the mask surface */
        AdjustOffsetAndSize(&ptOffset, &sizTrg, &ptMask, &psoMask->sizlBitmap);
    }
    else
    {
        psoMask = NULL;
        ptMask.x = 0;
        ptMask.y = 0;
    }

    /* Check if all has been clipped away */
    if ((sizTrg.cx <= 0) || (sizTrg.cy <= 0))
        return TRUE;

    /* Adjust the points */
    ptSrc.x += ptOffset.x;
    ptSrc.y += ptOffset.y;
    ptMask.x += ptOffset.x;
    ptMask.y += ptOffset.y;

    /* Check if we have a brush origin */
    if (pptlBrush)
    {
        /* calculate the new brush origin */
        ptBrush.x = pptlBrush->x + ptOffset.x;
        ptBrush.y = pptlBrush->y + ptOffset.y;
    }

    /* Recalculate the target rect */
    rcClipped.left = prclTrg->left + ptOffset.x;
    rcClipped.top = prclTrg->top + ptOffset.y;
    rcClipped.right = rcClipped.left + sizTrg.cx;
    rcClipped.bottom = rcClipped.top + sizTrg.cy;

    /* Is the target surface device managed? */
    if (SURFOBJ_flags(psoTrg) & HOOK_BITBLT)
    {
        /* Is the source a different device managed surface? */
        if (psoSrc && (psoSrc->hdev != psoTrg->hdev) &&
            (SURFOBJ_flags(psoSrc) & HOOK_BITBLT))
        {
            ERR("Need to copy to standard bitmap format!\n");
            NT_ASSERT(FALSE);
        }

        pfnBitBlt = GDIDEVFUNCS(psoTrg).BitBlt;
    }
    /* Otherwise is the source surface device managed? */
    else if (psoSrc && (SURFOBJ_flags(psoSrc) & HOOK_BITBLT))
    {
        pfnBitBlt = GDIDEVFUNCS(psoSrc).BitBlt;
    }
    else
    {
        pfnBitBlt = EngBitBlt;
    }

    bResult = pfnBitBlt(psoTrg,
                        psoSrc,
                        psoMask,
                        pco,
                        pxlo,
                        &rcClipped,
                        psoSrc ? &ptSrc : NULL,
                        psoMask ? &ptMask : NULL,
                        pbo,
                        pptlBrush ? &ptBrush : NULL,
                        rop4);

    // FIXME: cleanup temp surface!

    return bResult;
}

BOOL
APIENTRY
NtGdiEngBitBlt(
    IN SURFOBJ *psoTrgUMPD,
    IN SURFOBJ *psoSrcUMPD,
    IN SURFOBJ *psoMaskUMPD,
    IN CLIPOBJ *pcoUMPD,
    IN XLATEOBJ *pxloUMPD,
    IN RECTL *prclTrg,
    IN POINTL *pptlSrc,
    IN POINTL *pptlMask,
    IN BRUSHOBJ *pboUMPD,
    IN POINTL *pptlBrush,
    IN ROP4 rop4)
{
    RECTL  rclTrg;
    POINTL ptlSrc, ptlMask, ptlBrush;
    HSURF hsurfTrg, hsurfSrc = NULL, hsurfMask = NULL;
    HANDLE hBrushObj; // HUMPDOBJ
    SURFOBJ *psoTrg, *psoSrc, *psoMask;
    CLIPOBJ *pco;
    XLATEOBJ *pxlo;
    BRUSHOBJ *pbo;
    BOOL bResult;

    _SEH2_TRY
    {
        ProbeForRead(prclTrg, sizeof(RECTL), 1);
        rclTrg = *prclTrg;

        ProbeForRead(psoTrgUMPD, sizeof(SURFOBJ), 1);
        hsurfTrg = psoTrgUMPD->hsurf;

        if (ROP4_USES_SOURCE(rop4))
        {
            ProbeForRead(pptlSrc, sizeof(POINTL), 1);
            ptlSrc = *pptlSrc;

            ProbeForRead(psoSrcUMPD, sizeof(SURFOBJ), 1);
            hsurfSrc = psoSrcUMPD->hsurf;
        }

        if (ROP4_USES_MASK(rop4))
        {
            ProbeForRead(pptlMask, sizeof(POINTL), 1);
            ptlMask = *pptlMask;

            ProbeForRead(psoMaskUMPD, sizeof(SURFOBJ), 1);
            hsurfMask = psoMaskUMPD->hsurf;
        }

        if (ROP4_USES_PATTERN(rop4))
        {
            ProbeForRead(pptlBrush, sizeof(POINTL), 1);
            ptlBrush = *pptlBrush;

            ProbeForRead(psoSrcUMPD, sizeof(SURFOBJ), 1);
            hBrushObj = pboUMPD->pvRbrush; // FIXME
        }
    }
    _SEH2_EXCEPT(EXCEPTION_EXECUTE_HANDLER)
    {
        _SEH2_YIELD(return FALSE);
    }
    _SEH2_END;

    // FIXME: these need to be converted/locked!
    psoTrg = NULL;
    psoSrc = NULL;
    psoMask = NULL;
    pco = NULL;
    pxlo = NULL;
    pbo = NULL;

    bResult = EngBitBlt(psoTrg,
                        psoSrc,
                        psoMask,
                        pco,
                        pxlo,
                        &rclTrg,
                        pptlSrc ? &ptlSrc : NULL,
                        pptlMask ? &ptlMask : NULL,
                        pbo,
                        pptlBrush ? &ptlBrush : NULL,
                        rop4);

    return bResult;
}

BOOL
APIENTRY
EngCopyBits(
    SURFOBJ *psoTrg,
    SURFOBJ *psoSrc,
    CLIPOBJ *pco,
    XLATEOBJ *pxlo,
    RECTL *prclTrg,
    POINTL *pptlSrc)
{
    PFN_DrvCopyBits pfnCopyBits;

    /* Check if we have 2 incompatible non-bitmap surfaces */
    if ((psoSrc->iType != STYPE_BITMAP) &&
        (psoTrg->iType != STYPE_BITMAP) &&
        (psoSrc->hdev != psoTrg->hdev))
    {
        /* We need to create a temp bitmap */
        return EngSrcBufferedBitBlt(psoTrg,
                                    psoSrc,
                                    NULL,
                                    pco,
                                    pxlo,
                                    prclTrg,
                                    pptlSrc,
                                    NULL,
                                    NULL,
                                    NULL,
                                    ROP4_SRCCOPY);
    }

    /* Is the target surface device managed? */
    if ((psoTrg->iType != STYPE_BITMAP) ||
        (SURFOBJ_flags(psoTrg) & HOOK_COPYBITS))
    {
        pfnCopyBits = GDIDEVFUNCS(psoTrg).CopyBits;
    }
    else if ((psoSrc->iType != STYPE_BITMAP) ||
             (SURFOBJ_flags(psoSrc) & HOOK_COPYBITS))
    {
        pfnCopyBits = GDIDEVFUNCS(psoSrc).CopyBits;
    }
    else
    {
        /* Use SRCCOPY for 2 bitmaps */
        return EngBitBlt(psoTrg,
                         psoSrc,
                         NULL,
                         pco,
                         pxlo,
                         prclTrg,
                         pptlSrc,
                         NULL,
                         NULL,
                         NULL,
                         ROP4_SRCCOPY);
    }

    /* Forward to the driver */
    return pfnCopyBits(psoTrg, psoSrc, pco, pxlo, prclTrg, pptlSrc);
}

BOOL
APIENTRY
IntEngCopyBits(
    SURFOBJ *psoTrg,
    SURFOBJ *psoSrc,
    CLIPOBJ *pco,
    XLATEOBJ *pxlo,
    RECTL *prclTrg,
    POINTL *pptlSrc)
{
    return EngCopyBits(psoTrg, psoSrc, pco, pxlo, prclTrg, pptlSrc);
}
