using System;
using System.IO;
using System.Linq;
using System.Text;
using System.Xml.Linq;
internal static class LauncherPersistenceTests {
    static int checks;
    static void Check(bool ok,string name){if(!ok)throw new Exception(name);++checks;Console.WriteLine("PASS "+name);}
    public static int Main(string[] args){
        if(args.Length==4&&args[0]=="--prepare"){Console.WriteLine(GraphicsPerformance.PrepareConfig(args[1],args[2],bool.Parse(args[3])));return 0;}
        string root=Path.GetFullPath(args[0]);Directory.CreateDirectory(root);
        string bundled=Path.Combine(root,"bundled.cfg");File.WriteAllText(bundled,"# Starting values\nscale 100\nemissive 1\nfog 1\ncritical_parts 0\nmonster_feeding 0\n");
        foreach(bool lowFx in new[]{false,true}){
            string profile=Path.Combine(root,lowFx?"low FX profile":"normal profile","graphics.cfg");
            string prepared=GraphicsPerformance.PrepareConfig(profile,bundled,lowFx);
            Check(prepared==profile&&File.Exists(profile),"game receives persistent canonical path, low-FX="+lowFx);
            string seed=File.ReadAllText(profile);
            Check(seed.Contains(lowFx?"scale 75":"scale 100"),"starting resolution uses supported F4 values");
            Check(seed.Contains(lowFx?"emissive 0":"emissive 1"),"preset only seeds new graphical defaults");
            string applied="# F4 applied choices\nscale 100\nemissive 1\nfog 1\ncritical_parts 1\nmonster_feeding 1\nwp_bleed_damage_3 7\nmap_light_cache 0\n";
            File.WriteAllText(prepared,applied,new UTF8Encoding(false));
            byte[] expected=File.ReadAllBytes(profile);
            foreach(bool nextMode in new[]{lowFx,!lowFx}){
                string next=GraphicsPerformance.PrepareConfig(profile,bundled,nextMode);
                Check(next==profile&&File.ReadAllBytes(next).SequenceEqual(expected),"relaunch preserves all F4 values byte-for-byte, low-FX="+nextMode);
            }
            // Existing profiles must not depend on an available bundled template.
            Check(GraphicsPerformance.PrepareConfig(profile,"nonexistent-template.cfg",true)==profile,"existing profile needs no initialization template");
            string archive=Path.Combine(root,lowFx?"low.zip":"normal.zip");
            SettingsBackup.Export(archive,new XDocument(new XElement("launcher",new XElement("performance",lowFx?"1":"0"))),Path.GetDirectoryName(profile));
            var restored=SettingsBackup.Read(archive);
            Check(restored["graphics.cfg"].SequenceEqual(expected),"export contains the actual F4 profile");
            string imported=Path.Combine(root,lowFx?"import low":"import normal");Directory.CreateDirectory(imported);
            SettingsBackup.Restore(restored,Path.Combine(imported,"launcher.xml"),imported);
            string importedGraphics=Path.Combine(imported,"graphics.cfg");GraphicsPerformance.PrepareConfig(importedGraphics,bundled,true);
            Check(File.ReadAllBytes(importedGraphics).SequenceEqual(expected),"imported F4 values are not overwritten by Low-FX");
        }
        string reduced=Path.Combine(root,"already-reduced.cfg");File.WriteAllText(bundled,"scale 50\ncritical_parts 1\n");
        GraphicsPerformance.PrepareConfig(reduced,bundled,true);Check(File.ReadAllText(reduced).Contains("scale 50"),"Low-FX does not increase an already lower supported scale");
        Check(!Directory.EnumerateFiles(root,"*.tmp",SearchOption.AllDirectories).Any(),"initialization leaves no temporary files");
        Console.WriteLine("PASS "+checks+" launcher persistence/backup checks");return 0;
    }
}
