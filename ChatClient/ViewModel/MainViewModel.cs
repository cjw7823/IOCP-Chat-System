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
        private readonly NetworkClient _networkClient = new();

        [ObservableProperty]
        private string title = "채팅창";

        public ObservableCollection<ChatMessage> Messages { get; } = new();

        [ObservableProperty]
        private string inputText = string.Empty;

        public MainViewModel()
        {
            if (DesignerProperties.GetIsInDesignMode(new DependencyObject()))
            {
                Messages.Add(new ChatMessage
                {
                    SenderName = "강희은",
                    Text = "바이브 코딩으로 해봐",
                    TimeText = "오후 5:48",
                    IsMine = false
                });

                Messages.Add(new ChatMessage
                {
                    SenderName = "나",
                    Text = "광고에서 겁나봤던거네요",
                    TimeText = "오후 5:55",
                    IsMine = true
                });

                Messages.Add(new ChatMessage
                {
                    SenderName = "나",
                    Text = "네엡...도전해볼게요..!!",
                    TimeText = "오후 5:56",
                    IsMine = true
                });
            }

            _networkClient.PacketReceived += OnPacketReceived;
            _networkClient.SystemLog += OnSystemLog;

            _networkClient.Connect("127.0.0.1", 7575);
        }

        [RelayCommand]
        private void Send()
        {
            if (string.IsNullOrWhiteSpace(InputText))
                return;

            string text = InputText.Trim();

            bool ok = _networkClient.Send(CMDCODE.ChatMessage, text);
            if (!ok)
                return;

            Messages.Add(new ChatMessage
            {
                SenderName = "나",
                Text = text,
                TimeText = DateTime.Now.ToString("tt h:mm"),
                IsMine = true
            });

            InputText = string.Empty;
        }

        private void OnPacketReceived(CMDCODE cmd, string text)
        {
            Application.Current.Dispatcher.Invoke(() =>
            {
                switch (cmd)
                {
                    case CMDCODE.ChatMessage:
                        Messages.Add(new ChatMessage
                        {
                            SenderName = "상대",
                            Text = text,
                            TimeText = DateTime.Now.ToString("tt h:mm"),
                            IsMine = false
                        });
                        break;

                    case CMDCODE.SystemMessage:
                        Messages.Add(new ChatMessage
                        {
                            SenderName = "시스템",
                            Text = text,
                            TimeText = DateTime.Now.ToString("tt h:mm"),
                            IsMine = false
                        });
                        break;
                }
            });
        }

        private void OnSystemLog(string text)
        {
            Application.Current.Dispatcher.Invoke(() =>
            {
                Messages.Add(new ChatMessage
                {
                    SenderName = "시스템",
                    Text = text,
                    TimeText = DateTime.Now.ToString("tt h:mm"),
                    IsMine = false
                });
            });
        }
    }
}