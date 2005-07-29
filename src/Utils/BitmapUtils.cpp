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
#include "BitmapUtils.h"
#include "PlatformUtils.h"
#include <stdio.h>
#include <errno.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include "Interpolation.h"

/*
	NOTES ON ENDIAN CHAOS!!!!!!!!!!!!!!!!!!!
	
	Simply viewing the way pixels are stored in memory from low to high mem:
	BMP contains RGBRGBRGB triplets.
		(This is a direct byte-order read out of memory, so it can't get f-cked.
	PNG lib returns RGBARGBARGBA quads, but only because we set it in BGR mode.  (Don't ask me.)
		(As far as I can tell, pnglib is just goofy.)

	TiffLib on Mach gives it to us in ARGB format...and on windows in BGRA format.
		(Tifflib is endian-aware and always uses ARGB.  But at least it's consistent!)
	
	OpenGL strangely wants BGRA in memory when we say RGBA, which makes no sense to me.
	

	TODO...
		It would be nice to generic-ize these APIs so that we could:
		- refer to file formats by enum.
		- read and write any format (BMP, PNG, TIFF, JPG)
		- consider more formats (targa?)
		- handle indexed color
		- handle 16-bit color
 */



#if IBM
#define XMD_H
#define HAVE_BOOLEAN
#endif

// Note: the std jpeg lib does not have any #ifdef C++ name
// mangling protection.  Since wek now we're going to be CPP, 
// add it ourself.  Gross, but perhaps better than hacking up
// libjpeg??

#if !defined(__MACH__)
extern "C" {
#include <jpeglib.h>
#include <jerror.h>
}

#if APL
#include <Carbon.h>
#endif

#include <png.h>

#include <tiffio.h>
#endif

int		CreateBitmapFromFile(const char * inFilePath, struct ImageInfo * outImageInfo)
{
		struct	BMPHeader		header;
		struct	BMPImageDesc	imageDesc;
		long					pad;
		int 					err = 0;
		FILE *					fi = NULL;
		
	outImageInfo->data = NULL;

	fi = fopen(inFilePath, "rb");
	if (fi == NULL)
		goto bail;

	/*  First we read in the headers, endian flip them, sanity check them, and decide how big our 
		image is. */
		
	if (fread(&header, sizeof(header), 1, fi) != 1)
		goto bail;
	if (fread(&imageDesc, sizeof(imageDesc), 1, fi) != 1)
		goto bail;
	
	EndianFlipLong(&header.fileSize);
	EndianFlipLong(&header.dataOffset);
	
	EndianFlipLong(&imageDesc.imageWidth);
	EndianFlipLong(&imageDesc.imageHeight);
	EndianFlipShort(&imageDesc.bitCount);
	
	if ((header.signature1 != 'B') || 
		(header.signature2 != 'M') || 
		(imageDesc.bitCount != 24) ||
		(imageDesc.imageWidth <= 0) ||
		(imageDesc.imageHeight <= 0))
		goto bail;
	
	if ((header.fileSize - header.dataOffset) < (imageDesc.imageWidth * imageDesc.imageHeight * 3))
		goto bail;
	
	pad = (imageDesc.imageWidth * 3 + 3) & ~3;
	pad -= imageDesc.imageWidth * 3;
	
	outImageInfo->width = imageDesc.imageWidth;
	outImageInfo->height = imageDesc.imageHeight;
	outImageInfo->pad = pad;

	/* Now we can allocate an image buffer. */
	
	outImageInfo->channels = 3;
	outImageInfo->data = (unsigned char *) malloc(imageDesc.imageWidth * imageDesc.imageHeight * outImageInfo->channels + imageDesc.imageHeight * pad);
	if (outImageInfo->data == NULL)
		goto bail;
		
	/*  We can pretty much just read the bytes in; we know that we're 24 bit so there is no
		color table, and 24 bit BMP files cannot be compressed. */
		
	if (fread(outImageInfo->data, imageDesc.imageWidth * imageDesc.imageHeight * outImageInfo->channels + imageDesc.imageHeight * pad, 1, fi) != 1)
		goto bail;
	
	fclose(fi);	
	return 0;
	
bail:
	err = errno;
	if (fi != NULL)
		fclose(fi);
	if (outImageInfo->data != NULL)
		free(outImageInfo->data);
	if (err == 0)
		err = -1;
	return err;
}

