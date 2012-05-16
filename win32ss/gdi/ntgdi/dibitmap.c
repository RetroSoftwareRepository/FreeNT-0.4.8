/*
 * COPYRIGHT:        GNU GPL, see COPYING in the top level directory
 * PROJECT:          ReactOS Win32 kernelmode subsystem
 * PURPOSE:          GDI functions for device independent bitmaps
 * FILE:             subsystems/win32/win32k/objects/dibitmap.c
 * PROGRAMER:        Timo Kreuzer
 */

#include <win32k.h>

#define NDEBUG
#include <debug.h>

static
BOOL
DibGetBitmapFormat(
    _In_ const BITMAPINFO *pbmi,
    _Out_ PBITMAPFORMAT pbmf)
{
    /* Check for BITMAPCOREINFO */
    if (pbmi->bmiHeader.biSize < sizeof(BITMAPINFOHEADER))
    {
        PBITMAPCOREHEADER pbch = (PBITMAPCOREHEADER)pbmi;

        pbmf->sizel.cx = pbch->bcWidth;
        pbmf->sizel.cy = pbch->bcHeight;
        pbmf->cBitsPixel = pbch->bcBitCount * pbch->bcPlanes;
        pbmf->iCompression = BI_RGB;
    }
    else
    {
        pbmf->sizel.cx = pbmi->bmiHeader.biWidth;
        pbmf->sizel.cy = pbmi->bmiHeader.biHeight;
        pbmf->cBitsPixel = pbmi->bmiHeader.biBitCount * pbmi->bmiHeader.biPlanes;
        pbmf->iCompression = pbmi->bmiHeader.biCompression;
    }

    /* Get the bitmap format */
    pbmf->iFormat = BitmapFormat(pbmf->cBitsPixel, pbmf->iCompression);

    /* Check if we have a valid bitmap format */
    if (pbmf->iFormat == 0)
    {
        DPRINT1("Invalid format\n");
        return FALSE;
    }

    /* Check compressed format and top-down */
    if ((pbmf->iFormat > BMF_32BPP) && (pbmf->sizel.cy < 0))
    {
        DPRINT1("Compressed bitmaps cannot be top-down.\n");
        return FALSE;
    }

    // FIXME: check bitmap extensions / size

    return TRUE;
}

static
ULONG
DibGetImageSize(
    _In_ const BITMAPINFO *pbmi)
{
    LONG lWidth, lHeight;
    ULONG cBitsPixel;

    /* Check for BITMAPCOREINFO */
    if (pbmi->bmiHeader.biSize < sizeof(BITMAPINFOHEADER))
    {
        PBITMAPCOREHEADER pbch = (PBITMAPCOREHEADER)pbmi;
        lWidth = pbch->bcWidth;
        lHeight = pbch->bcHeight;
        cBitsPixel = pbch->bcBitCount * pbch->bcPlanes;
    }
    else
    {
        lWidth = pbmi->bmiHeader.biWidth;
        lHeight = pbmi->bmiHeader.biHeight;
        cBitsPixel = pbmi->bmiHeader.biBitCount * pbmi->bmiHeader.biPlanes;
    }

    return WIDTH_BYTES_ALIGN32(lWidth, cBitsPixel) * abs(lHeight);
}

ULONG
NTAPI
DibGetBitmapInfoSize(
    _In_ const BITMAPINFO *pbmi,
    _In_ ULONG iUsage)
{
    ULONG cjHeaderSize, cBitsPixel, cColors, cMaxColors;

    cjHeaderSize = pbmi->bmiHeader.biSize;

    if ((cjHeaderSize < sizeof(BITMAPCOREHEADER)) ||
        ((LONG)cjHeaderSize < 0))
    {
        return 0;
    }

    /* Check for BITMAPCOREINFO */
    if (cjHeaderSize < sizeof(BITMAPINFOHEADER))
    {
        PBITMAPCOREHEADER pbch = (PBITMAPCOREHEADER)pbmi;

        cBitsPixel = pbch->bcBitCount;
        cColors = (cBitsPixel <= 8) ? (1 << cBitsPixel) : 0;
        return sizeof(BITMAPCOREHEADER) + cColors *
               ((iUsage == DIB_RGB_COLORS) ? sizeof(RGBTRIPLE) : sizeof(WORD));
    }
    else  /* Use BITMAPINFOHEADER */
    {
        /* Check if the DIB uses bitfields */
        if (pbmi->bmiHeader.biCompression == BI_BITFIELDS)
        {
            /* Check for V4/v5 info */
            if (cjHeaderSize >= sizeof(BITMAPV4HEADER))
            {
                /* There are no colors or masks */
                return cjHeaderSize;
            }
            else
            {
                /* The color table is 3 masks */
                return cjHeaderSize + 3 * sizeof(RGBQUAD);
            }
        }

        cBitsPixel = pbmi->bmiHeader.biBitCount;
        if (cBitsPixel > 8)
            return cjHeaderSize;

        cMaxColors = (1 << cBitsPixel);
        cColors = pbmi->bmiHeader.biClrUsed;
        if ((cColors == 0) || (cColors > cMaxColors)) cColors = cMaxColors;

        if (iUsage == DIB_RGB_COLORS)
            return cjHeaderSize + cColors * sizeof(RGBQUAD);
        else
            return cjHeaderSize + cColors * sizeof(WORD);
    }
}

