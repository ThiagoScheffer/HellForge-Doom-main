using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Reflection;
using System.Security.Cryptography;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using System.Xml.Linq;
using System.Xml;

internal static class Data {
    internal static readonly string Home = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "DERPSolidMapsLauncher");
    internal static readonly string Screenshots = Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"Screenshots");
    internal static readonly string Profile = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData),"Local Games","DERP Solid Maps");
    internal static string BuildVersion {
        get {
            var assembly=Assembly.GetExecutingAssembly();
            var info=(AssemblyInformationalVersionAttribute)Attribute.GetCustomAttribute(assembly,typeof(AssemblyInformationalVersionAttribute));
            return info==null?"Development build":info.InformationalVersion;
        }
    }
    internal static string Quote(string s) {
        var b=new StringBuilder("\"");int slashes=0;
        foreach(char c in s) {
            if(c=='\\') {slashes++;continue;}
            if(c=='\"') {b.Append('\\',slashes*2+1);b.Append(c);}
            else {b.Append('\\',slashes);b.Append(c);}
            slashes=0;
        }
        b.Append('\\',slashes*2);b.Append('"');return b.ToString();
    }
    internal static bool Iwad(string path) {
        try {
            using(var f=File.OpenRead(path)) using(var r=new BinaryReader(f)) {
                if(f.Length<12 || Encoding.ASCII.GetString(r.ReadBytes(4))!="IWAD") return false;
                int n=r.ReadInt32(), off=r.ReadInt32();
                if(n<1 || n>100000 || off<12 || (long)off+(long)n*16>f.Length) return false;
                f.Position=off; bool palette=false, map=false;
                for(int i=0;i<n;i++) { int p=r.ReadInt32(), size=r.ReadInt32(); string name=Encoding.ASCII.GetString(r.ReadBytes(8)).TrimEnd('\0');
                    if(p<0 || size<0 || (long)p+size>f.Length) return false;
                    palette |= name=="PLAYPAL";
                    map |= name=="MAP01" || name=="E1M1";
                }
                return palette && map;
            }
        } catch { return false; }
    }
    internal static string Pk3Issue(string path) {
        if(!Path.GetExtension(path).Equals(".pk3",StringComparison.OrdinalIgnoreCase)) return null;
        if(FluidPack(path)!=0||VoxelPack(path))return null;
        using(var zip=ZipFile.OpenRead(path)) {
            string[] features={"ZSCRIPT","DECORATE","GLDEFS","MAPINFO","ZMAPINFO","VOXELDEF","MODELDEF","TEXTURES","SNDINFO","SNDSEQ","ANIMDEFS","SBARINFO","MENUDEF","CVARINFO","LOADACS","GAMEINFO"};
            foreach(var e in zip.Entries) {
                if(features.Contains(Path.GetFileNameWithoutExtension(e.FullName).ToUpperInvariant()) || e.FullName.StartsWith("filter/",StringComparison.OrdinalIgnoreCase) || e.FullName.StartsWith("shaders/",StringComparison.OrdinalIgnoreCase))
                    return Path.GetFileName(path)+" requires "+e.FullName+".\n\nThis engine loads classic resources and WAD maps from PK3s. It does not implement ZScript, DECORATE, custom shaders or voxels. This mod needs UZDoom/GZDoom.";
            }
        }
        return null;
    }
    // These native animation ports target these exact released packs. Do not
    // silently accept another ZScript mod merely because it has a similar name.
    internal static int FluidPack(string path) {
        if(!Path.GetExtension(path).Equals(".pk3",StringComparison.OrdinalIgnoreCase))return 0;
        string hash;
        using(var sha=SHA256.Create())using(var f=File.OpenRead(path))hash=BitConverter.ToString(sha.ComputeHash(f)).Replace("-","");
        if(hash=="47EE9E7BE5D8118E39B601F5DA3BE0735E3029BAF2AF31CD5245C718BE315E52")return 1;
        if(hash=="6C4E6E6E2FB2CB8741DC434EAC1B4B695D6919E3A984228757C94B271B011D8B")return 2;
        if(hash=="E906D93E31D30CDB25DB6965E2EB8EA9F3A7D2D1AAF635BAB8231ABA80C259DC")return 3;
        return 0;
    }
    internal static bool VoxelPack(string path) {
        if(!Path.GetExtension(path).Equals(".pk3",StringComparison.OrdinalIgnoreCase))return false;
        using(var zip=ZipFile.OpenRead(path)) {
            var names=new HashSet<string>(zip.Entries.Select(e=>e.FullName.ToLowerInvariant()));
            return names.Contains("voxeldef.txt")&&names.Contains("voxels/possa.kvx")&&names.Contains("zscript/cheellovox/cheellomonstersdoom1.zc");
        }
    }
    internal static string Arguments(string game,string[] files) {
        var classic=new List<string>();var sounds=new List<string>();string fluid=null,voxels=null;bool monsters=false;
        foreach(string f in files){
            if(!File.Exists(f))throw new IOException("Missing add-on: "+f);
            if(VoxelPack(f)){if(voxels!=null)throw new IOException("Choose only one VoxelDoom pack.");voxels=f;continue;}
            int pack=FluidPack(f);
            if(pack==1){if(fluid!=null)throw new IOException("Choose only one Fluid Weapons main pack.");fluid=f;sounds.Add(f);}
            else if(pack==2){if(fluid==null)throw new IOException("Place Fluid Weapons 1.0-UZ above its sound add-on in the list.");sounds.Add(f);}
            else if(pack==3){if(monsters)throw new IOException("Choose only one FluidMonsters pack.");monsters=true;}
            else{string issue=Pk3Issue(f);if(issue!=null)throw new IOException(issue);
                if(WadHas(f,"E5M1")&&!WadHas(game,"E4M1"))throw new IOException("SIGIL requires the Ultimate Doom IWAD (DOOM.WAD). Select that IWAD, then keep SIGIL in the add-on list.");
                classic.Add(f);}
        }
        string args="-solidmaps -uzmovement -iwad "+Quote(game);
        if(voxels!=null)args+=" -voxeldoom "+Quote(voxels);
        if(monsters)args+=" -fluidmonsters";
        if(classic.Count>0)args+=" -file "+String.Join(" ",classic.Select(Quote));
        if(fluid!=null)args+=" -fluidweapons "+Quote(fluid)+" -modsounds "+String.Join(" ",sounds.Select(Quote))+" -fluid-sounds -weaponinterpolation";
        return args;
    }
    internal static bool WadHas(string path,string wanted) {
        if(!Path.GetExtension(path).Equals(".wad",StringComparison.OrdinalIgnoreCase))return false;
        using(var f=File.OpenRead(path))using(var r=new BinaryReader(f)){
            if(f.Length<12)return false;string magic=Encoding.ASCII.GetString(r.ReadBytes(4));if(magic!="IWAD"&&magic!="PWAD")return false;
            int count=r.ReadInt32(),offset=r.ReadInt32();if(count<0||count>100000||offset<12||(long)offset+count*16L>f.Length)return false;
            f.Position=offset;for(int n=0;n<count;++n){r.ReadInt32();r.ReadInt32();if(Encoding.ASCII.GetString(r.ReadBytes(8)).TrimEnd('\0').Equals(wanted,StringComparison.OrdinalIgnoreCase))return true;}
        }return false;
    }
    internal static string BundledResource(string name) {
        byte[] bytes;
        using(var input=Assembly.GetExecutingAssembly().GetManifestResourceStream(name)) {
            if(input==null)throw new IOException("Missing bundled mod: "+name);
            using(var memory=new MemoryStream()){input.CopyTo(memory);bytes=memory.ToArray();}
        }
        string hash;using(var sha=SHA256.Create())hash=BitConverter.ToString(sha.ComputeHash(bytes)).Replace("-","");
        string folder=Path.Combine(Home,"bundled",hash);Directory.CreateDirectory(folder);
        string path=Path.Combine(folder,name);bool valid=false;
        if(File.Exists(path))using(var sha=SHA256.Create())using(var input=File.OpenRead(path))valid=BitConverter.ToString(sha.ComputeHash(input)).Replace("-","")==hash;
        if(!valid)File.WriteAllBytes(path,bytes);
        return path;
    }
    internal static string BundledArguments(string game,string[] files) {
        var bundled=new List<string>{BundledResource("FluidWeapons.pk3"),BundledResource("FluidWeapons-SFX.pk3"),BundledResource("FluidMonsters.pk3")};
        bundled.AddRange(files.Where(f=>FluidPack(f)==0));
        return Arguments(game,bundled.ToArray());
    }
    internal static string Engine() {
        byte[] bytes;
        using(var r=Assembly.GetExecutingAssembly().GetManifestResourceStream("doom.exe")) {
            if(r==null) throw new IOException("The embedded engine is missing.");
            using(var m=new MemoryStream()) { r.CopyTo(m); bytes=m.ToArray(); }
        }
        string hash; using(var sha=SHA256.Create()) hash=BitConverter.ToString(sha.ComputeHash(bytes)).Replace("-","");
        string dir=Path.Combine(Home,"engine",hash); Directory.CreateDirectory(dir);
        string path=Path.Combine(dir,"doom.exe");
        bool valid=false;
        if(File.Exists(path)) using(var sha=SHA256.Create()) using(var f=File.OpenRead(path)) valid=BitConverter.ToString(sha.ComputeHash(f)).Replace("-","")==hash;
        if(!valid) File.WriteAllBytes(path,bytes);
        File.Copy(BundledResource("DiggingMaterials.wad"),Path.Combine(dir,"DiggingMaterials.wad"),true);
        return path;
    }
    internal static List<string> Scan(IEnumerable<string> roots) {
        var found=new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        var visited=new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        var queue=new Queue<Tuple<string,int>>();
        foreach(var r in roots) if(!String.IsNullOrWhiteSpace(r) && Directory.Exists(r)) queue.Enqueue(Tuple.Create(r,0));
        while(queue.Count>0 && visited.Count<15000) {
            var d=queue.Dequeue();
            if(!visited.Add(d.Item1)) continue;
            try {
                if((File.GetAttributes(d.Item1)&FileAttributes.ReparsePoint)!=0) continue;
                foreach(var file in Directory.EnumerateFiles(d.Item1,"*.wad")) if(Iwad(file)) found.Add(Path.GetFullPath(file));
                if(d.Item2<8) foreach(var child in Directory.EnumerateDirectories(d.Item1)) queue.Enqueue(Tuple.Create(child,d.Item2+1));
            } catch(UnauthorizedAccessException) {} catch(IOException) {}
        }
        return found.OrderBy(s=>s,StringComparer.OrdinalIgnoreCase).ToList();
    }
}

