using System;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Windows.Forms;
using System.Xml.Linq;

internal static class LauncherLoadoutTests {
    static int checks;
    static void Check(bool ok,string name){if(!ok)throw new Exception(name);++checks;Console.WriteLine("PASS "+name);}
    static readonly BindingFlags Flags=BindingFlags.NonPublic|BindingFlags.Instance;
    static T Field<T>(Launcher f,string name){return (T)typeof(Launcher).GetField(name,Flags).GetValue(f);}
    static object Call(Launcher f,string name,params object[] args){return typeof(Launcher).GetMethod(name,Flags).Invoke(f,args);}
    [STAThread] static void Main() {
        Application.EnableVisualStyles();Application.SetCompatibleTextRenderingDefault(false);
        string root=Path.GetFullPath("build/launcher-loadout/profile-"+Guid.NewGuid().ToString("N"));Directory.CreateDirectory(root);
        foreach(string name in new[]{"Home","Profile","Screenshots"})typeof(Data).GetField(name,BindingFlags.Static|BindingFlags.NonPublic).SetValue(null,Path.Combine(root,name));
        Directory.CreateDirectory(Data.Home);
        string doom=Path.GetFullPath("test-references/DOOM.WAD"),doom2=Path.GetFullPath("test-references/DOOM2.WAD");
        string map1=Path.GetFullPath("test-references/oku2v31.wad"),map2=Path.GetFullPath("test-maps/MaterialGallery-Doom2.wad");
        string addon=Path.GetFullPath("launcher/DiggingMaterials.wad");
        Check(Data.MapWad(map1)&&Data.MapWad(map2),"actual map WADs recognized by contents");
        Check(!Data.MapWad(addon),"resource-only WAD remains in add-ons");
        // Migrate the old flat list, including two maps, retaining addon order.
        new XDocument(new XElement("launcher",new XElement("iwad",doom2),new XElement("selected",doom2),
            new XElement("mod",addon),new XElement("mod",map1),new XElement("mod",map2))).Save(Path.Combine(Data.Home,"launcher.xml"));
        using(var f=new Launcher()) {
            var mods=Field<ListBox>(f,"mods");var maps=Field<ListBox>(f,"maps");var tabs=Field<TabControl>(f,"loadoutTabs");
            var games=Field<ComboBox>(f,"games");var launchArguments=Field<TextBox>(f,"launchArguments");
            Check(Data.StartupEngineArguments(new[]{"-goredebug","-record","my demo"})=="-goredebug -record \"my demo\"",
                "startup arguments preserve flags and quoted values");
            Check(launchArguments.Text==""&&launchArguments.Parent.ClientRectangle.Contains(launchArguments.Bounds),
                "optional engine arguments fit the launcher footer");
            Check(games.DropDownWidth==games.Width,"IWAD list is the same width as its field");
            f.Size=f.MinimumSize;f.PerformLayout();
            Check(games.DropDownWidth==games.Width&&launchArguments.Parent.ClientRectangle.Contains(launchArguments.Bounds),
                "IWAD list and engine arguments still fit at minimum size");
            Check(tabs.TabPages.Count==2&&tabs.TabPages[0].Text=="ADD-ON LOADOUT"&&tabs.TabPages[1].Text=="MAP WADS","separate loadout and map tabs");
            Check(mods.Items.Count==1&&(string)mods.Items[0]==addon&&maps.Items.Count==2,"legacy mixed loadout migrates to independent lists");
            maps.SelectedItem=map1;
            Check(Data.Loadout(maps.SelectedItem as string,mods.Items.Cast<string>()).SequenceEqual(new[]{map1,addon}),"only chosen map loads, followed by addon order");
            maps.SelectedItem=map2;
            Check(mods.Items.Count==1&&(string)mods.Items[0]==addon,"switching maps preserves addon loadout");
            Call(f,"Save");
            using(var restored=new Launcher()) {
                Check((string)Field<ListBox>(restored,"maps").SelectedItem==map2,"selected map persists");
                Check(Field<ListBox>(restored,"mods").Items.Count==1&&Field<ListBox>(restored,"maps").Items.Count==2,"map library and addons persist separately");
            }
            maps.ClearSelected();Call(f,"Save");
            using(var restored=new Launcher())Check(Field<ListBox>(restored,"maps").SelectedIndex==-1&&Field<ListBox>(restored,"mods").Items.Count==1,"base maps choice persists without losing addons");
            using(var input=File.OpenRead(Path.Combine(Data.Home,"launcher.xml")))Check(SettingsBackup.ReadLauncher(input).Root.Elements("map").Count()==2,"backup reader accepts map library and selected map");
            Call(f,"SetLaunching",true);Check(!maps.Enabled&&!Field<ModernButton>(f,"addMaps").Enabled,"map controls disabled during game launch");
            Call(f,"SetLaunching",false);Check(maps.Enabled,"map controls restored after launch");
            // Create handles offscreen, with no screenshots or background IWAD scan.
            f.ShowInTaskbar=false;f.StartPosition=FormStartPosition.Manual;f.Location=new System.Drawing.Point(-20000,-20000);
            f.Show();
            var scan=Field<System.Threading.CancellationTokenSource>(f,"scanCancellation");if(scan!=null)scan.Cancel();
            Application.DoEvents();
            foreach(var size in new[]{new System.Drawing.Size(1150,830),new System.Drawing.Size(940,770)}) {
                f.Size=size;f.PerformLayout();Application.DoEvents();
                foreach(int tab in new[]{0,1}) {
                    tabs.SelectedIndex=tab;tabs.PerformLayout();Application.DoEvents();Call(f,tab==0?"LayoutMods":"LayoutMaps");
                    var card=Field<Surface>(f,tab==0?"modsCard":"mapsCard");
                    foreach(var button in card.Controls.OfType<ModernButton>())Check(card.ClientRectangle.Contains(button.Bounds)&&
                        !card.Controls.OfType<Label>().Any(label=>label.Visible&&label.Bounds.IntersectsWith(button.Bounds)),
                        "tab controls fit without overlapping labels at "+size.Width+" / "+button.Text);
                }
            }
            f.Close();
        }
        using(var mascot=new WadMascot()) {
            mascot.Load(doom);Check(mascot.Animated&&mascot.ActorName=="Baron of Hell","Doom Baron frames decode");
            mascot.Load(doom2);Check(mascot.Animated&&mascot.ActorName=="Arch-vile","Doom II Arch-vile frames decode");
            foreach(string name in new[]{"TNT.wad","PLUTONIA.wad"}) {
                string final=Path.Combine(root,name);File.Copy(doom2,final);
                mascot.Load(final);Check(mascot.Animated&&mascot.ActorName=="Baron of Hell",name+" launcher preference uses available Baron artwork");
            }
            mascot.Load(null);Check(!mascot.Animated&&mascot.ActorName=="","missing IWAD clears previous artwork");
        }
        Console.WriteLine("PASS "+checks+" launcher loadout and mascot checks");
    }
}