PBITMAPINFO
NTAPI
DibProbeAndCaptureBitmapInfo(
    _In_ const BITMAPINFO *pbmiUnsafe,
    _In_ ULONG iUsage,
    _In_ PULONG pcjInfo)
{
    BITMAPINFO *pbmi;
    ULONG cjInfo = *pcjInfo;

    /* Normalize the size to the maximum possible */
    cjInfo = min(cjInfo, sizeof(BITMAPV5HEADER) + 256 * sizeof(RGBQUAD));

    /* Check if the size is at least big enough for a core info */
    if (cjInfo < sizeof(BITMAPCOREHEADER))
    {
        return NULL;
    }

    /* Allocate a buffer for the bitmap info */
    pbmi = ExAllocatePoolWithTag(PagedPool, cjInfo, GDITAG_BITMAPINFO);
    if (!pbmi)
    {
        return NULL;
    }

    /* Enter SEH for copy */
    _SEH2_TRY
    {
        /* Probe and copy the data from user mode */
        ProbeForRead(pbmiUnsafe, cjInfo, 1);
        RtlCopyMemory(pbmi, pbmiUnsafe, cjInfo);
    }
    _SEH2_EXCEPT(EXCEPTION_EXECUTE_HANDLER)
    {
        _SEH2_YIELD(return NULL);
    }
    _SEH2_END;

    /* Check if we have at least as much as the actual header size */
    if (cjInfo >= pbmi->bmiHeader.biSize)
    {
        /* Calculate the real size of the bitmap info */
        cjInfo = DibGetBitmapInfoSize(pbmi, iUsage);

        /* Make sure the real size is not larger than our buffer */
        if (cjInfo <= *pcjInfo)
        {
            /* Update the real info size */
            *pcjInfo = cjInfo;

            /* Return the new bitmap info */
            return pbmi;
        }
    }

    /* Failure, free the buffer and return NULL */
    ExFreePoolWithTag(pbmi, GDITAG_BITMAPINFO);
    return NULL;
}

VOID
FORCEINLINE
DibFreeBitmapInfo(
    _In_ PBITMAPINFO pbmi)
{
    ExFreePoolWithTag(pbmi, GDITAG_BITMAPINFO);
}

PPALETTE
NTAPI
CreateDIBPalette(
    _In_ const BITMAPINFO *pbmi,
    _In_ PDC pdc,
    _In_ ULONG iUsage)
{
    PPALETTE ppal;
    ULONG i, cBitsPixel, cColors;

    if (pbmi->bmiHeader.biSize < sizeof(BITMAPINFOHEADER))
    {
        PBITMAPCOREINFO pbci = (PBITMAPCOREINFO)pbmi;
        cBitsPixel = pbci->bmciHeader.bcBitCount;
    }
    else
    {
        cBitsPixel = pbmi->bmiHeader.biBitCount;
    }

    /* Check if the colors are indexed */
    if (cBitsPixel <= 8)
    {
        /* We create a "full" palette */
        cColors = 1 << cBitsPixel;

        /* Allocate the palette */
        ppal = PALETTE_AllocPalette(PAL_INDEXED,
                                    cColors,
                                    NULL,
                                    0,
                                    0,
                                    0);

        /* Check if the BITMAPINFO specifies how many colors to use */
        if ((pbmi->bmiHeader.biSize >= sizeof(BITMAPINFOHEADER)) &&
            (pbmi->bmiHeader.biClrUsed != 0))
        {
            /* This is how many colors we can actually process */
            cColors = min(cColors, pbmi->bmiHeader.biClrUsed);
        }

        /* Check how to use the colors */
        if (iUsage == DIB_PAL_COLORS)
        {
            COLORREF crColor;

            /* The colors are an array of WORD indices into the DC palette */
            PWORD pwColors = (PWORD)((PCHAR)pbmi + pbmi->bmiHeader.biSize);

            /* Use the DCs palette or, if no DC is given, the default one */
            PPALETTE ppalDC = pdc ? pdc->dclevel.ppal : gppalDefault;

            /* Loop all color indices in the DIB */
            for (i = 0; i < cColors; i++)
            {
                /* Get the palette index and handle wraparound when exceeding
                   the number of colors in the DC palette */
                WORD wIndex = pwColors[i] % ppalDC->NumColors;

                /* USe the RGB value from the DC palette */
                crColor = PALETTE_ulGetRGBColorFromIndex(ppalDC, wIndex);
                PALETTE_vSetRGBColorForIndex(ppal, i, crColor);
            }
        }
        else if (iUsage == DIB_PAL_BRUSHHACK)
        {
            /* The colors are an array of WORD indices into the DC palette */
            PWORD pwColors = (PWORD)((PCHAR)pbmi + pbmi->bmiHeader.biSize);

            /* Loop all color indices in the DIB */
            for (i = 0; i < cColors; i++)
            {
                /* Set the index directly as the RGB color, the real palette
                   containing RGB values will be calculated when the brush is
                   realized */
                PALETTE_vSetRGBColorForIndex(ppal, i, pwColors[i]);
            }

            /* Mark the palette as a brush hack palette */
            ppal->flFlags |= PAL_BRUSHHACK;
        }
//        else if (iUsage == 2)
//        {
            // FIXME: this one is undocumented
//            ASSERT(FALSE);
//        }
        else if (pbmi->bmiHeader.biSize >= sizeof(BITMAPINFOHEADER))
        {
            /* The colors are an array of RGBQUAD values */
            RGBQUAD *prgb = (RGBQUAD*)((PCHAR)pbmi + pbmi->bmiHeader.biSize);

            // FIXME: do we need to handle PALETTEINDEX / PALETTERGB macro?

            /* Loop all color indices in the DIB */
            for (i = 0; i < cColors; i++)
            {
                /* Get the color value and translate it to a COLORREF */
                RGBQUAD rgb = prgb[i];
                COLORREF crColor = RGB(rgb.rgbRed, rgb.rgbGreen, rgb.rgbBlue);

                /* Set the RGB value in the palette */
                PALETTE_vSetRGBColorForIndex(ppal, i, crColor);
            }
        }
        else
        {
            /* The colors are an array of RGBTRIPLE values */
            RGBTRIPLE *prgb = (RGBTRIPLE*)((PCHAR)pbmi + pbmi->bmiHeader.biSize);

            /* Loop all color indices in the DIB */
            for (i = 0; i < cColors; i++)
            {
                /* Get the color value and translate it to a COLORREF */
                RGBTRIPLE rgb = prgb[i];
                COLORREF crColor = RGB(rgb.rgbtRed, rgb.rgbtGreen, rgb.rgbtBlue);

                /* Set the RGB value in the palette */
                PALETTE_vSetRGBColorForIndex(ppal, i, crColor);
            }
        }
    }
    else
    {
        /* This is a bitfield / RGB palette */
        ULONG flRedMask, flGreenMask, flBlueMask;

        /* Check if the DIB contains bitfield values */
        if ((pbmi->bmiHeader.biSize >= sizeof(BITMAPINFOHEADER)) &&
            (pbmi->bmiHeader.biCompression == BI_BITFIELDS))
        {
            /* Check if we have a v4/v5 header */
            if (pbmi->bmiHeader.biSize >= sizeof(BITMAPV4HEADER))
            {
                /* The masks are dedicated fields in the header */
                PBITMAPV4HEADER pbmV4Header = (PBITMAPV4HEADER)&pbmi->bmiHeader;
                flRedMask = pbmV4Header->bV4RedMask;
                flGreenMask = pbmV4Header->bV4GreenMask;
                flBlueMask = pbmV4Header->bV4BlueMask;
            }
            else
            {
                /* The masks are the first 3 values in the DIB color table */
                PDWORD pdwColors = (PVOID)((PCHAR)pbmi + pbmi->bmiHeader.biSize);
                flRedMask = pdwColors[0];
                flGreenMask = pdwColors[1];
                flBlueMask = pdwColors[2];
            }
        }
        else
        {
            /* Check what bit depth we have. Note: optimization flags are
               calculated in PALETTE_AllocPalette()  */
            if (cBitsPixel == 16)
            {
                /* This is an RGB 555 palette */
                flRedMask = 0x7C00;
                flGreenMask = 0x03E0;
                flBlueMask = 0x001F;
            }
            else
            {
                /* This is an RGB 888 palette */
                flRedMask = 0xFF0000;
                flGreenMask = 0x00FF00;
                flBlueMask = 0x0000FF;
            }
        }

        /* Allocate the bitfield palette */
        ppal = PALETTE_AllocPalette(PAL_BITFIELDS,
                                    0,
                                    NULL,
                                    flRedMask,
                                    flGreenMask,
                                    flBlueMask);
    }

    /* We're done, return the palette */
    return ppal;
}

