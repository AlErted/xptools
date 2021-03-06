/*
 * Copyright (c) 2007, Laminar Research.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#ifndef WED_PACKAGE_H
#define WED_PACKAGE_H

#include "GUI_Broadcaster.h"
#include "GUI_Listener.h"

class	WED_Document;

enum {

	status_None,
	status_XES,
	status_DSF,
	status_Stale,
	status_UpToDate

};


class	WED_Package : public GUI_Broadcaster, public GUI_Listener{
public:

	WED_Package(const char * inPath, bool inCreate);
	~WED_Package();

	int				GetTileStatus(int lon, int lat);
	WED_Document *	GetTileDocument(int lon, int lat);

	WED_Document *	OpenTile(int lon, int lat);
	WED_Document *	NewTile(int lon, int lat);

	void			Rescan(void);

	virtual	void	ReceiveMessage(
							GUI_Broadcaster *		inSrc,
							int						inMsg,
							int						inParam);

			bool	TryClose(void);
	static	bool	TryCloseAll(void);

private:


				 WED_Package(const WED_Package&);
	WED_Package& operator=  (const WED_Package&);

	string			mPackageBase;
	int				mStatus[360*180];
	WED_Document *	mTiles[360*180];

};

#endif
