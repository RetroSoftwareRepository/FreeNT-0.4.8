
/* Used as iUsage to create a PAL_BRUSHHACK palete */
#define DIB_PAL_BRUSHHACK 3

#define GDITAG_BITMAPINFO 'imbG'

typedef struct _BITMAPFORMAT
{
    SIZEL sizel;
    ULONG cBitsPixel;
    ULONG iCompression;
    ULONG iFormat;
} BITMAPFORMAT, *PBITMAPFORMAT;

ULONG
NTAPI
DibGetBitmapInfoSize(
    _In_ const BITMAPINFO *pbmi,
    _In_ ULONG iUsage);

INT
NTAPI
GreGetDIBitsInternal(
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

#define DIB_BitmapInfoSize
