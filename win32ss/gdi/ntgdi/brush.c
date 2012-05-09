/*
 * COPYRIGHT:         See COPYING in the top level directory
 * PROJECT:           ReactOS win32 subsystem
 * PURPOSE:           Functions for brushes
 * FILE:              subsystem/win32/win32k/objects/brush.c
 * PROGRAMER:         Timo Kreuzer (timo.kreuzer@reactos.org)
 */

#include <win32k.h>

#define NDEBUG
#include <debug.h>

ULONG gulBrushUnique;

BOOL
NTAPI
BRUSH_bAllocBrushAttr(PBRUSH pbr)
{
    PPROCESSINFO ppi;
    BRUSH_ATTR *pBrushAttr;

    // HACK: this currently does not work
    return 1;

    /* Get the PROCESSINFO pointer */
    ppi = PsGetCurrentProcessWin32Process();
    ASSERT(ppi);

    /* Allocate a brush attribute from the gdi pool */
    pBrushAttr = GdiPoolAllocate(ppi->pPoolBrushAttr);
    if (!pBrushAttr)
    {
        DPRINT1("Could not allocate brush attr\n");
        return FALSE;
    }

    /* Set the new brush attribute and copy the kernel mode data */
    pbr->pBrushAttr = pBrushAttr;
    *pbr->pBrushAttr = pbr->BrushAttr;

    /* Default hatch style is -1 */
    pbr->ulStyle = -1;

    pbr->ulBrushUnique = InterlockedIncrementUL(&gulBrushUnique);

    /* Set the object attribute in the handle table */
    GDIOBJ_vSetObjectAttr(&pbr->BaseObject, pBrushAttr);

    DPRINT("BRUSH_bAllocBrushAttr: pbr=%p, pbr->pdcattr=%p\n", pbr, pbr->pBrushAttr);
    return TRUE;
}

VOID
NTAPI
BRUSH_vFreeBrushAttr(PBRUSH pbr)
{
    PPROCESSINFO ppi;

    /* Don't do anything, when the kernel mode brush attribute is in use */
    if (pbr->pBrushAttr == &pbr->BrushAttr) return;

    if (pbr->pBrushAttr == NULL) return; // FIXME

    /* Reset the object attribute in the handle table */
    GDIOBJ_vSetObjectAttr(&pbr->BaseObject, NULL);

    /* Copy state back from user attribute to kernel */
    *pbr->pBrushAttr = pbr->BrushAttr;

    /* Free memory from the process gdi pool */
    ppi = PsGetCurrentProcessWin32Process();
    ASSERT(ppi);

    /* Free the user mode attribute to the gdi pool */
    GdiPoolFree(ppi->pPoolBrushAttr, pbr->pBrushAttr);

    /* Reset to kmode brush attribute */
    pbr->pBrushAttr = &pbr->BrushAttr;
}

PBRUSH
NTAPI
BRUSH_AllocBrushOrPen(UCHAR objt)
{
    PBRUSH pbrush;
    ASSERT((objt == GDIObjType_BRUSH_TYPE) ||
           (objt == GDIObjType_PEN_TYPE) ||
           (objt == GDIObjType_EXTPEN_TYPE));

    /* Allocate a brush object */
    pbrush = (PBRUSH)GDIOBJ_AllocateObject(objt, sizeof(BRUSH), 0);
    if (!pbrush)
    {
        return NULL;
    }

    /* Set the kernel mode brush attribute */
    pbrush->pBrushAttr = &pbrush->BrushAttr;

    // need more?
    return pbrush;
}

PBRUSH
NTAPI
BRUSH_AllocBrushOrPenWithHandle(UCHAR objt, ULONG ulOwner)
{
    PBRUSH pbrush;

    /* Allocate a brush without a handle */
    pbrush = BRUSH_AllocBrushOrPen(objt);
    if (!pbrush)
    {
        return NULL;
    }

    /* Insert the brush into the handle table */
    if (!GDIOBJ_hInsertObject(&pbrush->BaseObject, ulOwner))
    {
        DPRINT1("Could not insert brush into handle table.\n");
        GDIOBJ_vFreeObject(&pbrush->BaseObject);
        return NULL;
    }

    /* Check if a global brush is requested */
    if (ulOwner == GDI_OBJ_HMGR_PUBLIC)
    {
        pbrush->flAttrs |= BR_IS_GLOBAL;
    }
    else if (ulOwner == GDI_OBJ_HMGR_POWNED)
    {
        /* Allocate a brush attribute */
        if (!BRUSH_bAllocBrushAttr(pbrush))
        {
            DPRINT1("Could not allocate brush attr.\n");
            GDIOBJ_vDeleteObject(&pbrush->BaseObject);
            return NULL;
        }
    }

    return pbrush;
}

