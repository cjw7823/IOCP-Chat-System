using ChatClient.Model;
using ChatClient.Service;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using System;
using System.Buffers.Binary;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Linq;
using System.Net;
using System.Text;
using System.Threading.Tasks;
using System.Windows;

namespace ChatClient.ViewModel
{
    internal partial class ChattingViewModel : ObservableObject
    {
        private readonly NetworkClient _networkClient;
        private readonly MainViewModel _mainViewModel;

        public ObservableCollection<ChatMessage> Messages { get; } = new();

        [ObservableProperty]
        private string inputText = string.Empty;

        public ChattingViewModel(MainViewModel mainViewModel, NetworkClient net)
        {
            _mainViewModel = mainViewModel;
            _networkClient = net;

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

        private void OnPacketReceived(NetworkClient.Packet packet)
        {
            ushort senderIdLen = BinaryPrimitives.ReadUInt16BigEndian(packet.ExtHeader.AsSpan(0, 2));
            ushort messageLen = BinaryPrimitives.ReadUInt16BigEndian(packet.ExtHeader.AsSpan(2, 2));

            ReadOnlySpan<byte> payload = packet.Payload;
            string id = Encoding.UTF8.GetString(payload.Slice(0, senderIdLen));
            string message = Encoding.UTF8.GetString(payload.Slice(senderIdLen, messageLen));

            Application.Current.Dispatcher.Invoke(() =>
            {
                switch (packet.Cmd)
                {
                    case CMDCODE.ChatMessage:
                        Messages.Add(new ChatMessage
                        {
                            SenderName = id,
                            Text = message,
                            TimeText = DateTime.Now.ToString("tt h:mm"),
                            IsMine = false
                        });
                        break;

                    case CMDCODE.SystemMessage:
                        Messages.Add(new ChatMessage
                        {
                            SenderName = "시스템",
                            Text = message,
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

        [RelayCommand]
        private void GoToLogin()
        {
            _mainViewModel.ShowLoginPage();
        }
    }
}