PSURFACE
NTAPI
DibCreateDIBSurface(
    _In_ const BITMAPINFO *pbmi,
    _In_opt_ PDC pdc,
    _In_ ULONG iUsage,
    _In_ ULONG fjBitmap,
    _In_ PVOID pvBits,
    _In_ ULONG cjMaxBits)
{
    BITMAPFORMAT bitmapformat;
    PSURFACE psurf;
    PPALETTE ppal;

    /* Get the format of the DIB */
    if (!DibGetBitmapFormat(pbmi, &bitmapformat))
    {
        /* Invalid BITMAPINFO */
        return NULL;
    }

    /* Handle top-down bitmaps */
    if (bitmapformat.sizel.cy < 0) fjBitmap |= BMF_TOPDOWN;

    /* Allocate a surface for the DIB */
    psurf = SURFACE_AllocSurface(STYPE_BITMAP,
                                 bitmapformat.sizel.cx,
                                 abs(bitmapformat.sizel.cy),
                                 bitmapformat.iFormat,
                                 fjBitmap,
                                 0,
                                 pvBits);
    if (!psurf)
    {
        return NULL;
    }

    /* Create a palette from the color info */
    ppal = CreateDIBPalette(pbmi, pdc, iUsage);
    if (!ppal)
    {
        /* Free the surface and return NULL */
        GDIOBJ_vDeleteObject(&psurf->BaseObject);
        return NULL;
    }

    /* Dereference the old palette and set the new */
    PALETTE_ShareUnlockPalette(psurf->ppal);
    psurf->ppal = ppal;

    /* Return the surface */
    return psurf;
}


/* SYSTEM CALLS **************************************************************/

