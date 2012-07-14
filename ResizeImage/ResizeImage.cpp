#include <math.h>
#include <stdio.h>
#include <algorithm>

#include "ResizeImage.h"

#define CAP(x) ((x) < 0.0f ? 0.0f : (x) > 1.0f ? 1.0f : (x))
#define ROUND(x) (floor(x + 0.5))

const int LANCZOS_WINDOW = 2;
const float LANCZOS_BLUR = 1.25;
const int GAMMASIZE = 200000;

int maxUpscale = 100;

#pragma region Exported methods

void SetMaximumUpscale(int percentage)
{
	maxUpscale = percentage;
}

BITMAPINFOHEADER __declspec(dllexport) GetResizedImageInfo(int width, int height, int maxwidth, int maxheight)
{
	return CResizeImage(width, height, maxwidth, maxheight).DetermineNewSize();
}

void __declspec(dllexport) ResizeImage(unsigned char *buffer, BITMAPINFOHEADER bminfo, unsigned char *newbuffer, BITMAPINFOHEADER newbminfo)
{
	CResizeImage(buffer, bminfo, newbuffer, newbminfo).Resize();
}

#pragma endregion

#pragma region CResizeImage implemetation

#pragma region Constructors

CResizeImage::CResizeImage(int width, int height, int maxwidth, int maxheight)
	: width(width), height(height), maxwidth(maxwidth), maxheight(maxheight)
{
	InitGamma();
}

CResizeImage::CResizeImage(unsigned char *buffer, BITMAPINFOHEADER bminfo, unsigned char *newbuffer, BITMAPINFOHEADER newbminfo)
	: buffer(buffer), bminfo(bminfo), newbuffer(newbuffer), newbminfo(newbminfo)
{
	width = bminfo.biWidth;
	height = bminfo.biHeight;
	newwidth = newbminfo.biWidth;
	newheight = newbminfo.biHeight;
	bpp = bminfo.biBitCount >> 3;
	newbpp = newbminfo.biBitCount >> 3;

	InitGamma();
}

#pragma endregion

#pragma region Public methods

BITMAPINFOHEADER CResizeImage::DetermineNewSize(void)
{
	fx = (float)maxwidth / width;
	fy = (float)maxheight / height;

	if (width < maxwidth && height < maxheight && maxUpscale > 100)
	{
		float upscale = (float)maxUpscale / 100;

		if (fx < fy)
		{
			fx = fx > upscale ? upscale : fx;
			fy = fx;
		}
		else
		{
			fy = fy > upscale ? upscale : fy;
			fx = fy;
		}
	}
	else if (width > maxwidth || height > maxheight)
	{
		if (fx < fy)
			fy = fx;
		else
			fx = fy;
	}
	else
	{
		fx = 1.0f;
		fy = 1.0f;
	}

	newbminfo.biWidth = ROUND(fx * width);
	newbminfo.biHeight = ROUND(fy * height);

	return newbminfo;
}
	
void CResizeImage::Resize(void)
{
	fx = (float)width / newwidth;
	fy = (float)height / newheight;

	rowsize = CalculateRowSize(bpp, width);
	newrowsize = CalculateRowSize(newbpp, newwidth);

	if (fx > 1.0 || fy > 1.0)
		ScaleDown();
	else
		ScaleUp();
}

#pragma endregion

#pragma region Scaling implementation