int		WriteBitmapToFile(const struct ImageInfo * inImage, const char * inFilePath)
{	
		FILE *					fi = NULL;
		struct	BMPHeader		header;
		struct	BMPImageDesc	imageDesc;
		int						err = 0;
		
	/* First set up the appropriate header structures to match our bitmap. */	
		
	header.signature1 = 'B';
	header.signature2 = 'M';
	header.fileSize = sizeof(struct BMPHeader) + sizeof(struct BMPImageDesc) + ((inImage->width * 3 + inImage->pad) * inImage->height);
	header.reserved = 0;
	header.dataOffset = sizeof(struct BMPHeader) + sizeof(struct BMPImageDesc);
	EndianFlipLong(&header.fileSize);
	EndianFlipLong(&header.reserved);
	EndianFlipLong(&header.dataOffset);

	imageDesc.structSize = sizeof(imageDesc);
	imageDesc.imageWidth = inImage->width;
	imageDesc.imageHeight = inImage->height;
	imageDesc.planes = 1;
	imageDesc.bitCount = 24;
	imageDesc.compressionType = 0;
	imageDesc.imageSize = (inImage->width * inImage->height * 3);
	imageDesc.xPixelsPerM = 0;
	imageDesc.yPixelsPerM = 0;
	imageDesc.colorsUsed = 0;
	imageDesc.colorsImportant = 0;

	EndianFlipLong(&imageDesc.structSize);
	EndianFlipLong(&imageDesc.imageWidth);
	EndianFlipLong(&imageDesc.imageHeight);
	EndianFlipShort(&imageDesc.planes);
	EndianFlipShort(&imageDesc.bitCount);
	EndianFlipLong(&imageDesc.compressionType);
	EndianFlipLong(&imageDesc.imageSize);
	EndianFlipLong(&imageDesc.xPixelsPerM);
	EndianFlipLong(&imageDesc.yPixelsPerM);
	EndianFlipLong(&imageDesc.colorsUsed);
	EndianFlipLong(&imageDesc.colorsImportant);
		
	fi = fopen(inFilePath, "wb");
	if (fi == NULL)
		goto bail;
	
	/* We can just write out two headers and the data and be done. */
	
	if (fwrite(&header, sizeof(header), 1, fi) != 1)
		goto bail;
	if (fwrite(&imageDesc, sizeof(imageDesc), 1, fi) != 1)
		goto bail;
	if (fwrite(inImage->data, (inImage->width * 3 + inImage->pad) * inImage->height, 1, fi) != 1)
		goto bail;
	
	fclose(fi);
	return 0;

bail:
	err = errno;
	if (fi != NULL)
		fclose(fi);
	if (err == 0)
		err = -1;
	return err;
}

int		CreateNewBitmap(long inWidth, long inHeight, short inChannels, struct ImageInfo * outImageInfo)
{
	outImageInfo->width = inWidth;
	outImageInfo->height = inHeight;
	/* This nasty voodoo calculates the padding necessary to make each scanline a multiple of four bytes. */
	outImageInfo->pad = ((inWidth * inChannels + 3) & ~3) - (inWidth * inChannels);
	outImageInfo->channels = inChannels;
	outImageInfo->data = (unsigned char *) malloc(inHeight * ((inWidth * inChannels) + outImageInfo->pad));
	if (outImageInfo->data == NULL)
		return ENOMEM;
	return 0;
}

void	FillBitmap(const struct ImageInfo * inImageInfo, char c)
{
	memset(inImageInfo->data, c, inImageInfo->width * inImageInfo->height * inImageInfo->channels);
}

void	DestroyBitmap(const struct ImageInfo * inImageInfo)
{
	free(inImageInfo->data);
}


void	CopyBitmapSection(
			const struct ImageInfo *	inSrc,
			const struct ImageInfo	*	inDst,
			long				inSrcLeft,
			long				inSrcTop,
			long				inSrcRight,
			long				inSrcBottom,
			long				inDstLeft,
			long				inDstTop,
			long				inDstRight,
			long				inDstBottom)
{
	/*  This routine copies a subsection of one bitmap onto a subsection of another, using bicubic interpolation
		for scaling. */

	double	srcLeft = inSrcLeft, srcRight = inSrcRight, srcTop = inSrcTop, srcBottom = inSrcBottom; 
	double	dstLeft = inDstLeft, dstRight = inDstRight, dstTop = inDstTop, dstBottom = inDstBottom;
	
	/*	Here's why we subtract one from all of these...
		(Ignore bicubic interpolation for a moment please...)  
		Every destination pixel is comprised of two pixels horizontally and two vertically.  We use these widths and heights to do the rescaling
		of the image.  The goal is to have the rightmost pixel in the destination actually correspond to the rightmost pixel of the source.  In
		otherwords, we want to get (inDstRight - 1) from (inSrcRight - 1).  Now since we use [inDstLeft - inDstRight) as our set of pixels, this
		is the last pixel we ask for.  But we have to subtract one from the width to get the rescaling right, otherwise we map inDstRight to 
		inSrcRight, which for very large upscales make inDstRight - 1 derive from inSrcRight - (something less than one) which is a pixel partially
		off the right side of the bitmap, which is bad.
		Bicubic interpolation is not used when we're this close to the edge of the border, so it is not a factor.
	*/
	
	double	dstWidth = dstRight - dstLeft - 1.0;
	double	dstHeight = dstBottom - dstTop - 1.0;
	double	srcWidth = srcRight - srcLeft - 1.0;
	double	srcHeight = srcBottom - srcTop - 1.0;

	double	dx, dy;	
	
	long	srcRowBytes = inSrc->width * inSrc->channels + inSrc->pad;
	long	srcRowBytes2 = srcRowBytes * 2;
	long	dstRowBytes = inDst->width * inSrc->channels + inDst->pad;
	unsigned char *	srcBaseAddr = inSrc->data;
	unsigned char *	dstBaseAddr = inDst->data;
	
	int		channels;
	
	for (dy = dstTop; dy < dstBottom; dy += 1.0)
	{
		for (dx = dstLeft; dx < dstRight; dx += 1.0)
		{
			/*  For each pixel in the destination, find a pixel in the source.  Note that it may have a fractional part
				if we are scaling. */
				
			double	sx = ((dx - dstLeft) / dstWidth * srcWidth) + srcLeft;
			double	sy = ((dy - dstTop) / dstHeight * srcHeight) + srcTop;
		
			unsigned char *	dstPixel = dstBaseAddr + ((long) dx * inDst->channels) + ((long) dy * dstRowBytes);			
			unsigned char *	srcPixel = srcBaseAddr + ((long) sx * inSrc->channels) + ((long) sy * srcRowBytes);

			/* 	If we would need pixels from off the edge of the image for bicubic interpolation,
			 	just use bilinear. */
			 	
			if ((sx < 1) ||
				(sy < 1) ||
				(sx >= (inSrc->width - 2)) ||
				(sy >= (inSrc->height - 2)))
			{
				channels = inSrc->channels;
				while (channels--)
				{
					// While this interpolation works right, when it reads the last row, its mix ratio is 100% the top/left
					// BUT it reads the bot/right anyway.  This causes access violations on Windows.
					// (Apparently it's a "real" operating system...)

					// so we specifically check this case.

					double	mixH = sx - floor(sx);
					double	mixV = sy - floor(sy);

					unsigned char tl = *srcPixel;
					unsigned char tr = (mixH> 0.0) ? *(srcPixel+inSrc->channels) : 0;
					unsigned char bl = (mixV> 0.0) ? *(srcPixel+srcRowBytes) : 0;
					unsigned char br = ((mixH> 0.0) && (mixV > 0.0)) ? 
						*(srcPixel+srcRowBytes + inSrc->channels) : 0;

					/*  Take the pixel (rounded down to integer coords), the one to the right, below, and below to the right.  
						The fractional part of the pixel is our weighting for interpolation. */
					unsigned char pixel = (unsigned char) BilinearInterpolate2d(
								tl, tr, bl, br, mixH, mixV);			
					*dstPixel = pixel;
					++srcPixel;
					++dstPixel;
				}
			
			} else {
				channels = inSrc->channels;
				while (channels--)
				{
					/* Same as above, except we now take 16 pixels surrounding the location we want. */
					*dstPixel = (unsigned char) BicubicInterpolate2d(
									*(srcPixel-inSrc->channels-srcRowBytes),*(srcPixel-srcRowBytes),*(srcPixel+inSrc->channels-srcRowBytes),*(srcPixel+inSrc->channels*2-srcRowBytes),
									*(srcPixel-inSrc->channels),*srcPixel,*(srcPixel+inSrc->channels),*(srcPixel+inSrc->channels*2),
									*(srcPixel-inSrc->channels+srcRowBytes),*(srcPixel+srcRowBytes),*(srcPixel+inSrc->channels+srcRowBytes),*(srcPixel+inSrc->channels*2+srcRowBytes),
									*(srcPixel-inSrc->channels+srcRowBytes2),*(srcPixel+srcRowBytes2),*(srcPixel+inSrc->channels+srcRowBytes2),*(srcPixel+inSrc->channels*2+srcRowBytes2),
									sx - floor(sx), sy - floor(sy));
					++srcPixel;
					++dstPixel;
				}
			}

		}
	}	
}		