HBITMAP
APIENTRY
NtGdiCreateDIBSection(
    _In_ HDC hdc,
    _In_ OPTIONAL HANDLE hSectionApp,
    _In_ DWORD dwOffset,
    _In_ LPBITMAPINFO pbmiUser,
    _In_ DWORD iUsage,
    _In_ UINT cjInfo,
    _In_ FLONG fl,
    _In_ ULONG_PTR dwColorSpace,
    _Out_opt_ PVOID *ppvBits)
{
    PBITMAPINFO pbmi;
    PSURFACE psurfDIBSection;
    PVOID pvBits = NULL;
    PDC pdc;
    HANDLE hSecure;
    HBITMAP hbmp = NULL;

    /* Capture a safe copy of the bitmap info */
    pbmi = DibProbeAndCaptureBitmapInfo(pbmiUser, iUsage, &cjInfo);
    if (!pbmi)
    {
        /* Could not get bitmapinfo, fail. */
        return NULL;
    }

    /* Check if we got a DC */
    if (hdc)
    {
        /* Lock the DC */
        pdc = DC_LockDc(hdc);
        if (!pdc)
        {
            /* Fail */
            goto cleanup;
        }
    }
    else
    {
        /* No DC */
        pdc = NULL;
    }

    /* Check if the caller passed a section handle */
    if (hSectionApp)
    {
        /* Get the size of the image */
        SIZE_T cjSize = DibGetImageSize(pbmi);

        /* Map the section */
        pvBits = EngMapSectionView(hSectionApp, cjSize, dwOffset, &hSecure);
        if (!pvBits)
        {
            /* Fail */
            goto cleanup;
        }
    }

    /* Create the DIB section surface */
    psurfDIBSection = DibCreateDIBSurface(pbmi,
                                          pdc,
                                          iUsage,
                                          BMF_USERMEM,
                                          pvBits,
                                          0);
    if (psurfDIBSection)
    {
        /* Check if we used a section */
        if (hSectionApp)
        {
            /* Update section and secure handle */
            psurfDIBSection->hDIBSection = hSectionApp;
            psurfDIBSection->hSecure = hSecure;
        }

        /* Return the base address if requested */
        if (ppvBits) *ppvBits = psurfDIBSection->SurfObj.pvBits;

        /* Mark as API bitmap */
        psurfDIBSection->flags |= API_BITMAP;

        /* Mark the palette as a DIB section palette */
        psurfDIBSection->ppal->flFlags |= PAL_DIBSECTION;

        /* Save the handle and unlock the bitmap */
        hbmp = psurfDIBSection->BaseObject.hHmgr;
        SURFACE_UnlockSurface(psurfDIBSection);
    }
    else
    {
        /* Unmap the section view, if we had any */
        if (hSectionApp) EngUnmapSectionView(pvBits, dwOffset, hSecure);
    }

cleanup:
    /* Unlock the DC */
    if (pdc) DC_UnlockDc(pdc);

    /* Free the bitmap info buffer */
    DibFreeBitmapInfo(pbmi);

    return hbmp;
}

HBITMAP
NTAPI
GreCreateDIBitmapInternal(
    _In_ HDC hdc,
    _In_ INT cx,
    _In_ INT cy,
    _In_ DWORD fInit,
    _In_opt_ LPBYTE pjInit,
    _In_opt_ PBITMAPINFO pbmi,
    _In_ DWORD iUsage,
    _In_ UINT cjMaxBits,
    _In_ FLONG fl,
    _In_ HANDLE hcmXform)
{
    PDC pdc;
    PSURFACE psurfDIB, psurfDC, psurfBmp;
    ULONG iFormat;
    PPALETTE ppalBmp;
    HBITMAP hbmp;

    /* Check if we got a DC */
    if (hdc)
    {
        /* Lock the DC */
        pdc = DC_LockDc(hdc);
        if (!pdc)
        {
            return NULL;
        }

        psurfDC = pdc->dclevel.pSurface;
    }
    else
    {
        /* No DC */
        pdc = NULL;
    }

    if (pjInit)
    {
        psurfDIB = DibCreateDIBSurface(pbmi,
                                       pdc,
                                       iUsage,
                                       0,
                                       pjInit,
                                       cjMaxBits);
    }


    if (fInit & CBM_CREATDIB)
    {
        if (iUsage == 2) goto cleanup;

        /* Need a DC for DIB_PAL_COLORS */
        if ((iUsage == DIB_PAL_COLORS) && !pdc) goto cleanup;

        iFormat = psurfDIB->SurfObj.iBitmapFormat;

        if (psurfDIB)
        {
            ppalBmp = psurfDIB->ppal;
            GDIOBJ_vReferenceObjectByPointer(&ppalBmp->BaseObject);
        }
        else
        {
            ppalBmp = CreateDIBPalette(pbmi, pdc, iUsage);
        }
    }
    else
    {
        if (psurfDC)
        {
            /* Use the same format as the DC surface */
            iFormat = psurfDC->SurfObj.iBitmapFormat;
            ppalBmp = psurfDC->ppal;
        }
        else
        {
            __debugbreak();
        }

        GDIOBJ_vReferenceObjectByPointer(&ppalBmp->BaseObject);
    }

    /* Allocate a surface for the bitmap */
    psurfBmp = SURFACE_AllocSurface(STYPE_BITMAP, cx, cy, iFormat, 0, 0, NULL);
    if (psurfBmp)
    {
        /* Set new palette for the bitmap */
        SURFACE_vSetPalette(psurfBmp, ppalBmp);

        if (pjInit)
        {
            RECTL rclDest = {0, 0, cx, cy};
            POINTL ptlSrc = {0, 0};
            EXLATEOBJ exlo;

            /* Initialize XLATEOBJ */
            EXLATEOBJ_vInitialize(&exlo,
                                  psurfBmp->ppal,
                                  psurfDIB->ppal,
                                  RGB(0xff, 0xff, 0xff),
                                  RGB(0xff, 0xff, 0xff),
                                  RGB(0x00, 0x00, 0x00));

            EngCopyBits(&psurfBmp->SurfObj,
                        &psurfDIB->SurfObj,
                        NULL,
                        &exlo.xlo,
                        &rclDest,
                        &ptlSrc);

            EXLATEOBJ_vCleanup(&exlo);
        }

        /* Get the bitmap handle and unlock the bitmap */
        hbmp = psurfBmp->BaseObject.hHmgr;
        SURFACE_UnlockSurface(psurfBmp);
    }


cleanup:

    if (ppalBmp) PALETTE_ShareUnlockPalette(ppalBmp);

    return 0;
}

