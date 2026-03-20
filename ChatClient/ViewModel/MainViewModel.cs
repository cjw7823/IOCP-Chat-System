using ChatClient.Model;
using ChatClient.Service;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using System;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Windows;

namespace ChatClient.ViewModel
{
    public partial class MainViewModel : ObservableObject
    {
        [ObservableProperty]
        private object? currentViewModel;

        private readonly NetworkClient _networkClient = new();

        private readonly string ipAddr = "127.0.0.1";
        private readonly int port = 7575;

        public MainViewModel()
        {
            _networkClient.Connect(ipAddr, port);
            ShowLoginPage();
        }

        public void ShowChattingPage()
        {
            CurrentViewModel = new ChattingViewModel(this, _networkClient);
        }

        public void ShowLoginPage()
        {
            CurrentViewModel = new LoginViewModel(this, _networkClient);
        }
    }
}