inline double	Interp2(double frac, double sml, double big)
{
	return sml + frac * (big - sml);
}

void	CopyBitmapSectionWarped(
			const struct ImageInfo *	inSrc,
			const struct ImageInfo *	inDst,
			long				inTopLeftX,
			long				inTopLeftY,
			long				inTopRightX,
			long				inTopRightY,
			long				inBotRightX,
			long				inBotRightY,
			long				inBotLeftX,
			long				inBotLeftY,
			long				inDstLeft,
			long				inDstTop,
			long				inDstRight,
			long				inDstBottom)
{
	/*  This routine copies a subsection of one bitmap onto a subsection of another, using bicubic interpolation
		for scaling. */

	double	dstLeft = inDstLeft, dstRight = inDstRight, dstTop = inDstTop, dstBottom = inDstBottom; 
	double	topLeftX = inTopLeftX, topLeftY = inTopLeftY, topRightX = inTopRightX, topRightY = inTopRightY;
	double	botLeftX = inBotLeftX, botLeftY = inBotLeftY, botRightX = inBotRightX, botRightY = inBotRightY;
	
	/*	Here's why we subtract one from all of these...
		(Ignore bicubic interpolation for a moment please...)  
		Every destination pixel is comprised of two pixels horizontally and two vertically.  We use these widths and heights to do the rescaling
		of the image.  The goal is to have the rightmost pixel in the destination actually correspond to the rightmost pixel of the source.  In
		otherwords, we want to get (inDstRight - 1) from (inSrcRight - 1).  Now since we use [inDstLeft - inDstRight) as our set of pixels, this
		is the last pixel we ask for.  But we have to subtract one from the width to get the rescaling right, otherwise we map inDstRight to 
		inSrcRight, which for very large upscales make inDstRight - 1 derive from inSrcRight - (something less than one) which is a pixel partially
		off the right side of the bitmap, which is bad.
		Bicubic interpolation is not used when we're this close to the edge of the border, so it is not a factor.
	*/
	
	double	dstWidth = dstRight - dstLeft - 1.0;
	double	dstHeight = dstBottom - dstTop - 1.0;

	double	dx, dy;	
	
	long	srcRowBytes = inSrc->width * inSrc->channels + inSrc->pad;
	long	srcRowBytes2 = srcRowBytes * 2;
	long	dstRowBytes = inDst->width * inSrc->channels + inDst->pad;
	unsigned char *	srcBaseAddr = inSrc->data;
	unsigned char *	dstBaseAddr = inDst->data;
	
	int		channels;
	
	for (dy = dstTop; dy < dstBottom; dy += 1.0)
	{
		for (dx = dstLeft; dx < dstRight; dx += 1.0)
		{
			/*  For each pixel in the destination, find a pixel in the source.  Note that it may have a fractional part
				if we are scaling. */

			double frac_x = ((dx - dstLeft) / dstWidth);
			double frac_y = ((dy - dstTop) / dstHeight);
			
			double	sx = Interp2(frac_y, Interp2(frac_x, topLeftX, topRightX), Interp2(frac_x, botLeftX, botRightX));
			double	sy = Interp2(frac_x, Interp2(frac_y, topLeftY, botLeftY), Interp2(frac_y, topRightY, botRightY));

			unsigned char *	dstPixel = dstBaseAddr + ((long) dx * inDst->channels) + ((long) dy * dstRowBytes);			
			unsigned char *	srcPixel = srcBaseAddr + ((long) sx * inSrc->channels) + ((long) sy * srcRowBytes);

			/* 	If we would need pixels from off the edge of the image for bicubic interpolation,
			 	just use bilinear. */
			 	
			if ((sx < 1) ||
				(sy < 1) ||
				(sx >= (inSrc->width - 2)) ||
				(sy >= (inSrc->height - 2)))
			{
				channels = inSrc->channels;
				while (channels--)
				{
					// While this interpolation works right, when it reads the last row, its mix ratio is 100% the top/left
					// BUT it reads the bot/right anyway.  This causes access violations on Windows.
					// (Apparently it's a "real" operating system...)

					// so we specifically check this case.

					double	mixH = sx - floor(sx);
					double	mixV = sy - floor(sy);

					unsigned char tl = *srcPixel;
					unsigned char tr = (mixH> 0.0) ? *(srcPixel+inSrc->channels) : 0;
					unsigned char bl = (mixV> 0.0) ? *(srcPixel+srcRowBytes) : 0;
					unsigned char br = ((mixH> 0.0) && (mixV > 0.0)) ? 
						*(srcPixel+srcRowBytes + inSrc->channels) : 0;

					/*  Take the pixel (rounded down to integer coords), the one to the right, below, and below to the right.  
						The fractional part of the pixel is our weighting for interpolation. */
					unsigned char pixel = (unsigned char) BilinearInterpolate2d(
								tl, tr, bl, br, mixH, mixV);			
					*dstPixel = pixel;
					++srcPixel;
					++dstPixel;
				}
			
			} else {
				channels = inSrc->channels;
				while (channels--)
				{
					/* Same as above, except we now take 16 pixels surrounding the location we want. */
					*dstPixel = (unsigned char) BicubicInterpolate2d(
									*(srcPixel-inSrc->channels-srcRowBytes),*(srcPixel-srcRowBytes),*(srcPixel+inSrc->channels-srcRowBytes),*(srcPixel+inSrc->channels*2-srcRowBytes),
									*(srcPixel-inSrc->channels),*srcPixel,*(srcPixel+inSrc->channels),*(srcPixel+inSrc->channels*2),
									*(srcPixel-inSrc->channels+srcRowBytes),*(srcPixel+srcRowBytes),*(srcPixel+inSrc->channels+srcRowBytes),*(srcPixel+inSrc->channels*2+srcRowBytes),
									*(srcPixel-inSrc->channels+srcRowBytes2),*(srcPixel+srcRowBytes2),*(srcPixel+inSrc->channels+srcRowBytes2),*(srcPixel+inSrc->channels*2+srcRowBytes2),
									sx - floor(sx), sy - floor(sy));
					++srcPixel;
					++dstPixel;
				}
			}

		}
	}	
}	

