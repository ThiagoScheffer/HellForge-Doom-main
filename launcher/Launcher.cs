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
#if HELLFORGE_EXPERIMENTAL
    internal const string ProductName = "DOOM: HELLFORGE — Experimental";
    internal const string ProductShortName = "DOOM-Hellforge-Experimental";
    internal const string BuildLabel = "DESTRUCTION  /  WEAK POINTS  /  SURVIVAL AI  /  ENCOUNTER DIRECTOR";
    internal static readonly string Home = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "DOOM-HellforgeLauncher-Experimental");
    internal static readonly string Profile = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData),"Local Games","DOOM Hellforge Experimental");
#else
    internal const string ProductName = "DOOM: HELLFORGE";
    internal const string ProductShortName = "DOOM-Hellforge";
    internal const string BuildLabel = "DESTRUCTION  /  WEAK POINTS  /  SURVIVAL AI  /  ENCOUNTER DIRECTOR";
    internal static readonly string Home = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "DOOM-HellforgeLauncher");
    internal static readonly string Profile = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData),"Local Games","DOOM Hellforge");
#endif
    internal static readonly string Screenshots = Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"Screenshots");
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
#if HELLFORGE_EXPERIMENTAL
    // Classic geometry is a diagnostic opt-in: it cannot provide wall destruction.
    internal static string GeometryMode(bool classic) { return classic?"-nodigging":"-solidmaps"; }
    internal static string AmbushSightMode(bool enabled) { return enabled?" -ambushsightreuse":""; }
#endif
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
#if HELLFORGE_EXPERIMENTAL
        string args=GeometryMode(Environment.GetCommandLineArgs().Any(a=>a.Equals("--classic-geometry",StringComparison.OrdinalIgnoreCase)))+" -uzmovement -iwad "+Quote(game);
        args+=AmbushSightMode(Environment.GetCommandLineArgs().Any(a=>a.Equals("--ambush-sight-reuse",StringComparison.OrdinalIgnoreCase)));
#else
        string args="-solidmaps -uzmovement -iwad "+Quote(game);
#endif
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
    // Low-FX is a starting preset for new profiles. Existing F4 choices are never overridden.
    private static readonly Dictionary<string,string> LowFx = new Dictionary<string,string>(StringComparer.OrdinalIgnoreCase) {
        {"msaa","0"}, {"scale","75"}, {"soft_effects","0"}, {"surface_detail","0"},
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

    private static string[] StartingDefaults(string[] lines) {
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
        output.Add("# Low-FX starting defaults. Changes applied in F4 are saved to this profile.");
        foreach(string line in lines) {
            var trimmed=line.Trim();
            if(trimmed.Length==0 || trimmed.StartsWith("#")) {output.Add(line);continue;}
            var parts=trimmed.Split((char[])null,StringSplitOptions.RemoveEmptyEntries);
            string value;
            if(parts.Length>0 && LowFx.TryGetValue(parts[0],out value)) {
                // Never raise a render scale that the user has already reduced.
                if(parts[0].Equals("scale",StringComparison.OrdinalIgnoreCase))value=Math.Min(originalScale,75).ToString();
                if(seen.Add(parts[0])) output.Add(parts[0]+" "+value);
            } else output.Add(line);
        }
        foreach(var entry in LowFx)if(seen.Add(entry.Key))
            output.Add(entry.Key+" "+(entry.Key=="scale"?Math.Min(originalScale,75).ToString():entry.Value));
        return output.ToArray();
    }
    internal static string PrepareConfig(string profile,string bundled,bool lowFx) {
        if(File.Exists(profile))return profile;
        Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(profile)));
        string temporary=profile+".initial-"+Guid.NewGuid().ToString("N")+".tmp";
        try {
            if(lowFx)File.WriteAllLines(temporary,StartingDefaults(File.ReadAllLines(bundled)),new UTF8Encoding(false));
            else File.Copy(bundled,temporary);
            try {File.Move(temporary,profile);}
            catch(IOException){if(!File.Exists(profile))throw;}
        }finally{if(File.Exists(temporary))File.Delete(temporary);}
        return profile;
    }
}

