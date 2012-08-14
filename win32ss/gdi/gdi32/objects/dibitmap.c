

#define WIDTH_BYTES_ALIGN32(cx, bpp) ((((cx) * (bpp) + 31) & ~31) >> 3)

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


static
PBITMAPINFO
ConvertBitmapCoreInfo(
    _In_ const BITMAPINFO *pbmiCore,
    _In_ UINT iUsage)
{
    const BITMAPCOREINFO *pbci = (const BITMAPCOREINFO *)pbmiCore;
    BITMAPINFO *pbmi;
    ULONG i, cColors, cjColorTable;

    /* Get the number of colors in bmciColors */
    if (pbci->bmciHeader.bcBitCount == 1) cColors = 2;
    else if (pbci->bmciHeader.bcBitCount == 4) cColors = 16;
    else if (pbci->bmciHeader.bcBitCount == 8) cColors = 256;
    else cColors = 0;

    /* Calculate the size of the output color table */
    if (iUsage == DIB_RGB_COLORS)
        cjColorTable =  + cColors * sizeof(RGBTRIPLE);
    else
        cjColorTable = cColors * sizeof(WORD);

    /* Allocate the BITMAPINFO */
    pbmi = HeapAlloc(GetProcessHeap(), 0, sizeof(BITMAPINFOHEADER) + cjColorTable);
    if (!pbmi) return NULL;

    /* Fill the BITMAPINFOHEADER */
    pbmi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    pbmi->bmiHeader.biWidth = pbci->bmciHeader.bcWidth;
    pbmi->bmiHeader.biHeight = pbci->bmciHeader.bcHeight;
    pbmi->bmiHeader.biPlanes = pbci->bmciHeader.bcPlanes;
    pbmi->bmiHeader.biBitCount = pbci->bmciHeader.bcBitCount;
    pbmi->bmiHeader.biCompression = BI_RGB;
    pbmi->bmiHeader.biSizeImage = 0;
    pbmi->bmiHeader.biXPelsPerMeter = 0;
    pbmi->bmiHeader.biYPelsPerMeter = 0;
    pbmi->bmiHeader.biClrUsed = cColors;
    pbmi->bmiHeader.biClrImportant = cColors;

    /* Check what type of color table we have */
    if (iUsage == DIB_RGB_COLORS)
    {
        /* The colors are an array of RGBTRIPLE / RGBQUAD */
        RGBTRIPLE *prgbIn = (RGBTRIPLE*)pbci->bmciColors;
        RGBQUAD *prgbOut = (RGBQUAD*)pbmi->bmiColors;

        for (i = 0; i < cColors; i++)
        {
            prgbOut[i].rgbRed = prgbIn[i].rgbtRed;
            prgbOut[i].rgbGreen = prgbIn[i].rgbtGreen;
            prgbOut[i].rgbBlue = prgbIn[i].rgbtBlue;
            prgbOut[i].rgbReserved = 0;
        }
    }
    else
    {
        /* The colors are an array of WORD indices */
        PWORD pwColorsIn = (PWORD)pbci->bmciColors;
        PWORD pwColorsOut = (PWORD)pbmi->bmiColors;

        for (i = 0; i < cColors; i++)
            pwColorsOut[i] = pwColorsIn[i];

    }

    return pbmi;
}

VOID
WINAPI
ConvertBackToCoreInfo(
    _In_ BITMAPCOREINFO *pbci,
    _In_ const BITMAPINFO *pbmi,
    _In_ UINT iUsage)
{
    ULONG i, cColors;

    pbci->bmciHeader.bcWidth = (WORD)pbmi->bmiHeader.biWidth;
    pbci->bmciHeader.bcHeight = (WORD)pbmi->bmiHeader.biHeight;
    pbci->bmciHeader.bcPlanes = (WORD)pbmi->bmiHeader.biPlanes;
    pbci->bmciHeader.bcBitCount = (WORD)pbmi->bmiHeader.biBitCount;
    if (pbci->bmciHeader.bcBitCount > 16) pbci->bmciHeader.bcBitCount = 24;

    /* Get the number of colors in bmciColors */
    if (pbci->bmciHeader.bcBitCount == 1) cColors = 2;
    else if (pbci->bmciHeader.bcBitCount == 4) cColors = 16;
    else if (pbci->bmciHeader.bcBitCount == 8) cColors = 256;
    else cColors = 0;

    /* Check what type of color table we have */
    if (iUsage == DIB_RGB_COLORS)
    {
        /* The colors are an array of RGBTRIPLE / RGBQUAD */
        RGBTRIPLE *prgbOut = (RGBTRIPLE*)pbci->bmciColors;
        RGBQUAD *prgbIn = (RGBQUAD*)pbmi->bmiColors;

        for (i = 0; i < cColors; i++)
        {
            prgbOut[i].rgbtRed = prgbIn[i].rgbRed;
            prgbOut[i].rgbtGreen = prgbIn[i].rgbGreen;
            prgbOut[i].rgbtBlue = prgbIn[i].rgbBlue;
        }
    }
    else
    {
        /* The colors are an array of WORD indices */
        PWORD pwColorsOut = (PWORD)pbci->bmciColors;
        PWORD pwColorsIn = (PWORD)pbmi->bmiColors;

        for (i = 0; i < cColors; i++)
            pwColorsOut[i] = pwColorsIn[i];

    }

}