void	CopyBitmapSectionDirect(
			const struct ImageInfo&		inSrc,
			const struct ImageInfo&		inDst,
			long						inSrcLeft,
			long						inSrcTop,
			long						inDstLeft,
			long						inDstTop,
			long						inWidth,
			long						inHeight)
{
	if (inSrc.channels != inDst.channels) return;
	
	long	src_rb = inSrc.width * inSrc.channels + inSrc.pad;
	long	dst_rb = inDst.width * inDst.channels + inDst.pad;
	long	src_nr = src_rb - inWidth* inSrc.channels;
	long	dst_nr = dst_rb - inWidth* inDst.channels;
	unsigned char *	src_p = inSrc.data + inSrcTop * src_rb + inSrcLeft * inSrc.channels;
	unsigned char *	dst_p = inDst.data + inDstTop * dst_rb + inDstLeft * inDst.channels;
	
	while (inHeight--)
	{
		long ctr = inWidth * inSrc.channels;
		while (ctr--)
		{
			*dst_p++ = *src_p++;
		}
		src_p += src_nr;
		dst_p += dst_nr;
	}
	
}


void	RotateBitmapCCW(
			struct ImageInfo *	ioBitmap)
{
	/* We have to allocate a new bitmap to transfer our old data to.  The new bitmap might not have the same
	 * storage size as the old bitmap because of padding! */
	 
	long	newWidth = ioBitmap->height;
	long	newHeight = ioBitmap->width;
	long	newPad = ((newWidth * ioBitmap->channels + 3) & ~3) - (newWidth * ioBitmap->channels);
	unsigned char * newData = (unsigned char *) malloc(((newWidth * ioBitmap->channels) + newPad) * newHeight);
	if (newData == NULL)
		return;

	for (long y = 0; y < ioBitmap->height; ++y)
	for (long x = 0; x < ioBitmap->width; ++x)
	{
		long	nx = ioBitmap->height - y - 1;
		long	ny = x;
		
		unsigned char *	srcP = ioBitmap->data + (x * ioBitmap->channels) + (y * (ioBitmap->channels * ioBitmap->width + ioBitmap->pad));
		unsigned char *	dstP = newData + (nx * ioBitmap->channels) + (ny * (ioBitmap->channels * newWidth + newPad));
		long	chCount = ioBitmap->channels;
		while (chCount--)
		{
			*dstP++ = *srcP++;
		}
	}

