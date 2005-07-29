/* 
 * Copyright (c) 2004, Laminar Research.
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
#ifndef OE_TABLEPANE_H
#define OE_TABLEPANE_H

#include "OE_Pane.h"

class	OE_TablePane : public OE_Pane {
public:

					OE_TablePane(
                                   int                  inLeft,    
                                   int                  inTop,    
                                   int                  inRight,    
                                   int                  inBottom,    
                                   int                  inVisible,
                                   OE_Pane *			inSuper);
	virtual			~OE_TablePane();
	
	virtual	void	DrawCell(int row, int col, int cellLeft, int cellTop, int cellRight, int cellBottom)=0;
	virtual	void	ClickCell(int row, int col, int cellLeft, int cellTop, int cellRight, int cellBottom)=0;
	
	virtual	int		GetColCount(void)=0;
	virtual	int		GetRowCount(void)=0;
	virtual	int		GetColRight(int col)=0;	// Relative to 0!!
	virtual	int		GetRowBottom(int col)=0;	// Relative to 0, + = down!!
	
			void	GetCellDimensions(
								int			inRow,
								int			inColumn,
								int *		outLeft,
								int *		outTop,
								int *		outRight,
								int *		outBottom);
	
			void	SnapToCols(void);	
	
	virtual	void	DrawSelf(void);
	virtual	int		HandleClick(XPLMMouseStatus status, int x, int y, int button);

};

#endif