HBITMAP
APIENTRY
NtGdiCreateDIBitmapInternal(
    _In_ HDC hdc,
    _In_ INT cx,
    _In_ INT cy,
    _In_ DWORD fInit,
    _In_opt_ LPBYTE pjInit,
    _In_opt_ LPBITMAPINFO pbmiUser,
    _In_ DWORD iUsage,
    _In_ UINT cjMaxInitInfo,
    _In_ UINT cjMaxBits,
    _In_ FLONG fl,
    _In_ HANDLE hcmXform)
{
    PBITMAPINFO pbmi = NULL;
    HANDLE hSecure;
    HBITMAP hbmp;

//__debugbreak();

    if (fInit & CBM_INIT)
    {
        if (pjInit)
        {
            hSecure = EngSecureMem(pjInit, cjMaxBits);
            if (!hSecure)
                return NULL;
        }
    }
    else
    {
        pjInit = NULL;
    }

    if (pbmiUser)
    {
        pbmi = DibProbeAndCaptureBitmapInfo(pbmiUser, iUsage, &cjMaxInitInfo);
        if (!pbmi) goto cleanup;
    }


    hbmp = GreCreateDIBitmapInternal(hdc,
                                     cx,
                                     cy,
                                     fInit,
                                     pjInit,
                                     pbmi,
                                     iUsage,
                                     cjMaxBits,
                                     fl,
                                     hcmXform);

    if (pbmi)
    {
        DibFreeBitmapInfo(pbmi);
    }

cleanup:

    if (fInit & CBM_INIT)
    {
        EngUnsecureMem(hSecure);
    }

    return hbmp;
}

INT
APIENTRY
GreGetDIBitmapInfo(
    _In_ HBITMAP hbm,
    _Inout_ LPBITMAPINFO pbmi,
    _In_ UINT iUsage,
    _In_ UINT cjMaxInfo)
{
    PSURFACE psurf;

    /* Lock the bitmap */
    psurf = SURFACE_ShareLockSurface(hbm);
    if (!psurf)
    {
        return 0;
    }

    /* Fill the bitmap info header */
    pbmi->bmiHeader.biWidth = psurf->SurfObj.sizlBitmap.cx;
    if (psurf->SurfObj.fjBitmap & BMF_TOPDOWN)
        pbmi->bmiHeader.biHeight = -psurf->SurfObj.sizlBitmap.cy;
    else
        pbmi->bmiHeader.biHeight = psurf->SurfObj.sizlBitmap.cy;
    pbmi->bmiHeader.biPlanes = 1;
    pbmi->bmiHeader.biCompression = SURFACE_iCompression(psurf);
    pbmi->bmiHeader.biSizeImage = psurf->SurfObj.cjBits;
    pbmi->bmiHeader.biXPelsPerMeter = 0;
    pbmi->bmiHeader.biYPelsPerMeter = 0;
    pbmi->bmiHeader.biBitCount = gajBitsPerFormat[psurf->SurfObj.iBitmapFormat];

    /* Set the number of colors */
    if (pbmi->bmiHeader.biBitCount <= 8)
        pbmi->bmiHeader.biClrUsed = 1 << pbmi->bmiHeader.biBitCount;
    else
        pbmi->bmiHeader.biClrUsed = 0;

    pbmi->bmiHeader.biClrImportant = pbmi->bmiHeader.biClrUsed;

    if (pbmi->bmiHeader.biSize >= sizeof(BITMAPV4HEADER))
    {
        PBITMAPV4HEADER pbV4Header = (PBITMAPV4HEADER)pbmi;

        pbV4Header->bV4RedMask = 0;
        pbV4Header->bV4GreenMask = 0;
        pbV4Header->bV4BlueMask = 0;
        pbV4Header->bV4AlphaMask = 0;
        pbV4Header->bV4CSType = 0;
        //pbV4Header->bV4Endpoints;
        pbV4Header->bV4GammaRed = 0;
        pbV4Header->bV4GammaGreen = 0;
        pbV4Header->bV4GammaBlue = 0;
    }

    /* Unlock the bitmap surface */
    SURFACE_ShareUnlockSurface(psurf);

    return 1;
}