	free(ioBitmap->data);
	ioBitmap->data = newData;
	ioBitmap->width = newWidth;
	ioBitmap->height = newHeight;
	ioBitmap->pad = newPad;

}			
			
int	ConvertBitmapToAlpha(
			struct ImageInfo *		ioImage)
{
		unsigned char * 	oldData, * newData, * srcPixel, * dstPixel;
		long 	count;
		long	x,y;
		
	if (ioImage->channels == 4)
		return 0;
		
	/* We have to allocate a new bitmap that is larger than the old to store the alpha channel. */	
		
	newData = (unsigned char *) malloc(ioImage->width * ioImage->height * 4);
	if (newData == NULL)
		return ENOMEM;
	oldData = ioImage->data;
	
	srcPixel = oldData;
	dstPixel = newData;
	count = ioImage->width * ioImage->height;
	for (y = 0; y < ioImage->height; ++y)
	for (x = 0; x < ioImage->width; ++x)
	{
		/* For each pixel, if it is pure magenta, it becomes pure black transparent.  Otherwise it is 
		 * opaque and retains its color.  NOTE: one of the problems with the magenta=alpha strategy is 
		 * that we don't know what color was 'under' the transparency, so if we stretch or skew this bitmap
		 * we can't really do a good job of interpolating. */
		if ((srcPixel[0] == 0xFF) &&
			(srcPixel[1] == 0x00) &&
			(srcPixel[2] == 0xFF))
		{
			dstPixel[0] = 0;
			dstPixel[1] = 0;
			dstPixel[2] = 0;
			dstPixel[3] = 0;
		} else {
			dstPixel[0] = srcPixel[0];
			dstPixel[1] = srcPixel[1];
			dstPixel[2] = srcPixel[2];
			dstPixel[3] = 0xFF;	
		}
		
		srcPixel += 3;
		dstPixel += 4;
		
		if (x == (ioImage->width - 1))
			srcPixel += ioImage->pad;
	}
	
	ioImage->data = newData;
	ioImage->pad = 0;
	free(oldData);
	ioImage->channels = 4;
	return 0;
}			


int	ConvertAlphaToBitmap(
			struct ImageInfo *		ioImage)
{
		unsigned char * 	oldData, * newData, * srcPixel, * dstPixel;
		long 	count;
		long 	x,y;
		
	if (ioImage->channels == 3)
		return 0;

	/* Allocate a new bitmap at 3 channels.
	 * WARNING WARNING WARNING WARNING WARNING:
	 * THIS CODE IS BUGGY!!!!!!!!!!!!!!!!!!!!!
	 *
	 * There is no padding being allocated to our new buffer, but we do calculate
	 * a pad below.  If we are working with a non-multiple-of-four width image,
	 * we will scribble over memory and cause chaos.
	 */
	newData = (unsigned char *) malloc(ioImage->width * ioImage->height * 3);
	if (newData == NULL)
		return ENOMEM;	
	oldData = ioImage->data;
	
	ioImage->pad = ((ioImage->width * 3 + 3) & ~3) - (ioImage->width * 3);
	
	srcPixel = oldData;
	dstPixel = newData;
	count = ioImage->width * ioImage->height;
	
	for (y = 0; y < ioImage->height; ++y)
	for (x = 0; x < ioImage->width; ++x)
	{
		/* For each pixel, only full opaque is taken.  Here's why: if the pixel is part alpha, then it is a blend of an
		 * alpha pixel and a non-alpha pixel.  But...we don't have good color data for the alpha pixel; from the above
		 * routine we set the color to black.  So the color data for this pixel is mixed with black.  When viewed the 
		 * edges of a stretched bitmap will appear to turn dark before they fade out.
		 *
		 * But this point is moot anyway; we really only have one bit of alpha, on or off.  So we could pick any cutoff
		 * value and we'll still get a really sharp jagged line at the edge of the transparency.
		 *
		 * You might ask yourself, why does X-Plane do it this way?  The answer is that as of this writing, most graphics
		 * cards do not have the alpha-blending fill rate to blend the entire aircraft panel; this would be a huge hit on
		 * frame rate.  So Austin is using the alpha test mechanism for transparency, which is much faster but only one 
		 * bit deep.
		 *
		 */
		if (srcPixel[3] != 0xFF)
		{
			dstPixel[0] = 0xFF;
			dstPixel[1] = 0x00;
			dstPixel[2] = 0xFF;
		} else {
			dstPixel[0] = srcPixel[0];
			dstPixel[1] = srcPixel[1];
			dstPixel[2] = srcPixel[2];
		}
		
		srcPixel += 4;
		dstPixel += 3;

		if (x == (ioImage->width - 1))
			dstPixel += ioImage->pad;
	}
	
	ioImage->data = newData;
	free(oldData);
	ioImage->channels = 3;
	return 0;
}			

#pragma mark -

#if !defined(__MACH__)

/*
 * JPEG in-memory source manager
 *
 * Given a JFIF file that is entirley in memory, this object uses that buffer to provide data
 * to the JPEG lib.
 *
 */

typedef struct {
	struct jpeg_source_mgr pub;		// Fields inherited from all jpeg source mgs.

	JOCTET * 	buffer;				// Buffer start and size
	int			len;				//
} mem_source_mgr;
typedef struct mem_source_mgr *  mem_src_ptr;

METHODDEF(void) mem_init_source (j_decompress_ptr cinfo)
{
}

METHODDEF(boolean) mem_fill_input_buffer (j_decompress_ptr cinfo)
{
	// If we are asked to fill the buffer, we can't; we give JPEG
	// all of the memory up front.  So throw a fatal err.
    ERREXIT(cinfo, JERR_INPUT_EMPTY);
    return TRUE;
}

