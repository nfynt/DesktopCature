using System;
using System.Windows.Forms;
using System.Runtime.InteropServices;
using System.Drawing;
using System.IO;

namespace Shubham.DesktopCapLib
{
    public class Capture
    {
        public struct TexInfo
        {
            public UnityEngine.Texture2D unityTex2D;
            public byte[] bitStream;
        }

        public UnityEngine.Texture2D tex;
        public Rectangle screenSize;
        public TexInfo texInfo;


        public Capture()
        {
            tex = new UnityEngine.Texture2D(20, 30, UnityEngine.TextureFormat.DXT1, false);
            screenSize = Screen.PrimaryScreen.Bounds;
        }

        /// <summary>
        /// Get the current screen information.
        /// </summary>
        /// <returns>struct <i>TexInfo</i> which contains <i>Texture2D</i> and <i>byte</i> array</returns>
        public TexInfo GetCurrScreenInfo()
        {
            Bitmap target = new Bitmap(screenSize.Width, screenSize.Height);
            using (Graphics g = Graphics.FromImage(target))
            {
                g.CopyFromScreen(0, 0, 0, 0, new Size(screenSize.Width, screenSize.Height));
            }

            MemoryStream ms = new MemoryStream();
            target.Save(ms, System.Drawing.Imaging.ImageFormat.Jpeg);
            ms.Seek(0, SeekOrigin.Begin);

            this.texInfo.bitStream = ms.ToArray();
            this.texInfo.unityTex2D.LoadImage(this.texInfo.bitStream);

            return this.texInfo;
        }

        /// <summary>
        /// Get the current screen information for block of the screen
        /// </summary>
        /// <param name="x">start x</param>
        /// <param name="y">xtart y</param>
        /// <param name="width">width of the block</param>
        /// <param name="height">height of the block</param>
        ///  <returns>struct <i>TexInfo</i> which contains <i>Texture2D</i> and <i>byte</i> array</returns>
        public TexInfo GetCurrScreenInfo(int x, int y, int width, int height)
        {
            Bitmap target = new Bitmap(screenSize.Width, screenSize.Height);
            using (Graphics g = Graphics.FromImage(target))
            {
                g.CopyFromScreen(x, y, 0, 0, new Size(width, height));
            }

            MemoryStream ms = new MemoryStream();
            target.Save(ms, System.Drawing.Imaging.ImageFormat.Jpeg);
            ms.Seek(0, SeekOrigin.Begin);

            this.texInfo.bitStream = ms.ToArray();
            this.texInfo.unityTex2D.LoadImage(this.texInfo.bitStream);

            return this.texInfo;
        }

        /// <summary>
        /// Get the current screen in Unity Texture2D format.
        /// </summary>
        /// <returns>struct <i>TexInfo</i> which contains <i>Texture2D</i> and <i>byte</i> array</returns>
        public UnityEngine.Texture2D GetScreen()
        {
           // UnityEngine.Texture2D tex = new UnityEngine.Texture2D(20,30,UnityEngine.TextureFormat.DXT1,false);
            //Rectangle screenSize = Screen.PrimaryScreen.Bounds;
            Bitmap target = new Bitmap(screenSize.Width, screenSize.Height);
            using (Graphics g = Graphics.FromImage(target))
            {
                g.CopyFromScreen(0, 0, 0, 0,new Size(screenSize.Width, screenSize.Height));
            }

            MemoryStream ms = new MemoryStream();
            target.Save(ms, System.Drawing.Imaging.ImageFormat.Jpeg);
            ms.Seek(0, SeekOrigin.Begin);

            tex.LoadImage(ms.ToArray());

            return tex;
        }

        /// <summary>
        /// Saves the current frame at specified location.
        /// </summary>
        /// <param name="loc">full path with JPG extension only.</param>
        public void SaveCurrentFrame(string loc)
        {
            Bitmap target = new Bitmap(screenSize.Width, screenSize.Height);
            using (Graphics g = Graphics.FromImage(target))
            {
                g.CopyFromScreen(0, 0, 0, 0, new Size(screenSize.Width, screenSize.Height));
            }

            MemoryStream ms = new MemoryStream();
            target.Save(ms, System.Drawing.Imaging.ImageFormat.Jpeg);
            ms.Seek(0, SeekOrigin.Begin);
            File.WriteAllBytes(loc, ms.ToArray());
        }

        /// <summary>
        /// Get a block of the screen
        /// </summary>
        /// <param name="x">start x</param>
        /// <param name="y">xtart y</param>
        /// <param name="width">width of the block</param>
        /// <param name="height">height of the block</param>
        /// <returns></returns>
        public UnityEngine.Texture2D GetScreen(int x, int y, int width, int height)
        {
            // UnityEngine.Texture2D tex = new UnityEngine.Texture2D(20,30,UnityEngine.TextureFormat.DXT1,false);

            //Rectangle screenSize = Screen.PrimaryScreen.Bounds;
            Bitmap target = new Bitmap(width, height);
            using (Graphics g = Graphics.FromImage(target))
            {
                g.CopyFromScreen(x, y, 0, 0, new Size(width, height));
            }

            MemoryStream ms = new MemoryStream();
            target.Save(ms, System.Drawing.Imaging.ImageFormat.Jpeg);
            ms.Seek(0, SeekOrigin.Begin);

            tex.LoadImage(ms.ToArray());

            return tex;
        }
    }
}