void CResizeImage::ScaleDown(void)
{
	int nrrows = (int)ceil(fy + 1.0f);
	unsigned char **rows = new unsigned char *[nrrows];
	float *dxA, *dxB, *dyA = new float[nrrows];
	int *ixA, *iyA = new int[nrrows], *nrxA;
	unsigned char *newline = newbuffer, *c1, *c2;
	float starty, endy = 0.0f, dy, diffY;
	int pos, nrx, nrx1, nry, t3, t6;
	float sum1, sum2, sum3, f, area;

	ScaleDownPreCalculate(ixA, dxA, nrxA, dxB);

	for (int t1 = 0; t1 < newheight; t1++)
	{
		pos = t1 * fy;
		t3 = pos + nrrows < height + 1 ? 0 : pos - height + 1;

		for (int t2 = 0; t2 < nrrows + t3; t2++)
		{
			rows[t2] = buffer + (pos + t2) * rowsize;
		}

		starty = t1 * fy - pos;
		endy = (t1 + 1) * fy - pos + t3;
		diffY = endy - starty;
		nry = 0;

		for (float y = starty; y < endy; y = floor(y + 1.0f))
		{
			iyA[nry] = (int)y;
			if (endy - y > 1.0f)
				dyA[nry] = floor(y + 1.0f) - y;
			else
				dyA[nry] = endy - y;

			nry++;
		}

		nrx = 0;
		t6 = 0;

		for (int t2 = 0; t2 < newwidth; t2++)
		{
			t3 = nrxA[t2];
			area = (1.0f / (dxB[t2] * diffY)) * GAMMASIZE;
			sum1 = sum2 = sum3 = 0.0f;

			for (int t4 = 0; t4 < nry; t4++)
			{
				c1 = rows[iyA[t4]];
				dy = dyA[t4];

				nrx1 = nrx;

				for (int t5 = 0; t5 < t3; t5++)
				{
					f = dxA[nrx1] * dy;
					c2 = c1 + ixA[nrx1];

					sum1 += togamma[c2[0]] * f;
					sum2 += togamma[c2[1]] * f;
					sum3 += togamma[c2[2]] * f;

					nrx1++;
				}
			}
			
			newline[t6 + 0] = fromgamma[(int)(sum1 * area)];
			newline[t6 + 1] = fromgamma[(int)(sum2 * area)];
			newline[t6 + 2] = fromgamma[(int)(sum3 * area)];

			nrx += t3;
			t6  += newbpp;
		}

		newline += newrowsize;
	}

	delete rows;
	delete nrxA;
	delete ixA;
	delete iyA;
	delete dxA;
	delete dyA;
	delete dxB;
}

void CResizeImage::ScaleDownPreCalculate(int* &ixA, float* &dxA, int* &nrxA, float* &dxB)
{
	int nrcols = (int)ceil(fx + 1.0f);
	int nrx = 0, nrx1;
	float startx = 0.0f;
	float endx = fx;

	dxA = new float[nrcols * newwidth];
	dxB = new float[newwidth];
	ixA = new int[nrcols * newwidth];
	nrxA = new int[newwidth];

	for (int t1 = 0; t1 < newwidth; t1++)
	{
		endx = endx < width + 1 ? endx : endx - width + 1;

		for (float x = startx; x < endx; x = floor(x + 1.0f))
		{
			ixA[nrx] = (int)x * bpp;

			if (endx - x > 1.0f)
				dxA[nrx] = floor(x + 1.0f) - x;
			else
				dxA[nrx] = endx - x;

			nrx++;
		}

		if (t1 > 0)
			nrxA[t1] = nrx - nrx1;
		else
			nrxA[t1] = nrx;

		nrx1 = nrx;

		dxB[t1] = endx - startx;

		startx = endx;
		endx += fx;
	}
}

void CResizeImage::ScaleUp(void)
{
	int *startx, *endx, *starty, *endy;
	float **hweights, **vweights;
	float *hdensity, *vdensity;
	float *vweight, *hweight;
	float sumh1, sumh2, sumh3, sumv1, sumv2, sumv3;

	ScaleUpPreCalculate(startx, endx, starty, endy, hweights, vweights, hdensity, vdensity);

	for (int t1 = 0; t1 < newheight; t1++)
	{
		int nmaxv = endy[t1] - starty[t1], nmaxh;
        unsigned char *line = newbuffer + t1 * newrowsize;

		for (int t2 = 0; t2 < newwidth; t2++)
		{
			nmaxh = endx[t2] - startx[t2];
			unsigned char* ypos = buffer + starty[t1] * rowsize;

			sumv1 = sumv2 = sumv3 = 0.0f;
            vweight = vweights[t1];

			for (int t3 = 0; t3 < nmaxv; t3++)
			{
				unsigned char *xpos = ypos + startx[t2] * bpp;

				sumh1 = sumh2 = sumh3 = 0.0f;
                hweight = hweights[t2];

				for (int t4 = 0; t4 < nmaxh; t4++)
				{
					sumh1 += *hweight * togamma[xpos[0]];
					sumh2 += *hweight * togamma[xpos[1]];
					sumh3 += *hweight * togamma[xpos[2]];

					xpos += bpp;
                    hweight++;
				}

				sumv1 += *vweight * sumh1;
				sumv2 += *vweight * sumh2;
				sumv3 += *vweight * sumh3;

				ypos += rowsize;
                vweight++;
			}

			line[0] = fromgamma[(int)(CAP(sumv1) * GAMMASIZE)];
			line[1] = fromgamma[(int)(CAP(sumv2) * GAMMASIZE)];
			line[2] = fromgamma[(int)(CAP(sumv3) * GAMMASIZE)];

            line += newbpp;
		}
    }

	delete startx;
	delete endx;
	delete starty;
	delete endy;
    for (int t1 = 0; t1 < newwidth; t1++)
        delete hweights[t1];
    delete hweights;
    for (int t1 = 0; t1 < newheight; t1++)
        delete vweights[t1];
    delete vweights;
	delete hdensity;
	delete vdensity;
}