BOOL
NTAPI
BRUSH_bSetBrushOwner(PBRUSH pbr, ULONG ulOwner)
{
    // FIXME:
    if (pbr->flAttrs & BR_IS_GLOBAL) return TRUE;

    /* Set ownership of the pattern bitmap */
    if (pbr->hbmPattern)
        GreSetObjectOwner(pbr->hbmPattern, ulOwner);

    if ((ulOwner == GDI_OBJ_HMGR_PUBLIC) || ulOwner == GDI_OBJ_HMGR_NONE)
    {
        /* Free the brush attribute */
        BRUSH_vFreeBrushAttr(pbr);
    }
    else if (ulOwner == GDI_OBJ_HMGR_POWNED)
    {
        /* Check if there is no usermode attribute */
        if (pbr->pBrushAttr == &pbr->BrushAttr)
        {
            /* Free the brush attribute */
            BRUSH_bAllocBrushAttr(pbr);
        }
    }

    GDIOBJ_vSetObjectOwner(&pbr->BaseObject, ulOwner);

    return TRUE;
}

BOOL
NTAPI
GreSetBrushOwner(HBRUSH hBrush, ULONG ulOwner)
{
    BOOL bResult;
    PBRUSH pbrush;

    /* Reference the brush */
    pbrush = BRUSH_ShareLockBrush(hBrush);

    /* Set the brush owner */
    bResult = BRUSH_bSetBrushOwner(pbrush, ulOwner);

    /* Dereference the brush */
    BRUSH_ShareUnlockBrush(pbrush);
    return bResult;
}

BOOL
NTAPI
BRUSH_Cleanup(PVOID ObjectBody)
{
    PBRUSH pbrush = (PBRUSH)ObjectBody;

    /* We need to free bitmaps for pattern brushes, not for hatch brushes */
    if (pbrush->flAttrs & (BR_IS_BITMAP|BR_IS_DIB))
    {
        ASSERT(pbrush->hbmPattern);
        GreSetObjectOwner(pbrush->hbmPattern, GDI_OBJ_HMGR_POWNED);
        GreDeleteObject(pbrush->hbmPattern);
    }

    /* Check if there is a usermode attribute */
    if (pbrush->pBrushAttr != &pbrush->BrushAttr)
    {
        BRUSH_vFreeBrushAttr(pbrush);
    }

    /* Free the kmode styles array of EXTPENS */
    if (pbrush->pStyle)
    {
        ExFreePool(pbrush->pStyle);
    }

    return TRUE;
}

INT
FASTCALL
BRUSH_GetObject(PBRUSH pbrush, INT cjSize, LPLOGBRUSH plogbrush)
{
    /* Check if only size is requested */
    if ((plogbrush == NULL) || (cjSize == 0)) return sizeof(LOGBRUSH);

    /* Set colour */
    plogbrush->lbColor = pbrush->BrushAttr.lbColor;

    /* Default to 0 */
    plogbrush->lbHatch = 0;

    /* Get the type of style */
    if (pbrush->flAttrs & BR_IS_SOLID)
    {
        plogbrush->lbStyle = BS_SOLID;
    }
    else if (pbrush->flAttrs & BR_IS_NULL)
    {
        plogbrush->lbStyle = BS_NULL; // BS_HOLLOW
    }
    else if (pbrush->flAttrs & BR_IS_HATCH)
    {
        plogbrush->lbStyle = BS_HATCHED;
        plogbrush->lbHatch = pbrush->ulStyle;
    }
    else if (pbrush->flAttrs & BR_IS_DIB)
    {
        plogbrush->lbStyle = BS_DIBPATTERN;
        plogbrush->lbHatch = (ULONG_PTR)pbrush->hbmClient;
    }
    else if (pbrush->flAttrs & BR_IS_BITMAP)
    {
        plogbrush->lbStyle = BS_PATTERN;
    }
    else
    {
        plogbrush->lbStyle = 0; // ???
    }

    /* FIXME
    else if (pbrush->flAttrs & )
    {
        plogbrush->lbStyle = BS_INDEXED;
    }
    else if (pbrush->flAttrs & )
    {
        plogbrush->lbStyle = BS_DIBPATTERNPT;
    }
    */

    /* FIXME */
    return sizeof(LOGBRUSH);
}

