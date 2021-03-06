
#include <windows.h>
#include <tchar.h>
#include "AsmView.h"
#include "AsmViewInternal.h"

static const char *CtrlStr(DWORD ch)
{
	static const char *reps[] = 
	{
		"NUL", "SOH", "STX", "ETX", "EOT", "ENQ", "ACK", "BEL",
		"BS",  "HT",  "LF",  "VT",  "FF",  "CR",  "SO",  "SI",
		"DLE", "DC1", "DC2", "DC3", "DC4", "NAK", "SYN", "ETB",
		"CAN", "EM",  "SUB", "ESC", "FS",  "GS",  "RS",  "US"
	};

	return ch < ' ' ? reps[ch] : "???";
}

void PaintRect(HDC hdc, RECT *rect, COLORREF fill)
{
	fill = SetBkColor(hdc, fill);
	
	ExtTextOut(hdc, 0, 0, ETO_OPAQUE, rect, 0, 0, 0);
	
	SetBkColor(hdc, fill);
}

//
//	Return width of specified control-character
//
// int AsmView::CtrlCharWidth(HDC hdc, ULONG chValue, FONT *font)
// {
// 	SIZE sz;
// 	const char *str = CtrlStr(chValue % 32);
// 	GetTextExtentPoint32(hdc, str, strlen(str), &sz);
// 	return sz.cx + 4;
// }

//
//	AsmView::
//
int AsmView::NeatTextYOffset(FONT *font)
{
	return m_nMaxAscent + m_nHeightAbove - font->tm.tmAscent;
}

//
//	Wrapper for GetTextExtentPoint32. Takes into account
//	control-characters, tabs etc.
//
int AsmView::NeatTextWidth(HDC hdc, TCHAR *buf, int len, int nTabOrigin)
{
	SIZE	sz;
	int		width = 0;

	const int TABWIDTHPIXELS = TabWidth();

	if(len == -1)
		len = lstrlen(buf);

	for(int i = 0, lasti = 0; i <= len; i++)
	{
		if(i == len || buf[i] == '\t'/* || (TBYTE)buf[i] < 32*/)
		{
			GetTextExtentPoint32(hdc, buf + lasti, i - lasti, &sz);
			//GetTextExtentExPoint(hdc, buf + lasti, i - lasti,0,NULL,NULL,&sz);
			width += sz.cx;

			if(i < len && buf[i] == '\t')
			{
				width += TABWIDTHPIXELS - ((width - nTabOrigin) % TABWIDTHPIXELS);
				lasti  = i + 1;
			}
// 			else if(i < len && (TBYTE)buf[i] < 32)
// 			{
// 				width += CtrlCharWidth(hdc, buf[i], &m_FontAttr[0]);
// 				lasti  = i + 1;
// 			}
		}
	}

	return width;
}


//
//	Manually calculate the internal-leading and descent
//  values for a font by parsing a small bitmap of a single letter "E"
//	and locating the top and bottom of this letter.
//
void AsmView::InitCtrlCharFontAttr(HDC hdc, FONT *font)
{
	// create a temporary off-screen bitmap
	HDC		hdcTemp = CreateCompatibleDC(hdc);
	HBITMAP hbmTemp = CreateBitmap(font->tm.tmAveCharWidth, font->tm.tmHeight, 1, 1, 0);
	HANDLE  hbmOld  = SelectObject(hdcTemp, hbmTemp);
	HANDLE  hfnOld	= SelectObject(hdcTemp, font->hFont);

	// black-on-white text
	SetTextColor(hdcTemp,	RGB(0,0,0));
	SetBkColor(hdcTemp,		RGB(255,255,255));
	SetBkMode(hdcTemp,		OPAQUE);

	TextOut(hdcTemp, 0, 0, "E", 1);

	// give default values just in case the scan fails
	font->nInternalLeading	= font->tm.tmInternalLeading;
	font->nDescent			= font->tm.tmDescent;

	// scan downwards looking for the top of the letter 'E'
	for(int y = 0; y < font->tm.tmHeight; y++)
	{
		for(int x = 0; x < font->tm.tmAveCharWidth; x++)
		{
			COLORREF col;

			if((col = GetPixel(hdcTemp, x, y)) == RGB(0,0,0))
			{
				font->nInternalLeading = y;
				y = font->tm.tmHeight;
				break;
			}
		}
	}

	// scan upwards looking for the bottom of the letter 'E'
	for(int y = font->tm.tmHeight - 1; y >= 0; y--)
	{
		for(int x = 0; x < font->tm.tmAveCharWidth; x++)
		{
			COLORREF col;

			if((col = GetPixel(hdcTemp, x, y)) == RGB(0,0,0))
			{
				font->nDescent = font->tm.tmHeight - y - 1;
				y = 0;
				break;
			}
		}
	}

	// give larger fonts a thicker border
	if(font->nInternalLeading > 1 && font->nDescent > 1 && font->tm.tmHeight > 18)
	{
		font->nInternalLeading--;
		font->nDescent--;
	}

	// cleanup
	SelectObject(hdcTemp, hbmOld);
	SelectObject(hdcTemp, hfnOld);
	DeleteDC(hdcTemp);
	DeleteObject(hbmTemp);
}