void CResizeImage::ScaleUpPreCalculate(
	int* &startx, int* &endx, int* &starty, int* &endy, 
	float** &hweights, float** &vweights, float* &hdensity, float* &vdensity)
{
    float center, start, *vweight, *hweight;
	int nmax;

	startx = new int[newwidth];
	endx = new int[newwidth];
	starty = new int[newheight];
	endy = new int[newheight];
	hweights = new float*[newwidth];
	vweights = new float*[newheight];
	hdensity = new float[newwidth];
	vdensity = new float[newheight];

    // pre-calculation for each column
	for (int t1 = 0; t1 < newwidth; t1++)
	{
		center = ((float)t1 + 0.5f) * fx;

		startx[t1] = std::max<int>(center - LANCZOS_WINDOW, 0),
		endx[t1] = std::min<int>(ceil (center + LANCZOS_WINDOW), width - 1);
		nmax = endx[t1] - startx[t1];
        start = (float)startx[t1] + 0.5f - center;

		hweights[t1] = new float[nmax];
        hdensity[t1] = 0.0f;

		for (int t2 = 0; t2 < nmax; t2++)
		{
            hweights[t1][t2] = Lanczos((start + t2) / LANCZOS_BLUR);
			hdensity[t1] += hweights[t1][t2];
		}

        for (int t2 = 0; t2 < nmax; t2++)
            hweights[t1][t2] /= hdensity[t1];
	}

	// pre-calculation for each row
	for (int t1 = 0; t1 < newheight; t1++)
	{
		center = ((float)t1 + 0.5f) * fy;

		starty[t1] = std::max<int>(center - LANCZOS_WINDOW, 0),
		endy[t1] = std::min<int>(ceil (center + LANCZOS_WINDOW), height - 1);
		nmax = endy[t1] - starty[t1];
        start = (float)starty[t1] + 0.5f - center;

		vweights[t1] = new float[nmax];
        vdensity[t1] = 0.0f;

		for (int t2 = 0; t2 < nmax; t2++)
		{
			vweights[t1][t2] = Lanczos((start + t2) / LANCZOS_BLUR);
			vdensity[t1] += vweights[t1][t2];
		}

        for (int t2 = 0; t2 < nmax; t2++)
            vweights[t1][t2] /= vdensity[t1];
	}
}

#pragma endregion

#pragma region Private methods

int CResizeImage::CalculateRowSize(int bpp, int width)
{
	int rowsize = bpp * width;

	return rowsize + ((rowsize % 4 > 0) ? 4 - (rowsize % 4) : 0);
}

float CResizeImage::Lanczos(float x)
{
	if (x == 0.0f)
		return 1.0f;
	else
	{
		float xpi = x * 3.141593f;

		return LANCZOS_WINDOW * sin(xpi) * sin(xpi / LANCZOS_WINDOW) / (xpi * xpi);
	}
}

#pragma endregion

#pragma region Static gamma members

float *CResizeImage::togamma = NULL;
unsigned char *CResizeImage::fromgamma = NULL;

void CResizeImage::InitGamma(void)
{
	if (togamma != NULL && fromgamma != NULL)
		return;

	togamma = new float[256];

	for (int i1 = 0; i1 < 256; i1++)
		togamma[i1] = (float)pow(i1 / 255.0, 2.2);

	fromgamma = new unsigned char[GAMMASIZE + 50000];

    for (int i2 = 0; i2 < GAMMASIZE + 1; i2++)
		fromgamma[i2] = (unsigned char)(pow((double)i2 / GAMMASIZE, 1 / 2.2) * 255);

	// In case of overflow
	for (int i3 = GAMMASIZE + 1; i3 < GAMMASIZE + 50000; i3++)
		fromgamma[i3] = 255;
}

#pragma endregion

#pragma endregion