VOID
NTAPI
GreSetSolidBrushColor(HBRUSH hbr, COLORREF crColor)
{
    PBRUSH pbrush;

    pbrush = BRUSH_ShareLockBrush(hbr);
    if (!pbrush) return;

    if (pbrush->flAttrs & BR_IS_SOLID)
    {
        pbrush->BrushAttr.lbColor = crColor & 0xFFFFFF;
    }
    BRUSH_ShareUnlockBrush(pbrush);
}

HBRUSH
NTAPI
GreCreateBrushInternal(
    _In_ ULONG ulStyle,
    _In_ HBITMAP hbmPattern,
    _In_ HBITMAP hbmClient,
    _In_ COLORREF crColor,
    _In_ FLONG flAttrib)
{
    PBRUSH pbr;
    HBRUSH hbr;
    ULONG ulOwner;
    UCHAR objt;

    /* Set the desired object type */
    if (flAttrib & BR_IS_OLDSTYLEPEN) objt = GDIObjType_PEN_TYPE;
    else if (flAttrib & BR_IS_PEN) objt = GDIObjType_EXTPEN_TYPE;
    else objt = GDIObjType_BRUSH_TYPE;

    /* Set object owner */
    if (flAttrib & BR_IS_GLOBAL) ulOwner = GDI_OBJ_HMGR_PUBLIC;
    else ulOwner = GDI_OBJ_HMGR_POWNED;

    /* Allocate a brush */
    pbr = BRUSH_AllocBrushOrPenWithHandle(objt, ulOwner);
    if (pbr == NULL)
    {
        EngSetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return NULL;
    }

    /* Set brush properties */
    pbr->flAttrs = flAttrib;
    pbr->hbmPattern = hbmPattern;
    pbr->hbmClient = hbmClient;
    pbr->ulStyle = ulStyle;
    pbr->BrushAttr.lbColor = crColor;

    /* Get the handle and unlock the brush */
    hbr = pbr->BaseObject.hHmgr;
    GDIOBJ_vUnlockObject(&pbr->BaseObject);

    return hbr;
}

HBRUSH
NTAPI
GreCreateNullBrush(VOID)
{
    /* Call the internal worker function */
    return GreCreateBrushInternal(0, NULL, NULL, 0, BR_IS_NULL|BR_IS_GLOBAL);
}

/* SYSTEM CALLS **************************************************************/

HBRUSH
APIENTRY
NtGdiCreateSolidBrush(
    _In_ COLORREF crColor,
    _In_opt_ HBRUSH hbr)
{
    /* Call the internal worker function */
    return GreCreateBrushInternal(0, NULL, NULL, crColor, BR_IS_SOLID);
}

HBRUSH
APIENTRY
NtGdiCreateHatchBrushInternal(
    _In_ ULONG ulStyle,
    _In_ COLORREF crColor,
    _In_ BOOL bPen)
{
    FLONG flAttr = BR_IS_HATCH | (bPen ? BR_IS_PEN : 0);

    /* Check parameters */
    if ((ulStyle < 0) || (ulStyle >= HS_DDI_MAX))
    {
        return 0;
    }

    /* Call the internal worker function */
    return GreCreateBrushInternal(ulStyle, NULL, NULL, crColor, flAttr);
}

