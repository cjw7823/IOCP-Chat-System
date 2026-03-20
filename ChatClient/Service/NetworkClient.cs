using ChatClient.Model;
using System;
using System.IO;
using System.Net.Sockets;
using System.Threading;

namespace ChatClient.Service
{
    public class NetworkClient : IDisposable
    {
        private TcpClient? _client;
        private NetworkStream? _stream;
        private Thread? _receiveThread;
        private volatile bool _running;
        public class Packet
        {
            public CMDCODE Cmd;
            public byte[]? ExtHeader;   //ushort,ushort
            public byte[]? Payload;
        }

        public event Action<Packet>? PacketReceived;
        public event Action<string>? SystemLog;

        public bool IsConnected => _client?.Connected == true;

        public bool Connect(string ip, int port)
        {
            try
            {
                _client = new TcpClient();
                _client.Connect(ip, port);
                _stream = _client.GetStream();

                _running = true;
                _receiveThread = new Thread(ReceiveLoop)
                {
                    IsBackground = true
                };
                _receiveThread.Start();

                SystemLog?.Invoke("서버 연결 성공");
                return true;
            }
            catch (Exception ex)
            {
                SystemLog?.Invoke($"서버 연결 실패: {ex.Message}");
                return false;
            }
        }

        public void Disconnect()
        {
            _running = false;

            try { _stream?.Close(); } catch { }
            try { _client?.Close(); } catch { }

            _stream = null;
            _client = null;
        }

        public bool Send(CMDCODE cmd, string text)
        {
            if (_stream == null)
                return false;

            try
            {
                byte[] payload = Protocol.EncodeString(text);
                byte[] packet = Protocol.BuildPacket(cmd, payload);
                _stream.Write(packet, 0, packet.Length);
                _stream.Flush();
                return true;
            }
            catch (Exception ex)
            {
                SystemLog?.Invoke($"전송 실패: {ex.Message}");
                return false;
            }
        }

        public bool Send(CMDCODE cmd, byte[] data)
        {
            if (_stream == null)
                return false;

            try
            {
                byte[] payload = data;
                byte[] packet = Protocol.BuildPacket(cmd, payload);
                _stream.Write(packet, 0, packet.Length);
                _stream.Flush();
                return true;
            }
            catch (Exception ex)
            {
                SystemLog?.Invoke($"전송 실패: {ex.Message}");
                return false;
            }
        }

        private void ReceiveLoop()
        {
            try
            {
                while (_running && _stream != null)
                {
                    byte[] headerBuffer = ReadExact(_stream, Protocol.HeaderSize);
                    Protocol.ReadHeader(headerBuffer, out CMDCODE cmd, out ushort extHeaderSize, out uint payloadSize);

                    switch (cmd)
                    {
                        case CMDCODE.ChatMessage:
                            if (payloadSize == 0) continue;
                            break;

                        case CMDCODE.SystemMessage:
                            // payload 없어도 의미 있을 수 있음
                            break;

                        case CMDCODE.LoginResponse:
                            if (payloadSize == 0) continue;
                            break;

                        case CMDCODE.RegisterResponse:
                            if (payloadSize == 0) continue;
                            break;

                        default:
                            // 알 수 없는 cmd면 payload는 소비해야 스트림 정렬이 안 깨짐
                            if (payloadSize > 0)
                                _ = ReadExact(_stream, (int)payloadSize);
                            continue;
                    }

                    byte[] extHeaderBuffer = extHeaderSize > 0
                        ? ReadExact(_stream, (int)extHeaderSize)
                        : Array.Empty<byte>();

                    byte[] payloadBuffer = payloadSize > 0
                        ? ReadExact(_stream, (int)payloadSize)
                        : Array.Empty<byte>();

                    Packet packet = new Packet();
                    packet.Cmd = cmd;
                    packet.ExtHeader = extHeaderBuffer;
                    packet.Payload = payloadBuffer;

                    PacketReceived?.Invoke(packet); //payloadBuffer는 ntoh안함.
                }
            }
            catch (Exception ex)
            {
                if (_running)
                {
                    SystemLog?.Invoke($"수신 종료: {ex.Message}");
                }
            }
            finally
            {
                Disconnect();
            }
        }

        private static byte[] ReadExact(Stream stream, int size)
        {
            byte[] buffer = new byte[size];
            int offset = 0;

            while (offset < size)
            {
                int read = stream.Read(buffer, offset, size - offset);
                if (read == 0)
                    throw new IOException("서버 연결이 종료되었습니다.");

                offset += read;
            }

            return buffer;
        }

        public void Dispose()
        {
            Disconnect();
        }
    }
}