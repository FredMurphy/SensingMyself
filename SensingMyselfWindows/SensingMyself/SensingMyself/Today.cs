using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace SensingMyself
{
    public partial class Today : Form
    {
        public Today()
        {
            InitializeComponent();
        }

        private void okButton_Click(object sender, EventArgs e)
        {
            Close();
        }

        private void Today_Load(object sender, EventArgs e)
        {
            var service = Program.GetHeartService();
            var readings = service.GetToday();

            summaryLabel.Text = $"You have taken {readings.Count} readings for  {DateTime.Today.ToLongDateString()}";

            if (readings.Any())
            {
                var minHeartRate = readings.Min(r => r.HeartRate);
                minHeartRateLabel.Text = $"{minHeartRate} bpm";

                var maxHeartRate = readings.Max(r => r.HeartRate);
                maxHeartRateLabel.Text = $"{maxHeartRate} bpm";

                var minO2 = readings.Min(r => r.SpO2);
                minO2Label.Text = $"{minO2:#.00}%";

                var maxO2 = readings.Max(r => r.SpO2);
                maxO2Label.Text = $"{maxO2:#.00}%";
            } else
            {
                minHeartRateLabel.Text = maxHeartRateLabel.Text = minO2Label.Text = maxO2Label.Text = "-";
            }
        }
    }
}
