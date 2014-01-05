#include "utils/async_if_test_helper.hxx"

#include "nmranet/NMRAnetWriteFlow.hxx"

namespace NMRAnet
{

class MockCanFrameHandler : public IncomingFrameHandler
{
public:
    MOCK_METHOD0(get_allocator, AllocatorBase*());
    MOCK_METHOD2(handle_message,
                 void(struct can_frame* message, Notifiable* done));
};

MATCHER_P(IsExtCanFrameWithId, id, "")
{
    if (!IS_CAN_FRAME_EFF(*arg))
        return false;
    return ((uint32_t)id) == GET_CAN_FRAME_ID_EFF(*arg);
}

TEST_F(AsyncIfTest, Setup)
{
}

TEST_F(AsyncIfTest, InjectFrame)
{
    SendPacket(":X195B432DN05010103;");
    Wait();
}

TEST_F(AsyncIfTest, InjectFrameAndExpectHandler)
{
    StrictMock<MockCanFrameHandler> h;
    if_can_->frame_dispatcher()->RegisterHandler(0x195B4000, 0x1FFFF000, &h);
    EXPECT_CALL(h, get_allocator()).WillRepeatedly(Return(nullptr));
    EXPECT_CALL(h, handle_message(IsExtCanFrameWithId(0x195B432D), _))
        .WillOnce(WithArg<1>(Invoke(&InvokeNotification)));

    SendPacket(":X195B432DN05010103;");
    Wait();

    SendPacket(":X195F432DN05010103;");
    SendPacket(":X195F432DN05010103;");

    Wait();
    EXPECT_CALL(h, get_allocator()).WillRepeatedly(Return(nullptr));
    EXPECT_CALL(h, handle_message(IsExtCanFrameWithId(0x195B4777), _))
        .WillOnce(WithArg<1>(Invoke(&InvokeNotification)));
    EXPECT_CALL(h, handle_message(IsExtCanFrameWithId(0x195B4222), _))
        .WillOnce(WithArg<1>(Invoke(&InvokeNotification)));
    SendPacket(":X195B4777N05010103;");
    SendPacket(":X195F4333N05010103;");
    SendPacket(":X195B4222N05010103;");
    Wait();
    if_can_->frame_dispatcher()->UnregisterHandler(0x195B4000, 0x1FFFF000, &h);
}

TEST_F(AsyncIfTest, WriteFrame)
{
    ExpectPacket(":X195B432DNAA;");
    TypedSyncAllocation<CanFrameWriteFlow> w(if_can_->write_allocator());
    struct can_frame* f = w.result()->mutable_frame();
    SET_CAN_FRAME_EFF(*f);
    SET_CAN_FRAME_ID_EFF(*f, 0x195B432D);
    f->can_dlc = 1;
    f->data[0] = 0xaa;
    w.result()->Send(nullptr);
}

TEST_F(AsyncIfTest, WriteMultipleFrames)
{
    EXPECT_CALL(can_bus_, MWrite(":X195B432DNAA;")).Times(10);
    for (int i = 0; i < 10; ++i)
    {
        TypedSyncAllocation<CanFrameWriteFlow> w(if_can_->write_allocator());
        struct can_frame* f = w.result()->mutable_frame();
        SET_CAN_FRAME_EFF(*f);
        SET_CAN_FRAME_ID_EFF(*f, 0x195B432D);
        f->can_dlc = 1;
        f->data[0] = 0xaa;
        w.result()->Send(nullptr);
        TypedSyncAllocation<CanFrameWriteFlow> ww(if_can_->write_allocator());
        SET_CAN_FRAME_RTR(*ww.result()->mutable_frame());
        ww.result()->Cancel();
    }
}

class AsyncMessageCanTests : public AsyncIfTest
{
protected:
    AsyncMessageCanTests()
    {
        if_can_->add_addressed_message_support(2);
    }
};

TEST_F(AsyncMessageCanTests, WriteByMTI)
{
    TypedSyncAllocation<WriteFlow> falloc(if_can_->global_write_allocator());

    ExpectPacket(":X195B422AN0102030405060708;");
    falloc.result()->WriteGlobalMessage(If::MTI_EVENT_REPORT, TEST_NODE_ID,
                                        EventIdToBuffer(0x0102030405060708ULL),
                                        nullptr);
}

TEST_F(AsyncMessageCanTests, WriteByMTIShort)
{
    TypedSyncAllocation<WriteFlow> falloc(if_can_->global_write_allocator());

    ExpectPacket(":X195B422AN3132333435;");
    Buffer* b = buffer_alloc(5);
    const char data[] = "12345";
    memcpy(b->start(), data, 5);
    b->advance(5);
    falloc.result()->WriteGlobalMessage(If::MTI_EVENT_REPORT, TEST_NODE_ID, b,
                                        nullptr);
}

TEST_F(AsyncMessageCanTests, WriteByMTIAddressedShort)
{
    TypedSyncAllocation<WriteFlow> falloc(if_can_->global_write_allocator());

    ExpectPacket(":X1982822AN00003132333435;");
    Buffer* b = buffer_alloc(5);
    const char data[] = "12345";
    memcpy(b->start(), data, 5);
    b->advance(5);
    falloc.result()->WriteGlobalMessage(If::MTI_PROTOCOL_SUPPORT_INQUIRY,
                                        TEST_NODE_ID, b, nullptr);
}

TEST_F(AsyncMessageCanTests, WriteByMTIAddressedFragmented)
{
    TypedSyncAllocation<WriteFlow> falloc(if_can_->global_write_allocator());

    ExpectPacket(":X1982822AN1000303132333435;"); // first frame
    ExpectPacket(":X1982822AN3000363738393031;"); // middle frame
    ExpectPacket(":X1982822AN3000323334353637;"); // middle frame
    ExpectPacket(":X1982822AN20003839;");         // last frame
    Buffer* b = buffer_alloc(20);
    const char data[] = "01234567890123456789";
    memcpy(b->start(), data, 20);
    b->advance(20);
    /** This is somewhat cheating, because we use the global message write flow
     * to send an addressed message. @TODO(balazs.racz): replace this with
     * addressed write flow once that is ready and working. Add checks for this
     * not to happen in production. */
    falloc.result()->WriteGlobalMessage(If::MTI_PROTOCOL_SUPPORT_INQUIRY,
                                        TEST_NODE_ID, b, nullptr);
}

TEST_F(AsyncMessageCanTests, WriteByMTIMultiple)
{
    EXPECT_CALL(can_bus_, MWrite(":X195B422AN0102030405060708;")).Times(100);
    for (int i = 0; i < 100; ++i)
    {
        TypedSyncAllocation<WriteFlow> falloc(
            if_can_->global_write_allocator());
        falloc.result()->WriteGlobalMessage(
            If::MTI_EVENT_REPORT, TEST_NODE_ID,
            EventIdToBuffer(0x0102030405060708ULL), nullptr);
    }
}

TEST_F(AsyncMessageCanTests, WriteByMTIIgnoreDatagram)
{
    TypedSyncAllocation<WriteFlow> falloc(if_can_->global_write_allocator());

    EXPECT_CALL(can_bus_, MWrite(_)).Times(0);
    falloc.result()->WriteGlobalMessage(If::MTI_DATAGRAM, TEST_NODE_ID,
                                        EventIdToBuffer(0x0102030405060708ULL),
                                        nullptr);
}

class MockMessageHandler : public IncomingMessageHandler
{
public:
    MOCK_METHOD0(get_allocator, AllocatorBase*());
    MOCK_METHOD2(handle_message,
                 void(IncomingMessage* message, Notifiable* done));
};

MATCHER_P(IsBufferValue, id, "")
{
    uint64_t value = htobe64(id);
    if (arg->used() != 8)
        return false;
    if (memcmp(&value, arg->start(), 8))
        return false;
    return true;
}

MATCHER_P(IsBufferNodeValue, id, "")
{
    uint64_t value = htobe64(id);
    if (arg->used() != 6)
        return false;
    uint8_t* expected = reinterpret_cast<uint8_t*>(&value) + 2;
    uint8_t* actual = static_cast<uint8_t*>(arg->start());
    if (memcmp(expected, actual, 6))
    {
        for (int i = 0; i < 6; ++i)
        {
            if (expected[i] != actual[i])
            {
                LOG(INFO, "mismatch at position %d, expected %02x actual %02x",
                    i, expected[i], actual[i]);
            }
        }
        return false;
    }
    return true;
}

TEST_F(AsyncMessageCanTests, WriteByMTIGlobalDoesLoopback)
{
    StrictMock<MockMessageHandler> h;
    EXPECT_CALL(h, get_allocator()).WillRepeatedly(Return(nullptr));
    EXPECT_CALL(
        h, handle_message(
               Pointee(AllOf(Field(&IncomingMessage::mti, If::MTI_EVENT_REPORT),
                             Field(&IncomingMessage::payload, NotNull()),
                             Field(&IncomingMessage::payload,
                                   IsBufferValue(0x0102030405060708ULL)))),
               _)).WillOnce(WithArg<1>(Invoke(&InvokeNotification)));
    if_can_->dispatcher()->RegisterHandler(0, 0, &h);

    TypedSyncAllocation<WriteFlow> falloc(if_can_->global_write_allocator());
    ExpectPacket(":X195B422AN0102030405060708;");
    falloc.result()->WriteGlobalMessage(If::MTI_EVENT_REPORT, TEST_NODE_ID,
                                        EventIdToBuffer(0x0102030405060708ULL),
                                        nullptr);
    Wait();
}

TEST_F(AsyncNodeTest, WriteByMTIAddressedDoesLoopback)
{
    StrictMock<MockMessageHandler> h;
    EXPECT_CALL(h, get_allocator()).WillRepeatedly(Return(nullptr));
    EXPECT_CALL(
        h,
        handle_message(
            Pointee(AllOf(
                Field(&IncomingMessage::mti, If::MTI_EVENTS_IDENTIFY_ADDRESSED),
                Field(&IncomingMessage::payload, NotNull()),
                Field(&IncomingMessage::payload,
                      IsBufferValue(0x0102030405060708ULL)),
                Field(&IncomingMessage::dst, Field(&NodeHandle::alias, 0x22A)),
                Field(&IncomingMessage::dst,
                      Field(&NodeHandle::id, TEST_NODE_ID)),
                Field(&IncomingMessage::dst_node, node_))),
            _)).WillOnce(WithArg<1>(Invoke(&InvokeNotification)));
    if_can_->dispatcher()->RegisterHandler(0, 0, &h);

    TypedSyncAllocation<WriteFlow> falloc(if_can_->addressed_write_allocator());
    SyncNotifiable n;
    /** Here we are using a new source node ID number, which would normally
     * trigger an alias allocation. However, since the message never makes it
     * to the canbus (is looped back), that does not happen.*/
    falloc.result()->WriteAddressedMessage(
        If::MTI_EVENTS_IDENTIFY_ADDRESSED, TEST_NODE_ID + 1,
        {TEST_NODE_ID, 0x22A}, EventIdToBuffer(0x0102030405060708ULL), &n);
    n.WaitForNotification();
    Wait();
}

TEST_F(AsyncMessageCanTests, WriteByMTIAllocatesLocalAlias)
{
    TypedSyncAllocation<WriteFlow> falloc(if_can_->global_write_allocator());

    CreateAllocatedAlias();
    ExpectNextAliasAllocation();
    ExpectPacket(":X1070133AN02010D000004;");
    ExpectPacket(":X195B433AN0102030405060708;");
    SyncNotifiable n;
    falloc.result()->WriteGlobalMessage(If::MTI_EVENT_REPORT, TEST_NODE_ID + 1,
                                        EventIdToBuffer(0x0102030405060708ULL),
                                        &n);
    n.WaitForNotification();
    EXPECT_EQ(0x33AU, if_can_->local_aliases()->lookup(TEST_NODE_ID + 1));
    EXPECT_EQ(TEST_NODE_ID + 1,
              if_can_->local_aliases()->lookup(NodeAlias(0x33A)));
}

TEST_F(AsyncMessageCanTests, AliasConflictAllocatedNode)
{
    // This alias is in the cache since the setup routine.
    EXPECT_EQ(TEST_NODE_ID, if_can_->local_aliases()->lookup(NodeAlias(0x22A)));
    // If someone else uses it (not for CID frame)
    SendPacket(":X1800022AN;");
    Wait();
    // Then it disappears from there.
    EXPECT_EQ(0U, if_can_->local_aliases()->lookup(NodeAlias(0x22A)));
}

TEST_F(AsyncMessageCanTests, AliasConflictCIDReply)
{
    // This alias is in the cache since the setup routine.
    EXPECT_EQ(TEST_NODE_ID, if_can_->local_aliases()->lookup(NodeAlias(0x22A)));
    // If someone else sends a CID frame, then we respond with an RID frame
    ExpectPacket(":X1070022AN;");
    SendPacket(":X1700022AN;");
    Wait();

    ExpectPacket(":X1070022AN;");
    SendPacket(":X1612322AN;");
    Wait();

    ExpectPacket(":X1070022AN;");
    SendPacket(":X1545622AN;");
    Wait();

    ExpectPacket(":X1070022AN;");
    SendPacket(":X1478922AN;");
    Wait();

    // And we still have it in the cache.
    EXPECT_EQ(TEST_NODE_ID, if_can_->local_aliases()->lookup(NodeAlias(0x22A)));
}

TEST_F(AsyncMessageCanTests, ReservedAliasReclaimed)
{
    if_can_->local_aliases()->remove(NodeAlias(0x22A)); // resets the cache.
    TypedSyncAllocation<WriteFlow> falloc(if_can_->global_write_allocator());
    CreateAllocatedAlias();
    ExpectNextAliasAllocation();
    ExpectPacket(":X1070133AN02010D000003;");
    ExpectPacket(":X195B433AN0102030405060708;");
    falloc.result()->WriteGlobalMessage(If::MTI_EVENT_REPORT, TEST_NODE_ID,
                                        EventIdToBuffer(0x0102030405060708ULL),
                                        nullptr);
    usleep(250000);
    Wait();
    // Here we have the next reserved alias.
    EXPECT_EQ(AliasCache::RESERVED_ALIAS_NODE_ID,
              if_can_->local_aliases()->lookup(NodeAlias(0x44C)));
    // A CID packet gets replied to.
    ExpectPacket(":X1070044CN;");
    SendPacket(":X1478944CN;");
    Wait();
    // We still have it in the cache.
    EXPECT_EQ(AliasCache::RESERVED_ALIAS_NODE_ID,
              if_can_->local_aliases()->lookup(NodeAlias(0x44C)));
    // We kick it out with a regular frame.
    SendPacket(":X1800044CN;");
    Wait();
    EXPECT_EQ(0U, if_can_->local_aliases()->lookup(NodeAlias(0x44C)));
    // At this point we have an invalid alias in the reserved_aliases()
    // queue. We check here that a new node gets a new alias.
    ExpectNextAliasAllocation();
    // Unfortunately we have to guess the second next alias here because we
    // can't inject it. We can only inject one alias at a time, but now two
    // will be allocated in one go.
    ExpectNextAliasAllocation(0x6AA);
    ExpectPacket(":X1070144DN02010D000004;");
    ExpectPacket(":X195B444DN0102030405060709;");
    SyncNotifiable n;
    TypedSyncAllocation<WriteFlow> ffalloc(if_can_->global_write_allocator());
    ffalloc.result()->WriteGlobalMessage(If::MTI_EVENT_REPORT, TEST_NODE_ID + 1,
                                         EventIdToBuffer(0x0102030405060709ULL),
                                         &n);
    n.WaitForNotification();
    EXPECT_EQ(TEST_NODE_ID + 1,
              if_can_->local_aliases()->lookup(NodeAlias(0x44D)));
    usleep(250000);
    EXPECT_EQ(AliasCache::RESERVED_ALIAS_NODE_ID,
              if_can_->local_aliases()->lookup(NodeAlias(0x6AA)));
}

TEST_F(AsyncIfTest, PassGlobalMessageToIf)
{
    static const NodeAlias alias = 0x210U;
    static const NodeID id = 0x050101FFFFDDULL;
    StrictMock<MockMessageHandler> h;
    EXPECT_CALL(h, get_allocator()).WillRepeatedly(Return(nullptr));
    EXPECT_CALL(
        h,
        handle_message(
            Pointee(AllOf(
                Field(&IncomingMessage::mti, If::MTI_EVENT_REPORT),
                Field(&IncomingMessage::src, Field(&NodeHandle::alias, alias)),
                Field(&IncomingMessage::src, Field(&NodeHandle::id, id)),
                Field(&IncomingMessage::dst, WriteHelper::global()),
                Field(&IncomingMessage::dst_node, IsNull()),
                Field(&IncomingMessage::payload, NotNull()),
                Field(&IncomingMessage::payload,
                      IsBufferValue(0x0102030405060708ULL)))),
            _)).WillOnce(WithArg<1>(Invoke(&InvokeNotification)));
    if_can_->dispatcher()->RegisterHandler(0x5B4, 0xffff, &h);

    if_can_->remote_aliases()->add(id, alias);

    SendPacket(":X195B4210N0102030405060708;");
    Wait();
}

TEST_F(AsyncIfTest, PassGlobalMessageToIfUnknownSource)
{
    static const NodeAlias alias = 0x210U;
    StrictMock<MockMessageHandler> h;
    EXPECT_CALL(h, get_allocator()).WillRepeatedly(Return(nullptr));
    EXPECT_CALL(
        h,
        handle_message(
            Pointee(AllOf(
                Field(&IncomingMessage::mti, If::MTI_EVENT_REPORT),
                Field(&IncomingMessage::src, Field(&NodeHandle::alias, alias)),
                Field(&IncomingMessage::src, Field(&NodeHandle::id, 0)),
                Field(&IncomingMessage::dst, WriteHelper::global()),
                Field(&IncomingMessage::dst_node, IsNull()),
                Field(&IncomingMessage::payload, NotNull()),
                Field(&IncomingMessage::payload,
                      IsBufferValue(0x0102030405060708ULL)))),
            _)).WillOnce(WithArg<1>(Invoke(&InvokeNotification)));
    if_can_->dispatcher()->RegisterHandler(0x5B4, 0xffff, &h);

    SendPacket(":X195B4210N0102030405060708;");
    Wait();
}

TEST_F(AsyncNodeTest, PassAddressedMessageToIf)
{
    static const NodeAlias alias = 0x210U;
    static const NodeID id = 0x050101FFFFDDULL;
    StrictMock<MockMessageHandler> h;
    EXPECT_CALL(h, get_allocator()).WillRepeatedly(Return(nullptr));
    EXPECT_CALL(
        h,
        handle_message(
            Pointee(AllOf(
                Field(&IncomingMessage::mti, If::MTI_VERIFY_NODE_ID_ADDRESSED),
                Field(&IncomingMessage::src, Field(&NodeHandle::alias, alias)),
                Field(&IncomingMessage::src, Field(&NodeHandle::id, id)),
                Field(&IncomingMessage::dst, Field(&NodeHandle::alias, 0x22A)),
                Field(&IncomingMessage::dst,
                      Field(&NodeHandle::id, TEST_NODE_ID)),
                Field(&IncomingMessage::dst_node, node_),
                Field(&IncomingMessage::payload, IsNull()))),
            _)).WillOnce(WithArg<1>(Invoke(&InvokeNotification)));
    if_can_->dispatcher()->RegisterHandler(0x488, 0xffff, &h);

    if_can_->remote_aliases()->add(id, alias);

    SendPacket(":X19488210N022A;");
    Wait();
}

TEST_F(AsyncNodeTest, PassAddressedMessageToIfWithPayloadUnknownSource)
{
    static const NodeAlias alias = 0x210U;
    StrictMock<MockMessageHandler> h;
    EXPECT_CALL(h, get_allocator()).WillRepeatedly(Return(nullptr));
    EXPECT_CALL(
        h,
        handle_message(
            Pointee(AllOf(
                Field(&IncomingMessage::mti, If::MTI_VERIFY_NODE_ID_ADDRESSED),
                Field(&IncomingMessage::src, Field(&NodeHandle::alias, alias)),
                Field(&IncomingMessage::src, Field(&NodeHandle::id, 0)),
                Field(&IncomingMessage::dst, Field(&NodeHandle::alias, 0x22A)),
                Field(&IncomingMessage::dst,
                      Field(&NodeHandle::id, TEST_NODE_ID)),
                Field(&IncomingMessage::dst_node, node_),
                Field(&IncomingMessage::payload, NotNull()),
                Field(&IncomingMessage::payload,
                      IsBufferNodeValue(0x010203040506ULL)))),
            _)).WillOnce(WithArg<1>(Invoke(&InvokeNotification)));
    if_can_->dispatcher()->RegisterHandler(0x488, 0xffff, &h);

    SendPacket(":X19488210N022A010203040506;");
    Wait();
}

TEST_F(AsyncNodeTest, SendAddressedMessageToAlias)
{
    static const NodeAlias alias = 0x210U;
    static const NodeID id = 0x050101FFFFDDULL;

    ExpectPacket(":X1948822AN0210050101FFFFDD;");
    TypedSyncAllocation<WriteFlow> falloc(if_can_->addressed_write_allocator());
    SyncNotifiable n;
    falloc.result()->WriteAddressedMessage(If::MTI_VERIFY_NODE_ID_ADDRESSED,
                                           TEST_NODE_ID, {0, alias},
                                           node_id_to_buffer(id), &n);
    n.WaitForNotification();
}

TEST_F(AsyncNodeTest, SendAddressedMessageToNodeWithCachedAlias)
{
    static const NodeAlias alias = 0x210U;
    static const NodeID id = 0x050101FFFFDDULL;

    ExpectPacket(":X1948822AN0210050101FFFFDD;");
    TypedSyncAllocation<WriteFlow> falloc(if_can_->addressed_write_allocator());
    SyncNotifiable n;
    if_can_->remote_aliases()->add(id, alias);
    falloc.result()->WriteAddressedMessage(If::MTI_VERIFY_NODE_ID_ADDRESSED,
                                           TEST_NODE_ID, {id, 0},
                                           node_id_to_buffer(id), &n);
    n.WaitForNotification();
}

TEST_F(AsyncNodeTest, SendAddressedMessageToNodeWithConflictingAlias)
{
    static const NodeAlias alias = 0x210U;
    static const NodeID id = 0x050101FFFFDDULL;

    ExpectPacket(":X1948822AN0210050101FFFFDD;");
    TypedSyncAllocation<WriteFlow> falloc(if_can_->addressed_write_allocator());
    SyncNotifiable n;
    // Both the cache and the caller gives an alias. System should use the
    // cache.
    if_can_->remote_aliases()->add(id, alias);
    falloc.result()->WriteAddressedMessage(If::MTI_VERIFY_NODE_ID_ADDRESSED,
                                           TEST_NODE_ID, {id, 0x111},
                                           node_id_to_buffer(id), &n);
    n.WaitForNotification();
}

TEST_F(AsyncNodeTest, SendAddressedMessageToNodeCacheMiss)
{
    static const NodeID id = 0x050101FFFFDDULL;

    TypedSyncAllocation<WriteFlow> falloc(if_can_->addressed_write_allocator());
    SyncNotifiable n;
    // An AME frame should be sent out.
    ExpectPacket(":X1070222AN050101FFFFDD;");
    falloc.result()->WriteAddressedMessage(If::MTI_VERIFY_NODE_ID_ADDRESSED,
                                           TEST_NODE_ID, {id, 0},
                                           node_id_to_buffer(id), &n);
    SendPacketAndExpectResponse(
        ":X10701210N050101FFFFDD;",   // AMD frame
        ":X1948822AN0210050101FFFFDD;");

    n.WaitForNotification();
}

extern long long ADDRESSED_MESSAGE_LOOKUP_TIMEOUT_NSEC;

TEST_F(AsyncNodeTest, SendAddressedMessageToNodeCacheMissTimeout)
{
    static const NodeID id = 0x050101FFFFDDULL;
    long long saved_timeout = ADDRESSED_MESSAGE_LOOKUP_TIMEOUT_NSEC;

    TypedSyncAllocation<WriteFlow> falloc(if_can_->addressed_write_allocator());
    SyncNotifiable n;
    // An AME frame should be sent out.
    ExpectPacket(":X1070222AN050101FFFFDD;");
    ADDRESSED_MESSAGE_LOOKUP_TIMEOUT_NSEC = MSEC_TO_NSEC(20);
    falloc.result()->WriteAddressedMessage(If::MTI_VERIFY_NODE_ID_ADDRESSED,
                                           TEST_NODE_ID, {id, 0},
                                           node_id_to_buffer(id), &n);
    // Then a verify node id global.
    ExpectPacket(":X1949022AN050101FFFFDD;");
    // Then given up.
    n.WaitForNotification();
    ADDRESSED_MESSAGE_LOOKUP_TIMEOUT_NSEC = saved_timeout;
}

TEST_F(AsyncNodeTest, SendAddressedMessageToNodeCacheMissAMDTimeout)
{
    static const NodeID id = 0x050101FFFFDDULL;
    long long saved_timeout = ADDRESSED_MESSAGE_LOOKUP_TIMEOUT_NSEC;

    TypedSyncAllocation<WriteFlow> falloc(if_can_->addressed_write_allocator());
    SyncNotifiable n;
    // An AME frame should be sent out.
    ExpectPacket(":X1070222AN050101FFFFDD;");
    ADDRESSED_MESSAGE_LOOKUP_TIMEOUT_NSEC = MSEC_TO_NSEC(20);
    falloc.result()->WriteAddressedMessage(If::MTI_VERIFY_NODE_ID_ADDRESSED,
                                           TEST_NODE_ID, {id, 0},
                                           node_id_to_buffer(id), &n);
    Wait();
    ADDRESSED_MESSAGE_LOOKUP_TIMEOUT_NSEC = SEC_TO_NSEC(10);
    ExpectPacket(":X1949022AN050101FFFFDD;");
    usleep(30000);
    SendPacketAndExpectResponse(
        ":X19170210N050101FFFFDD;",   // Node id verified message
        ":X1948822AN0210050101FFFFDD;");
    n.WaitForNotification();
    ADDRESSED_MESSAGE_LOOKUP_TIMEOUT_NSEC = saved_timeout;
}

TEST_F(AsyncNodeTest, SendAddressedMessageFromNewNodeWithCachedAlias)
{
    static const NodeAlias alias = 0x210U;
    static const NodeID id = 0x050101FFFFDDULL;

    TypedSyncAllocation<WriteFlow> falloc(if_can_->addressed_write_allocator());
    SyncNotifiable n;
    if_can_->remote_aliases()->add(id, alias);
    // Simulate cache miss on local alias cache.
    if_can_->local_aliases()->remove(0x22A);
    CreateAllocatedAlias();
    ExpectNextAliasAllocation();
    ExpectPacket(":X1070133AN02010D000003;");  // AMD for our new alias.
    // And the frame goes out.
    ExpectPacket(":X1948833AN0210050101FFFFDD;");
    falloc.result()->WriteAddressedMessage(If::MTI_VERIFY_NODE_ID_ADDRESSED,
                                           TEST_NODE_ID, {id, 0},
                                           node_id_to_buffer(id), &n);
    n.WaitForNotification();
}


TEST_F(AsyncNodeTest, SendAddressedMessageFromNewNodeWithCacheMiss)
{
    static const NodeID id = 0x050101FFFFDDULL;

    TypedSyncAllocation<WriteFlow> falloc(if_can_->addressed_write_allocator());
    SyncNotifiable n;
    // Simulate cache miss on local alias cache.
    if_can_->local_aliases()->remove(0x22A);
    CreateAllocatedAlias();
    ExpectNextAliasAllocation();
    ExpectPacket(":X1070133AN02010D000003;");  // AMD for our new alias.
    // And the new alias will do the lookup. Not with an AME frame but straight
    // to the verify node id.
    ExpectPacket(":X1949033AN050101FFFFDD;");
    falloc.result()->WriteAddressedMessage(If::MTI_VERIFY_NODE_ID_ADDRESSED,
                                           TEST_NODE_ID, {id, 0},
                                           node_id_to_buffer(id), &n);
    Wait();
    SendPacketAndExpectResponse(
        ":X19170210N050101FFFFDD;",   // Node id verified message
        ":X1948833AN0210050101FFFFDD;");
    n.WaitForNotification();
}

TEST_F(AsyncNodeTest, SendAddressedMessageFromNewNodeWithCacheMissTimeout)
{
    static const NodeID id = 0x050101FFFFDDULL;
    long long saved_timeout = ADDRESSED_MESSAGE_LOOKUP_TIMEOUT_NSEC;
    ADDRESSED_MESSAGE_LOOKUP_TIMEOUT_NSEC = MSEC_TO_NSEC(20);

    TypedSyncAllocation<WriteFlow> falloc(if_can_->addressed_write_allocator());
    SyncNotifiable n;
    // Simulate cache miss on local alias cache.
    if_can_->local_aliases()->remove(0x22A);
    CreateAllocatedAlias();
    ExpectNextAliasAllocation();
    ExpectPacket(":X1070133AN02010D000003;");  // AMD for our new alias.
    // And the new alias will do the lookup. Not with an AME frame but straight
    // to the verify node id.
    ExpectPacket(":X1949033AN050101FFFFDD;");
    falloc.result()->WriteAddressedMessage(If::MTI_VERIFY_NODE_ID_ADDRESSED,
                                           TEST_NODE_ID, {id, 0},
                                           node_id_to_buffer(id), &n);
    n.WaitForNotification();
    // The expectation here is that no more can frames are generated.
    ADDRESSED_MESSAGE_LOOKUP_TIMEOUT_NSEC = saved_timeout;
}

} // namespace NMRAnet