//
//	Display an ASCII control character in inverted colours
//  to what is currently set in the DC
//
// int AsmView::PaintCtrlChar(HDC hdc, int xpos, int ypos, ULONG chValue, FONT *font)
// {
// 	SIZE  sz;
// 	RECT  rect;
// 	const char *str = CtrlStr(chValue % 32);
// 
// 	int yoff = NeatTextYOffset(font);
// 
// 	COLORREF fg = GetTextColor(hdc);
// 	COLORREF bg = GetBkColor(hdc); 
// 
// 	// find out how big the text will be
// 	GetTextExtentPoint32(hdc, str, strlen(str), &sz);
// 	SetRect(&rect, xpos, ypos, xpos + sz.cx + 4, ypos + m_nLineHeight);
// 
// 	// paint the background white
// 	if(GetBkMode(hdc) == OPAQUE)
// 		PaintRect(hdc, &rect, bg);
// 
// 	// adjust rectangle for first black block
// 	rect.right  -= 1;
// 	rect.top    += font->nInternalLeading + yoff;
// 	rect.bottom =  rect.top + font->tm.tmHeight - font->nDescent - font->nInternalLeading;
// 
// 	// paint the first black block
// 	PaintRect(hdc, &rect, fg);
// 	
// 	// prepare device context
// 	fg = SetTextColor(hdc, bg);
// 	bg = SetBkColor(hdc, fg);
// 	
// 	// paint the text and the second "black" block at the same time
// 	InflateRect(&rect, -1, 1);
// 	ExtTextOut(hdc, xpos+1, ypos+yoff, ETO_OPAQUE|ETO_CLIPPED, &rect, str, strlen(str), 0);
// 	
// 	// restore device context
// 	SetTextColor(hdc, fg);
// 	SetBkColor(hdc, bg);
// 	
// 	return sz.cx + 4;
// }

//
//	Update the lineheight based on current font settings
//
VOID AsmView::RecalcLineHeight()
{
	m_nLineHeight	= 0;
	m_nMaxAscent	= 0;

	// find the tallest font in the AsmView
	for(int i = 0; i < m_nNumFonts; i++)
	{
		// always include a font's external-leading
		int fontheight = m_FontAttr[i].tm.tmHeight + 
						 m_FontAttr[i].tm.tmExternalLeading;

		m_nLineHeight = max(m_nLineHeight, fontheight);
		m_nMaxAscent  = max(m_nMaxAscent, m_FontAttr[i].tm.tmAscent);
	}

	// add on the above+below spacings
	m_nLineHeight += m_nHeightAbove + m_nHeightBelow;

	// force caret resize if we've got the focus
	if(GetFocus() == m_hWnd)
	{
		OnKillFocus(0);
		OnSetFocus(0);
	}
}

//
//	Set a font for the AsmView
//
LONG AsmView::SetFont(HFONT hFont, int idx)
{
	FONT *font = &m_FontAttr[idx];

	// need a DC to query font data
	HDC    hdc  = GetDC(0);
	HANDLE hold = SelectObject(hdc, hFont);

	// get font settings
	font->hFont = hFont;
	GetTextMetrics(hdc, &font->tm);

	// pre-calc the control-characters for this font
	InitCtrlCharFontAttr(hdc, font);
	
	// cleanup
	SelectObject(hdc, hold);
	ReleaseDC(0, hdc);

	RecalcLineHeight();
	UpdateMarginWidth();

	return 0;
}

//
//	Add a secondary font to the AsmView
//
LONG AsmView::AddFont(HFONT hFont)
{
	int idx = m_nNumFonts++;

	SetFont(hFont, idx);
	UpdateMetrics();

	return 0;
}

//
//	WM_SETFONT handler: set a new default font
//
LONG AsmView::OnSetFont(HFONT hFont)
{
	// default font is always #0
	SetFont(hFont, 0);

	m_nFontWidth = m_FontAttr[0].tm.tmAveCharWidth;
	UpdateMetrics();

	return 0;
}

//
//	Set spacing (in pixels) above and below each line - 
//  this is in addition to the external-leading of a font
//
LONG AsmView::SetLineSpacing(int nAbove, int nBelow)
{
	m_nHeightAbove = nAbove;
	m_nHeightBelow = nBelow;
	RecalcLineHeight();
	return TRUE;
}

//
//	
//
int AsmView::TabWidth()
{
	return m_nTabWidthChars * m_nFontWidth;
}