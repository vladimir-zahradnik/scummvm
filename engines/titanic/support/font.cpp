/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include "common/textconsole.h"
#include "titanic/support/font.h"
#include "titanic/support/files_manager.h"
#include "titanic/titanic.h"

namespace Titanic {

STFont::STFont() {
	_dataPtr = nullptr;
	_dataSize = 0;
	_fontHeight = 0;
	_dataWidth = 0;
	_fontR = _fontG = _fontB = 0;
}

STFont::~STFont() {
	delete[] _dataPtr;
}

void STFont::load(int fontNumber) {
	assert(!_dataPtr);
	Common::SeekableReadStream *stream = g_vm->_filesManager->getResource(
		CString::format("STFONT/%d", fontNumber));
	if (!stream)
		error("Could not locate the specified font");

	_fontHeight = stream->readUint32LE();
	_dataWidth = stream->readUint32LE();
	for (uint idx = 0; idx < 256; ++idx)
		_chars[idx]._width = stream->readUint32LE();
	for (uint idx = 0; idx < 256; ++idx)
		_chars[idx]._offset = stream->readUint32LE();

	_dataSize = stream->readUint32LE();
	_dataPtr = new byte[_dataSize];
	stream->read(_dataPtr, _dataSize);

	delete stream;
}

void STFont::setColor(byte r, byte g, byte b) {
	_fontR = r;
	_fontG = g;
	_fontB = b;
}

uint16 STFont::getColor() const {
	return g_system->getScreenFormat().RGBToColor(_fontR, _fontG, _fontB);
}

int STFont::getTextBounds(const CString &str, int maxWidth, Point *sizeOut) const {
	Point textSize;

	// Reset output dimensions if provided
	if (sizeOut)
		*sizeOut = Point(0, 0);

	if (_fontHeight == 0 || !_dataPtr)
		// No font, so return immediately
		return 0;
	
	// Loop through the characters of the string
	if (!str.empty()) {
		for (const char *strP = str.c_str(); *strP; ++strP) {
			if (*strP == TEXTCMD_NPC) {
				strP += 3;
			} else if (*strP == TEXTCMD_SET_COLOR) {
				strP += 4;
			} else {
				if (*strP == ' ') {
					// Check fo rline wrapping
					checkLineWrap(textSize, maxWidth, strP);
				}

				extendBounds(textSize, *strP, maxWidth);
			}
		}
	}

	if (sizeOut)
		*sizeOut = textSize;

	return textSize.y + _fontHeight;
}

int STFont::stringWidth(const CString &text) const {
	if (text.empty())
		return 0;

	const char *srcP = text.c_str();
	int total = 0;
	char c;
	while ((c = *srcP++)) {
		if (c == 26) {
			// Skip over command parameter bytes
			srcP += 3;
		} else if (c == TEXTCMD_SET_COLOR) {
			// Skip over command parameter bytes
			srcP += 4;
		} else if (c != '\n') {
			total += _chars[(byte)c]._width;
		}
	}

	return total;
}

int STFont::writeString(CVideoSurface *surface, const Rect &rect1, const Rect &destRect,
		int yOffset, const CString &str, CTextCursor *textCursor) {
	if (!_fontHeight || !_dataPtr)
		return -1;

	Point textSize(0, -yOffset);
	Rect destBounds = destRect;
	destBounds.constrain(rect1);
	if (destBounds.isEmpty())
		return -1;

	const char *endP = nullptr;
	const char *strEndP = str.c_str() + str.size() - 1;
	for (const char *srcP = str.c_str(); *srcP; ++srcP) {
		if (*srcP == TEXTCMD_NPC) {
			srcP += 3;
		} else if (*srcP == TEXTCMD_SET_COLOR) {
			// Change the color used for characters
			byte r = *++srcP;
			byte g = *++srcP;
			byte b = *++srcP;
			++srcP;
			setColor(r, g, b);
		} else {
			if (*srcP == ' ') {
				// Check fo rline wrapping
				checkLineWrap(textSize, rect1.width(), srcP);
				if (!*srcP)
					return endP - str.c_str();
			}

			if (*srcP != '\n') {
				WriteCharacterResult result = writeChar(surface, *srcP, textSize, rect1, &destBounds);
				if (result == WC_OUTSIDE_BOTTOM)
					return endP - str.c_str();
				else if (result == WC_IN_BOUNDS)
					endP = srcP;
			}

			if (srcP < strEndP)
				extendBounds(textSize, *srcP, rect1.width());
		}
	}

	if (textCursor && textCursor->getMode() == -2) {
		Point cursorPos(rect1.left + textSize.x, rect1.top + textSize.y);
		textCursor->setPos(cursorPos);
	}

	return endP ? endP - str.c_str() : 0;
}

WriteCharacterResult STFont::writeChar(CVideoSurface *surface, unsigned char c, const Point &pt,
	const Rect &destRect, const Rect *srcRect) {
	if (c == 233)
		c = '$';

	Rect tempRect;
	tempRect.left = _chars[c]._offset;
	tempRect.right = _chars[c]._offset + _chars[c]._width;
	tempRect.top = 0;
	tempRect.bottom = _fontHeight;
	Point destPos(pt.x + destRect.left, pt.y + destRect.top);

	if (srcRect->isEmpty())
		srcRect = &destRect;
	if (destPos.y > srcRect->bottom)
		return WC_OUTSIDE_BOTTOM;

	if ((destPos.y + tempRect.height()) > srcRect->bottom) {
		tempRect.bottom += tempRect.top - destPos.y;
	}

	if (destPos.y < srcRect->top) {
		if ((tempRect.height() + destPos.y) < srcRect->top)
			return WC_OUTSIDE_TOP;

		tempRect.top += srcRect->top - destPos.y;
		destPos.y = srcRect->top;
	}

	if (destPos.x < srcRect->left) {
		if ((tempRect.width() + destPos.x) < srcRect->left)
			return WC_OUTSIDE_LEFT;

		tempRect.left += srcRect->left - destPos.x;
		destPos.x = srcRect->left;
	} else {
		if ((tempRect.width() + destPos.x) > srcRect->right) {
			if (destPos.x > srcRect->right)
				return WC_OUTSIDE_RIGHT;

			tempRect.right += srcRect->left - destPos.x;
		}
	}

	copyRect(surface, destPos, tempRect);
	return WC_IN_BOUNDS;
}

void STFont::copyRect(CVideoSurface *surface, const Point &pt, Rect &rect) {
	if (surface->lock()) {
		uint16 *lineP = surface->getBasePtr(pt.x, pt.y);
		uint16 color = getColor();

		for (int yp = rect.top; yp < rect.bottom; ++yp, lineP += surface->getWidth()) {
			uint16 *destP = lineP;
			for (int xp = rect.left; xp < rect.right; ++xp, ++destP) {
				const byte *srcP = _dataPtr + yp * _dataWidth + xp;
				surface->changePixel(destP, &color, *srcP >> 3, true);
			}
		}

		surface->unlock();
	}
}

void STFont::extendBounds(Point &textSize, byte c, int maxWidth) const {
	textSize.x += _chars[c]._width;

	if (c == '\n' || textSize.x > maxWidth) {
		textSize.x = 0;
		textSize.y += _fontHeight;
	}
}

void STFont::checkLineWrap(Point &textSize, int maxWidth, const char *&str) const {
	bool flag = false;
	int totalWidth = 0;
	for (const char *srcPtr = str; *srcPtr && *srcPtr != ' '; ++srcPtr) {
		if (*srcPtr == ' ' && flag)
			break;

		if (*srcPtr == TEXTCMD_NPC) {
			srcPtr += 3;
		} else if (*srcPtr == TEXTCMD_SET_COLOR) {
			srcPtr += 4;
		} else {
			totalWidth += _chars[(byte)*srcPtr]._width;
			flag = true;
		}
	}
	
	if ((textSize.x + totalWidth) >= maxWidth && totalWidth < maxWidth) {
		// Word wrap
		textSize.x = 0;
		textSize.y += _fontHeight;
		++str;
	}
}

} // End of namespace Titanic
