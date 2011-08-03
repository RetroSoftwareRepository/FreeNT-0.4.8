#pragma once

/* GDI logical font object */
typedef struct _LFONT TEXTOBJ, *PTEXTOBJ;

/*  Internal interface  */

#define  TEXTOBJ_UnlockText(pBMObj) GDIOBJ_vUnlockObject ((POBJ)pBMObj)
NTSTATUS FASTCALL TextIntCreateFontIndirect(CONST LPLOGFONTW lf, HFONT *NewFont);
BOOL FASTCALL InitFontSupport(VOID);
ULONG FASTCALL FontGetObject(PTEXTOBJ TextObj, ULONG Count, PVOID Buffer);
BOOL FASTCALL GreTextOutW(HDC,int,int,LPCWSTR,int);
HFONT FASTCALL GreCreateFontIndirectW( LOGFONTW * );
BOOL WINAPI GreGetTextMetricsW( _In_  HDC hdc, _Out_ LPTEXTMETRICW lptm);

BOOL
NTAPI
GreExtTextOutW(
    IN HDC,
    IN INT,
    IN INT,
    IN UINT,
    IN OPTIONAL RECTL*,
    IN LPWSTR,
    IN INT,
    IN OPTIONAL LPINT,
    IN DWORD);
