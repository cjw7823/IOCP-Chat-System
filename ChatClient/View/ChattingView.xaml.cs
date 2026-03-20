using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
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
    /// <summary>
    /// ChattingPage.xaml에 대한 상호 작용 논리
    /// </summary>
    public partial class ChattingView : UserControl
    {
        public ChattingView()
        {
            InitializeComponent();
            //DataContext = new ViewModel.ChattingViewModel();
        }

        private void TextBox_TextChanged(object sender, TextChangedEventArgs e)
        {

        }

        private void ChatList_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {

        }

        private void InputBox_TextChanged(object sender, TextChangedEventArgs e)
        {
            if (string.IsNullOrWhiteSpace(InputBox.Text))
                watermarkText.Visibility = Visibility.Visible;
            else
                watermarkText.Visibility = Visibility.Collapsed;
        }
    }
}
