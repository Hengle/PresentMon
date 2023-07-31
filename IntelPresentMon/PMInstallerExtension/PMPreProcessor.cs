using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.IO;
using Microsoft.Tools.WindowsInstallerXml;

namespace pm_installer
{
    public class pm_wix_extension : WixExtension
    {
        private pmi_preprocessor_extension preprocessor_extension;

        public override PreprocessorExtension
           PreprocessorExtension
        {
            get
            {
                if (this.preprocessor_extension == null)
                {
                    this.preprocessor_extension =
                       new pmi_preprocessor_extension();
                }

                return this.preprocessor_extension;
            }
        }
    }

    public class pmi_preprocessor_extension :
        PreprocessorExtension
    {
        private static string[] prefixes = { "pm" };

        public override string[] Prefixes
        {
            get
            {
                return prefixes;
            }
        }

        public override string EvaluateFunction(
           string prefix, string function, string[] args)
        {
            string result = null;

            switch (prefix)
            {
                case "pm":
                    switch (function)
                    {
                        case "CheckFileExists":
                            if (args.Length > 0)
                            {
                                if(File.Exists(args[0]))
                                {
                                    result = "true";
                                } else
                                {
                                    result = "false";
                                }
                            }
                            else
                            {
                                result = "false";
                            }
                            break;
                        case "GetPmConsoleAppDir":
                            if (args.Length > 0)
                            {
                                DirectoryInfo di = Directory.GetParent(args[0]);
                                if (args[0].EndsWith(@"\") == true)
                                {
                                    // If the incoming path ends in a "\" we need to
                                    // call GetParent one more time
                                    di = Directory.GetParent(di.FullName);
                                }
                                // We only check for release when packaging up the PresentMon console application
                                string pm_console_full_path = Path.Combine(di.FullName, @"Build\Release\PresentMon-dev-x64.exe");
                                if (File.Exists(pm_console_full_path))
                                {
                                    result = pm_console_full_path;
                                }
                            }
                            break;
                        case "ConsoleAppExists":
                            if (args.Length > 0)
                            {
                                DirectoryInfo di = Directory.GetParent(args[0]);
                                if (args[0].EndsWith(@"\") == true)
                                {
                                    // If the incoming path ends in a "\" we need to
                                    // call GetParent one more time
                                    di = Directory.GetParent(di.FullName);
                                }
                                // We only check for release when packaging up the PresentMon console application
                                string pm_console_full_path = Path.Combine(di.FullName, @"Build\Release\PresentMon-dev-x64.exe");
                                if (File.Exists(pm_console_full_path))
                                {
                                    result = "true";
                                }
                                else
                                {
                                    result = "false";
                                }
                            }
                            else
                            {
                                result = "false";
                            }
                            break;
                    }
                    break;
            }
            return result;
        }
    }
}