internal static class SettingsBackup {
    internal static readonly string[] ConfigFiles={"graphics.cfg","doom.cfg","heretic.cfg"};
    const int MaxFileSize=4*1024*1024;
    internal static XDocument ReadLauncher(Stream input) {
        using(var reader=XmlReader.Create(input,new XmlReaderSettings { DtdProcessing=DtdProcessing.Prohibit,XmlResolver=null,MaxCharactersInDocument=MaxFileSize })) {
            var document=XDocument.Load(reader);
            if(document.Root==null || document.Root.Name!="launcher" || document.Root.Elements().Any(e=>e.Name!="iwad"&&e.Name!="selected"&&e.Name!="mod"))
                throw new InvalidDataException("This file does not contain launcher settings.");
            return document;
        }
    }
    internal static void Export(string path,XDocument launcher,string profile) {
        // Build completely before opening the destination, so read failures leave it intact.
        using(var memory=new MemoryStream()) {
            using(var zip=new ZipArchive(memory,ZipArchiveMode.Create,true)) {
                using(var output=zip.CreateEntry("launcher.xml").Open())launcher.Save(output);
                foreach(string name in ConfigFiles) {
                    string source=Path.Combine(profile,name);
                    if(!File.Exists(source))continue;
                    if(new FileInfo(source).Length>MaxFileSize)throw new IOException("Settings file is too large: "+name);
                    using(var output=zip.CreateEntry(name).Open())using(var input=File.OpenRead(source))input.CopyTo(output);
                }
            }
            File.WriteAllBytes(path,memory.ToArray());
        }
    }
    internal static Dictionary<string,byte[]> Read(string path) {
        var files=new Dictionary<string,byte[]>(StringComparer.OrdinalIgnoreCase);
        using(var zip=ZipFile.OpenRead(path)) {
            if(zip.Entries.Count>ConfigFiles.Length+1)throw new InvalidDataException("Unexpected files in settings backup.");
            foreach(var entry in zip.Entries) {
                string name=entry.FullName;
                if((name!="launcher.xml"&&!ConfigFiles.Contains(name)) || files.ContainsKey(name) || entry.Length>MaxFileSize)
                    throw new InvalidDataException("Invalid settings backup entry: "+name);
                using(var input=entry.Open())using(var output=new MemoryStream()) {
                    var buffer=new byte[8192];int count;
                    while((count=input.Read(buffer,0,buffer.Length))>0) {
                        if(output.Length+count>MaxFileSize)throw new InvalidDataException("Settings file is too large.");
                        output.Write(buffer,0,count);
                    }
                    files.Add(name,output.ToArray());
                }
            }
        }
        if(!files.ContainsKey("launcher.xml"))throw new InvalidDataException("The backup is missing launcher.xml.");
        using(var input=new MemoryStream(files["launcher.xml"]))ReadLauncher(input);
        return files;
    }
    internal static void Restore(Dictionary<string,byte[]> files,string launcherPath,string profile) {
        var previous=new Dictionary<string,byte[]>();
        try {
            foreach(var file in files) {
                string target=file.Key=="launcher.xml"?launcherPath:Path.Combine(profile,file.Key);
                previous.Add(target,File.Exists(target)?File.ReadAllBytes(target):null);
                Directory.CreateDirectory(Path.GetDirectoryName(target));
                File.WriteAllBytes(target,file.Value);
            }
        } catch {
            foreach(var file in previous) {
                try { if(file.Value==null)File.Delete(file.Key);else File.WriteAllBytes(file.Key,file.Value); } catch(IOException) {} catch(UnauthorizedAccessException) {}
            }
            throw;
        }
    }
}

