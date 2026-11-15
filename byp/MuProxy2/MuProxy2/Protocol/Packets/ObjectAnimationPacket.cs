namespace WinFormsMitm.Protocol.Packets
{
    /// <summary>
    /// C1 18 - ObjectAnimation (by server).
    /// Sent when an object performs an animation (attack, skill, etc).
    /// 
    /// Structure (9 bytes):
    ///   [0]    C1          Packet type
    ///   [1]    09          Packet length
    ///   [2]    18          Packet type identifier (opcode)
    ///   [3..4] ObjectId    ShortBigEndian — entity performing the animation
    ///   [5]    Direction   Byte — direction of the animation
    ///   [6]    Animation   Byte — animation type (0x78 = attack)
    ///   [7..8] TargetId    ShortBigEndian — target entity (0 if no target)
    /// </summary>
    [Packet(PacketType.ObjectAnimation, 0x18, 0x00)]
    public class ObjectAnimationPacket : IPacket
    {
        public PacketId Id => new PacketId(0x18, 0x00);

        [PacketField(3, Size = 2)]
        public ushort ObjectId { get; set; }

        [PacketField(5, Size = 1)]
        public byte Direction { get; set; }

        [PacketField(6, Size = 1)]
        public byte Animation { get; set; }

        [PacketField(7, Size = 2)]
        public ushort TargetId { get; set; }
    }
}