HBITMAP
WINAPI
CreateDIBitmap(
    _In_ HDC hdc,
    _In_ const BITMAPINFOHEADER *pbmih,
    _In_ DWORD fdwInit,
    _In_ const VOID *pbInit,
    _In_ const BITMAPINFO *pbmi,
    _In_ UINT iUsage)
{
    LONG cx, cy;
    BOOL bConvertedInfo = FALSE;
    HBITMAP hbmp = NULL;
    ULONG cjInfo, cjImageSize;

    if (pbmi)
    {
        if (pbmi->bmiHeader.biSize == sizeof(BITMAPCOREHEADER))
        {
            pbmi = ConvertBitmapCoreInfo(pbmi, iUsage);
            bConvertedInfo = TRUE;
        }

        /* Get the size of the BITMAPINFO */
        cjInfo = DibGetBitmapInfoSize(pbmi, iUsage);

        /* Check if we have bits */
        if (pbInit)
        {
            /* Get the size of the image */
            cjImageSize = pbmi->bmiHeader.biSizeImage;

            /* Check if we need to calculate the size */
            if ((cjImageSize == 0) &&
                ((pbmi->bmiHeader.biCompression == BI_RGB) ||
                (pbmi->bmiHeader.biCompression == BI_BITFIELDS)))
            {
                /* Calculate the image size */
                cjImageSize = abs(pbmi->bmiHeader.biHeight) *
                              WIDTH_BYTES_ALIGN32(pbmi->bmiHeader.biWidth,
                                                  pbmi->bmiHeader.biBitCount);
            }
        }
    }
    else
    {
        cjInfo = 0;
    }

    if (fdwInit & CBM_CREATDIB)
    {
        if (!pbmi) return NULL;

        //__debugbreak();
        return 0;
    }
    else
    {
        if (!pbmih)
        {
            SetLastError(ERROR_INVALID_PARAMETER);
            goto cleanup;
        }

        if (pbmih->biSize == sizeof(BITMAPCOREHEADER))
        {
            PBITMAPCOREHEADER pbch = (PBITMAPCOREHEADER)pbmih;
            cx = pbch->bcWidth;
            cy = abs(pbch->bcHeight);
        }
        else if (pbmih->biSize >= sizeof(BITMAPINFOHEADER))
        {
            cx = pbmih->biWidth;
            cy = abs(pbmih->biHeight);
        }
        else
        {
            SetLastError(ERROR_INVALID_PARAMETER);
            goto cleanup;
        }

        if ((cx == 0) || (cy == 0))
        {
            return GetStockObject(DEFAULT_BITMAP);
        }
    }


    hbmp = NtGdiCreateDIBitmapInternal(hdc,
                                       cx,
                                       cy,
                                       fdwInit,
                                       (PVOID)pbInit,
                                       (PVOID)pbmi,
                                       iUsage,
                                       cjInfo,
                                       cjImageSize,
                                       0,
                                       0);

cleanup:
    if (bConvertedInfo && pbmi) HeapFree(GetProcessHeap(), 0, (PVOID)pbmi);

    return hbmp;
}

INT
WINAPI
GetDIBits(
    HDC hdc,
    HBITMAP hbmp,
    UINT uStartScan,
    UINT cScanLines,
    LPVOID pvBits,
    LPBITMAPINFO pbmi,
    UINT iUsage)
{
    UINT cjBmpScanSize;
    UINT cjInfoSize;
    INT iResult;
    BITMAPCOREINFO *pbci = NULL;

    if (!hdc || !GdiIsHandleValid((HGDIOBJ)hdc) || !pbmi)
    {
        GdiSetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }

    if (pbmi && (pbmi->bmiHeader.biSize == sizeof(BITMAPCOREHEADER)))
    {
        pbci = (BITMAPCOREINFO *)pbmi;
        pbmi = ConvertBitmapCoreInfo(pbmi, iUsage);
        if (!pbmi)
            return 0;
    }

    cjBmpScanSize = DIB_BitmapMaxBitsSize(pbmi, cScanLines);
    cjInfoSize = DIB_BitmapInfoSize(pbmi, iUsage);

    if (pvBits )
    {
        /* Check for compressed formats */
        if ((pbmi->bmiHeader.biSize >= sizeof(BITMAPINFOHEADER)) &&
            (pbmi->bmiHeader.biCompression >= BI_JPEG))
        {
            SetLastError(ERROR_INVALID_PARAMETER);
            return 0;
        }
    }

    iResult = NtGdiGetDIBitsInternal(hdc,
                                  hbmp,
                                  uStartScan,
                                  cScanLines,
                                  pvBits,
                                  pbmi,
                                  iUsage,
                                  cjBmpScanSize,
                                  cjInfoSize);

    if (pbci)
    {
        ConvertBackToCoreInfo(pbci, pbmi, iUsage);
        HeapFree(GetProcessHeap(), 0, (PVOID)pbmi);
    }

    return iResult;
}