INT
APIENTRY
GreGetDIBits(
    _In_ HDC hdc,
    _In_ HBITMAP hbm,
    _In_ UINT iStartScan,
    _In_ UINT cScans,
    _Out_opt_ LPBYTE pjBits,
    _Inout_ LPBITMAPINFO pbmi,
    _In_ UINT iUsage,
    _In_ UINT cjMaxBits,
    _In_ UINT cjMaxInfo)
{
    PDC pdc;
    PSURFACE psurfBmp;
    ULONG iCompression, cBitsPixel, cjLine;
    LONG lDeltaDst;
    PBYTE pjSrc;
    INT iResult = 0;

    /* Check bitmap info */
    if ((pbmi->bmiHeader.biCompression >= BI_JPEG) ||
        (pbmi->bmiHeader.biWidth <= 0) ||
        (pbmi->bmiHeader.biHeight == 0))
    {
        return 0;
    }

    /* Check if we have a proper bit count value */
    if ((pbmi->bmiHeader.biBitCount != 1) &&
        (pbmi->bmiHeader.biBitCount != 4) &&
        (pbmi->bmiHeader.biBitCount != 8) &&
        (pbmi->bmiHeader.biBitCount != 16) &&
        (pbmi->bmiHeader.biBitCount != 24) &&
        (pbmi->bmiHeader.biBitCount != 32))
    {
        return 0;
    }

    /* Lock the DC */
    pdc = DC_LockDc(hdc);
    if (!pdc)
    {
        return 0;
    }

    /* Lock the bitmap */
    psurfBmp = SURFACE_ShareLockSurface(hbm);
    if (!psurfBmp)
    {
        DC_UnlockDc(pdc);
        return 0;
    }

    /* Check if the bitmap is compatile with the dc */
    if (!DC_bIsBitmapCompatible(pdc, psurfBmp))
    {
        /* Dereference the bitmap, unlock the DC and fail. */
        SURFACE_ShareUnlockSurface(psurfBmp);
        DC_UnlockDc(pdc);
        return 0;
    }

    /* Calculate width of one dest line */
    lDeltaDst = WIDTH_BYTES_ALIGN32(pbmi->bmiHeader.biWidth,
                                    pbmi->bmiHeader.biBitCount);

    /* Calculate the image size */
    pbmi->bmiHeader.biSizeImage = lDeltaDst * abs(pbmi->bmiHeader.biHeight);

    /* Set planes to 1 */
    pbmi->bmiHeader.biPlanes = 1;

    /* Check if there is anything to do */
    if (pjBits == NULL)
    {
        /* Nothing to copy, but we had success */
        iResult = 1;
    }
    else if (iStartScan < (ULONG)psurfBmp->SurfObj.sizlBitmap.cy)
    {
        /* Calculate the maximum number of scan lines to be copied */
        cScans = min(cScans, psurfBmp->SurfObj.sizlBitmap.cy - iStartScan);
        cScans = min(cScans, cjMaxBits / lDeltaDst);

        /* Get the bit depth of the bitmap */
        cBitsPixel = gajBitsPerFormat[psurfBmp->SurfObj.iBitmapFormat];

        /* Get the compression of the bitmap */
        iCompression = SURFACE_iCompression(psurfBmp);

        /* Check if the requested format matches the actual one */
        if ((cBitsPixel == pbmi->bmiHeader.biBitCount) &&
            (iCompression == pbmi->bmiHeader.biCompression))
        {
            /* Calculate the maximum length of a line */
            cjLine = min(lDeltaDst, abs(psurfBmp->SurfObj.lDelta));

            /* Check for top-down bitmaps */
            if (pbmi->bmiHeader.biHeight > 0)
            {
                /* Adjust start position and lDelta */
                pjBits += (cScans - 1) * lDeltaDst;
                lDeltaDst = -lDeltaDst;
            }

            /* Calculate the start address from where to copy */
            pjSrc = psurfBmp->SurfObj.pvScan0;
            pjSrc += iStartScan * psurfBmp->SurfObj.lDelta;

            /* Save number of copied scans */
            iResult = cScans;

            /* Loop all scan lines */
            while (cScans--)
            {
                /* Copy one scan line */
                RtlCopyMemory(pjBits, pjSrc, cjLine);

                /* go to the next scan line */
                pjSrc += psurfBmp->SurfObj.lDelta;
                pjBits += lDeltaDst;
            }

            {
                // TODO: create color table
            }
        }
        else
        {
            RECTL rclDest = {0, 0, pbmi->bmiHeader.biWidth - 1, cScans -1};
            POINTL ptlSrc = {0, iStartScan};
            EXLATEOBJ exlo;
            PSURFACE psurfDIB;

            /* Create a DIB surface */
            psurfDIB = DibCreateDIBSurface(pbmi,
                                           pdc,
                                           iUsage,
                                           0,
                                           pjBits,
                                           cjMaxBits);
            if (psurfDIB)
            {
                /* Initialize XLATEOBJ */
                EXLATEOBJ_vInitialize(&exlo,
                                      psurfDIB->ppal,
                                      psurfBmp->ppal,
                                      RGB(0xff, 0xff, 0xff),
                                      pdc->pdcattr->crBackgroundClr,
                                      pdc->pdcattr->crForegroundClr);

                EngCopyBits(&psurfDIB->SurfObj,
                            &psurfBmp->SurfObj,
                            NULL,
                            &exlo.xlo,
                            &rclDest,
                            &ptlSrc);

                /* Cleanup */
                EXLATEOBJ_vCleanup(&exlo);
                GDIOBJ_vDeleteObject(&psurfDIB->BaseObject);

                /* Return the number of copied scan lnes */
                iResult = cScans;
            }
        }
    }
    else
    {
        /* There is nothing to copy */
        iResult = 0;
    }

    /* Unlock the bitmap surface and the DC */
    SURFACE_ShareUnlockSurface(psurfBmp);
    DC_UnlockDc(pdc);

    return iResult;
}

