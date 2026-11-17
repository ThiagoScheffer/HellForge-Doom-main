using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Drawing;
using System.Reflection;
using System.Windows.Forms;
using System.Xml.Linq;
internal static class LauncherUpdateTests {
 static int count;
 static void Check(bool ok,string name){if(!ok)throw new Exception(name);Console.WriteLine("PASS "+name);count++;}
 [STAThread] static void Main(string[] args){
  Application.EnableVisualStyles();Application.SetCompatibleTextRenderingDefault(false);
  string root=Path.GetFullPath("build/launcher-update-check");Directory.CreateDirectory(root);
  typeof(Data).GetField("Home",BindingFlags.Static|BindingFlags.NonPublic).SetValue(null,root);
  typeof(Data).GetField("Profile",BindingFlags.Static|BindingFlags.NonPublic).SetValue(null,Path.Combine(root,"profile"));Directory.CreateDirectory(Data.Profile);
  string original="# preserved\r\nhud_scale 75\r\nhud_scale 80\r\ncustom_future 42\r\n";
  string merged=LauncherConfig.Merge(original,new Dictionary<string,string>{{"hud_scale","90"},{"speedrun_hud","1"}});
  Check(merged.Contains("# preserved")&&merged.Contains("custom_future 42"),"unknown keys and comments retained");
  Check(merged.Split(new[]{"hud_scale"},StringSplitOptions.None).Length==2&&merged.Contains("hud_scale 90"),"duplicate overrides replaced consistently");
  Check(LauncherConfig.Merge(merged,new Dictionary<string,string>{{"hud_scale",""}}).IndexOf("hud_scale",StringComparison.Ordinal)<0,"blank removes override");
  bool rejected=false;try{LauncherConfig.Merge(original,new Dictionary<string,string>{{"hud_scale","NaN"}});}catch(InvalidDataException){rejected=true;}Check(rejected,"nonfinite values rejected");
  string cfg=Path.Combine(Data.Profile,"graphics.cfg");File.WriteAllText(cfg,original);LauncherConfig.Write(cfg,original,merged);Check(File.ReadAllText(cfg)==merged,"atomic write succeeds");
  rejected=false;try{LauncherConfig.Write(cfg,original,"broken");}catch(IOException){rejected=true;}Check(rejected&&File.ReadAllText(cfg)==merged,"external modification preserved");
  string failed=Path.Combine(cfg,"graphics.cfg");rejected=false;try{LauncherConfig.Write(failed,"","broken");}catch(IOException){rejected=true;}Check(rejected&&File.ReadAllText(cfg)==merged,"failed write keeps existing configuration");
  File.WriteAllText(Path.Combine(Data.Profile,"doomsav0.dsg"),"keep save");
  SettingsBackup.Export(Path.Combine(root,"recovery.zip"),new XDocument(new XElement("launcher")),Data.Profile);
  Check(File.Exists(Path.Combine(root,"recovery.zip"))&&File.ReadAllText(Path.Combine(Data.Profile,"doomsav0.dsg"))=="keep save","backup does not touch saves");
  using(var resource=Assembly.GetExecutingAssembly().GetManifestResourceStream("settings-schema.xml"))Check(XDocument.Load(resource).Root.Elements("option").Count()==355,"all 355 F4 and balance controls embedded");
  using(var mascot=new WadMascot()){mascot.Load(args[0]);Check(mascot.Animated,"Arch-vile stock frames decode from user IWAD");using(var b=new Bitmap(180,200)){using(var g=Graphics.FromImage(b)){g.Clear(Palette.Background);mascot.Draw(g,new Rectangle(10,10,150,180),3);}b.Save(Path.Combine(root,"archvile.png"));}mascot.Load(null);Check(!mascot.Animated,"missing artwork fallback clears resources");}
  Check(WadMascot.Decode(new byte[8],new byte[768])==null,"malformed patch safely rejected");
  using(var dialog=new LauncherSettingsDialog(cfg,()=>{})){dialog.Show();Application.DoEvents();using(var b=new Bitmap(dialog.Width,dialog.Height)){dialog.DrawToBitmap(b,dialog.ClientRectangle);b.Save(Path.Combine(root,"settings.png"));}dialog.Close();}
  File.WriteAllText(Path.Combine(Data.Profile,"doom.cfg"),"test config");
  LauncherConfig.ResetProfile(Data.Profile);
  Check(!File.Exists(cfg)&&!File.Exists(Path.Combine(Data.Profile,"doom.cfg"))&&File.ReadAllText(Path.Combine(Data.Profile,"doomsav0.dsg"))=="keep save","fresh start clears only configs and preserves saves");
  using(var launcher=new Launcher()){
   typeof(Launcher).GetMethod("AddGame",BindingFlags.Instance|BindingFlags.NonPublic).Invoke(launcher,new object[]{Path.GetFullPath(args[0])});
   launcher.Show();Application.DoEvents();
   foreach(int width in new[]{1150,940}){launcher.ClientSize=new Size(width,830);Application.DoEvents();using(var b=new Bitmap(launcher.Width,launcher.Height)){launcher.DrawToBitmap(b,new Rectangle(0,0,b.Width,b.Height));b.Save(Path.Combine(root,"launcher-"+width+".png"));}}
   launcher.Close();
  }
  Console.WriteLine("PASS "+count+" launcher update checks; captures saved to "+root);
 }
}