internal sealed class Hero : Panel {
    Image art;
    internal Hero() {
        DoubleBuffered=true;
        using(var stream=Assembly.GetExecutingAssembly().GetManifestResourceStream("hero.png"))
            if(stream!=null)using(var image=Image.FromStream(stream))art=new Bitmap(image);
    }
    protected override void OnPaint(PaintEventArgs e) {
        base.OnPaint(e); var g=e.Graphics;
        if(art!=null)g.DrawImage(art,new Rectangle(320,0,420,274));
        using(var fade=new System.Drawing.Drawing2D.LinearGradientBrush(ClientRectangle,Color.FromArgb(255,13,19,17),Color.FromArgb(25,13,19,17),0f))g.FillRectangle(fade,ClientRectangle);
        using(var title=new Font("Segoe UI",23,FontStyle.Bold))g.DrawString("TUIN DOOM ENHANCED",title,Brushes.White,20,38);
        using(var small=new Font("Segoe UI",10,FontStyle.Bold))
        using(var accent=new SolidBrush(Color.FromArgb(153,219,109)))g.DrawString("SOLID GEOMETRY - DESTRUCTION BUILD",small,accent,23,92);
        using(var version=new Font("Segoe UI",9))g.DrawString("Version "+Data.BuildVersion,version,Brushes.LightGray,23,124);
        using(var pen=new Pen(Color.FromArgb(106,157,75),2))g.DrawLine(pen,20,Height-2,Width-20,Height-2);
    }
    protected override void Dispose(bool disposing) {if(disposing&&art!=null){art.Dispose();art=null;}base.Dispose(disposing);}
}
internal sealed class Launcher : Form {
    ComboBox games=new ComboBox(); ListBox mods=new ListBox(); Label status=new Label(); Button scan,play;
    string settings=Path.Combine(Data.Home,"launcher.xml"); bool scanning;
    [STAThread] static void Main() {
        Application.EnableVisualStyles(); Application.SetCompatibleTextRenderingDefault(false);
        Application.ThreadException+=(s,e)=>MessageBox.Show(e.Exception.Message,"Tuin-Doom-Enhanced");
        Application.Run(new Launcher());
    }
    Button Button(string text, int x, int y, int width, Action action) {
        var b=new Button { Text=text,Left=x,Top=y,Width=width,Height=32,FlatStyle=FlatStyle.Flat,BackColor=Color.FromArgb(35,45,40),ForeColor=Color.FromArgb(224,234,226),Cursor=Cursors.Hand }; b.FlatAppearance.BorderColor=Color.FromArgb(68,85,72); b.FlatAppearance.MouseOverBackColor=Color.FromArgb(55,75,56); b.Click+=(s,e)=>action(); Controls.Add(b); return b;
    }
    void Label(string text,int x,int y,int width,int height) { Controls.Add(new Label { Text=text,Left=x,Top=y,Width=width,Height=height }); }
    public Launcher() {
        Text="Tuin-Doom-Enhanced - "+Data.BuildVersion; ClientSize=new Size(740,680); FormBorderStyle=FormBorderStyle.FixedSingle; MaximizeBox=false;
        StartPosition=FormStartPosition.CenterScreen; Font=new Font("Segoe UI",10); BackColor=Color.FromArgb(19,25,23); ForeColor=Color.FromArgb(219,228,220);

        Label("Base game (IWAD)",20,55,500,25);
        games.SetBounds(20,83,580,30); games.DropDownStyle=ComboBoxStyle.DropDownList; games.DropDownWidth=1000; Controls.Add(games);
        Button("Browse...",610,81,108,BrowseGame);
        scan=Button("Scan for IWADs",20,123,155,()=>Scan(null));
        Button("Scan folder...",185,123,140,()=>{using(var d=new FolderBrowserDialog()) { d.Description="Scan this folder and its subfolders for Doom IWADs"; if(d.ShowDialog()==DialogResult.OK) Scan(new[]{d.SelectedPath}); }});
        Button("Multiplayer",490,123,228,()=>PlayLan());
        Button("Screenshots",340,123,140,()=>{
            try { Directory.CreateDirectory(Data.Screenshots);Process.Start(new ProcessStartInfo(Data.Screenshots){UseShellExecute=true}); }
            catch(Exception e){MessageBox.Show(this,e.Message,"Cannot open screenshots");}
        });
        Label("Add-ons - loaded from top to bottom (later files override earlier files)",20,173,700,25);
        mods.SetBounds(20,204,580,149); mods.HorizontalScrollbar=true; mods.AllowDrop=true; Controls.Add(mods);
        mods.DragEnter+=(s,e)=>{ if(e.Data.GetDataPresent(DataFormats.FileDrop)) e.Effect=DragDropEffects.Copy; };
        mods.DragDrop+=(s,e)=>Add((string[])e.Data.GetData(DataFormats.FileDrop));
        Button("Add files...",610,204,108,()=>{using(var d=new OpenFileDialog { Filter="Doom add-ons (*.wad;*.pk3)|*.wad;*.pk3",Multiselect=true }) if(d.ShowDialog()==DialogResult.OK) Add(d.FileNames);});
        Button("Remove",610,243,108,()=>{int i=mods.SelectedIndex;if(i>=0)mods.Items.RemoveAt(i);});
        Button("Move up",610,282,108,()=>MoveMod(-1)); Button("Move down",610,321,108,()=>MoveMod(1));
        Label("VoxelDoom supported: models and native lighting. Other PK3 scripts are not supported.",20,363,700,45);
        status.SetBounds(20,421,520,35); status.Text="Ready"; Controls.Add(status);
        play=Button("Play",610,417,108,Play);
        foreach(Control control in Controls)control.Top+=140;
        Label("Settings backup - launcher choices, controls and graphics",20,609,700,25);
        Button("Export settings...",20,638,170,ExportSettings);
        Button("Import settings...",200,638,170,ImportSettings);
        games.BackColor=mods.BackColor=Color.FromArgb(28,36,31);
        games.ForeColor=mods.ForeColor=Color.FromArgb(224,234,226);
        mods.BorderStyle=BorderStyle.FixedSingle;
        status.ForeColor=Color.FromArgb(161,183,164);
        play.Text="PLAY DOOM"; play.Font=new Font(Font,FontStyle.Bold);
        play.BackColor=Color.FromArgb(116,173,77);play.ForeColor=Color.FromArgb(13,25,14);
        play.FlatAppearance.MouseOverBackColor=Color.FromArgb(149,202,108);
        Controls.Add(new Hero {Left=0,Top=0,Width=740,Height=180});
        LoadSettings(); FormClosing+=(s,e)=>Save(); Shown+=(s,e)=>Scan(null);
    }
    void BrowseGame() { using(var d=new OpenFileDialog { Filter="Doom IWAD (*.wad)|*.wad" }) if(d.ShowDialog()==DialogResult.OK) { if(!Data.Iwad(d.FileName)) { MessageBox.Show("Choose a valid Doom / Doom II / Final Doom / Freedoom IWAD."); return; } AddGame(d.FileName); games.SelectedItem=d.FileName; } }
    void AddGame(string p) { if(!games.Items.Cast<string>().Contains(p,StringComparer.OrdinalIgnoreCase))games.Items.Add(p); if(games.SelectedIndex<0)games.SelectedIndex=0; }
    async void Scan(string[] roots) {
        if(scanning)return; scanning=true;scan.Enabled=false;status.Text="Scanning for IWADs...";
        try {
            if(roots==null) {
                var list=new List<string> { AppDomain.CurrentDomain.BaseDirectory,Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments),Environment.GetFolderPath(Environment.SpecialFolder.DesktopDirectory),Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile),"Downloads"),@"C:\GZDoom",@"C:\Games",@"C:\GOG Games" };
                string env=Environment.GetEnvironmentVariable("DOOMWADDIR"); if(env!=null)list.Add(env);
                env=Environment.GetEnvironmentVariable("DOOMWADPATH"); if(env!=null)list.AddRange(env.Split(';'));
                string steam=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86),@"Steam\steamapps\common");list.Add(steam);
                roots=list.ToArray();
            }
            var found=await Task.Run(()=>Data.Scan(roots)); if(IsDisposed)return;
            foreach(var f in found)AddGame(f);
            status.Text=games.Items.Count==0?"No IWAD found. Use Browse or Scan folder.":games.Items.Count+" IWAD(s) available.";
        } catch(Exception e) { if(!IsDisposed)status.Text="Scan failed: "+e.Message; }
        finally { scanning=false;if(!IsDisposed)scan.Enabled=true; }
    }
    void Add(IEnumerable<string> files) {
        foreach(var p in files) {
            string ext=Path.GetExtension(p).ToLowerInvariant();
            if((ext!=".wad" && ext!=".pk3") || !File.Exists(p))continue;
            if(Data.Iwad(p)) { AddGame(p);games.SelectedItem=p;continue; }
            if(Data.FluidPack(p)!=0)continue; // Built in; configured in the in-game menu.
            if(!mods.Items.Cast<string>().Contains(p,StringComparer.OrdinalIgnoreCase))mods.Items.Add(Path.GetFullPath(p));
        }
    }
    void MoveMod(int delta) { int i=mods.SelectedIndex,j=i+delta; if(i<0 || j<0 || j>=mods.Items.Count)return;object o=mods.Items[i];mods.Items.RemoveAt(i);mods.Items.Insert(j,o);mods.SelectedIndex=j; }
    void LoadSettings() {
        try { if(!File.Exists(settings))return;XElement x;using(var input=File.OpenRead(settings))x=SettingsBackup.ReadLauncher(input).Root;
            foreach(var e in x.Elements("iwad"))if(File.Exists(e.Value))AddGame(e.Value);
            var selected=x.Element("selected");if(selected!=null && games.Items.Contains(selected.Value))games.SelectedItem=selected.Value;
            Add(x.Elements("mod").Select(e=>e.Value));
        } catch {status.Text="Could not restore saved choices.";}
    }
    XDocument CurrentSettings() { return new XDocument(new XElement("launcher",games.Items.Cast<string>().Select(p=>new XElement("iwad",p)),new XElement("selected",games.SelectedItem??""),mods.Items.Cast<string>().Select(p=>new XElement("mod",p)))); }
    void Save() { try { Directory.CreateDirectory(Data.Home);CurrentSettings().Save(settings); } catch(Exception e) { status.Text="Settings not saved: "+e.Message; } }
    void ExportSettings() {
        using(var dialog=new SaveFileDialog { Title="Export settings backup",Filter="Settings backup (*.zip)|*.zip",DefaultExt="zip",FileName="Tuin-Doom-settings-"+DateTime.Now.ToString("yyyyMMdd-HHmmss")+".zip" }) {
            if(dialog.ShowDialog(this)!=DialogResult.OK)return;
            try { SettingsBackup.Export(dialog.FileName,CurrentSettings(),Data.Profile);status.Text="Settings backup exported."; }
            catch(Exception e) { MessageBox.Show(this,e.Message,"Cannot export settings",MessageBoxButtons.OK,MessageBoxIcon.Error); }
        }
    }
    void ImportSettings() {
        if(scanning) { MessageBox.Show(this,"Wait for the IWAD scan to finish, then import settings.","Scan in progress");return; }
        using(var dialog=new OpenFileDialog { Title="Import settings backup",Filter="Settings backup (*.zip)|*.zip" }) {
            if(dialog.ShowDialog(this)!=DialogResult.OK)return;
            try {
                var files=SettingsBackup.Read(dialog.FileName);
                if(MessageBox.Show(this,"Replace launcher choices and the game settings included in this backup?\n\nA recovery backup will be saved first. IWADs, add-ons and saved games are not included; missing file paths will be skipped.","Import settings",MessageBoxButtons.OKCancel,MessageBoxIcon.Question)!=DialogResult.OK)return;
                string folder=Path.Combine(Data.Home,"backups");Directory.CreateDirectory(folder);
                string recovery=Path.Combine(folder,"before-import-"+DateTime.Now.ToString("yyyyMMdd-HHmmss-fff")+"-"+Guid.NewGuid().ToString("N")+".zip");
                SettingsBackup.Export(recovery,CurrentSettings(),Data.Profile);
                SettingsBackup.Restore(files,settings,Data.Profile);
                games.Items.Clear();mods.Items.Clear();LoadSettings();
                status.Text="Settings imported.";
                MessageBox.Show(this,"Settings imported. Missing IWAD and add-on paths were skipped.\n\nPrevious settings backed up to:\n"+recovery,"Settings restored",MessageBoxButtons.OK,MessageBoxIcon.Information);
            } catch(Exception e) { MessageBox.Show(this,e.Message,"Cannot import settings",MessageBoxButtons.OK,MessageBoxIcon.Error); }
        }
    }
    void Play(){PlaySession(false);}
    void PlayLan(){PlaySession(true);}
    async void PlaySession(bool multiplayer) {
        if(games.SelectedItem==null) { BrowseGame();return; }
        play.Enabled=false;
        foreach(Control control in Controls)control.Enabled=false;
        try {
            string game=(string)games.SelectedItem;
            if(!Data.Iwad(game))throw new IOException("The selected IWAD is missing or invalid.");
            if(mods.Items.Count>18)throw new IOException("Choose at most 18 add-ons.");
            var files=mods.Items.Cast<string>().ToArray();
            string args=await Task.Run(()=>Data.BundledArguments(game,files));
            string engine=await Task.Run(()=>Data.Engine());
            status.Text="Checking game files for LAN...";
            var packs=new[]{Data.BundledResource("FluidWeapons.pk3"),Data.BundledResource("FluidWeapons-SFX.pk3"),Data.BundledResource("FluidMonsters.pk3")};
            string signature=await Task.Run(()=>LanMenu.Fingerprint(engine,game,packs.Concat(files.Where(f=>Data.FluidPack(f)==0)).ToArray()));
            if(multiplayer)args+=" -lanmenu";
            string profile=Data.Profile;
            Directory.CreateDirectory(profile);
            string graphics=Path.Combine(profile,"graphics.cfg");
            if(!File.Exists(graphics))File.Copy(Data.BundledResource("graphics.cfg"),graphics);
            args+=" -graphics "+Data.Quote(graphics);
            Save();status.Text="Starting Tuin-Doom-Enhanced...";
            var start=new ProcessStartInfo(engine,args) { WorkingDirectory=Data.Home,UseShellExecute=false,CreateNoWindow=true,RedirectStandardOutput=true,RedirectStandardError=true };
            start.EnvironmentVariables["TUIN_SCREENSHOTS"]=Data.Screenshots;
            start.EnvironmentVariables["TUIN_LAN_SIGNATURE"]=signature;
            try { start.EnvironmentVariables["TUIN_LAN_ADDRESSES"]=String.Join(" / ",System.Net.Dns.GetHostAddresses(System.Net.Dns.GetHostName()).Where(a=>a.AddressFamily==System.Net.Sockets.AddressFamily.InterNetwork).Select(a=>a.ToString())); } catch(System.Net.Sockets.SocketException) { }
            using(var p=Process.Start(start)) {
                var stdout=p.StandardOutput.ReadToEndAsync();var stderr=p.StandardError.ReadToEndAsync();
                status.Text="Game running. Close the game to return here.";
                await Task.Run(()=>p.WaitForExit());
                File.WriteAllText(Path.Combine(Data.Home,"last-game.log"),await stdout+await stderr);
                status.Text=p.ExitCode==0?"Ready":"Game exited with an error. See last-game.log in launcher data.";
            }
        } catch(Exception e) { MessageBox.Show(this,e.Message,"Cannot start game",MessageBoxButtons.OK,MessageBoxIcon.Information);status.Text="Ready"; }
        finally {if(!IsDisposed){foreach(Control control in Controls)control.Enabled=true;play.Enabled=true;}}
    }
}