internal static class Palette {
    internal static readonly Color Background=Color.FromArgb(18,14,13);
    internal static readonly Color Card=Color.FromArgb(33,25,23);
    internal static readonly Color Input=Color.FromArgb(23,18,17);
    internal static readonly Color Border=Color.FromArgb(101,66,53);
    internal static readonly Color Text=Color.FromArgb(243,232,222);
    internal static readonly Color Subtle=Color.FromArgb(190,169,155);
    internal static readonly Color Accent=Color.FromArgb(255,138,54);
    internal static readonly Color AccentHover=Color.FromArgb(255,177,90);
    internal static readonly Color AccentDark=Color.FromArgb(66,37,27);
    internal static readonly Color Hover=Color.FromArgb(85,48,32);
    internal static readonly Color QuietHover=Color.FromArgb(57,37,29);
    internal static readonly Color Disabled=Color.FromArgb(43,34,30);
    internal static readonly Color Selection=Color.FromArgb(83,44,29);
    internal static readonly Color Separator=Color.FromArgb(59,40,32);
    internal static readonly Color HeroShade=Background;
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
    private Control backgroundParent;
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
    // OnPaint owns the entire surface, including the pixels outside the curve.
    protected override void OnPaintBackground(PaintEventArgs e) {}
    protected override void OnPaint(PaintEventArgs e) {
        var g=e.Graphics;
        Color backdrop=Palette.Background;
        for(Control parent=Parent;parent!=null;parent=parent.Parent)
            if(parent.BackColor.A==255){backdrop=parent.BackColor;break;}
        using(var clear=new SolidBrush(backdrop))g.FillRectangle(clear,ClientRectangle);
        if(Width<4||Height<4)return;
        g.SmoothingMode=System.Drawing.Drawing2D.SmoothingMode.AntiAlias;
        var r=new Rectangle(1,1,Math.Max(1,Width-3),Math.Max(1,Height-3));
        Color bg=Primary?(hovered?Palette.AccentHover:Palette.Accent):
            Quiet?(hovered?Palette.QuietHover:Palette.Card):
            (hovered?Palette.Hover:Palette.AccentDark);
        Color fg=!Enabled?Palette.Subtle:Primary?Palette.Background:Palette.Text;
        if(!Enabled)bg=Palette.Disabled;
        using(var shape=Round(r,9))
        using(var fill=new SolidBrush(bg))
        using(var line=new Pen(Primary?bg:Palette.Border)) {
            g.FillPath(fill,shape);g.DrawPath(line,shape);
        }
        TextRenderer.DrawText(g,Text,Font,r,fg,bg,TextFormatFlags.HorizontalCenter|TextFormatFlags.VerticalCenter|TextFormatFlags.EndEllipsis|TextFormatFlags.NoPrefix);
        if(Focused && ShowFocusCues && Width>9 && Height>9)
            ControlPaint.DrawFocusRectangle(g,new Rectangle(4,4,Width-9,Height-9),Primary?Palette.Background:Palette.AccentHover,bg);
    }
    protected override void OnParentChanged(EventArgs e) {
        if(backgroundParent!=null)backgroundParent.BackColorChanged-=ParentBackgroundChanged;
        base.OnParentChanged(e);
        backgroundParent=Parent;
        if(backgroundParent!=null)backgroundParent.BackColorChanged+=ParentBackgroundChanged;
        Invalidate();
    }
    void ParentBackgroundChanged(object sender,EventArgs e){Invalidate();}
    protected override void Dispose(bool disposing) {
        if(disposing && backgroundParent!=null)backgroundParent.BackColorChanged-=ParentBackgroundChanged;
        base.Dispose(disposing);
    }
    protected override void OnChangeUICues(UICuesEventArgs e){base.OnChangeUICues(e);Invalidate();}
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
    private readonly WadMascot mascot=new WadMascot();
    private string monsterPath;
    private int animationTick;