METHODDEF(void) mem_skip_input_data (j_decompress_ptr cinfo, long num_bytes)
{
	mem_src_ptr src = (mem_src_ptr) cinfo->src;
	src->pub.next_input_byte += (size_t) num_bytes;
	src->pub.bytes_in_buffer -= (size_t) num_bytes;
}

METHODDEF(void) mem_term_source (j_decompress_ptr cinfo)
{
}

/*
 * JPEG exception-based error handler.  This throws an exception
 * rather than calling exit() on the process.
 *
 */
METHODDEF(void) throw_error_exit (j_common_ptr cinfo)
{
	// On a fatal error, we deallocate the struct first,
	// then throw.  This is a good idea because the cinfo
	// struct may go out of scope during the throw; this 
	// relieves client code from having to worry about 
	// order of destruction.
	jpeg_destroy(cinfo);
	throw EXIT_FAILURE;
}

METHODDEF(void)
eat_output_message (j_common_ptr cinfo)
{
	// If the user needed to see something, this is where
	// we'd find out.  We currently don't have a good way
	// of showing the users messages.
	char buffer[JMSG_LENGTH_MAX]; 
	(*cinfo->err->format_message) (cinfo, buffer);
}

GLOBAL(struct jpeg_error_mgr *)
jpeg_throw_error (struct jpeg_error_mgr * err)
{
	// This routine sets up our various error handlers.
	// We use their decision making logic, and change
	// two of our own handlers.
	jpeg_std_error(err);
	err->error_exit = throw_error_exit;
	err->output_message = eat_output_message;

	return err;
}






int		CreateBitmapFromJPEG(const char * inFilePath, struct ImageInfo * outImageInfo)
{
	// We bail immediately if the file is no good.  This prevents us from 
	// having to keep track of file openings; if we have a problem, but the file must be
	// closed.	
	outImageInfo->data = NULL;
	FILE * fi = fopen(inFilePath, "rb");
	if (!fi) return errno;

	try {

			struct jpeg_decompress_struct cinfo;
			struct jpeg_error_mgr		  jerr;

		cinfo.err = jpeg_throw_error(&jerr);
		jpeg_create_decompress(&cinfo);

		jpeg_stdio_src(&cinfo, fi);

		jpeg_read_header(&cinfo, TRUE);

		jpeg_start_decompress(&cinfo);

		outImageInfo->width = cinfo.output_width;
		outImageInfo->height = cinfo.output_height;
		outImageInfo->pad = 0;
		outImageInfo->channels = 3;
		outImageInfo->data = (unsigned char *) malloc(outImageInfo->width * outImageInfo->height * outImageInfo->channels);

		int linesize = outImageInfo->width * outImageInfo->channels;
		int linecount = outImageInfo->height;
		unsigned char * p = outImageInfo->data + (linecount - 1) * linesize;
		while (linecount--)
		{
			if (jpeg_read_scanlines (&cinfo, &p, 1) == 0)
				break;
			
			if (cinfo.output_components == 1)
			for (int n = cinfo.output_width - 1; n >= 0; --n)
			{
				p[n*3+2] = p[n*3+1] = p[n*3] = p[n];
			}
			p -= linesize;
		}

		jpeg_finish_decompress(&cinfo);

		jpeg_destroy_decompress(&cinfo);
		fclose(fi);
		return 0;
	} catch (...) {
		// If we ever get an exception, it's because we got a fatal JPEG error.  Our
		// error handler deallocates the jpeg struct, so all we have to do is close the 
		// file and bail.
		fclose(fi);
		return 1;
	}
}	



int		CreateBitmapFromJPEGData(void * inBytes, int inLength, struct ImageInfo * outImageInfo)
{
	try {
			struct jpeg_decompress_struct cinfo;
			struct jpeg_error_mgr		  jerr;

		cinfo.err = jpeg_throw_error(&jerr);
		jpeg_create_decompress(&cinfo);

		mem_source_mgr	src;

		cinfo.src = (struct jpeg_source_mgr *) &src;

		src.pub.init_source = mem_init_source;
		src.pub.fill_input_buffer = mem_fill_input_buffer;
		src.pub.skip_input_data = mem_skip_input_data;
		src.pub.resync_to_restart = jpeg_resync_to_restart; /* use default method */
		src.pub.term_source = mem_term_source;
		src.buffer = (JOCTET *) inBytes;
		src.len = inLength;
		src.pub.bytes_in_buffer = inLength; /* forces fill_input_buffer on first read */
		src.pub.next_input_byte = (JOCTET *) inBytes; /* until buffer loaded */

		jpeg_read_header(&cinfo, TRUE);

		jpeg_start_decompress(&cinfo);

		outImageInfo->width = cinfo.output_width;
		outImageInfo->height = cinfo.output_height;
		outImageInfo->pad = 0;
		outImageInfo->channels = 3;
		outImageInfo->data = (unsigned char *) malloc(outImageInfo->width * outImageInfo->height * outImageInfo->channels);

		int linesize = outImageInfo->width * outImageInfo->channels;
		int linecount = outImageInfo->height;
		unsigned char * p = outImageInfo->data + (linecount - 1) * linesize;
		while (linecount--)
		{
			if (jpeg_read_scanlines (&cinfo, &p, 1) == 0)
				break;
			
			for (int n = cinfo.output_width - 1; n >= 0; --n)
			{
				if (cinfo.output_components == 1)
					p[n*3+2] = p[n*3+1] = p[n*3] = p[n];
				else
					swap(p[n*3+2],p[n*3]);
			}
			p -= linesize;
		}

		jpeg_finish_decompress(&cinfo);

		jpeg_destroy_decompress(&cinfo);
		return 0;
	} catch (...) {
		// If we get an exceptoin, cinfo is already cleaned up; just bail.
		return 1; 			
	}
}


