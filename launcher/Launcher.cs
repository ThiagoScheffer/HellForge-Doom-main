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
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;
using System.Xml.Linq;
using System.Xml;
using System.Runtime.InteropServices;

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
    // Extract embedded resources with bounded buffers instead of duplicating the
    // entire engine and PK3 archives in managed memory. The content-addressed
    // path retains the original layout expected by the build and game engine.
    static bool HashMatches(string path,string expected) {
        try {
            using(var f=File.OpenRead(path))using(var sha=SHA256.Create())
                return BitConverter.ToString(sha.ComputeHash(f)).Replace("-","")==expected;
        } catch(IOException) {return false;}
    }
    static string ExtractResource(string name,string category) {
        var assembly=Assembly.GetExecutingAssembly();
        string hash;
        using(var input=assembly.GetManifestResourceStream(name)) {
            if(input==null)throw new IOException("Missing bundled resource: "+name);
            using(var sha=SHA256.Create())
                hash=BitConverter.ToString(sha.ComputeHash(input)).Replace("-","");
        }
        string folder=Path.Combine(Home,category,hash);
        Directory.CreateDirectory(folder);
        string target=Path.Combine(folder,name);
        if(HashMatches(target,hash))return target;
        string temporary=target+"."+Guid.NewGuid().ToString("N")+".tmp";
        try {
            using(var input=assembly.GetManifestResourceStream(name)) {
                if(input==null)throw new IOException("Missing bundled resource: "+name);
                using(var output=new FileStream(temporary,FileMode.CreateNew,FileAccess.Write,FileShare.None,81920))
                    input.CopyTo(output,81920);
            }
            if(!HashMatches(temporary,hash))throw new InvalidDataException("Embedded resource failed verification: "+name);
            if(File.Exists(target))File.Delete(target);
            File.Move(temporary,target);
            return target;
        } finally {
            try {if(File.Exists(temporary))File.Delete(temporary);}catch(IOException){}
        }
    }
    internal static string BundledResource(string name) {return ExtractResource(name,"bundled");}
    internal static string BundledArguments(string game,string[] files) {
        var bundled=new List<string>{BundledResource("FluidWeapons.pk3"),BundledResource("FluidWeapons-SFX.pk3"),BundledResource("FluidMonsters.pk3")};
        bundled.AddRange(files.Where(f=>FluidPack(f)==0));
        return Arguments(game,bundled.ToArray());
    }
    internal static string Engine() {
        string engine=ExtractResource("doom.exe","engine");
        string directory=Path.GetDirectoryName(engine);
        string materials=BundledResource("DiggingMaterials.wad");
        string target=Path.Combine(directory,"DiggingMaterials.wad");
        // This small gameplay WAD is refreshed just as in the original launcher.
        File.Copy(materials,target,true);
        return engine;
    }
    internal static List<string> Scan(IEnumerable<string> roots,CancellationToken cancel) {
        var found=new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        var visited=new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        var queue=new Queue<Tuple<string,int>>();
        foreach(var r in roots) if(!String.IsNullOrWhiteSpace(r) && Directory.Exists(r)) queue.Enqueue(Tuple.Create(r,0));
        while(queue.Count>0 && visited.Count<15000 && !cancel.IsCancellationRequested) {
            var d=queue.Dequeue();
            if(!visited.Add(d.Item1)) continue;
            try {
                if((File.GetAttributes(d.Item1)&FileAttributes.ReparsePoint)!=0) continue;
                foreach(var file in Directory.EnumerateFiles(d.Item1,"*.wad")) {
                    if(cancel.IsCancellationRequested)break;
                    if(Iwad(file))found.Add(Path.GetFullPath(file));
                }
                if(d.Item2<8 && !cancel.IsCancellationRequested)
                    foreach(var child in Directory.EnumerateDirectories(d.Item1)) {
                        if(cancel.IsCancellationRequested)break;
                        queue.Enqueue(Tuple.Create(child,d.Item2+1));
                    }
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
            if(document.Root==null || document.Root.Name!="launcher" || document.Root.Elements().Any(e=>e.Name!="iwad"&&e.Name!="selected"&&e.Name!="mod"&&e.Name!="performance"))
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

// Visual and startup layer. Compatible with the existing .NET Framework 4.x build.
internal static class GraphicsPerformance {
    // Only graphical effects are overridden; gameplay, controls, audio and IWAD
    // selection remain as configured by the player. The original file is never edited.
    private static readonly Dictionary<string,string> LowFx = new Dictionary<string,string>(StringComparer.OrdinalIgnoreCase) {
        {"msaa","0"}, {"scale","85"}, {"soft_effects","0"}, {"surface_detail","0"},
        {"varied_highlights","0"}, {"uneven_tiles","0"}, {"uneven_walls","0"},
        {"weapon_lighting","0"}, {"glass_reflections","0"}, {"emissive","0"},
        {"glossy_screens","0"}, {"glowing_eyes","0"}, {"blood_shine","0"},
        {"baked_lights","0"}, {"window_light","0"}, {"dim_lit_rooms","0"},
        {"sun_shadows","0"}, {"bounce_light","0"}, {"sky_light","0"},
        {"baked_occlusion","0"}, {"decoration_shadows","0"}, {"texel_lighting","0"},
        {"soft_light","0"}, {"light_flow","0"}, {"door_light","0"},
        {"moving_relight","0"}, {"eye_adaptation","0"}, {"flashlight_shadows","0"},
        {"player_shadow","0"}, {"stippled_shadows","0"}, {"fog","0"},
        {"sun_shafts","0"}, {"sun_scatter","0"}, {"sun_disc","0"},
        {"dust_motes","0"}, {"embers","0"}, {"heat_haze","0"},
        {"reflections","0"}, {"retro_reflections","0"}, {"caustics","0"},
        {"ceiling_caustics","0"}, {"caustics_computed","0"},
        {"caustics_grow","0"}, {"caustics_angle","0"}, {"caustics_sway","0"},
        {"caustics_sprites","0"}, {"caustics_shots","0"}, {"shore_ripples","0"},
        {"glow_edges","0"}, {"damp_shores","0"}, {"splashes","0"},
        {"worn_plates","0"}, {"window_streaks","0"}, {"weather","0"},
        {"fluid_hot_smoke","0"}, {"monster_metal_sparks","0"}, {"door_sparks","0"},
        {"unoccluded_surface_lights","0"}, {"grid_sprite_light","0"}
    };

    internal static string CreateTemporaryConfig(string original) {
        var lines = File.ReadAllLines(original);
        var seen = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        int originalScale = 100;
        foreach(string line in lines) {
            var trimmed=line.Trim();
            if(trimmed.StartsWith("#") || trimmed.Length==0) continue;
            var parts=trimmed.Split((char[])null,StringSplitOptions.RemoveEmptyEntries);
            if(parts.Length>1 && parts[0].Equals("scale",StringComparison.OrdinalIgnoreCase)) {
                int candidate;
                if(int.TryParse(parts[1],out candidate) && candidate>=25 && candidate<=200)originalScale=candidate;
            }
        }
        var output = new List<string>(lines.Length+LowFx.Count+2);
        output.Add("# Temporary low-FX configuration; the original graphics.cfg is not changed.");
        foreach(string line in lines) {
            var trimmed=line.Trim();
            if(trimmed.Length==0 || trimmed.StartsWith("#")) {output.Add(line);continue;}
            var parts=trimmed.Split((char[])null,StringSplitOptions.RemoveEmptyEntries);
            string value;
            if(parts.Length>0 && LowFx.TryGetValue(parts[0],out value)) {
                // Never raise a render scale that the user has already reduced.
                if(parts[0].Equals("scale",StringComparison.OrdinalIgnoreCase))value=Math.Min(originalScale,85).ToString();
                if(seen.Add(parts[0])) output.Add(parts[0]+" "+value);
            } else output.Add(line);
        }
        foreach(var entry in LowFx)if(seen.Add(entry.Key))
            output.Add(entry.Key+" "+(entry.Key=="scale"?Math.Min(originalScale,85).ToString():entry.Value));
        Directory.CreateDirectory(Data.Home);
        string path=Path.Combine(Data.Home,"graphics-lowfx-"+Guid.NewGuid().ToString("N")+".cfg");
        File.WriteAllLines(path,output.ToArray(),new UTF8Encoding(false));
        return path;
    }
}

internal static class Palette {
    internal static readonly Color Background=Color.FromArgb(9,15,13);
    internal static readonly Color Card=Color.FromArgb(18,27,23);
    internal static readonly Color Input=Color.FromArgb(11,19,16);
    internal static readonly Color Border=Color.FromArgb(41,58,47);
    internal static readonly Color Text=Color.FromArgb(231,240,232);
    internal static readonly Color Subtle=Color.FromArgb(137,162,143);
    internal static readonly Color Accent=Color.FromArgb(169,237,107);
    internal static readonly Color AccentDark=Color.FromArgb(25,46,30);
}

internal sealed class Surface : Panel {
    internal Surface() {
        DoubleBuffered=true;
        BackColor=Palette.Card;
        Margin=new Padding(0);
    }
    protected override void OnPaint(PaintEventArgs e) {
        base.OnPaint(e);
        using(var border=new Pen(Palette.Border))e.Graphics.DrawRectangle(border,0,0,Width-1,Height-1);
    }
}

internal sealed class ModernButton : Button {
    private bool hovered;
    internal bool Primary {get;set;}
    internal bool Quiet {get;set;}
    internal ModernButton(string text) {
        Text=text; TabStop=true; Cursor=Cursors.Hand;
        FlatStyle=FlatStyle.Flat; FlatAppearance.BorderSize=0;
        SetStyle(ControlStyles.UserPaint|ControlStyles.AllPaintingInWmPaint|ControlStyles.OptimizedDoubleBuffer|ControlStyles.ResizeRedraw,true);
        Font=new Font("Segoe UI Semibold",9.5f,FontStyle.Bold);
        Size=new Size(128,36);
        BackColor=Palette.Card; ForeColor=Palette.Text;
    }
    static System.Drawing.Drawing2D.GraphicsPath Round(Rectangle r,int radius) {
        var p=new System.Drawing.Drawing2D.GraphicsPath();
        int d=Math.Min(Math.Min(r.Width,r.Height),radius*2);
        if(d<2){p.AddRectangle(r);return p;}
        p.AddArc(r.Left,r.Top,d,d,180,90);p.AddArc(r.Right-d,r.Top,d,d,270,90);
        p.AddArc(r.Right-d,r.Bottom-d,d,d,0,90);p.AddArc(r.Left,r.Bottom-d,d,d,90,90);
        p.CloseFigure();return p;
    }
    protected override void OnPaint(PaintEventArgs e) {
        var g=e.Graphics;g.SmoothingMode=System.Drawing.Drawing2D.SmoothingMode.AntiAlias;
        var r=new Rectangle(1,1,Math.Max(1,Width-3),Math.Max(1,Height-3));
        Color bg=Primary?(hovered?Color.FromArgb(197,249,143):Palette.Accent):
            Quiet?(hovered?Color.FromArgb(32,53,39):Palette.Card):
            (hovered?Color.FromArgb(39,63,45):Palette.AccentDark);
        Color fg=Primary?Palette.Background:Enabled?Palette.Text:Palette.Subtle;
        if(!Enabled)bg=Color.FromArgb(27,36,30);
        using(var shape=Round(r,9))
        using(var fill=new SolidBrush(bg))
        using(var line=new Pen(Primary?bg:Palette.Border)) {
            g.FillPath(fill,shape);g.DrawPath(line,shape);
        }
        TextRenderer.DrawText(g,Text,Font,r,fg,TextFormatFlags.HorizontalCenter|TextFormatFlags.VerticalCenter|TextFormatFlags.EndEllipsis|TextFormatFlags.NoPrefix);
        if(Focused && ShowFocusCues)using(var pen=new Pen(Palette.Accent))g.DrawRectangle(pen,4,4,Width-9,Height-9);
    }
    protected override void OnMouseEnter(EventArgs e){hovered=true;Invalidate();base.OnMouseEnter(e);}
    protected override void OnMouseLeave(EventArgs e){hovered=false;Invalidate();base.OnMouseLeave(e);}
    protected override void OnEnabledChanged(EventArgs e){Invalidate();base.OnEnabledChanged(e);}
    protected override void OnGotFocus(EventArgs e){Invalidate();base.OnGotFocus(e);}
    protected override void OnLostFocus(EventArgs e){Invalidate();base.OnLostFocus(e);}
}

internal sealed class ParallaxHero : Control {
    private Bitmap image;
    private Bitmap overlay;
    private readonly System.Windows.Forms.Timer animation;
    private readonly Font heading=new Font("Segoe UI",33f,FontStyle.Bold,GraphicsUnit.Point);
    private readonly Font eyebrow=new Font("Segoe UI Semibold",9f,FontStyle.Bold,GraphicsUnit.Point);
    private readonly Font subtitle=new Font("Segoe UI",10.5f,FontStyle.Regular,GraphicsUnit.Point);
    private readonly Font mono=new Font("Consolas",9f,FontStyle.Regular,GraphicsUnit.Point);
    private readonly string versionMark="BUILD  "+Data.BuildVersion.Split('|')[0].Trim();
    private float x,y,targetX,targetY;
    private bool sleeping;

    internal ParallaxHero() {
        SetStyle(ControlStyles.UserPaint|ControlStyles.AllPaintingInWmPaint|ControlStyles.OptimizedDoubleBuffer|ControlStyles.ResizeRedraw,true);
        TabStop=false;Cursor=Cursors.Default;BackColor=Palette.Background;
        animation=new System.Windows.Forms.Timer { Interval=33 };
        animation.Tick+=(s,e)=>Step();
        LoadArt();
    }
    void LoadArt() {
        if(image!=null)return;
        using(var stream=Assembly.GetExecutingAssembly().GetManifestResourceStream("hero.png"))
            if(stream!=null)using(var loaded=Image.FromStream(stream))image=new Bitmap(loaded);
        BuildOverlay();
    }
    void BuildOverlay() {
        if(overlay!=null){overlay.Dispose();overlay=null;}
        if(sleeping || Width<1 || Height<1)return;
        overlay=new Bitmap(Width,Height,System.Drawing.Imaging.PixelFormat.Format32bppPArgb);
        using(var g=Graphics.FromImage(overlay)) {
            using(var brush=new System.Drawing.Drawing2D.LinearGradientBrush(
                new Rectangle(0,0,Width,Height),Color.FromArgb(252,7,17,12),Color.FromArgb(44,7,17,12),0f))
                g.FillRectangle(brush,0,0,Width,Height);
            using(var brush=new System.Drawing.Drawing2D.LinearGradientBrush(
                new Rectangle(0,0,Width,Height),Color.FromArgb(0,7,17,12),Color.FromArgb(136,7,17,12),90f))
                g.FillRectangle(brush,0,0,Width,Height);
        }
    }
    protected override void OnResize(EventArgs e){base.OnResize(e);BuildOverlay();Invalidate();}
    protected override void OnMouseMove(MouseEventArgs e) {
        base.OnMouseMove(e);
        if(sleeping || Width<=0 || Height<=0)return;
        targetX=(e.X/(float)Width-0.5f)*-28f;
        targetY=(e.Y/(float)Height-0.5f)*-15f;
        if(!animation.Enabled)animation.Start();
    }
    protected override void OnMouseLeave(EventArgs e) {
        targetX=targetY=0;
        if(!sleeping && !animation.Enabled)animation.Start();
        base.OnMouseLeave(e);
    }
    void Step() {
        if(sleeping || !Visible || FindForm()==null || FindForm().WindowState==FormWindowState.Minimized) {
            animation.Stop();return;
        }
        x+=(targetX-x)*0.22f;y+=(targetY-y)*0.22f;
        if(Math.Abs(targetX-x)<0.15f && Math.Abs(targetY-y)<0.15f) {
            x=targetX;y=targetY;animation.Stop();
        }
        Invalidate();
    }
    protected override void OnPaint(PaintEventArgs e) {
        var g=e.Graphics;g.Clear(Palette.Background);
        if(image!=null && Width>0 && Height>0) {
            const int bleed=38;
            double scale=Math.Max((Width+bleed*2)/(double)image.Width,(Height+bleed*2)/(double)image.Height);
            int w=(int)Math.Ceiling(image.Width*scale),h=(int)Math.Ceiling(image.Height*scale);
            var dest=new Rectangle((Width-w)/2+(int)Math.Round(x),(Height-h)/2+(int)Math.Round(y),w,h);
            g.InterpolationMode=System.Drawing.Drawing2D.InterpolationMode.Bilinear;
            g.DrawImage(image,dest);
        }
        if(overlay!=null)g.DrawImageUnscaled(overlay,0,0);
        using(var accent=new SolidBrush(Palette.Accent))
        using(var pale=new SolidBrush(Palette.Text))
        using(var muted=new SolidBrush(Palette.Subtle)) {
            g.FillRectangle(accent,33,32,34,3);
            g.DrawString("THE MODERN CLASSIC DOOM EXPERIENCE",eyebrow,accent,33,48);
            g.DrawString("TUIN DOOM",heading,pale,29,75);
            g.DrawString("ENHANCED",heading,pale,29,125);
            g.DrawString("SOLID GEOMETRY   /   DESTRUCTION BUILD",subtitle,muted,33,197);
            if(Width>790) {
                var size=g.MeasureString(versionMark,mono);
                g.DrawString(versionMark,mono,muted,Math.Max(530,Width-(int)size.Width-27),20);
            }
        }
        using(var border=new Pen(Palette.Border))g.DrawRectangle(border,0,0,Width-1,Height-1);
    }
    // Called as soon as the child game process successfully launches.
    // The WinForms UI remains in memory, but bitmap/GDI overlay + all animation stop.
    internal void EnterGameMode() {
        sleeping=true;animation.Stop();x=y=targetX=targetY=0;
        if(image!=null){image.Dispose();image=null;}
        if(overlay!=null){overlay.Dispose();overlay=null;}
        Invalidate();
    }
    internal void LeaveGameMode() {
        sleeping=false;LoadArt();Invalidate();
    }
    protected override void Dispose(bool disposing) {
        if(disposing){animation.Stop();animation.Dispose();if(image!=null)image.Dispose();if(overlay!=null)overlay.Dispose();heading.Dispose();eyebrow.Dispose();subtitle.Dispose();mono.Dispose();}
        base.Dispose(disposing);
    }
}

internal sealed class Launcher : Form {
    private readonly ComboBox games=new ComboBox();
    private readonly ListBox mods=new ListBox();
    private readonly Label status=new Label();
    private readonly Label libraryInfo=new Label();
    private readonly Label modHint=new Label();
    private readonly Label emptyMods=new Label();
    private readonly CheckBox lowFx=new CheckBox();
    private readonly ToolTip tips=new ToolTip();
    private readonly string settings=Path.Combine(Data.Home,"launcher.xml");
    private readonly Font sectionFont=new Font("Segoe UI Semibold",13f,FontStyle.Bold);
    private readonly Font tagFont=new Font("Segoe UI Semibold",9f,FontStyle.Bold);
    private readonly Font listFont=new Font("Segoe UI",9.5f);
    private readonly Font listMono=new Font("Consolas",9f);
    private Surface libraryCard,modsCard,footer;
    private ParallaxHero hero;
    private ModernButton scan,play,lan,browseGame,scanFolder,screenshots,addMods,removeMod,upMod,downMod,export,import;
    private bool scanning,launching,gameRunning;
    private CancellationTokenSource scanCancellation;

    [STAThread] static void Main() {
        Application.EnableVisualStyles();Application.SetCompatibleTextRenderingDefault(false);
        Application.ThreadException+=(s,e)=>MessageBox.Show(e.Exception.Message,"Tuin Doom Enhanced",MessageBoxButtons.OK,MessageBoxIcon.Error);
        Application.Run(new Launcher());
    }
    static Label TextLabel(string text,Font font,Color color) {
        return new Label {Text=text,Font=font,ForeColor=color,BackColor=Color.Transparent,AutoSize=false};
    }
    static ModernButton CreateButton(string text,Action click,bool primary=false,bool quiet=false) {
        var b=new ModernButton(text){Primary=primary,Quiet=quiet};b.Click+=(s,e)=>click();return b;
    }
    static void StyleCard(Surface card,Control parent) {card.Dock=DockStyle.Fill;parent.Controls.Add(card);}

    public Launcher() {
        Text="Tuin Doom Enhanced  •  "+Data.BuildVersion.Split('|')[0].Trim();
        ClientSize=new Size(1150,774);MinimumSize=new Size(940,710);
        StartPosition=FormStartPosition.CenterScreen;AutoScaleMode=AutoScaleMode.Dpi;
        Font=new Font("Segoe UI",9.5f);BackColor=Palette.Background;ForeColor=Palette.Text;
        SetStyle(ControlStyles.OptimizedDoubleBuffer,true);

        var root=new TableLayoutPanel {Dock=DockStyle.Fill,Padding=new Padding(18),BackColor=Palette.Background,ColumnCount=1,RowCount=3};
        root.RowStyles.Add(new RowStyle(SizeType.Absolute,252));
        root.RowStyles.Add(new RowStyle(SizeType.Percent,100));
        root.RowStyles.Add(new RowStyle(SizeType.Absolute,94));
        root.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,100));Controls.Add(root);
        hero=new ParallaxHero { Dock=DockStyle.Fill,Margin=new Padding(0,0,0,14) };root.Controls.Add(hero,0,0);
        var two=new TableLayoutPanel {Dock=DockStyle.Fill,Margin=Padding.Empty,ColumnCount=2,RowCount=1};
        two.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,44));
        two.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,56));
        two.RowStyles.Add(new RowStyle(SizeType.Percent,100));root.Controls.Add(two,0,1);
        libraryCard=new Surface {Dock=DockStyle.Fill,Margin=new Padding(0,0,7,0)};
        modsCard=new Surface {Dock=DockStyle.Fill,Margin=new Padding(7,0,0,0)};
        two.Controls.Add(libraryCard,0,0);two.Controls.Add(modsCard,1,0);
        footer=new Surface {Dock=DockStyle.Fill,Margin=new Padding(0,14,0,0)};root.Controls.Add(footer,0,2);

        BuildLibrary();BuildMods();BuildFooter();
        libraryCard.Resize+=(s,e)=>LayoutLibrary();
        modsCard.Resize+=(s,e)=>LayoutMods();
        footer.Resize+=(s,e)=>LayoutFooter();
        LayoutLibrary();LayoutMods();LayoutFooter();
        LoadSettings();UpdateInfo();
        FormClosing+=(s,e)=>Save();
        Shown+=(s,e)=>{TryDarkTitleBar();Scan(null);};
    }
    [DllImport("dwmapi.dll",PreserveSig=true)]
    static extern int DwmSetWindowAttribute(IntPtr window,int attribute,ref int value,int size);
    void TryDarkTitleBar() {
        try {
            int enabled=1;
            if(DwmSetWindowAttribute(Handle,20,ref enabled,sizeof(int))!=0)
                DwmSetWindowAttribute(Handle,19,ref enabled,sizeof(int));
        }catch(DllNotFoundException){}catch(EntryPointNotFoundException){}
    }
    void BuildLibrary() {
        libraryCard.Controls.Add(TextLabel("01  /  GAME LIBRARY",sectionFont,Palette.Text));
        libraryCard.Controls.Add(TextLabel("Choose the IWAD used as your base game.",Font,Palette.Subtle));
        libraryCard.Controls.Add(TextLabel("BASE GAME  /  IWAD",tagFont,Palette.Accent));
        games.DropDownStyle=ComboBoxStyle.DropDownList;games.FlatStyle=FlatStyle.Flat;
        games.Font=new Font("Segoe UI",10f);games.BackColor=Palette.Input;games.ForeColor=Palette.Text;
        games.DropDownWidth=900;games.SelectedIndexChanged+=(s,e)=>UpdateInfo();libraryCard.Controls.Add(games);
        browseGame=CreateButton("BROWSE FOR IWAD",BrowseGame);libraryCard.Controls.Add(browseGame);
        scan=CreateButton("SCAN LIBRARY",()=>Scan(null));libraryCard.Controls.Add(scan);
        scanFolder=CreateButton("SCAN FOLDER",()=>{using(var d=new FolderBrowserDialog()){d.Description="Scan this folder and its subfolders for Doom IWADs";if(d.ShowDialog(this)==DialogResult.OK)Scan(new[]{d.SelectedPath});}});libraryCard.Controls.Add(scanFolder);
        screenshots=CreateButton("OPEN SCREENSHOTS",()=>{
            try {Directory.CreateDirectory(Data.Screenshots);Process.Start(new ProcessStartInfo(Data.Screenshots){UseShellExecute=true});}
            catch(Exception e){MessageBox.Show(this,e.Message,"Cannot open screenshots",MessageBoxButtons.OK,MessageBoxIcon.Error);}
        },quiet:true);libraryCard.Controls.Add(screenshots);
        libraryInfo.Font=new Font("Segoe UI",9f);libraryInfo.ForeColor=Palette.Subtle;libraryInfo.BackColor=Color.Transparent;libraryInfo.AutoEllipsis=true;
        libraryCard.Controls.Add(libraryInfo);
    }
    void LayoutLibrary() {
        if(libraryCard==null)return;int w=libraryCard.ClientSize.Width;int inner=Math.Max(200,w-44);
        var labels=libraryCard.Controls.OfType<Label>().Where(c=>c!=libraryInfo).ToArray();
        if(labels.Length>=3){labels[0].SetBounds(22,20,inner,28);labels[1].SetBounds(22,54,inner,20);labels[2].SetBounds(22,100,inner,20);}
        games.SetBounds(22,126,inner,34);
        browseGame.SetBounds(22,172,inner,38);
        int gap=8,half=(inner-gap)/2;
        scan.SetBounds(22,220,half,36);scanFolder.SetBounds(22+half+gap,220,inner-half-gap,36);
        screenshots.SetBounds(22,268,inner,36);
        libraryInfo.SetBounds(22,Math.Max(315,libraryCard.ClientSize.Height-43),inner,28);
    }
    void BuildMods() {
        modsCard.Controls.Add(TextLabel("02  /  ADD-ON LOADOUT",sectionFont,Palette.Text));
        modsCard.Controls.Add(TextLabel("Drag & drop WAD / PK3 files. Order matters.",Font,Palette.Subtle));
        mods.DrawMode=DrawMode.OwnerDrawFixed;mods.ItemHeight=32;mods.IntegralHeight=false;
        mods.BorderStyle=BorderStyle.None;mods.HorizontalScrollbar=false;
        mods.BackColor=Palette.Input;mods.ForeColor=Palette.Text;
        mods.DrawItem+=DrawMod;mods.SelectedIndexChanged+=(s,e)=>{
            tips.SetToolTip(mods,mods.SelectedItem as string??"Drag WAD / PK3 files here");
            removeMod.Enabled=upMod.Enabled=downMod.Enabled=mods.SelectedIndex>=0;
        };
        mods.AllowDrop=true;mods.DragEnter+=OnDragFiles;mods.DragDrop+=OnDropFiles;
        modsCard.AllowDrop=true;modsCard.DragEnter+=OnDragFiles;modsCard.DragDrop+=OnDropFiles;
        modsCard.Controls.Add(mods);
        emptyMods.Text="DROP ADD-ONS HERE\n\nYour mods will appear in load order.";
        emptyMods.Font=tagFont;emptyMods.TextAlign=ContentAlignment.MiddleCenter;
        emptyMods.ForeColor=Palette.Subtle;emptyMods.BackColor=Palette.Input;
        emptyMods.AllowDrop=true;emptyMods.DragEnter+=OnDragFiles;emptyMods.DragDrop+=OnDropFiles;
        modsCard.Controls.Add(emptyMods);emptyMods.BringToFront();
        modHint.Text="Classic WADs and native supported packs. Script-only PK3s may require GZDoom.";
        modHint.Font=new Font("Segoe UI",8.75f);modHint.ForeColor=Palette.Subtle;modHint.BackColor=Color.Transparent;
        modsCard.Controls.Add(modHint);
        addMods=CreateButton("+ ADD FILES",()=>{using(var d=new OpenFileDialog {Filter="Doom add-ons (*.wad;*.pk3)|*.wad;*.pk3",Multiselect=true})if(d.ShowDialog(this)==DialogResult.OK)Add(d.FileNames);});
        removeMod=CreateButton("REMOVE",()=>{int i=mods.SelectedIndex;if(i>=0){mods.Items.RemoveAt(i);UpdateInfo();}},quiet:true);
        upMod=CreateButton("MOVE UP",()=>MoveMod(-1),quiet:true);
        downMod=CreateButton("MOVE DOWN",()=>MoveMod(1),quiet:true);
        modsCard.Controls.Add(addMods);modsCard.Controls.Add(removeMod);modsCard.Controls.Add(upMod);modsCard.Controls.Add(downMod);
        removeMod.Enabled=upMod.Enabled=downMod.Enabled=false;
    }
    void LayoutMods() {
        if(modsCard==null)return;int w=modsCard.ClientSize.Width,h=modsCard.ClientSize.Height;
        int inner=Math.Max(240,w-44);
        var labels=modsCard.Controls.OfType<Label>().Where(c=>c!=modHint).ToArray();
        if(labels.Length>=2){labels[0].SetBounds(22,20,inner,28);labels[1].SetBounds(22,54,inner,20);}
        mods.SetBounds(22,92,inner,Math.Max(80,h-218));
        emptyMods.SetBounds(mods.Left,mods.Top,mods.Width,mods.Height);
        modHint.SetBounds(22,Math.Max(185,h-111),inner,40);
        int btnY=h-54,gap=7;
        int unit=(inner-gap*3)/4;
        addMods.SetBounds(22,btnY,unit,36);
        removeMod.SetBounds(22+unit+gap,btnY,unit,36);
        upMod.SetBounds(22+(unit+gap)*2,btnY,unit,36);
        downMod.SetBounds(22+(unit+gap)*3,btnY,inner-(unit+gap)*3,36);
    }
    void DrawMod(object sender,DrawItemEventArgs e) {
        if(e.Index<0)return;
        bool selected=(e.State&DrawItemState.Selected)==DrawItemState.Selected;
        var g=e.Graphics;
        using(var fill=new SolidBrush(selected?Color.FromArgb(35,66,41):Palette.Input))g.FillRectangle(fill,e.Bounds);
        using(var line=new Pen(Color.FromArgb(31,47,38)))g.DrawLine(line,e.Bounds.Left,e.Bounds.Bottom-1,e.Bounds.Right,e.Bounds.Bottom-1);
        string path=(string)mods.Items[e.Index];
        string number=(e.Index+1).ToString("00");
        TextRenderer.DrawText(g,number,listMono,new Rectangle(e.Bounds.Left+10,e.Bounds.Top+2,32,e.Bounds.Height-2),
            selected?Palette.Accent:Palette.Subtle,TextFormatFlags.Left|TextFormatFlags.VerticalCenter);
        TextRenderer.DrawText(g,Path.GetFileName(path),listFont,new Rectangle(e.Bounds.Left+48,e.Bounds.Top+1,e.Bounds.Width-67,e.Bounds.Height-2),
            selected?Color.White:Palette.Text,TextFormatFlags.Left|TextFormatFlags.VerticalCenter|TextFormatFlags.EndEllipsis|TextFormatFlags.NoPrefix);
        if(selected)using(var accent=new SolidBrush(Palette.Accent))g.FillRectangle(accent,e.Bounds.Left,0+e.Bounds.Top,3,e.Bounds.Height);
        e.DrawFocusRectangle();
    }
    void OnDragFiles(object sender,DragEventArgs e) {
        e.Effect=e.Data.GetDataPresent(DataFormats.FileDrop)?DragDropEffects.Copy:DragDropEffects.None;
    }
    void OnDropFiles(object sender,DragEventArgs e) {
        var files=e.Data.GetData(DataFormats.FileDrop) as string[];if(files!=null)Add(files);
    }
    void BuildFooter() {
        status.Text="Ready to launch";status.Font=new Font("Segoe UI",9f);
        status.ForeColor=Palette.Subtle;status.AutoEllipsis=true;status.BackColor=Color.Transparent;
        footer.Controls.Add(status);
        export=CreateButton("EXPORT SETTINGS",ExportSettings,quiet:true);footer.Controls.Add(export);
        import=CreateButton("IMPORT SETTINGS",ImportSettings,quiet:true);footer.Controls.Add(import);
        lowFx.Text="LOW-FX GAME MODE";lowFx.Checked=true;lowFx.AutoSize=false;
        lowFx.FlatStyle=FlatStyle.Flat;lowFx.ForeColor=Palette.Accent;lowFx.BackColor=Palette.Card;
        lowFx.Font=tagFont;lowFx.Cursor=Cursors.Hand;footer.Controls.Add(lowFx);
        tips.SetToolTip(lowFx,"Default ON: launch with a temporary low-effects graphics.cfg and suspend launcher animation. Your normal graphics.cfg is preserved.");
        lan=CreateButton("LAN / CO-OP",PlayLan);footer.Controls.Add(lan);
        play=CreateButton("▶  PLAY GAME",Play,primary:true);footer.Controls.Add(play);
        AcceptButton=play;
    }
    void LayoutFooter() {
        if(footer==null)return;int w=footer.ClientSize.Width;
        status.SetBounds(18,7,Math.Max(260,w-490),22);
        export.SetBounds(18,38,138,30);import.SetBounds(166,38,138,30);
        lowFx.SetBounds(318,40,Math.Max(150,w-720),25);
        lan.SetBounds(w-380,14,142,50);play.SetBounds(w-226,14,208,50);
    }
    void UpdateInfo() {
        if(libraryInfo==null || IsDisposed)return;
        libraryInfo.Text=games.Items.Count+" base game(s) detected  •  "+mods.Items.Count+" custom add-on(s)";
        emptyMods.Visible=mods.Items.Count==0;
        if(modsCard!=null)mods.Invalidate();
    }
    void BrowseGame() {
        using(var d=new OpenFileDialog {Filter="Doom IWAD (*.wad)|*.wad"})if(d.ShowDialog(this)==DialogResult.OK){
            if(!Data.Iwad(d.FileName)){MessageBox.Show(this,"Choose a valid Doom / Doom II / Final Doom / Freedoom IWAD.","Invalid IWAD");return;}
            AddGame(d.FileName);games.SelectedItem=Path.GetFullPath(d.FileName);
        }
    }
    void AddGame(string path) {
        string p=Path.GetFullPath(path);
        if(!games.Items.Cast<string>().Contains(p,StringComparer.OrdinalIgnoreCase))games.Items.Add(p);
        if(games.SelectedIndex<0)games.SelectedIndex=0;
        UpdateInfo();
    }
    async void Scan(string[] roots) {
        if(scanning || launching || gameRunning)return;
        scanning=true;scan.Enabled=false;scanFolder.Enabled=false;status.Text="Searching for installed IWADs…";
        var cancellation=new CancellationTokenSource();scanCancellation=cancellation;
        try {
            if(roots==null){
                var list=new List<string> {AppDomain.CurrentDomain.BaseDirectory,Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments),Environment.GetFolderPath(Environment.SpecialFolder.DesktopDirectory),Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile),"Downloads"),@"C:\GZDoom",@"C:\Games",@"C:\GOG Games"};
                string env=Environment.GetEnvironmentVariable("DOOMWADDIR");if(env!=null)list.Add(env);
                env=Environment.GetEnvironmentVariable("DOOMWADPATH");if(env!=null)list.AddRange(env.Split(';'));
                list.Add(Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86),@"Steam\steamapps\common"));
                roots=list.ToArray();
            }
            var found=await Task.Run(()=>Data.Scan(roots,cancellation.Token));
            if(IsDisposed || Disposing || cancellation.IsCancellationRequested || launching || gameRunning)return;
            foreach(string path in found)AddGame(path);
            status.Text=games.Items.Count==0?"No IWAD detected. Use Browse or Scan Folder.":games.Items.Count+" IWAD(s) ready.";
        } catch(Exception e){if(!IsDisposed&&!Disposing)status.Text="Scan failed: "+e.Message;}
        finally{
            scanning=false;if(scanCancellation==cancellation)scanCancellation=null;
            cancellation.Dispose();
            if(!IsDisposed&&!Disposing){scan.Enabled=!launching;scanFolder.Enabled=!launching;UpdateInfo();}
        }
    }
    void Add(IEnumerable<string> files) {
        foreach(string p in files) {
            string ext=Path.GetExtension(p).ToLowerInvariant();
            if((ext!=".wad"&&ext!=".pk3")||!File.Exists(p))continue;
            if(Data.Iwad(p)){AddGame(p);games.SelectedItem=Path.GetFullPath(p);continue;}
            if(Data.FluidPack(p)!=0)continue; // Built-in; configured in game.
            string absolute=Path.GetFullPath(p);
            if(!mods.Items.Cast<string>().Contains(absolute,StringComparer.OrdinalIgnoreCase))mods.Items.Add(absolute);
        }
        UpdateInfo();
    }
    void MoveMod(int delta) {
        int i=mods.SelectedIndex,j=i+delta;
        if(i<0||j<0||j>=mods.Items.Count)return;
        object item=mods.Items[i];mods.Items.RemoveAt(i);mods.Items.Insert(j,item);mods.SelectedIndex=j;
    }
    void LoadSettings() {
        try {
            if(!File.Exists(settings))return;
            XElement root;using(var stream=File.OpenRead(settings))root=SettingsBackup.ReadLauncher(stream).Root;
            foreach(var entry in root.Elements("iwad"))if(File.Exists(entry.Value))AddGame(entry.Value);
            var selected=root.Element("selected");
            if(selected!=null&&games.Items.Contains(selected.Value))games.SelectedItem=selected.Value;
            Add(root.Elements("mod").Select(e=>e.Value));
            var performance=root.Element("performance");
            if(performance!=null)lowFx.Checked=performance.Value!="0";
        } catch {status.Text="Could not restore saved choices.";}
    }
    XDocument CurrentSettings() {
        return new XDocument(new XElement("launcher",
            games.Items.Cast<string>().Select(p=>new XElement("iwad",p)),
            new XElement("selected",games.SelectedItem??""),
            mods.Items.Cast<string>().Select(p=>new XElement("mod",p)),
            new XElement("performance",lowFx.Checked?"1":"0")));
    }
    void Save() {
        try {Directory.CreateDirectory(Data.Home);CurrentSettings().Save(settings);}
        catch(Exception e){if(!IsDisposed)status.Text="Settings not saved: "+e.Message;}
    }
    void ExportSettings() {
        using(var dialog=new SaveFileDialog {Title="Export settings backup",Filter="Settings backup (*.zip)|*.zip",DefaultExt="zip",FileName="Tuin-Doom-settings-"+DateTime.Now.ToString("yyyyMMdd-HHmmss")+".zip"}) {
            if(dialog.ShowDialog(this)!=DialogResult.OK)return;
            try{SettingsBackup.Export(dialog.FileName,CurrentSettings(),Data.Profile);status.Text="Settings backup exported.";}
            catch(Exception e){MessageBox.Show(this,e.Message,"Cannot export settings",MessageBoxButtons.OK,MessageBoxIcon.Error);}
        }
    }
    void ImportSettings() {
        if(scanning){MessageBox.Show(this,"Wait for the IWAD scan to finish, then import settings.","Scan in progress");return;}
        using(var dialog=new OpenFileDialog {Title="Import settings backup",Filter="Settings backup (*.zip)|*.zip"}){
            if(dialog.ShowDialog(this)!=DialogResult.OK)return;
            try{
                var files=SettingsBackup.Read(dialog.FileName);
                if(MessageBox.Show(this,"Replace launcher choices and the game settings included in this backup?\n\nA recovery backup will be saved first. IWADs, add-ons and saves are not included; missing file paths will be skipped.","Import settings",MessageBoxButtons.OKCancel,MessageBoxIcon.Question)!=DialogResult.OK)return;
                string folder=Path.Combine(Data.Home,"backups");Directory.CreateDirectory(folder);
                string recovery=Path.Combine(folder,"before-import-"+DateTime.Now.ToString("yyyyMMdd-HHmmss-fff")+"-"+Guid.NewGuid().ToString("N")+".zip");
                SettingsBackup.Export(recovery,CurrentSettings(),Data.Profile);
                SettingsBackup.Restore(files,settings,Data.Profile);
                games.Items.Clear();mods.Items.Clear();lowFx.Checked=true;LoadSettings();UpdateInfo();
                status.Text="Settings imported.";
                MessageBox.Show(this,"Settings imported. Missing IWAD and add-on paths were skipped.\n\nRecovery backup:\n"+recovery,"Settings restored",MessageBoxButtons.OK,MessageBoxIcon.Information);
            }catch(Exception e){MessageBox.Show(this,e.Message,"Cannot import settings",MessageBoxButtons.OK,MessageBoxIcon.Error);}
        }
    }
    void SetLaunching(bool value) {
        launching=value;
        foreach(Control control in new Control[]{games,mods,browseGame,scan,scanFolder,screenshots,addMods,removeMod,upMod,downMod,export,import,lowFx,lan,play})
            control.Enabled=!value;
        if(!value){scan.Enabled=!scanning;scanFolder.Enabled=!scanning;removeMod.Enabled=upMod.Enabled=downMod.Enabled=mods.SelectedIndex>=0;}
    }
    void Play(){PlaySession(false);}
    void PlayLan(){PlaySession(true);}
    async void PlaySession(bool multiplayer) {
        if(launching||gameRunning)return;
        if(games.SelectedItem==null){BrowseGame();return;}
        SetLaunching(true);
        if(scanCancellation!=null)scanCancellation.Cancel();
        string temporaryGraphics=null;
        bool enteredGameMode=false;
        try {
            string game=(string)games.SelectedItem;
            if(!Data.Iwad(game))throw new IOException("The selected IWAD is missing or invalid.");
            if(mods.Items.Count>18)throw new IOException("Choose at most 18 add-ons.");
            string[] files=mods.Items.Cast<string>().ToArray();
            bool performance=lowFx.Checked;
            status.Text="Preparing game resources…";
            string args=await Task.Run(()=>Data.BundledArguments(game,files));
            string engine=await Task.Run(()=>Data.Engine());
            if(IsDisposed||Disposing)return;
            status.Text="Verifying multiplayer compatibility…";
            string signature=await Task.Run(()=>{
                var packs=new[]{Data.BundledResource("FluidWeapons.pk3"),Data.BundledResource("FluidWeapons-SFX.pk3"),Data.BundledResource("FluidMonsters.pk3")};
                return LanMenu.Fingerprint(engine,game,packs.Concat(files.Where(f=>Data.FluidPack(f)==0)).ToArray());
            });
            if(IsDisposed||Disposing)return;
            if(multiplayer)args+=" -lanmenu";
            Directory.CreateDirectory(Data.Profile);
            string graphics=Path.Combine(Data.Profile,"graphics.cfg");
            if(!File.Exists(graphics))File.Copy(Data.BundledResource("graphics.cfg"),graphics);
            if(performance){temporaryGraphics=GraphicsPerformance.CreateTemporaryConfig(graphics);graphics=temporaryGraphics;}
            args+=" -graphics "+Data.Quote(graphics);
            Save();status.Text="Launching game…";
            var start=new ProcessStartInfo(engine,args){WorkingDirectory=Data.Home,UseShellExecute=false,CreateNoWindow=true,RedirectStandardOutput=true,RedirectStandardError=true};
            start.EnvironmentVariables["TUIN_SCREENSHOTS"]=Data.Screenshots;
            start.EnvironmentVariables["TUIN_LAN_SIGNATURE"]=signature;
            try{start.EnvironmentVariables["TUIN_LAN_ADDRESSES"]=String.Join(" / ",System.Net.Dns.GetHostAddresses(System.Net.Dns.GetHostName()).Where(a=>a.AddressFamily==System.Net.Sockets.AddressFamily.InterNetwork).Select(a=>a.ToString()));}catch(System.Net.Sockets.SocketException){}
            string logPath=Path.Combine(Data.Home,"last-game.log");
            using(var writer=new StreamWriter(new FileStream(logPath,FileMode.Create,FileAccess.Write,FileShare.Read,8192))) {
                var writeLock=new object();
                using(var process=new Process {StartInfo=start}) {
                    process.OutputDataReceived+=(s,e)=>{if(e.Data!=null)lock(writeLock)writer.WriteLine(e.Data);};
                    process.ErrorDataReceived+=(s,e)=>{if(e.Data!=null)lock(writeLock)writer.WriteLine("[stderr] "+e.Data);};
                    if(!process.Start())throw new IOException("Windows could not start the game engine.");
                    process.BeginOutputReadLine();process.BeginErrorReadLine();
                    gameRunning=true;enteredGameMode=true;
                    hero.EnterGameMode();Hide(); // Zero animation; embedded artwork is disposed.
                    int exitCode=await Task.Run(()=>{process.WaitForExit();return process.ExitCode;});
                    lock(writeLock)writer.Flush();
                    if(!IsDisposed&&!Disposing)status.Text=exitCode==0?"Game closed. Ready to play again.":"Game exited with code "+exitCode+". See last-game.log.";
                }
            }
        }catch(Exception e){
            if(!IsDisposed&&!Disposing){status.Text="Launch failed.";MessageBox.Show(this,e.Message,"Cannot start game",MessageBoxButtons.OK,MessageBoxIcon.Error);}
        }finally{
            if(temporaryGraphics!=null)try{File.Delete(temporaryGraphics);}catch(IOException){}catch(UnauthorizedAccessException){}
            gameRunning=false;
            if(!IsDisposed&&!Disposing){
                if(enteredGameMode){hero.LeaveGameMode();Show();WindowState=FormWindowState.Normal;Activate();}
                SetLaunching(false);UpdateInfo();
            }
        }
    }
    protected override void Dispose(bool disposing) {
        if(disposing){sectionFont.Dispose();tagFont.Dispose();listFont.Dispose();listMono.Dispose();tips.Dispose();}
        base.Dispose(disposing);
    }
}