    internal ParallaxHero() {
        SetStyle(ControlStyles.UserPaint|ControlStyles.AllPaintingInWmPaint|ControlStyles.OptimizedDoubleBuffer|ControlStyles.ResizeRedraw,true);
        TabStop=false;Cursor=Cursors.Default;BackColor=Palette.Background;
        animation=new System.Windows.Forms.Timer { Interval=66 };
        animation.Tick+=(s,e)=>Step();
        LoadArt();
    }
    internal void LoadMonster(string path){monsterPath=path;if(!sleeping){mascot.Load(path);if(mascot.Animated)animation.Start();Invalidate();}}
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
                new Rectangle(0,0,Width,Height),Color.FromArgb(252,Palette.HeroShade),Color.FromArgb(44,Palette.HeroShade),0f))
                g.FillRectangle(brush,0,0,Width,Height);
            using(var brush=new System.Drawing.Drawing2D.LinearGradientBrush(
                new Rectangle(0,0,Width,Height),Color.FromArgb(0,Palette.HeroShade),Color.FromArgb(136,Palette.AccentDark),90f))
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
        if(sleeping){animation.Stop();return;}
        if(!Visible || FindForm()==null || FindForm().WindowState==FormWindowState.Minimized)return;
        animationTick++;
        x+=(targetX-x)*0.22f;y+=(targetY-y)*0.22f;
        if(Math.Abs(targetX-x)<0.15f && Math.Abs(targetY-y)<0.15f) {
            x=targetX;y=targetY;if(!mascot.Animated)animation.Stop();
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
            g.DrawString("DOOM:",heading,pale,29,75);
            g.DrawString("HELLFORGE",heading,pale,29,125);
            var features=Data.BuildLabel.Split(new[]{"  /  "},StringSplitOptions.None);
            g.DrawString(features[0]+"  /  "+features[1],eyebrow,accent,33,191);
            g.DrawString(features[2]+"  /  "+features[3],eyebrow,muted,33,211);
            if(Width>790) {
                var size=g.MeasureString(versionMark,mono);
                g.DrawString(versionMark,mono,muted,Math.Max(530,Width-(int)size.Width-27),20);
                mascot.Draw(g,new Rectangle(Width-200,53,145,163),animationTick);
            }
        }
        using(var border=new Pen(Palette.Border))g.DrawRectangle(border,0,0,Width-1,Height-1);
    }
    // Called as soon as the child game process successfully launches.
    // The WinForms UI remains in memory, but bitmap/GDI overlay + all animation stop.
    internal void EnterGameMode() {
        sleeping=true;animation.Stop();mascot.Dispose();x=y=targetX=targetY=0;
        if(image!=null){image.Dispose();image=null;}
        if(overlay!=null){overlay.Dispose();overlay=null;}
        Invalidate();
    }
    internal void LeaveGameMode() {
        sleeping=false;LoadArt();LoadMonster(monsterPath);Invalidate();
    }
    protected override void Dispose(bool disposing) {
        if(disposing){animation.Stop();animation.Dispose();mascot.Dispose();if(image!=null)image.Dispose();if(overlay!=null)overlay.Dispose();heading.Dispose();eyebrow.Dispose();subtitle.Dispose();mono.Dispose();}
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
    private ModernButton scan,play,lan,browseGame,scanFolder,screenshots,addMods,removeMod,upMod,downMod,export,import,clearCache,openLog,openConfig,gameSettings,resetAll;
    private bool scanning,launching,gameRunning;
    private CancellationTokenSource scanCancellation;

    [STAThread] static void Main() {
        Application.EnableVisualStyles();Application.SetCompatibleTextRenderingDefault(false);
        Application.ThreadException+=(s,e)=>MessageBox.Show(e.Exception.Message,Data.ProductName,MessageBoxButtons.OK,MessageBoxIcon.Error);
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
        Text=Data.ProductName+"  •  "+Data.BuildVersion.Split('|')[0].Trim();
        ClientSize=new Size(1150,830);MinimumSize=new Size(940,770);
        using(var stream=Assembly.GetExecutingAssembly().GetManifestResourceStream("hellforge.ico"))if(stream!=null)using(var icon=new Icon(stream))Icon=(Icon)icon.Clone();
        StartPosition=FormStartPosition.CenterScreen;AutoScaleMode=AutoScaleMode.Dpi;
        Font=new Font("Segoe UI",9.5f);BackColor=Palette.Background;ForeColor=Palette.Text;
        SetStyle(ControlStyles.OptimizedDoubleBuffer,true);

        var root=new TableLayoutPanel {Dock=DockStyle.Fill,Padding=new Padding(18),BackColor=Palette.Background,ColumnCount=1,RowCount=3};
        root.RowStyles.Add(new RowStyle(SizeType.Absolute,252));
        root.RowStyles.Add(new RowStyle(SizeType.Percent,100));
        root.RowStyles.Add(new RowStyle(SizeType.Absolute,140));
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
        games.SelectedIndexChanged+=(s,e)=>hero.LoadMonster(games.SelectedItem as string);
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
        var labels=modsCard.Controls.OfType<Label>().Where(c=>c!=modHint&&c!=emptyMods).ToArray();
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
        using(var fill=new SolidBrush(selected?Palette.Selection:Palette.Input))g.FillRectangle(fill,e.Bounds);
        using(var line=new Pen(Palette.Separator))g.DrawLine(line,e.Bounds.Left,e.Bounds.Bottom-1,e.Bounds.Right,e.Bounds.Bottom-1);
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
        clearCache=CreateButton("CLEAR LIGHTING CACHE",ClearLightingCache,quiet:true);footer.Controls.Add(clearCache);
        export=CreateButton("EXPORT SETTINGS",ExportSettings,quiet:true);footer.Controls.Add(export);
        import=CreateButton("IMPORT SETTINGS",ImportSettings,quiet:true);footer.Controls.Add(import);
        lowFx.Text="LOW-FX DEFAULTS";lowFx.Checked=true;lowFx.AutoSize=false;
        lowFx.FlatStyle=FlatStyle.Flat;lowFx.ForeColor=Palette.Accent;lowFx.BackColor=Palette.Card;
        lowFx.Font=tagFont;lowFx.Cursor=Cursors.Hand;footer.Controls.Add(lowFx);
        tips.SetToolTip(lowFx,"Starting preset for new profiles only. Existing profiles are unchanged. Your F4 Apply/close choices are always saved and used on the next launch.");
        lan=CreateButton("LAN / CO-OP",PlayLan);footer.Controls.Add(lan);
        play=CreateButton("▶  PLAY GAME",Play,primary:true);footer.Controls.Add(play);
        openLog=CreateButton("OPEN LAST LOG",()=>OpenFile(Path.Combine(Data.Home,"last-game.log")),quiet:true);
        openConfig=CreateButton("OPEN GRAPHICS.CFG",()=>OpenFile(Path.Combine(Data.Profile,"graphics.cfg")),quiet:true);
        gameSettings=CreateButton("GAME SETTINGS / TABS",EditGameSettings);
        resetAll=CreateButton("CLEAR ALL / FRESH START",ResetAll,quiet:true);
        foreach(var button in new[]{openLog,openConfig,gameSettings,resetAll})footer.Controls.Add(button);
        tips.SetToolTip(resetAll,"Back up and reset settings and add-ons. Save games and installed IWADs are kept.");
        AcceptButton=play;
    }
    void LayoutFooter() {
        if(footer==null)return;int w=footer.ClientSize.Width;
        gameSettings.SetBounds(18,7,180,30);openLog.SetBounds(208,7,130,30);
        openConfig.SetBounds(348,7,170,30);resetAll.SetBounds(528,7,210,30);
        status.SetBounds(18,47,282,22);
        clearCache.SetBounds(318,45,180,25);
        export.SetBounds(18,78,138,30);import.SetBounds(166,78,138,30);
        lowFx.SetBounds(318,80,Math.Max(150,w-720),25);
        lan.SetBounds(w-380,54,142,50);play.SetBounds(w-226,54,208,50);
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
    void OpenFile(string path) {
        try {
            if(!File.Exists(path)){MessageBox.Show(this,"This file has not been created yet:\n"+path,Data.ProductName);return;}
            Process.Start(new ProcessStartInfo("notepad.exe",Data.Quote(path)){UseShellExecute=true});
        }catch(Exception e){MessageBox.Show(this,e.Message,Data.ProductName);}
    }
    string BackupSettings() {
        string folder=Path.Combine(Data.Home,"backups");Directory.CreateDirectory(folder);
        string path=Path.Combine(folder,"before-edit-"+DateTime.UtcNow.ToString("yyyyMMdd-HHmmss-fff")+"-"+Guid.NewGuid().ToString("N")+".zip");
        SettingsBackup.Export(path,CurrentSettings(),Data.Profile);return path;
    }
    void EditGameSettings() {
        if(launching||gameRunning)return;
        try {
            string path=GraphicsPerformance.PrepareConfig(Path.Combine(Data.Profile,"graphics.cfg"),Data.BundledResource("graphics.cfg"),lowFx.Checked);
            using(var dialog=new LauncherSettingsDialog(path,()=>BackupSettings()))if(dialog.ShowDialog(this)==DialogResult.OK)status.Text="Settings saved for the next launch.";
        }catch(Exception e){MessageBox.Show(this,e.Message,"Cannot edit settings");}
    }
    void ResetAll() {
        if(launching||gameRunning)return;
        if(MessageBox.Show(this,"Back up and reset launcher/game settings and remove all selected add-ons?\n\nSaves, installed IWADs, screenshots, logs and backups are kept.","Fresh start",MessageBoxButtons.OKCancel,MessageBoxIcon.Warning)!=DialogResult.OK)return;
        try {
            string backup=BackupSettings();
            LauncherConfig.ResetProfile(Data.Profile);
            mods.Items.Clear();lowFx.Checked=true;Save();UpdateInfo();
            status.Text="Fresh defaults ready. Saves kept; settings backed up.";
            MessageBox.Show(this,"Settings reset. New defaults apply on next launch.\n\nBackup:\n"+backup,"Fresh start complete");
        }catch(Exception e){MessageBox.Show(this,e.Message,"Reset incomplete; recovery backup retained",MessageBoxButtons.OK,MessageBoxIcon.Error);}
    }
    void ClearLightingCache() {
        if(gameRunning||launching)return;
        try {
            string folder=Path.GetFullPath(Path.Combine(Data.Home,"Cache","Maps"));
            // Delete only generated entries, never follow links out of the cache.
            ClearCacheFolder(folder);
            status.Text="Lighting cache cleared. Maps bake again on next load.";
        } catch(Exception e) {MessageBox.Show(this,e.Message,"Cannot clear lighting cache",MessageBoxButtons.OK,MessageBoxIcon.Error);}
    }
    internal static void ClearCacheFolder(string folder) {
        if(!Directory.Exists(folder))return;
        if((File.GetAttributes(folder)&FileAttributes.ReparsePoint)!=0)throw new IOException("The lighting cache folder is a link.");
        foreach(string file in Directory.GetFiles(folder)) {
            if((File.GetAttributes(file)&FileAttributes.ReparsePoint)!=0)continue;
            if(file.EndsWith(".bake",StringComparison.OrdinalIgnoreCase)||file.EndsWith(".tmp",StringComparison.OrdinalIgnoreCase))File.Delete(file);
        }
        foreach(string child in Directory.GetDirectories(folder)) {
            if((File.GetAttributes(child)&FileAttributes.ReparsePoint)!=0)continue;
            ClearCacheFolder(child);
            if(!Directory.EnumerateFileSystemEntries(child).Any())Directory.Delete(child);
        }
    }
    void ExportSettings() {
        using(var dialog=new SaveFileDialog {Title="Export settings backup",Filter="Settings backup (*.zip)|*.zip",DefaultExt="zip",FileName="HellForge-Doom-settings-"+DateTime.Now.ToString("yyyyMMdd-HHmmss")+".zip"}) {
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
        foreach(Control control in new Control[]{games,mods,browseGame,scan,scanFolder,screenshots,addMods,removeMod,upMod,downMod,export,import,clearCache,openLog,openConfig,gameSettings,resetAll,lowFx,lan,play})
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
            string graphics=GraphicsPerformance.PrepareConfig(Path.Combine(Data.Profile,"graphics.cfg"),Data.BundledResource("graphics.cfg"),performance);
            args+=" -graphics "+Data.Quote(graphics);
            Save();status.Text="Launching game…";
            var start=new ProcessStartInfo(engine,args){WorkingDirectory=Data.Home,UseShellExecute=false,CreateNoWindow=true,RedirectStandardOutput=true,RedirectStandardError=true};
            start.EnvironmentVariables["HELLFORGE_SCREENSHOTS"]=Data.Screenshots;
            start.EnvironmentVariables["HELLFORGE_MAP_CACHE"]=Path.Combine(Data.Home,"Cache","Maps");
            start.EnvironmentVariables["TUIN_SCREENSHOTS"]=Data.Screenshots;
            start.EnvironmentVariables["TUIN_MAP_CACHE"]=Path.Combine(Data.Home,"Cache","Maps");
            string crashFolder=Path.Combine(Data.Home,"crashes");Directory.CreateDirectory(crashFolder);
            start.EnvironmentVariables["HELLFORGE_CRASH_DIR"]=crashFolder;
            start.EnvironmentVariables["TUIN_CRASH_DIR"]=crashFolder;
            start.EnvironmentVariables["TUIN_LAN_SIGNATURE"]=signature;
            try{start.EnvironmentVariables["TUIN_LAN_ADDRESSES"]=String.Join(" / ",System.Net.Dns.GetHostAddresses(System.Net.Dns.GetHostName()).Where(a=>a.AddressFamily==System.Net.Sockets.AddressFamily.InterNetwork).Select(a=>a.ToString()));}catch(System.Net.Sockets.SocketException){}
            string logPath=Path.Combine(Data.Home,"last-game.log");
            using(var writer=new StreamWriter(new FileStream(logPath,FileMode.Create,FileAccess.Write,FileShare.Read,8192))) {
                writer.AutoFlush=true;writer.WriteLine("Launcher build "+Data.BuildVersion+"; engine "+engine);
                var writeLock=new object();
                using(var process=new Process {StartInfo=start}) {
                    process.OutputDataReceived+=(s,e)=>{if(e.Data!=null)lock(writeLock)writer.WriteLine(e.Data);};
                    process.ErrorDataReceived+=(s,e)=>{if(e.Data!=null)lock(writeLock)writer.WriteLine("[stderr] "+e.Data);};
                    if(!process.Start())throw new IOException("Windows could not start the game engine.");
                    process.BeginOutputReadLine();process.BeginErrorReadLine();
                    gameRunning=true;enteredGameMode=true;
                    hero.EnterGameMode();Hide(); // Zero animation; embedded artwork is disposed.
                    int exitCode=await Task.Run(()=>{process.WaitForExit();return process.ExitCode;});
                    lock(writeLock){writer.WriteLine("Engine exit: "+exitCode+" (0x"+unchecked((uint)exitCode).ToString("X8")+")");writer.Flush();}
                    if(!IsDisposed&&!Disposing)status.Text=exitCode==0?"Game closed. Ready to play again.":"Game exited with code "+exitCode+". See last-game.log.";
                }
            }
        }catch(Exception e){
            if(!IsDisposed&&!Disposing){status.Text="Launch failed.";MessageBox.Show(this,e.Message,"Cannot start game",MessageBoxButtons.OK,MessageBoxIcon.Error);}
        }finally{
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

// Launcher-only editing: preserve unrecognised keys/comments and never guess engine defaults.
internal static class LauncherConfig {
    internal static string Read(string path){return File.Exists(path)?File.ReadAllText(path):"";}
    internal static string Merge(string text,IDictionary<string,string> edits) {
        var pending=new Dictionary<string,string>(edits,StringComparer.Ordinal);
        var lines=new List<string>();
        foreach(string line in text.Replace("\r\n","\n").Split('\n')) {
            string key=line.TrimStart('\uFEFF',' ','\t').Split(new[]{' ','\t'},StringSplitOptions.RemoveEmptyEntries).FirstOrDefault();
            if(key!=null&&edits.ContainsKey(key))continue;
            lines.Add(line);
        }
        foreach(var entry in pending)if(!String.IsNullOrWhiteSpace(entry.Value)) {
            double number;
            if(!Double.TryParse(entry.Value,System.Globalization.NumberStyles.Float,System.Globalization.CultureInfo.InvariantCulture,out number)||Double.IsNaN(number)||Double.IsInfinity(number))throw new InvalidDataException("Invalid numeric value for "+entry.Key);
            lines.Add(entry.Key+" "+number.ToString("R",System.Globalization.CultureInfo.InvariantCulture));
        }
        return String.Join(Environment.NewLine,lines).TrimEnd()+Environment.NewLine;
    }
    internal static void ResetProfile(string profile) {
        foreach(string name in SettingsBackup.ConfigFiles) {
            string path=Path.Combine(profile,name);if(File.Exists(path))File.Delete(path);
        }
    }
    internal static void Write(string path,string original,string content) {
        if(Read(path)!=original)throw new IOException("Configuration changed outside this window. Close and reopen settings before saving.");
        Directory.CreateDirectory(Path.GetDirectoryName(path));
        string temp=path+".saving-"+Guid.NewGuid().ToString("N")+".tmp";
        try {
            using(var f=new FileStream(temp,FileMode.CreateNew,FileAccess.Write,FileShare.None)) {
                byte[] bytes=new UTF8Encoding(false).GetBytes(content);f.Write(bytes,0,bytes.Length);f.Flush(true);
            }
            if(File.Exists(path))File.Replace(temp,path,null);else File.Move(temp,path);
        } finally {if(File.Exists(temp))File.Delete(temp);}
    }
}

internal sealed class LauncherSettingsDialog : Form {
    readonly string path,original;
    readonly Action backup;
    readonly List<DataGridView> grids=new List<DataGridView>();
    readonly Dictionary<string,string> initial=new Dictionary<string,string>();
    internal LauncherSettingsDialog(string config,Action createBackup) {
        path=config;backup=createBackup;original=LauncherConfig.Read(path);
        Text="HELLFORGE / GAME SETTINGS";Size=new Size(1000,700);MinimumSize=new Size(760,480);
        StartPosition=FormStartPosition.CenterParent;BackColor=Palette.Background;ForeColor=Palette.Text;Font=new Font("Segoe UI",9f);
        var layout=new TableLayoutPanel{Dock=DockStyle.Fill,RowCount=3,ColumnCount=1,Padding=new Padding(12)};
        layout.RowStyles.Add(new RowStyle(SizeType.Absolute,65));layout.RowStyles.Add(new RowStyle(SizeType.Percent,100));layout.RowStyles.Add(new RowStyle(SizeType.Absolute,48));Controls.Add(layout);
        layout.Controls.Add(new Label{Dock=DockStyle.Fill,Text="Edit values, then Apply. Blank removes an override; legacy keys may still apply. Choices show numeric codes.\nChanges apply on the next game launch; engine validation and compatibility restrictions still apply. A settings backup is created before writing.",ForeColor=Palette.Subtle},0,0);
        var tabs=new TabControl{Dock=DockStyle.Fill,Multiline=true};layout.Controls.Add(tabs,0,1);
        foreach(string line in original.Split('\n')) {
            string clean=line.TrimStart('\uFEFF').Split('#',';')[0].Trim();
            string[] pair=clean.Split(new[]{' ','\t'},StringSplitOptions.RemoveEmptyEntries);
            if(pair.Length==2)initial[pair[0]]=pair[1];
        }
        var options=new List<XElement>();
        using(var resource=Assembly.GetExecutingAssembly().GetManifestResourceStream("settings-schema.xml"))if(resource!=null)options.AddRange(XDocument.Load(resource).Root.Elements("option"));
        var known=new HashSet<string>(options.Select(o=>(string)o.Attribute("key")));
        foreach(string key in initial.Keys.Where(k=>!known.Contains(k)))options.Add(new XElement("option",new XAttribute("key",key),new XAttribute("title",key),new XAttribute("category","Advanced / tuning")));
        foreach(var group in options.GroupBy(o=>(string)o.Attribute("category"))) {
            var page=new TabPage(group.Key){BackColor=Palette.Card};tabs.TabPages.Add(page);
            var grid=new DataGridView{Dock=DockStyle.Fill,BackgroundColor=Palette.Input,BorderStyle=BorderStyle.None,AllowUserToAddRows=false,AllowUserToDeleteRows=false,RowHeadersVisible=false,AutoSizeColumnsMode=DataGridViewAutoSizeColumnsMode.Fill,AutoSizeRowsMode=DataGridViewAutoSizeRowsMode.AllCells};
            grid.DefaultCellStyle.BackColor=Palette.Input;grid.DefaultCellStyle.ForeColor=Palette.Text;grid.DefaultCellStyle.SelectionBackColor=Palette.AccentDark;grid.DefaultCellStyle.SelectionForeColor=Palette.Text;
            grid.DefaultCellStyle.WrapMode=DataGridViewTriState.True;grid.ColumnHeadersDefaultCellStyle.BackColor=Palette.Card;grid.ColumnHeadersDefaultCellStyle.ForeColor=Palette.Text;grid.EnableHeadersVisualStyles=false;
            grid.Columns.Add("setting","Setting");grid.Columns.Add("value","Value");grid.Columns.Add("help","Choices / range / config key");grid.Columns[0].ReadOnly=grid.Columns[2].ReadOnly=true;grid.Columns[1].FillWeight=30;
            foreach(var option in group) {
                string key=(string)option.Attribute("key"),value;initial.TryGetValue(key,out value);
                string help=(string)option.Attribute("choices")??"";
                if(option.Attribute("min")!=null)help+="  min "+(string)option.Attribute("min");
                if(option.Attribute("max")!=null)help+="  max "+(string)option.Attribute("max");
                int row=grid.Rows.Add((string)option.Attribute("title"),value??"",help+"  ["+key+"]");grid.Rows[row].Tag=option;
                string choices=(string)option.Attribute("choices");
                if(choices!=null) {
                    var cell=new DataGridViewComboBoxCell{FlatStyle=FlatStyle.Flat,DisplayStyle=DataGridViewComboBoxDisplayStyle.DropDownButton,DisplayMember="Value",ValueMember="Key"};
                    var items=new List<KeyValuePair<string,string>>{new KeyValuePair<string,string>("","Default (no override)")};
                    items.AddRange(choices.Split('|').Select(c=>new KeyValuePair<string,string>(c.Substring(0,c.IndexOf('=')),c.Substring(c.IndexOf('=')+1))));
                    if(!String.IsNullOrEmpty(value)&&!items.Any(item=>item.Key==value))items.Add(new KeyValuePair<string,string>(value,"Saved: "+value));
                    cell.DataSource=items;grid.Rows[row].Cells[1]=cell;cell.Value=value??"";
                }
            }
            grids.Add(grid);page.Controls.Add(grid);
        }
        foreach(string name in new[]{"doom.cfg","heretic.cfg"}) {
            string file=Path.Combine(Data.Profile,name);if(!File.Exists(file))continue;
            var page=new TabPage(name);tabs.TabPages.Add(page);
            var open=new Button{Text="Open "+name+" in text editor (advanced controls / bindings)",Dock=DockStyle.Top,Height=42};
            open.Click+=(s,e)=>{try{Process.Start(new ProcessStartInfo("notepad.exe",Data.Quote(file)){UseShellExecute=true});}catch(Exception ex){MessageBox.Show(this,ex.Message);}};
            page.Controls.Add(new Label{Text="This native configuration uses its own text format. Save in the editor before starting the game.",Dock=DockStyle.Top,Height=48});page.Controls.Add(open);
        }
        var buttons=new FlowLayoutPanel{Dock=DockStyle.Fill,FlowDirection=FlowDirection.RightToLeft};layout.Controls.Add(buttons,0,2);
        var cancel=new Button{Text="CANCEL",DialogResult=DialogResult.Cancel,Width=110,Height=32};buttons.Controls.Add(cancel);CancelButton=cancel;
        var apply=new Button{Text="APPLY SETTINGS",Width=150,Height=32};buttons.Controls.Add(apply);apply.Click+=(s,e)=>Apply();
    }
    void Apply() {
        try {
            var edits=new Dictionary<string,string>();
            foreach(var grid in grids) {grid.EndEdit();foreach(DataGridViewRow row in grid.Rows) {
                var option=(XElement)row.Tag;string key=(string)option.Attribute("key"),old;
                string value=Convert.ToString(row.Cells[1].Value).Trim();initial.TryGetValue(key,out old);
                if(value==(old??""))continue;
                if(value.Length!=0) {
                    double n;
                    if(!Double.TryParse(value,System.Globalization.NumberStyles.Float,System.Globalization.CultureInfo.InvariantCulture,out n)||Double.IsInfinity(n)||Double.IsNaN(n))throw new InvalidDataException(key+": enter a finite numeric value.");
                    string choices=(string)option.Attribute("choices");
                    if(choices!=null&&!choices.Split('|').Any(c=>c.Split('=')[0]==value))throw new InvalidDataException(key+": use a listed numeric choice.");
                    foreach(string bound in new[]{"min","max"}) {
                        double limit;var attr=option.Attribute(bound);
                        if(attr!=null&&Double.TryParse(attr.Value,System.Globalization.NumberStyles.Float,System.Globalization.CultureInfo.InvariantCulture,out limit)&&(bound=="min"?n<limit:n>limit))throw new InvalidDataException(key+": value outside the displayed range.");
                    }
                }
                edits[key]=value;
            }}
            if(edits.Count!=0){string content=LauncherConfig.Merge(original,edits);backup();LauncherConfig.Write(path,original,content);}
            DialogResult=DialogResult.OK;Close();
        } catch(Exception e){MessageBox.Show(this,e.Message,"Settings not saved",MessageBoxButtons.OK,MessageBoxIcon.Error);}
    }
}

// Decode a handful of stock patches from the user's IWAD; commercial artwork is never bundled.
internal sealed class WadMascot : IDisposable {
    readonly List<Bitmap> actors=new List<Bitmap>(),fire=new List<Bitmap>();
    internal bool Animated {get{return actors.Count>0;}}
    internal void Load(string path) {
        Dispose();if(String.IsNullOrEmpty(path)||!File.Exists(path))return;
        try {
            using(var stream=File.OpenRead(path))using(var reader=new BinaryReader(stream)) {
                if(stream.Length<12)return;reader.ReadInt32();int count=reader.ReadInt32(),offset=reader.ReadInt32();
                if(count<1||count>100000||offset<12||(long)offset+count*16L>stream.Length)return;
                var entries=new Dictionary<string,Tuple<int,int>>();stream.Position=offset;
                for(int i=0;i<count;i++){int start=reader.ReadInt32(),length=reader.ReadInt32();string name=Encoding.ASCII.GetString(reader.ReadBytes(8)).TrimEnd('\0');if(start>=0&&length>=0&&(long)start+length<=stream.Length)entries[name]=Tuple.Create(start,length);}
                Tuple<int,int> palette;if(!entries.TryGetValue("PLAYPAL",out palette)||palette.Item2<768)return;
                stream.Position=palette.Item1;byte[] colors=reader.ReadBytes(768);
                foreach(string name in new[]{"VILEA1","VILEB1","VILEC1","VILED1","FIREA0","FIREB0","FIREC0","FIRED0","FIREE0","FIREF0"}) {
                    Tuple<int,int> entry;bool mirrored=false;
                    if(!entries.TryGetValue(name,out entry)) {
                        var combined=entries.FirstOrDefault(p=>p.Key.StartsWith(name,StringComparison.Ordinal));
                        if(combined.Key==null){combined=entries.FirstOrDefault(p=>p.Key.Length==8&&p.Key.StartsWith("VILE",StringComparison.Ordinal)&&p.Key.Substring(6,2)==name.Substring(4));mirrored=combined.Key!=null;}
                        if(combined.Key==null)continue;entry=combined.Value;
                    }
                    if(entry.Item2<8||entry.Item2>1048576)continue;
                    stream.Position=entry.Item1;byte[] patch=reader.ReadBytes(entry.Item2);Bitmap frame=Decode(patch,colors);
                    if(frame!=null){if(mirrored)frame.RotateFlip(RotateFlipType.RotateNoneFlipX);(name.StartsWith("VILE",StringComparison.Ordinal)?actors:fire).Add(frame);}
                }
            }
        }catch(IOException){Dispose();}catch(ArgumentException){Dispose();}
    }
    internal static Bitmap Decode(byte[] bytes,byte[] colors) {
        if(bytes.Length<8||colors.Length<768)return null;
        int w=BitConverter.ToUInt16(bytes,0),h=BitConverter.ToUInt16(bytes,2);
        if(w<1||h<1||w>512||h>512||8+w*4>bytes.Length)return null;
        var image=new Bitmap(w,h,System.Drawing.Imaging.PixelFormat.Format32bppArgb);
        for(int x=0;x<w;x++) {
            int at=BitConverter.ToInt32(bytes,8+x*4),last=-1;
            if(at<8+w*4||at>=bytes.Length){image.Dispose();return null;}
            int posts=0;
            while(at<bytes.Length&&bytes[at]!=255&&posts++<512) {
                if(at+3>=bytes.Length){image.Dispose();return null;}
                int top=bytes[at++],length=bytes[at++];at++;
                if(top<=last)top+=last;last=top;
                if(at+length>=bytes.Length){image.Dispose();return null;}
                for(int y=0;y<length;y++)if(top+y<h){int color=bytes[at+y]*3;image.SetPixel(x,top+y,Color.FromArgb(255,colors[color],colors[color+1],colors[color+2]));}
                at+=length+1;
            }
        }
        return image;
    }
    internal void Draw(Graphics g,Rectangle bounds,int tick) {
        if(actors.Count==0) {
            using(var brush=new SolidBrush(Palette.Subtle))using(var font=new Font("Consolas",8f))g.DrawString("ARCH-VILE FX\nSelect a Doom II IWAD",font,brush,bounds);
            return;
        }
        var mode=g.InterpolationMode;g.InterpolationMode=System.Drawing.Drawing2D.InterpolationMode.NearestNeighbor;
        var actor=actors[(tick/4)%actors.Count];float scale=Math.Min(bounds.Width/(float)Math.Max(actor.Width,100),bounds.Height/(float)actor.Height);
        int w=(int)(actor.Width*scale),h=(int)(actor.Height*scale),left=bounds.Left+(bounds.Width-w)/2,top=bounds.Bottom-h;
        using(var glow=new SolidBrush(Color.FromArgb(45+(tick%4)*9,Palette.Accent)))g.FillEllipse(glow,bounds.Left,bounds.Bottom-19,bounds.Width,18);
        g.DrawImage(actor,new Rectangle(left,top,w,h));
        if(fire.Count>0){var flame=fire[tick%fire.Count];int fw=Math.Min(bounds.Width,(int)(flame.Width*scale));g.DrawImage(flame,new Rectangle(bounds.Left+(bounds.Width-fw)/2,bounds.Bottom-(int)(flame.Height*scale),fw,(int)(flame.Height*scale)));}
        g.InterpolationMode=mode;
    }
    public void Dispose(){foreach(var frame in actors)frame.Dispose();foreach(var frame in fire)frame.Dispose();actors.Clear();fire.Clear();}
}