HBRUSH
APIENTRY
NtGdiCreatePatternBrushInternal(
    _In_ HBITMAP hBitmap,
    _In_ BOOL bPen,
    _In_ BOOL b8x8)
{
    HBITMAP hbmPattern;
    FLONG flAttr = BR_IS_BITMAP | (bPen ? BR_IS_PEN : 0);

    /* Make a copy of the given bitmap */
    hbmPattern = BITMAP_CopyBitmap(hBitmap);
    if (hbmPattern == NULL)
    {
        EngSetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return NULL;
    }

    /* Call the internal worker function */
    return GreCreateBrushInternal(0, hbmPattern, hBitmap, 0, flAttr);
}

HBRUSH
APIENTRY
NtGdiCreateDIBBrush(
    _In_ PVOID pvPackedDIB,
    _In_ ULONG iUsage,// FLONG flColorSpec,
    _In_ UINT cjDIBSize,
    _In_ BOOL b8x8,
    _In_ BOOL bPen,
    _In_ PVOID pClient)
{
    FLONG flAttr = BR_IS_DIB | (bPen ? BR_IS_PEN : 0);
    PVOID pvSaveDIB, pvSafeBits;
    PBITMAPINFO pbmi;
    HBITMAP hbmPattern;
    ULONG cjInfoSize;

    /* Parameter check */
    if (iUsage > 2)
    {
        EngSetLastError(ERROR_INVALID_PARAMETER);
        return NULL;
    }

    /* Allocate a buffer large enough to hold the complete packed DIB */
    pvSaveDIB = ExAllocatePoolWithTag(PagedPool, cjDIBSize, TAG_DIB);
    if (pvSaveDIB == NULL)
    {
        EngSetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return NULL;
    }

    /* Enter SEH for buffer transfer */
    _SEH2_TRY
    {
        /* Probe and copy the whole DIB */
        ProbeForRead(pvPackedDIB, cjDIBSize, 1);
        RtlCopyMemory(pvSaveDIB, pvPackedDIB, cjDIBSize);
    }
    _SEH2_EXCEPT(EXCEPTION_EXECUTE_HANDLER)
    {
        /* Got an exception, cleanup, set error and return 0 */
        ExFreePoolWithTag(pvSaveDIB, TAG_DIB);
        SetLastNtError(_SEH2_GetExceptionCode());
        _SEH2_YIELD(return 0;)
    }
    _SEH2_END;

    /* Calculate the size of the bitmap info */
    cjInfoSize = GreGetBitmapInfoSize(pvSaveDIB, iUsage);

    /* Check sanity of the sizes */
    if ((cjInfoSize < sizeof(BITMAPCOREHEADER)) || // info?
        (cjInfoSize > cjDIBSize))
    {
        /* Something is wrong with the bitmap */
        ExFreePoolWithTag(pvSaveDIB, TAG_DIB);
        return NULL;
    }

    /* Get pointers to BITMAPINFO and to the bits */
    pbmi = pvSaveDIB;
    pvSafeBits = (PUCHAR)pvSaveDIB + cjInfoSize;

    /* When DIB_PAL_COLORS is requested, we don't create the RGB palette
       entries directly, instead we need to create a fake palette containing
       pal indices, which is converted into a real palette when the brush
       is realized. */
    if (iUsage == DIB_PAL_COLORS) iUsage = DIB_PAL_BRUSHHACK;

    /* Create the pattern bitmap from the DIB. */
    hbmPattern = GreCreateDIBitmapInternal(NULL,
                                           pbmi->bmiHeader.biWidth,
                                           abs(pbmi->bmiHeader.biHeight),
                                           CBM_INIT,
                                           pvSafeBits,
                                           pbmi,
                                           iUsage,// FIXME!!!
                                           0,
                                           cjDIBSize - cjInfoSize,
                                           NULL);

    /* Free the buffer already */
    ExFreePoolWithTag(pvSaveDIB, TAG_DIB);

    /* Check for failure */
    if (!hbmPattern)
    {
        EngSetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return NULL;
    }

    /* Remember if the palette consists of palette indices */
    if (iUsage == DIB_PAL_COLORS) flAttr |= BR_IS_DIBPALCOLORS;

    /* Call the internal worker function */
    return GreCreateBrushInternal(0, hbmPattern, pvPackedDIB, 0, flAttr);
}


/* EOF */
