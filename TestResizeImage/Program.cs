using System;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;
using System.Drawing;

namespace TestResizeImage
{
    class Program
    {
        [StructLayout(LayoutKind.Sequential)]
        public struct BITMAPINFOHEADER
        {
            public uint biSize;
            public int biWidth;
            public int biHeight;
            public ushort biPlanes;
            public ushort biBitCount;
            public uint biCompression;
            public uint biSizeImage;
            public int biXPelsPerMeter;
            public int biYPelsPerMeter;
            public uint biClrUsed;
            public uint biClrImportant;
        }

        [DllImport("ResizeImage.dll", CallingConvention = CallingConvention.Cdecl)]
        public static extern void SetMaximumUpscale(int percentage);

        [DllImport("ResizeImage.dll", CallingConvention = CallingConvention.Cdecl)]
        public static extern BITMAPINFOHEADER GetResizedImageInfo(int width, int height, int maxwidth, int maxheight);

        [DllImport("ResizeImage.dll", CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr ResizeImage(IntPtr buffer, BITMAPINFOHEADER bminfo, IntPtr newbuffer, BITMAPINFOHEADER newbminfo);

        static void Main()
        {
            SetMaximumUpscale(200);

            Bitmap imgsrc = new Bitmap("icon.jpg");

            var infosrc = new BITMAPINFOHEADER
                {
                    biWidth = imgsrc.Width,
                    biHeight = imgsrc.Height,
                    biBitCount = (ushort)Image.GetPixelFormatSize(imgsrc.PixelFormat)
                };
            var infodest = GetResizedImageInfo(imgsrc.Width, imgsrc.Height, 1600, 1200);

            Rectangle recsrc = new Rectangle(0, 0, imgsrc.Width, imgsrc.Height);
            BitmapData datasrc = imgsrc.LockBits(recsrc, ImageLockMode.WriteOnly, imgsrc.PixelFormat);

            Bitmap imgdest = new Bitmap(infodest.biWidth, infodest.biHeight, PixelFormat.Format24bppRgb);
            Rectangle recdest = new Rectangle(0, 0, imgdest.Width, imgdest.Height);
            BitmapData datadest = imgdest.LockBits(recdest, ImageLockMode.WriteOnly, imgdest.PixelFormat);

            infodest.biBitCount = (ushort)Image.GetPixelFormatSize(imgdest.PixelFormat);
            ResizeImage(datasrc.Scan0, infosrc, datadest.Scan0, infodest);

            imgsrc.UnlockBits(datasrc);
            imgdest.UnlockBits(datadest);

            imgdest.Save("icondest.png", ImageFormat.Png);

            Console.ReadKey();
        }
    }
}