INT
APIENTRY
NtGdiGetDIBitsInternal(
    _In_ HDC hdc,
    _In_ HBITMAP hbm,
    _In_ UINT iStartScan,
    _In_ UINT cScans,
    _Out_opt_ LPBYTE pjBits,
    _Inout_ LPBITMAPINFO pbmiUser,
    _In_ UINT iUsage,
    _In_ UINT cjMaxBits,
    _In_ UINT cjMaxInfo)
{
    PBITMAPINFO pbmi;
    HANDLE hSecure;
    INT iResult = 0;

    /* Check for bad iUsage */
    if (iUsage > 2) return 0;

    /* Check if the size of the bitmap info is large enough */
    if (cjMaxInfo < sizeof(BITMAPINFOHEADER))
    {
        return 0;
    }

    /* Use maximum size */
    cjMaxInfo = min(cjMaxInfo, sizeof(BITMAPV5HEADER) + 256 * sizeof(RGBQUAD));

    /* Allocate a buffer the bitmapinfo */
    pbmi = ExAllocatePoolWithTag(PagedPool, cjMaxInfo, GDITAG_BITMAPINFO);
    if (!pbmi)
    {
        /* Fail */
        return 0;
    }

    /* Use SEH */
    _SEH2_TRY
    {
        /* Probe and copy the BITMAPINFO */
        ProbeForWrite(pbmiUser, cjMaxInfo, 1);
        RtlCopyMemory(pbmi, pbmiUser, cjMaxInfo);
    }
    _SEH2_EXCEPT(EXCEPTION_EXECUTE_HANDLER)
    {
        _SEH2_YIELD(goto cleanup;)
    }
    _SEH2_END;

    /* Check if the header size is large enough */
    if ((pbmi->bmiHeader.biSize < sizeof(BITMAPINFOHEADER)) ||
        (pbmi->bmiHeader.biSize > cjMaxInfo))
    {
        iResult = 0;
        goto cleanup;
    }

    /* Check if the caller provided bitmap bits */
    if (pjBits)
    {
        /* Secure the user mode memory */
        hSecure = EngSecureMem(pjBits, cjMaxBits);
        if (!hSecure)
        {
            goto cleanup;
        }
    }

    /* Check if the bitcount member is initialized */
    if (pbmi->bmiHeader.biBitCount != 0)
    {
        /* Call the internal function */
        iResult = GreGetDIBits(hdc,
                               hbm,
                               iStartScan,
                               cScans,
                               pjBits,
                               pbmi,
                               iUsage,
                               cjMaxBits,
                               cjMaxInfo);
    }
    else if (pjBits == NULL)
    {
        /* Just get the BITMAPINFO */
        iResult = GreGetDIBitmapInfo(hbm, pbmi, iUsage, cjMaxInfo);
    }
    else
    {
        /* This combination is not valid */
        iResult = 0;
    }

    /* Check for success */
    if (iResult)
    {
        /* Use SEH to copy back to user mode */
        _SEH2_TRY
        {
            /* Buffer is already probed, copy the data back */
            RtlCopyMemory(pbmiUser, pbmi, cjMaxInfo);
        }
        _SEH2_EXCEPT(EXCEPTION_EXECUTE_HANDLER)
        {
        }
        _SEH2_END;
    }

cleanup:
    if (hSecure) EngUnsecureMem(hSecure);
    ExFreePoolWithTag(pbmi, GDITAG_BITMAPINFO);

    return iResult;
}

INT
NTAPI
GreStretchDIBitsInternal(
    _In_ HDC hdc,
    _In_ INT xDst,
    _In_ INT yDst,
    _In_ INT cxDst,
    _In_ INT cyDst,
    _In_ INT xSrc,
    _In_ INT ySrc,
    _In_ INT cxSrc,
    _In_ INT cySrc,
    _In_opt_ LPBYTE pjInit,
    _In_ LPBITMAPINFO pbmi,
    _In_ ULONG iUsage,
    _In_ DWORD dwRop,
    _In_ ULONG cjMaxInfo,
    _In_ ULONG cjMaxBits,
    _In_ BOOL bTransformCoordinates,
    _In_ HANDLE hcmXform)
{
    PDC pdc;
    RECTL rcSrc, rcDst;
    EXLATEOBJ exlo;
    PSURFACE psurfDIB, psurfDst;
    BOOL bResult;

    /* Lock the DC */
    pdc = DC_LockDc(hdc);
    if (!pdc)
    {
        return 0;
    }

    /* Get the dest surface */
    psurfDst = pdc->dclevel.pSurface;
    if (!psurfDst)
    {
        DC_UnlockDc(pdc);
        return 0;
    }

    /* Create the DIB surface */
    psurfDIB = DibCreateDIBSurface(pbmi,
                                   pdc,
                                   iUsage,
                                   0,
                                   pjInit,
                                   cjMaxBits);
    if (!psurfDIB)
    {
        DC_UnlockDc(pdc);
        return 0;
    }

    /* Calculate source and destination rect */
    rcSrc.left = xSrc;
    rcSrc.top = ySrc;
    rcSrc.right = xSrc + cxSrc;
    rcSrc.bottom = ySrc + cySrc;
    rcDst.left = xDst;
    rcDst.top = yDst;

    if (bTransformCoordinates)
    {
        /* Use the size in logical units */
        rcDst.right = rcDst.left + cxDst;
        rcDst.bottom = rcDst.top + cyDst;

        /* Transform destination rect to device coordinates */
        IntLPtoDP(pdc, (POINTL*)&rcDst, 2);

        /* Get the new dest size in device units */
        cxDst = rcDst.right - rcDst.left;
        cyDst = rcDst.bottom - rcDst.top;
    }
    else
    {
        /* Transform destination start point to device coordinates */
        IntLPtoDP(pdc, (POINTL*)&rcDst, 1);

        /* Use the size in device units */
        rcDst.right = rcDst.left + cxDst;
        rcDst.bottom = rcDst.top + cyDst;
    }

    /* Compensate for the DC origin */
    RECTL_vOffsetRect(&rcDst, pdc->ptlDCOrig.x, pdc->ptlDCOrig.y);

    /* Initialize XLATEOBJ */
    EXLATEOBJ_vInitialize(&exlo,
                          psurfDIB->ppal,
                          psurfDst->ppal,
                          RGB(0xff, 0xff, 0xff),
                          pdc->pdcattr->crBackgroundClr,
                          pdc->pdcattr->crForegroundClr);

    /* Prepare DC for blit */
    DC_vPrepareDCsForBlit(pdc, rcDst, NULL, rcSrc);

    /* Perform the stretch operation (note: this will go to
       EngBitBlt when no stretching is needed) */
    bResult = IntEngStretchBlt(&psurfDst->SurfObj,
                               &psurfDIB->SurfObj,
                               NULL,
                               pdc->rosdc.CombinedClip,
                               &exlo.xlo,
                               &rcDst,
                               &rcSrc,
                               NULL,
                               &pdc->eboFill.BrushObject,
                               NULL,
                               ROP3_TO_ROP4(dwRop));

    /* Cleanup */
    DC_vFinishBlit(pdc, NULL);
    EXLATEOBJ_vCleanup(&exlo);
    GDIOBJ_vDeleteObject(&psurfDIB->BaseObject);
    DC_UnlockDc(pdc);

    return bResult ? cySrc : 0;
}

