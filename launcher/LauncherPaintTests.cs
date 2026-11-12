using System;
using System.Drawing;
using System.IO;
using System.Reflection;
using System.Windows.Forms;

internal static class LauncherPaintTests {
    static int checks;
    static readonly BindingFlags Methods=BindingFlags.Instance|BindingFlags.NonPublic;
    static void Check(bool condition,string label){if(!condition)throw new Exception(label);checks++;Console.WriteLine("PASS "+label);}
    static void Event(Control c,string method,EventArgs args){typeof(ModernButton).GetMethod(method,Methods).Invoke(c,new object[]{args});}
    static void Paint(ModernButton button,Bitmap target){
        using(var graphics=Graphics.FromImage(target))
            Event(button,"OnPaint",new PaintEventArgs(graphics,button.ClientRectangle));
    }
    static bool Equal(Bitmap a,Bitmap b){
        for(int y=0;y<a.Height;y++)for(int x=0;x<a.Width;x++)if(a.GetPixel(x,y)!=b.GetPixel(x,y))return false;
        return true;
    }
    [STAThread] static void Main(){
        Application.EnableVisualStyles();Application.SetCompatibleTextRenderingDefault(false);
        Directory.CreateDirectory("build/launcher-paint");
        using(var panel=new Panel{BackColor=Palette.Card})
        foreach(float scale in new[]{1f,1.5f,2f})
        foreach(string kind in new[]{"primary","ordinary","quiet"})
        using(var button=new ModernButton("PLAY GAME"){Primary=kind=="primary",Quiet=kind=="quiet",Size=new Size((int)(180*scale),(int)(40*scale))})
        using(var original=new Bitmap(button.Width,button.Height))
        using(var surface=new Bitmap(button.Width,button.Height)){
            panel.Controls.Add(button);button.Font=new Font("Segoe UI Semibold",9.5f*scale,FontStyle.Bold);
            Paint(button,original);Paint(button,surface);
            for(int i=0;i<30;i++){
                Event(button,"OnMouseEnter",EventArgs.Empty);Paint(button,surface);
                if(i==0)surface.Save("build/launcher-paint/"+kind+"-"+scale+"-hover.png");
                Event(button,"OnMouseLeave",EventArgs.Empty);Paint(button,surface);
            }
            Check(Equal(original,surface),kind+" hover restores every pixel at "+scale);
            Check(surface.GetPixel(0,0).ToArgb()==panel.BackColor.ToArgb(),kind+" corner matches parent");
            button.Enabled=false;Paint(button,surface);button.Enabled=true;Paint(button,surface);
            Check(Equal(original,surface),kind+" enable transition restores every pixel");
            Event(button,"OnMouseDown",new MouseEventArgs(MouseButtons.Left,1,8,8,0));
            Event(button,"OnMouseLeave",EventArgs.Empty);Paint(button,surface);
            Event(button,"OnMouseUp",new MouseEventArgs(MouseButtons.Left,1,-8,-8,0));Paint(button,surface);
            Check(Equal(original,surface),kind+" drag-out release restores every pixel");
            Size size=button.Size;button.Size=new Size(size.Width+20,size.Height+10);button.Size=size;
            Paint(button,surface);Check(Equal(original,surface),kind+" resize restores every pixel");
            panel.BackColor=Palette.Input;Paint(button,surface);
            Check(surface.GetPixel(0,0).ToArgb()==Palette.Input.ToArgb(),kind+" repaints changed parent");
            panel.BackColor=Palette.Card;Paint(button,surface);
            surface.Save("build/launcher-paint/"+kind+"-"+scale+"-restored.png");
            panel.Controls.Remove(button);
        }
        // Use isolated settings for layout captures.
        string root=Path.GetFullPath("build/launcher-paint/profile");
        var fields=BindingFlags.Static|BindingFlags.NonPublic;
        foreach(string name in new[]{"Home","Profile","Screenshots"})typeof(Data).GetField(name,fields).SetValue(null,Path.Combine(root,name));
        using(var form=new Launcher()){
            form.ShowInTaskbar=false;form.StartPosition=FormStartPosition.Manual;form.Location=new Point(-20000,-20000);
            form.Show();Application.DoEvents();
            Size initial=form.Size;form.Size=form.MinimumSize;Application.DoEvents();
            using(var capture=new Bitmap(form.Width,form.Height)){form.DrawToBitmap(capture,new Rectangle(Point.Empty,capture.Size));capture.Save("build/launcher-paint/launcher-minimum.png");}
            form.Size=initial;
            foreach(float scale in new[]{1f,1.5f,2f}){
                if(scale!=1f)form.Scale(new SizeF(scale/(scale==1.5f?1f:1.5f),scale/(scale==1.5f?1f:1.5f)));
                form.PerformLayout();
                using(var capture=new Bitmap(form.Width,form.Height)){form.DrawToBitmap(capture,new Rectangle(Point.Empty,capture.Size));capture.Save("build/launcher-paint/launcher-"+scale+".png");}
            }
            form.Close();
        }
        using(var lan=new LanMenu("test-references/DOOM2.WAD",new string[0],"paint-test"))
        {
            lan.ShowInTaskbar=false;lan.StartPosition=FormStartPosition.Manual;lan.Location=new Point(-20000,-20000);
            lan.Show();Application.DoEvents();
            using(var capture=new Bitmap(lan.Width,lan.Height)){
                lan.DrawToBitmap(capture,new Rectangle(Point.Empty,capture.Size));capture.Save("build/launcher-paint/lan.png");
            }
            lan.Close();
        }
        Console.WriteLine("PASS "+checks+" painting checks; scaling captures use synthetic Control.Scale.");
    }
}
