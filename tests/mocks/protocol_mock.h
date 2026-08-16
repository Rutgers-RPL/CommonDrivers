#include "protocol.h"

#include <gmock/gmock.h>

namespace Platform {
class MockProtocol : public Protocol {
public:
    explicit MockProtocol(ProtocolType type) : Protocol(type) {}
    MOCK_METHOD(bool, read, (ConstSpan cmd, Span buffer, AddressSize size),
                (override));
    MOCK_METHOD(bool, write,
                (ConstSpan cmd, ConstSpan buffer, AddressSize size),
                (override));
    MOCK_METHOD(bool, readDMA, (Span buffer), (override));
    MOCK_METHOD(bool, configure, (Config config), (override));
};
} // namespace Platform