INT
APIENTRY
NtGdiSetDIBitsToDeviceInternal(
    _In_ HDC hdcDest,
    _In_ INT xDst,
    _In_ INT yDst,
    _In_ DWORD cx,
    _In_ DWORD cy,
    _In_ INT xSrc,
    _In_ INT ySrc,
    _In_ DWORD iStartScan,
    _In_ DWORD cNumScan,
    _In_ LPBYTE pjInitBits,
    _In_ LPBITMAPINFO pbmiUser,
    _In_ DWORD iUsage,
    _In_ UINT cjMaxBits,
    _In_ UINT cjMaxInfo,
    _In_ BOOL bTransformCoordinates,
    _In_opt_ HANDLE hcmXform)
{
    PBITMAPINFO pbmi;
    HANDLE hSecure;
    INT iResult = 0;

    /* Capture a safe copy of the bitmap info */
    pbmi = DibProbeAndCaptureBitmapInfo(pbmiUser, iUsage, &cjMaxInfo);
    if (!pbmi)
    {
        /* Could not get bitmapinfo, fail. */
        return 0;
    }

    /* Secure the user mode buffer for the bits */
    hSecure = EngSecureMemForRead(pjInitBits, cjMaxBits);
    if (hSecure)
    {
        /* Call the internal function with the secure pointers */
        iResult = GreStretchDIBitsInternal(hdcDest,
                                           xDst,
                                           yDst,
                                           cx,
                                           cy,
                                           xSrc,
                                           ySrc,
                                           cx,
                                           cy,
                                           pjInitBits,
                                           pbmi,
                                           iUsage,
                                           MAKEROP4(SRCCOPY, SRCCOPY),
                                           cjMaxInfo,
                                           cjMaxBits,
                                           bTransformCoordinates,
                                           hcmXform);

        /* Unsecure the memory */
        EngUnsecureMem(hSecure);
    }

    /* Free the bitmap info */
    DibFreeBitmapInfo(pbmi);

    /* Return the result */
    return iResult;
}


INT
APIENTRY
NtGdiStretchDIBitsInternal(
    _In_ HDC hdc,
    _In_ INT xDst,
    _In_ INT yDst,
    _In_ INT cxDst,
    _In_ INT cyDst,
    _In_ INT xSrc,
    _In_ INT ySrc,
    _In_ INT cxSrc,
    _In_ INT cySrc,
    _In_opt_ LPBYTE pjInit,
    _In_ LPBITMAPINFO pbmiUser,
    _In_ DWORD iUsage,
    _In_ DWORD dwRop, // ms ntgdi.h says dwRop4(?)
    _In_ UINT cjMaxInfo,
    _In_ UINT cjMaxBits,
    _In_ HANDLE hcmXform)
{
    PBITMAPINFO pbmi;
    HANDLE hSecure;
    INT iResult = 0;

    /* Capture a safe copy of the bitmap info */
    pbmi = DibProbeAndCaptureBitmapInfo(pbmiUser, iUsage, &cjMaxInfo);
    if (!pbmi)
    {
        /* Could not get bitmapinfo, fail. */
        return 0;
    }

    /* Secure the user mode buffer for the bits */
    hSecure = EngSecureMemForRead(pjInit, cjMaxBits);
    if (hSecure)
    {
        /* Call the internal function with the secure pointers */
        iResult = GreStretchDIBitsInternal(hdc,
                                           xDst,
                                           yDst,
                                           cxDst,
                                           cyDst,
                                           xSrc,
                                           ySrc,
                                           cxSrc,
                                           cySrc,
                                           pjInit,
                                           pbmi,
                                           iUsage,
                                           dwRop,
                                           cjMaxInfo,
                                           cjMaxBits,
                                           TRUE,
                                           hcmXform);

        /* Unsecure the memory */
        EngUnsecureMem(hSecure);
    }

    /* Free the bitmap info */
    DibFreeBitmapInfo(pbmi);

    /* Return the result */
    return iResult;
}


///////////////////////////////////////////////////////////////

// old stuff

BITMAPINFO*
FASTCALL
_DIB_ConvertBitmapInfo(CONST BITMAPINFO* pbmi, DWORD Usage)
{
    __debugbreak();
    return 0;
}

VOID
FASTCALL
_DIB_FreeConvertedBitmapInfo(BITMAPINFO* converted, BITMAPINFO* orig)
{
    if(converted != orig)
        ExFreePoolWithTag(converted, TAG_DIB);
}

