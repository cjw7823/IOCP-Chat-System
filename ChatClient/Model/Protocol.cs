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
        Login = 200,
        Logout = 300
    }

    public static class Protocol
    {
        public const int HeaderSize = 6;

        public static byte[] BuildPacket(CMDCODE cmd, byte[] payload)
        {
            payload ??= Array.Empty<byte>();

            byte[] buffer = new byte[HeaderSize + payload.Length];

            BinaryPrimitives.WriteUInt16BigEndian(buffer.AsSpan(0, 2), (ushort)cmd);
            BinaryPrimitives.WriteUInt32BigEndian(buffer.AsSpan(2, 4), (uint)payload.Length);

            if (payload.Length > 0)
            {
                payload.CopyTo(buffer, HeaderSize);
            }

            return buffer;
        }

        public static void ReadHeader(byte[] headerBuffer, out CMDCODE cmd, out uint payloadSize)
        {
            cmd = (CMDCODE)BinaryPrimitives.ReadUInt16BigEndian(headerBuffer.AsSpan(0, 2));
            payloadSize = BinaryPrimitives.ReadUInt32BigEndian(headerBuffer.AsSpan(2, 4));
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
