using CommunityToolkit.Mvvm.ComponentModel;
using System.Text;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Navigation;
using System.Windows.Shapes;

namespace ChatClient.View
{
    public partial class MainWindow : Window
    {
        [ObservableProperty]
        private string title = "채팅창";

        public MainWindow()
        {
            InitializeComponent();
            DataContext = new ViewModel.MainViewModel();
        }
    }
}