using System;
using System.Buffers.Binary;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace ChatClient.Model
{
    public enum CMDCODE : ushort
    {
        ChatMessage = 1,
        SystemMessage = 100,

        LoginRequest = 200,
        LoginResponse = 201,
        LogoutRequest = 300,
        LogoutResponse = 301,

        RegisterRequest = 400,
        RegisterResponse = 401,
    }

    public enum AuthResult : ushort
    {
        Success = 0,
        InvalidId = 1,
        WrongPassword = 2,
        DuplicateId = 3,
        ServerError = 4,
    }

    public static class Protocol
    {
        public const int HeaderSize = 8;

        public static byte[] BuildPacket(CMDCODE cmd, byte[] payload)
        {
            payload ??= Array.Empty<byte>();

            byte[] buffer = new byte[HeaderSize + payload.Length];

            BinaryPrimitives.WriteUInt16BigEndian(buffer.AsSpan(0, 2), (ushort)cmd);
            BinaryPrimitives.WriteUInt16BigEndian(buffer.AsSpan(2, 2), 0);  //클라이언트는 확장헤더 신경 안씀.
            BinaryPrimitives.WriteUInt32BigEndian(buffer.AsSpan(4, 4), (uint)payload.Length);

            if (payload.Length > 0)
            {
                payload.CopyTo(buffer, HeaderSize);
            }

            return buffer;
        }

        public static void ReadHeader(byte[] headerBuffer, out CMDCODE cmd, out ushort extHeaderSize, out uint payloadSize)
        {
            cmd = (CMDCODE)BinaryPrimitives.ReadUInt16BigEndian(headerBuffer.AsSpan(0, 2));
            extHeaderSize = BinaryPrimitives.ReadUInt16BigEndian(headerBuffer.AsSpan(2, 2));
            payloadSize = BinaryPrimitives.ReadUInt32BigEndian(headerBuffer.AsSpan(4, 4));
        }

        public static byte[] EncodeString(string text)
        {
            return Encoding.UTF8.GetBytes(text ?? string.Empty);
        }

        public static string DecodeString(byte[] data)
        {
            return Encoding.UTF8.GetString(data);
        }
    }
}
