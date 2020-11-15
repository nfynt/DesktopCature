using System;
using System.Windows.Forms;
using System.Runtime.InteropServices;
using System.Drawing;
using System.IO;

namespace Shubham.DesktopCapLib
{
    public class Capture
    {

        public UnityEngine.Texture2D tex;
        public Rectangle screenSize;

		private Bitmap target;
		private MemoryStream target_ms;
		private int width, height;
		private int startX, startY;

		/// <summary>
		/// Get active screen(s) information
		/// </summary>
		/// <returns>string info for screen in new line</returns>
		public static string GetScreenInfo()
		{
			string info = "";
			int ind = 0;
			foreach (Screen sc in Screen.AllScreens)
			{
				info += ind + ":" + sc.DeviceName + " size:" + sc.WorkingArea.ToString() + "\n";
				ind++;
			}

			return info;
		}

		public Capture(int x=0, int y=0, int width=0, int height=0)
        {
			screenSize = Screen.PrimaryScreen.Bounds;
			UpdateCaptureScreen(x, y, width, height);
		}
        
		/// <summary>
		/// Set new Capture screen size
		/// </summary>
		/// <param name="x">source start x</param>
		/// <param name="y">source start y</param>
		/// <param name="width">source width</param>
		/// <param name="height">source height</param>
		public void UpdateCaptureScreen(int x, int y, int width, int height)
		{
			tex = new UnityEngine.Texture2D(20, 30, UnityEngine.TextureFormat.DXT1, false);

			startX = x;
			startY = y;
			this.width = (width != 0) ? width : screenSize.Width;
			this.height = (height != 0) ? height : screenSize.Height;

			target = new Bitmap(this.width - x, this.height - y);
			target_ms = new MemoryStream();
		}

        /// <summary>
        /// Get the current screen in Unity Texture2D format.
        /// </summary>
        /// <returns>struct <i>TexInfo</i> which contains <i>Texture2D</i> and <i>byte</i> array</returns>
        public UnityEngine.Texture2D GetScreen()
        {
            using (Graphics g = Graphics.FromImage(target))
            {
                g.CopyFromScreen(startX, startY, 0, 0,new Size(width,height));
            }

            target.Save(target_ms, System.Drawing.Imaging.ImageFormat.Jpeg);
            target_ms.Seek(0, SeekOrigin.Begin);

			UnityEngine.ImageConversion.LoadImage(tex, target_ms.ToArray());

            return tex;
        }

		/// <summary>
		/// Update the Unity texture 2d in place
		/// </summary>
		/// <param name="texture">Texture2D object to be updated</param>
		public void UpdateUnityTexture(ref UnityEngine.Texture2D texture)
		{
			using (Graphics g = Graphics.FromImage(target))
			{
				g.CopyFromScreen(startX,startY, 0, 0, new Size(width,height));
			}

			target.Save(target_ms, System.Drawing.Imaging.ImageFormat.Jpeg);
			target_ms.Seek(0, SeekOrigin.Begin);

			UnityEngine.ImageConversion.LoadImage(texture, target_ms.ToArray());
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
		
    }
}

/*
 __  _ _____   ____  _ _____  
|  \| | __\ `v' /  \| |_   _| 
| | ' | _| `. .'| | ' | | |   
|_|\__|_|   !_! |_|\__| |_|
 
*/