void my_error  (png_structp,png_const_charp err){}
void my_warning(png_structp,png_const_charp err){}

unsigned char *			png_start_pos 	= NULL;
unsigned char *			png_end_pos 	= NULL;
unsigned char *			png_current_pos	= NULL;

void png_buffered_read_func(png_structp png_ptr, png_bytep data, png_size_t length)
{
   if((png_current_pos+length)>png_end_pos)
		png_error(png_ptr,"PNG Read Error, overran end of buffer!");
   memcpy(data,png_current_pos,length);
   png_current_pos+=length;
}


int		CreateBitmapFromPNG(const char * inFilePath, struct ImageInfo * outImageInfo, bool leaveIndexed)
{
	png_uint_32	width, height;
	int bit_depth,color_type,interlace_type,compression_type,P_filter_type;

	png_structp		pngPtr = NULL;
	png_infop		infoPtr = NULL;
	unsigned char *	buffer = NULL;
	FILE *			file = NULL;
	int				fileLength = 0;
	outImageInfo->data = NULL;
	char** 			rows = NULL;

	pngPtr = png_create_read_struct(PNG_LIBPNG_VER_STRING,(png_voidp)NULL,my_error,my_warning);
	if(!pngPtr) goto bail;

	infoPtr=png_create_info_struct(pngPtr);
	if(!infoPtr) goto bail;

	file = fopen(inFilePath, "rb");
	if (!file) goto bail;
	fseek(file, 0, SEEK_END);
	fileLength = ftell(file);
	fseek(file, 0, SEEK_SET);
	buffer = new unsigned char[fileLength];
	if (!buffer) goto bail;
	if (fread(buffer, 1, fileLength, file) != fileLength) goto bail;
	fclose(file);
	file = NULL;

	png_start_pos = buffer;
	png_current_pos = buffer;
	png_end_pos = buffer + fileLength;

	if (png_sig_cmp(png_current_pos,0,8)) goto bail;

	png_set_interlace_handling(pngPtr);
	if(setjmp(pngPtr->jmpbuf))
	{
		goto bail;
	}

	png_init_io      (pngPtr,NULL						);
	png_set_read_fn  (pngPtr,NULL,png_buffered_read_func);
	png_set_sig_bytes(pngPtr,8							);	png_current_pos+=8;
	png_read_info	 (pngPtr,infoPtr					);

	png_get_IHDR(pngPtr,infoPtr,&width,&height,
			&bit_depth,&color_type,&interlace_type,
			&compression_type,&P_filter_type);

	outImageInfo->width = width;
	outImageInfo->height = height;

	double lcl_gamma;			// This will be the gamma of the file if it has one.
#if APL							// Macs and PCs have different gamma responses.
	double screen_gamma=1.8;	// Darks look darker and brights brighter on the PC.
#endif							// Macs are more even.
#if IBM||LIN
	double screen_gamma=2.2;
#endif

	if(  png_get_gAMA (pngPtr,infoPtr     ,&lcl_gamma))		// Perhaps the file has its gamma recorded, for example by photoshop. Just tell png to callibrate for our hw platform.
	     png_set_gamma(pngPtr,screen_gamma, lcl_gamma);
	else png_set_gamma(pngPtr,screen_gamma, 1.0/1.8  );		// If the file doesn't have gamma, assume it was drawn on a Mac.

	if(color_type==PNG_COLOR_TYPE_PALETTE && bit_depth<= 8)if (!leaveIndexed)	png_set_expand	  (pngPtr);
	if(color_type==PNG_COLOR_TYPE_GRAY    && bit_depth<  8)						png_set_expand	  (pngPtr);
	if(png_get_valid(pngPtr,infoPtr,PNG_INFO_tRNS)        )						png_set_expand	  (pngPtr);
	if(										 bit_depth==16)						png_set_strip_16  (pngPtr);
	if(										 bit_depth<  8)						png_set_packing	  (pngPtr);
	if(            color_type==PNG_COLOR_TYPE_GRAY		  )if (!leaveIndexed)	png_set_gray_to_rgb (pngPtr);
	if(            color_type==PNG_COLOR_TYPE_GRAY_ALPHA  )if (!leaveIndexed)	png_set_gray_to_rgb (pngPtr);
	switch(color_type) {
	case PNG_COLOR_TYPE_GRAY:		outImageInfo->channels = leaveIndexed ? 1 : 3;		break;
	case PNG_COLOR_TYPE_GRAY_ALPHA:	outImageInfo->channels = leaveIndexed ? 2 : 4;		break;
	case PNG_COLOR_TYPE_PALETTE:	outImageInfo->channels = leaveIndexed ? 1 : 3;		break;
	case PNG_COLOR_TYPE_RGB:		outImageInfo->channels = 					3;		break;
	case PNG_COLOR_TYPE_RGBA:		outImageInfo->channels = 					4;		break;
	default: goto bail;
	}
	png_set_bgr(pngPtr);
	png_read_update_info(pngPtr,infoPtr);

	outImageInfo->pad = 0;
	outImageInfo->data = (unsigned char *) malloc(outImageInfo->width * outImageInfo->height * outImageInfo->channels);
	if (!outImageInfo->data) goto bail;

	rows=(char**)malloc(height*sizeof(char*));
	if (!rows) goto bail;
	
	for(int i=0;i<height;i++)
	{
		rows[i]=(char*)outImageInfo->data     +((outImageInfo->height-1-i)*(outImageInfo->width)*(outImageInfo->channels));
	}

	png_read_image(pngPtr,(png_byte**)rows);										// Now we just tell pnglib to read in the data.  When done our row ptrs will be filled in.
	free(rows);
	rows = NULL;

	delete [] buffer;
	buffer = NULL;

	png_destroy_read_struct(&pngPtr,(png_infopp)&infoPtr,(png_infopp)NULL);
	
	return 0;
bail:

	if (pngPtr && infoPtr)		png_destroy_read_struct(&pngPtr,(png_infopp)&infoPtr,(png_infopp)NULL);
	else if (pngPtr)			png_destroy_read_struct(&pngPtr,(png_infopp)NULL,(png_infopp)NULL);
	if (buffer)					delete [] buffer;
	if (file)					fclose(file);
	if (outImageInfo->data)		free(outImageInfo->data);
	if (rows) 					free(rows);

	return -1;	

}

