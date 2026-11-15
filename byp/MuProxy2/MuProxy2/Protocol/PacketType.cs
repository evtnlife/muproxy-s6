namespace WinFormsMitm.Protocol
{
    public enum PacketType
    {
        Unknown,
        // KeepAlive
        Ping, Pong, ClientReady, Heartbeat,
        // Character
        Damage, MonsterDie, UserDie, Life, Mana,
        FruitResult, RewardExperience, QuestReward,
        MapServerMove, ConnectClient, CloseClient,
        CharacterList, CharacterInfo, CharacterRegen, LevelUp, LevelUpPoint,
        MonsterDamage, MasterInfo, MasterLevelUp,
        NewCharacterInfo, NewCharacterCalc, GensBattleInfo, NewMessage,
        // Entity
        EntityUpdate, EntityPosition, EntityRangeCheck, EntityLookup, EntityAlloc, EntityProcess, EntityMove,
        ObjectList, EntitySpawn, EntityInfo, EntityAppearance, ObjectAnimation,
        // Server
        ServerAck, ServerInfo, ResetState,
        // Client actions
        CreateCharacter, SelectCharacter, CharacterUpdate,
        WalkTo, MouseMove, AttackMonster,
        SkillAttack, SkillAttackLarge, SkillEffect,
        // World
        Chat, StringMessage, MapState, WeatherTime, SimpleNotification,
        // Unknown SEND
        SendAction10, SendAction83,

        // RECV extras
        MainCheck, OptionData, NpcTalk, ItemMove, QuestWorld,
        // F3 sub extras
        CharacterNameCheck, Inventory, Warehouse, ChaosMix,
        SkillList, SkillListMulti,
        // F6 sub
        QuestWorldDialog,
        Other
    }
}
