
/* Used as iUsage to create a PAL_BRUSHHACK palete */
#define DIB_PAL_BRUSHHACK 3

#define GDITAG_BITMAPINFO 'imbG'

typedef struct _BITMAPFORMAT
{
    SIZEL sizel;
    ULONG cBitsPixel;
    ULONG iCompression;
    ULONG iFormat;
    ULONG cjWidthBytes;
    ULONG cjImageSize;
} BITMAPFORMAT, *PBITMAPFORMAT;

ULONG
NTAPI
DibGetBitmapInfoSize(
    _In_ const BITMAPINFO *pbmi,
    _In_ ULONG iUsage);

INT
APIENTRY
GreGetDIBitmapInfo(
    _In_ HBITMAP hbm,
    _Inout_ LPBITMAPINFO pbmi,
    _In_ UINT iUsage,
    _In_ UINT cjMaxInfo);

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
    _In_ UINT cjMaxInfo);

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
    _In_ HANDLE hcmXform);


// other stuff

HANDLE
APIENTRY
EngSecureMemForRead(PVOID Address, ULONG Length);

#define ROP3_TO_ROP4(dwRop3) \
    ((((dwRop3) & 0xFF0000) >> 8) | (((dwRop3) & 0xFF0000) >> 16))


// old stuff

BITMAPINFO*
FASTCALL
DIB_ConvertBitmapInfo(CONST BITMAPINFO* pbmi, DWORD Usage);

VOID
FASTCALL
DIB_FreeConvertedBitmapInfo(BITMAPINFO* converted, BITMAPINFO* orig);

INT FASTCALL DIB_BitmapInfoSize(const BITMAPINFO * info, WORD coloruse);

INT
FORCEINLINE
DIB_GetDIBImageBytes(INT  width, INT height, INT depth)
{
    return WIDTH_BYTES_ALIGN32(width, depth) * (height < 0 ? -height : height);
}

UINT
APIENTRY
IntGetDIBColorTable(
    HDC hDC,
    UINT StartIndex,
    UINT Entries,
    RGBQUAD *Colors);

W32KAPI
INT
APIENTRY
OldNtGdiSetDIBitsToDeviceInternal(
    IN HDC hDC,
    IN INT XDest,
    IN INT YDest,
    IN DWORD Width,
    IN DWORD Height,
    IN INT XSrc,
    IN INT YSrc,
    IN DWORD StartScan,
    IN DWORD ScanLines,
    IN LPBYTE Bits,
    IN LPBITMAPINFO bmi,
    IN DWORD ColorUse,
    IN UINT cjMaxBits,
    IN UINT cjMaxInfo,
    IN BOOL bTransformCoordinates,
    IN OPTIONAL HANDLE hcmXform);

INT
APIENTRY
OldNtGdiGetDIBitsInternal(
    HDC hDC,
    HBITMAP hBitmap,
    UINT StartScan,
    UINT ScanLines,
    LPBYTE Bits,
    LPBITMAPINFO Info,
    UINT Usage,
    UINT MaxBits,
    UINT MaxInfo);

W32KAPI
INT
APIENTRY
OldNtGdiStretchDIBitsInternal(
    IN HDC hdc,
    IN INT xDst,
    IN INT yDst,
    IN INT cxDst,
    IN INT cyDst,
    IN INT xSrc,
    IN INT ySrc,
    IN INT cxSrc,
    IN INT cySrc,
    IN OPTIONAL LPBYTE pjInit,
    IN LPBITMAPINFO pbmi,
    IN DWORD dwUsage,
    IN DWORD dwRop, // MS ntgdi.h says dwRop4(?)
    IN UINT cjMaxInfo,
    IN UINT cjMaxBits,
    IN HANDLE hcmXform);

HBITMAP
APIENTRY
OldNtGdiCreateDIBitmapInternal(
    IN HDC hDc,
    IN INT cx,
    IN INT cy,
    IN DWORD fInit,
    IN OPTIONAL LPBYTE pjInit,
    IN OPTIONAL LPBITMAPINFO pbmi,
    IN DWORD iUsage,
    IN UINT cjMaxInitInfo,
    IN UINT cjMaxBits,
    IN FLONG fl,
    IN HANDLE hcmXform);

HBITMAP
APIENTRY
OldNtGdiCreateDIBSection(
    IN HDC hDC,
    IN OPTIONAL HANDLE hSection,
    IN DWORD dwOffset,
    IN BITMAPINFO* bmi,
    IN DWORD Usage,
    IN UINT cjHeader,
    IN FLONG fl,
    IN ULONG_PTR dwColorSpace,
    OUT PVOID *Bits);


