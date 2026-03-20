using ChatClient.ViewModel;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Printing;
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
    public partial class LoginView : UserControl
    {
        public LoginView()
        {
            InitializeComponent();
        }

        private void TextBox_TextChanged(object sender, TextChangedEventArgs e)
        {
            if (string.IsNullOrWhiteSpace(idBox.Text))
                watermarkText1.Visibility = Visibility.Visible;
            else
                watermarkText1.Visibility = Visibility.Collapsed;
        }

        private void pwBox_PasswordChanged(object sender, RoutedEventArgs e)
        {
            if (pwBox.Password.Length <= 0)
                watermarkText2.Visibility = Visibility.Visible;
            else
                watermarkText2.Visibility = Visibility.Collapsed;

            if (DataContext is LoginViewModel vm)
            {
                vm.Password = pwBox.Password;
            }
        }
    }
}
