#ifndef __RESIZEIMAGE_H
#define __RESIZEIMAGE_H

#include <windows.h>

extern "C"
{
	void __declspec(dllexport) SetMaximumUpscale(int percentage);
	BITMAPINFOHEADER __declspec(dllexport) GetResizedImageInfo(int width, int height, int maxwidth, int maxheight);
	void __declspec(dllexport) ResizeImage(unsigned char *buffer, BITMAPINFOHEADER bminfo, unsigned char *newbuffer, BITMAPINFOHEADER newbminfo);
};

class CResizeImage
{
	public:

		CResizeImage(int width, int height, int maxwidth, int maxheight);
		CResizeImage(unsigned char *buffer, BITMAPINFOHEADER bminfo, unsigned char *newbuffer, BITMAPINFOHEADER newbminfo);

		BITMAPINFOHEADER DetermineNewSize(void);
		void Resize(void);

	private:
	
		int width, newwidth;
		int height, newheight;
		int bpp, newbpp;
		int maxwidth;
		int maxheight;

		float fx, fy;
		int rowsize, newrowsize;

		unsigned char *buffer;
		unsigned char *newbuffer;
		BITMAPINFOHEADER bminfo;
		BITMAPINFOHEADER newbminfo;

		void ScaleDown(void);
		void ScaleDownPreCalculate(int* &ixA, float* &dxA, int* &nrxA, float* &dxB);
		void ScaleUp(void);
		void ScaleUpPreCalculate(
			int* &startx, int* &endx, int* &starty, int* &endy, 
			float** &hweights, float** &vweights, float* &hdensity, float* &vdensity);

		int CalculateRowSize(int bpp, int width);
		float Lanczos(float x);

		static float *togamma;
		static unsigned char *fromgamma;
		static void InitGamma(void);
};

#endif
