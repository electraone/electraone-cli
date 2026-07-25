#include "commands/event_decoder.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace commands
{

    namespace
    {

        std::string timestampPrefix()
        {
            using namespace std::chrono;
            auto now = system_clock::now();
            auto ms =
                duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
            std::time_t t = system_clock::to_time_t(now);
            std::tm tmBuf{};
#ifdef _WIN32
            localtime_s(&tmBuf, &t);
#else
            localtime_r(&t, &tmBuf);
#endif
            std::ostringstream os;
            os << std::put_time(&tmBuf, "%H:%M:%S") << '.' << std::setfill('0')
               << std::setw(3) << ms.count();
            return os.str();
        }

    } // namespace

    std::string describeEvent(const sysex::ParsedResponse &r)
    {
        std::ostringstream os;

        if (r.isAck) {
            os << "ACK";
            if (r.transactionId.has_value())
                os << " (txn " << *r.transactionId << ")";
            return os.str();
        }
        if (r.isNack) {
            os << "NACK";
            if (r.transactionId.has_value())
                os << " (txn " << *r.transactionId << ")";
            return os.str();
        }

        auto byteAt = [&](size_t i) -> int {
            return i < r.payload.size() ? r.payload[i] : -1;
        };

        if (r.category == 0x7E) {
            switch (r.command) {
                case 0x02:
                    os << "Preset Switch: bank=" << byteAt(0)
                       << " slot=" << byteAt(1);
                    return os.str();
                case 0x03:
                    os << "Snapshot List Change";
                    return os.str();
                case 0x31:
                    os << "Capture List Change";
                    return os.str();
                case 0x0A:
                    os << "Pot Touch: pot=" << byteAt(0) << " control="
                       << sysex::decode14bit(static_cast<uint8_t>(byteAt(1)),
                                             static_cast<uint8_t>(byteAt(2)))
                       << " touched=" << byteAt(3);
                    return os.str();
                case 0x05:
                    os << "Preset List Change";
                    return os.str();
                case 0x06:
                    os << "Page Switch: page=" << byteAt(0);
                    return os.str();
                case 0x07:
                    os << "Control Set Switch: set=" << byteAt(0);
                    return os.str();
                case 0x08:
                    // Docs give Preset Bank Switch (1-byte payload) and USB Host
                    // Change (0-byte payload) the same category/command bytes;
                    // disambiguate by payload length since that's all we have.
                    if (r.payload.empty()) {
                        os << "USB Host Change";
                    } else {
                        os << "Preset Bank Switch: bank=" << byteAt(0);
                    }
                    return os.str();
                case 0x04:
                    os << "Snapshot Bank Switch: bank=" << byteAt(0);
                    return os.str();
                case 0x2D: {
                    uint32_t bytes = r.payload.size() >= 4
                                         ? sysex::decode28bit(r.payload[0],
                                                              r.payload[1],
                                                              r.payload[2],
                                                              r.payload[3])
                                         : 0;
                    os << "Progress Report: " << bytes << " bytes transferred";
                    return os.str();
                }
                default:
                    break;
            }
        }

        if (r.category == 0x7F && r.command == 0x00) {
            os << "Log: " << r.payloadAsText();
            return os.str();
        }

        if (r.category == 0x03) {
            // Control MIDI Learn's unsolicited data event has only a single
            // header byte (0x03) before the JSON payload, unlike every other
            // command, so the "command" byte parseResponse split off is really
            // the JSON's first character.
            os << "MIDI Learn Info: " << static_cast<char>(r.command)
               << r.payloadAsText();
            return os.str();
        }

        os << "0x" << std::hex << static_cast<int>(r.category) << "/0x"
           << static_cast<int>(r.command) << std::dec
           << " payload=" << sysex::toHex(r.payload);
        return os.str();
    }

    void printEventWithTimestamp(const sysex::ParsedResponse &resp)
    {
        std::cout << "[" << timestampPrefix() << "] " << describeEvent(resp)
                  << "\n";
    }

} // namespace commands
