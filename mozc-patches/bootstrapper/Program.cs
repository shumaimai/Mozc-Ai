using System.Diagnostics;
using System.Reflection;
using System.Runtime.InteropServices;

internal static class Program
{
    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    private static extern int MessageBoxW(IntPtr hWnd, string text, string caption, uint type);

    private static int Main()
    {
        string tempDir = Path.Combine(
            Path.GetTempPath(),
            "MozcNiConjunction-" + Guid.NewGuid().ToString("N"));
        string msiPath = Path.Combine(tempDir, "Mozc64.msi");

        try
        {
            Directory.CreateDirectory(tempDir);

            using Stream? resource = Assembly.GetExecutingAssembly()
                .GetManifestResourceStream("Mozc64.msi");
            if (resource is null)
            {
                throw new InvalidOperationException("Embedded Mozc64.msi was not found.");
            }

            using (FileStream output = File.Create(msiPath))
            {
                resource.CopyTo(output);
            }

            ProcessStartInfo startInfo = new()
            {
                FileName = "msiexec.exe",
                Arguments = $"/i \"{msiPath}\"",
                UseShellExecute = true,
                Verb = "runas",
            };

            using Process? process = Process.Start(startInfo);
            if (process is null)
            {
                throw new InvalidOperationException("Failed to start Windows Installer.");
            }

            process.WaitForExit();
            return process.ExitCode is 0 or 1641 or 3010 ? 0 : process.ExitCode;
        }
        catch (Exception ex)
        {
            MessageBoxW(IntPtr.Zero, ex.Message, "Mozc Ni Conjunction Setup", 0x10);
            return 1;
        }
        finally
        {
            try
            {
                if (Directory.Exists(tempDir))
                {
                    Directory.Delete(tempDir, recursive: true);
                }
            }
            catch
            {
            }
        }
    }
}