int		WriteBitmapToPNG(const struct ImageInfo * inImage, const char * inFilePath, char * inPalette, int inPaletteLen)
{
	FILE *		file = NULL;
	png_structp	png_ptr = NULL;
	png_infop	info_ptr = NULL;
	char **		rows = NULL;
	
	file = fopen(inFilePath, "wb");
	if (!file) goto bail;

	png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,(png_voidp)NULL,my_error,my_warning);
    if (!png_ptr) goto bail;

	info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) goto bail;

	if (setjmp(png_jmpbuf(png_ptr))) goto bail;
    
	png_init_io(png_ptr, file);
	png_set_filter(png_ptr, 0, PNG_FILTER_NONE);
    png_set_compression_level(png_ptr, Z_BEST_COMPRESSION);
	png_set_compression_mem_level(png_ptr, 8);
	png_set_compression_strategy(png_ptr, Z_DEFAULT_STRATEGY);
	png_set_compression_window_bits(png_ptr, 15);
	png_set_compression_method(png_ptr, 8);
	png_set_compression_buffer_size(png_ptr, 8192);

	png_set_bgr(png_ptr);

	png_set_PLTE(png_ptr, info_ptr, (png_colorp) inPalette, inPaletteLen);

    png_set_IHDR(png_ptr, info_ptr, inImage->width, inImage->height, 8, 
    	(inImage->channels == 1) ? (inPalette ? PNG_COLOR_TYPE_PALETTE : PNG_COLOR_TYPE_GRAY) :
    	((inImage->channels == 3) ? PNG_COLOR_TYPE_RGB : PNG_COLOR_TYPE_RGBA), 
    	PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_DEFAULT);

	png_write_info(png_ptr, info_ptr);


	rows=(char**)malloc(inImage->height*sizeof(char*));
	if (!rows) goto bail;
	
	for(int i=0;i<inImage->height;i++)
	{
		rows[i]=(char*)inImage->data+((inImage->height-1-i)*((inImage->width)*(inImage->channels)+inImage->pad));
	}

	png_write_image(png_ptr,(png_byte**)rows);
	free(rows);
	rows = NULL;

	png_write_end(png_ptr, info_ptr);                    
    
    fclose(file);
	file = NULL;
	png_destroy_write_struct(&png_ptr, &info_ptr);
	
	return 0;
	
bail:

	if (png_ptr && info_ptr)	png_destroy_write_struct(&png_ptr, &info_ptr);
	else if (png_ptr)			png_destroy_write_struct(&png_ptr, NULL);
	if (file)					fclose(file);
	if (rows)					free(rows);
	
	return -1;
    
}

static	void	IgnoreTiffWarnings(const char *, const char*, va_list)
{
}

int		CreateBitmapFromTIF(const char * inFilePath, struct ImageInfo * outImageInfo)
{
	TIFFErrorHandler	errH = TIFFSetWarningHandler(IgnoreTiffWarnings);
    TIFF* tif = TIFFOpen(inFilePath, "r");
    if (tif == NULL) goto bail;

	int result = -1;
	uint32 w, h;
	uint16 cc;
	size_t npixels;
	uint32* raster;
		
		
	TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &w);
	TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &h);
	TIFFGetField(tif, TIFFTAG_SAMPLESPERPIXEL, &cc);
	npixels = w * h;
	raster = (uint32*) _TIFFmalloc(npixels * sizeof (uint32));
	if (raster != NULL) {
	    if (TIFFReadRGBAImage(tif, w, h, raster, 0)) {
			
			outImageInfo->data = (unsigned char *) malloc(npixels * 4);
			outImageInfo->width = w;
			outImageInfo->height = h;
			outImageInfo->channels = 4;
			outImageInfo->pad = 0;
			int	count = outImageInfo->width * outImageInfo->height;
			unsigned char * d = outImageInfo->data;
			unsigned char * s = (unsigned char *) raster;
			while (count--)
			{
#if APL			
				d[0] = s[1];
				d[1] = s[2];
				d[2] = s[3];
				d[3] = s[0];
#elif IBM
				d[0] = s[2];
				d[1] = s[1];
				d[2] = s[0];
				d[3] = s[3];
#else
	#error PLATFORM NOT DEFINED
#endif				
				s += 4;
				d += 4;
			}
			result = 0;
	    }
	    _TIFFfree(raster);
	}
	TIFFClose(tif);    
	TIFFSetWarningHandler(errH);
    return result;
bail:
	TIFFSetWarningHandler(errH);
	return -1;    
}

#endif /* !defined(__MACH_) */