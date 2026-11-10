using System;
using System.IO;
using System.IO.Compression;
using System.Drawing;
using System.Linq;
using System.Collections.Generic;
using System.Windows.Forms;
using System.Xml.Linq;

internal static class SettingsBackupTests {
    static IEnumerable<Control> Descendants(Control parent) {
        foreach(Control child in parent.Controls) {
            yield return child;
            foreach(Control nested in Descendants(child))yield return nested;
        }
    }
    static void Check(bool condition,string name) { if(!condition)throw new Exception(name);Console.WriteLine("PASS "+name); }
    static void Rejected(string path,string name) {
        bool rejected=false;try { SettingsBackup.Read(path); } catch(InvalidDataException) { rejected=true; } catch(System.Xml.XmlException) { rejected=true; }
        Check(rejected,name);
    }
    static void Archive(string path,params string[] entries) {
        using(var zip=ZipFile.Open(path,ZipArchiveMode.Create))foreach(string name in entries)
            using(var writer=new StreamWriter(zip.CreateEntry(name).Open()))writer.Write(name=="launcher.xml"?"<launcher><selected /></launcher>":"test");
    }
    [STAThread] static void Main() {
        string root=Path.Combine(Path.GetTempPath(),"Tuin-settings-tests-"+Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(root);
        try {
            // Keep the UI check isolated too: construction can initialize its
            // profile. These paths are overridden only in this test process.
            var fields=System.Reflection.BindingFlags.Static|System.Reflection.BindingFlags.NonPublic;
            typeof(Data).GetField("Home",fields).SetValue(null,Path.Combine(root,"launcher-home"));
            typeof(Data).GetField("Profile",fields).SetValue(null,Path.Combine(root,"launcher-profile"));
            typeof(Data).GetField("Screenshots",fields).SetValue(null,Path.Combine(root,"screenshots"));
            Directory.CreateDirectory(Data.Home);Directory.CreateDirectory(Data.Profile);
            string source=Path.Combine(root,"source"),target=Path.Combine(root,"target"),backup=Path.Combine(root,"backup.zip");
            Directory.CreateDirectory(source);Directory.CreateDirectory(target);
            File.WriteAllText(Path.Combine(source,"graphics.cfg"),"graphics test");
            File.WriteAllText(Path.Combine(source,"doom.cfg"),"controls test");
            File.WriteAllText(Path.Combine(source,"save0.dsg"),"save game");
            var settings=new XDocument(new XElement("launcher",new XElement("iwad",@"C:\Games\DOOM.WAD"),new XElement("selected",@"C:\Games\DOOM.WAD"),new XElement("mod","first.wad"),new XElement("mod","second.pk3")));
            SettingsBackup.Export(backup,settings,source);
            var files=SettingsBackup.Read(backup);
            Check(files.Count==3&&!files.ContainsKey("save0.dsg"),"backup includes settings and excludes saves");
            string launcher=Path.Combine(target,"launcher.xml");
            SettingsBackup.Restore(files,launcher,target);
            Check(XNode.DeepEquals(settings,XDocument.Load(launcher)),"IWAD selection and mod order round trip");
            Check(File.ReadAllText(Path.Combine(target,"graphics.cfg"))=="graphics test"&&File.ReadAllText(Path.Combine(target,"doom.cfg"))=="controls test","game preferences round trip");
            File.WriteAllText(launcher,"previous launcher");
            using(var locked=File.Open(Path.Combine(target,"doom.cfg"),FileMode.Open,FileAccess.ReadWrite,FileShare.None)) {
                bool failed=false;try { SettingsBackup.Restore(files,launcher,target); } catch(IOException) { failed=true; }
                Check(failed&&File.ReadAllText(launcher)=="previous launcher","failed import rolls back prior writes");
            }
            string invalid=Path.Combine(root,"invalid.zip");
            Archive(invalid,"launcher.xml","../doom.cfg");Rejected(invalid,"rejects path traversal");File.Delete(invalid);
            Archive(invalid,"launcher.xml","launcher.xml");Rejected(invalid,"rejects duplicate entries");File.Delete(invalid);
            Archive(invalid,"doom.cfg");Rejected(invalid,"rejects missing launcher settings");File.Delete(invalid);
            using(var zip=ZipFile.Open(invalid,ZipArchiveMode.Create))using(var writer=new StreamWriter(zip.CreateEntry("launcher.xml").Open()))writer.Write("<!DOCTYPE launcher [<!ENTITY x 'test'>]><launcher><selected>&x;</selected></launcher>");
            Rejected(invalid,"rejects XML entities");File.Delete(invalid);
            using(var zip=ZipFile.Open(invalid,ZipArchiveMode.Create))using(var writer=new StreamWriter(zip.CreateEntry("launcher.xml").Open()))writer.Write("<unrelated />");
            Rejected(invalid,"rejects unrelated XML");File.Delete(invalid);
            using(var zip=ZipFile.Open(invalid,ZipArchiveMode.Create))using(var output=zip.CreateEntry("launcher.xml").Open())output.Write(new byte[4*1024*1024+1],0,4*1024*1024+1);
            Rejected(invalid,"rejects oversized settings");
            Check(Data.BuildVersion.Contains("engine ")&&!Data.BuildVersion.Contains("Development"),"build metadata available");
            Application.EnableVisualStyles();Application.SetCompatibleTextRenderingDefault(false);
            using(var form=new Launcher()) {
                form.ShowInTaskbar=false;form.StartPosition=FormStartPosition.Manual;form.Location=new Point(-20000,-20000);
                form.Show();Application.DoEvents();
                Check(Descendants(form).Any(c=>c.Text=="EXPORT SETTINGS")&&Descendants(form).Any(c=>c.Text=="IMPORT SETTINGS"),"backup controls present");
                Check(form.Controls.Cast<Control>().All(c=>c.Bottom<=form.ClientSize.Height),"controls fit launcher height");
                using(var bitmap=new Bitmap(form.Width,form.Height)) { form.DrawToBitmap(bitmap,new Rectangle(Point.Empty,bitmap.Size));bitmap.Save("build/launcher-preview.png"); }
            }
        } finally { Directory.Delete(root,true); }
        Console.WriteLine("All settings backup checks passed.");
    }
}
