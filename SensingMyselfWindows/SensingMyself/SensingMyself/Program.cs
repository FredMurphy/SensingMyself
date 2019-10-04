using System;
using System.Configuration;
using System.Text;
using System.Windows.Forms;
using Microsoft.Azure.Devices;

namespace SensingMyself
{
    static class Program
    {
        private static HeartService heartService;
        private static ServiceClient serviceClient;

        /// <summary>
        /// The main entry point for the application.
        /// </summary>
        [STAThread]
        static void Main()
        {
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            
            var menu = new ContextMenuStrip();
            menu.Items.Add("Today", null, TodayClick);
            menu.Items.Add("Take a reading", null, ReadingClick);
            menu.Items.Add("About", null, AboutClick);
            menu.Items.Add("Exit", null, ExitClick);

            var tray = new NotifyIcon
            {
                Visible = true,
                Text = "Sensing Myself",
                Icon = Properties.Resources.Heart,
                ContextMenuStrip = menu
            };

            Application.Run();

        }

        private static void ReadingClick(object sender, EventArgs e)
        {
            GetServiceClient().SendAsync(
                "SensingMe",
                new Microsoft.Azure.Devices.Message(Encoding.ASCII.GetBytes("Read")),
                TimeSpan.FromSeconds(10)).Wait();
        }

        private static void TodayClick(object sender, EventArgs e)
        {
            var readings = GetHeartService().GetToday();
        }

        private static void AboutClick(object sender, EventArgs e)
        {
            MessageBox.Show("about!");
        }
        private static void ExitClick(object sender, EventArgs e)
        {
            Application.Exit();
        }

        private static HeartService GetHeartService()
        {
            return heartService ?? (heartService = new HeartService());
        }

        private static ServiceClient GetServiceClient()
        {
            return serviceClient ?? (serviceClient = ServiceClient.CreateFromConnectionString(
                ConfigurationManager.ConnectionStrings["iotHub"].ConnectionString,
                TransportType.Amqp
                ));
        }
    }
}
