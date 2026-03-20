using ChatClient.Model;
using ChatClient.Service;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using System;
using System.Buffers.Binary;
using System.Net;
using System.Text;
using System.Windows;

namespace ChatClient.ViewModel
{
    internal partial class LoginViewModel : ObservableObject
    {
        private readonly MainViewModel _mainViewModel;
        private readonly NetworkClient _networkClient;

        [ObservableProperty]
        private string userId = string.Empty;

        [ObservableProperty]
        private string password = string.Empty;

        [ObservableProperty]
        private string statusMessage = string.Empty;

        [ObservableProperty]
        private bool isLoggingIn;

        public LoginViewModel(MainViewModel mainViewModel, NetworkClient net)
        {
            _mainViewModel = mainViewModel;
            _networkClient = net;

            _networkClient.PacketReceived += OnPacketReceived;
            _networkClient.SystemLog += OnSystemLog;
        }

        [RelayCommand]
        private void ClickLogin()
        {
            if (IsLoggingIn)
                return;

            if (string.IsNullOrWhiteSpace(UserId))
            {
                StatusMessage = "아이디를 입력하세요.";
                return;
            }

            if (string.IsNullOrWhiteSpace(Password))
            {
                StatusMessage = "비밀번호를 입력하세요.";
                return;
            }

            try
            {
                byte[] body = BuildPacketData(UserId, Password);
                bool ok = _networkClient.Send(CMDCODE.LoginRequest, body);
                if (!ok)
                {
                    StatusMessage = "로그인 요청 전송에 실패했습니다.";
                    return;
                }

                IsLoggingIn = true;
                StatusMessage = "로그인 중...";
            }
            catch (Exception ex)
            {
                StatusMessage = ex.Message;
            }
        }

        [RelayCommand]
        private void ClickRegister()
        {
            if (IsLoggingIn)
                return;

            if (string.IsNullOrWhiteSpace(UserId))
            {
                StatusMessage = "아이디를 입력하세요.";
                return;
            }

            if (string.IsNullOrWhiteSpace(Password))
            {
                StatusMessage = "비밀번호를 입력하세요.";
                return;
            }

            try
            {
                byte[] body = BuildPacketData(UserId, Password);
                bool ok = _networkClient.Send(CMDCODE.RegisterRequest, body);
                if (!ok)
                {
                    StatusMessage = "회원가입 요청 전송에 실패했습니다.";
                    return;
                }

                StatusMessage = "회원가입 요청 중...";
            }
            catch (Exception ex)
            {
                StatusMessage = ex.Message;
            }
        }

        byte[] BuildPacketData(string userId, string password)
        {
            var data = new byte[512]; // id[256] + pw[256]
            Array.Clear(data, 0, 512);

            byte[] encoded1 = Encoding.UTF8.GetBytes(userId);
            byte[] encoded2 = Encoding.UTF8.GetBytes(password);

            if (encoded1.Length >= 256 || encoded2.Length >= 256)
                throw new InvalidOperationException($"문자열이 너무 깁니다. 최대 255바이트까지 허용됩니다.");

            Buffer.BlockCopy(encoded1, 0, data, 0, encoded1.Length);
            Buffer.BlockCopy(encoded2, 0, data, 256, encoded2.Length);

            return data;
        }

        private void OnPacketReceived(NetworkClient.Packet packet)
        {
            //extHeader가 없을거라 가정.
            Application.Current.Dispatcher.Invoke(() =>
            {
                switch (packet.Cmd)
                {
                    case CMDCODE.LoginResponse:
                        HandleLoginResponse(packet.Payload);
                        break;

                    case CMDCODE.RegisterResponse:
                        HandleRegisterResponse(packet.Payload);
                        break;

                    case CMDCODE.SystemMessage:
                        StatusMessage = Encoding.UTF8.GetString(packet.Payload!);
                        break;
                }
            });
        }

        private void HandleLoginResponse(byte[]? payload)
        {
            IsLoggingIn = false;

            if (payload == null || payload.Length < sizeof(ushort))
            {
                StatusMessage = "잘못된 로그인 응답입니다.";
                return;
            }

            AuthResult result = ParseAuthResult(payload);

            switch (result)
            {
                case AuthResult.Success:
                    StatusMessage = "로그인 성공";
                    _mainViewModel.ShowChattingPage();
                    break;

                case AuthResult.InvalidId:
                    StatusMessage = "존재하지 않는 아이디입니다.";
                    break;

                case AuthResult.WrongPassword:
                    StatusMessage = "비밀번호가 틀렸습니다.";
                    break;

                case AuthResult.ServerError:
                    StatusMessage = "서버 오류가 발생했습니다.";
                    break;

                default:
                    StatusMessage = "알 수 없는 로그인 응답입니다.";
                    break;
            }
        }

        private void HandleRegisterResponse(byte[]? payload)
        {
            IsLoggingIn = false;

            if (payload == null || payload.Length < sizeof(ushort))
            {
                StatusMessage = "잘못된 회원가입 응답입니다.";
                return;
            }

            AuthResult result = ParseAuthResult(payload);

            switch (result)
            {
                case AuthResult.Success:
                    StatusMessage = "회원가입 성공";
                    break;

                case AuthResult.InvalidId:
                    StatusMessage = "아이디 또는 비밀번호가 올바르지 않습니다.";
                    break;

                case AuthResult.DuplicateId:
                    StatusMessage = "이미 존재하는 아이디입니다.";
                    break;

                case AuthResult.ServerError:
                    StatusMessage = "서버 오류가 발생했습니다.";
                    break;

                default:
                    StatusMessage = "알 수 없는 회원가입 응답입니다.";
                    break;
            }
        }

        private AuthResult ParseAuthResult(byte[] payload)
        {
            ushort raw = BitConverter.ToUInt16(payload, 0);
            raw = (ushort)IPAddress.NetworkToHostOrder((short)raw);
            return (AuthResult)raw;
        }

        private void OnSystemLog(string text)
        {
            Application.Current.Dispatcher.Invoke(() =>
            {
                StatusMessage = text;
            });
        }
    }
}