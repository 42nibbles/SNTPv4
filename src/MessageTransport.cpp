/**
 * @file MessageTransport.cpp
 * @author Michael Hoffmann
 * @author Dieter Zumkehr
 * @brief 
 * @version 0.9.0
 * @date 2021-11-10
 * 
 * @copyright Copyright (c) 2021
 * 
 */

#include "MessageTransport.h"
#include <Arduino.h>
#include <cerrno>

/**
 * @brief Prints the message of a Kiss-o'-Death (KoD) packet.
 * 
 * @param code Reference Identifier value as described in RFC 4330 "8. The Kiss-o'-Death Packet".
 * @return true if the code has been recognized;  false if the code is unknown.
 * 
 * "The kiss codes can provide useful information for an intelligent client.  These codes are
 * encoded in four-character ASCII strings left justified and zero filled.  The strings are
 * designed for character displays and log files.  Usually, only a few of these codes can
 * occur with SNTP clients, including DENY, RSTR, and RATE.  Others can occur more rarely,
 * including INIT and STEP, when the server is in some special temporary condition."
 */
bool NTPMessageTransport::printKissCode(const char *code)
{
    constexpr uint8_t TABLE_SIZE = 14;
    const struct
    {
        char code[5];
        String msg;
    } kod_table[TABLE_SIZE] = {
        {"ACST", "The association belongs to a anycast server."},
        {"AUTH", "Server authentication failed."},
        {"AUTO", "Autokey sequence failed."},
        {"BCST", "The association belongs to a broadcast server."},
        {"CRYP", "Cryptographic authentication or identification failed."},
        {"DENY", "Access denied by remote server."},
        {"DROP", "Lost peer in symmetric mode."},
        {"RSTR", "Access denied due to local policy."},
        {"INIT", "The association has not yet synchronized for the first time."},
        {"MCST", "The association belongs to a manycast server."},
        {"NKEY", "No key found.  Either the key was never installed or is not trusted."},
        {"RATE", "Rate exceeded.  The server has temporarily denied access because the client exceeded the rate threshold."},
        {"RMOT", "Somebody is tinkering with the association from a remote host running ntpdc.  Not to worry unless some rascal has stolen your keys."},
        {"STEP", "A step change in system time has occurred, but the association has not yet resynchronized."}};

    bool valid_code = false;
    for (uint_fast8_t i = 0; i < TABLE_SIZE; i++)
    {
        if (strcmp(code, kod_table[i].code) == 0)
        {
            // The code matches one of the given codes from our table and therefore is considered valid.
            valid_code = true;
            Serial.println(F("Your SNTP client has received a Kiss-o'-Death (KoD) packet from its server."));
            Serial.print(F("-- Code: "));
            Serial.println(kod_table[i].code);
            Serial.print(F("-- Message: "));
            Serial.println(kod_table[i].msg.c_str());
            break;
        }
    }
    if (!valid_code)
    {
        Serial.println(F("Your SNTP client has received a Kiss-o'-Death (KoD) packet from its server, but the code is unknown."));
        char code_s[5];
        strncpy(code_s, code, 4);
        code_s[4] = '\0';
        Serial.print(F("-- Unknown code: "));
        Serial.println(code_s);
    }
    return valid_code;
}

/**
 * @brief Sends a packet to the server and gets back it reply.
 * @param[in,out] *packet The packet for the server request/reply.
 * @return true if the operation was successful, false in case of failure.
 * 
 * If something goes wrong this functions sets the errno variable.
 */
bool NTPMessageTransport::packetExchange(struct ntp_packet *packet, unsigned long timeout)
{
    if ((packet == nullptr) || timeout == 0)
    {
        // Invalid argument.
        errno = EINVAL;
        return false;
    }
    // Assure the network resources are avaible.
    if (net_provider() == false)
    {
        // Unable to proceed.  Error code has been set.  Giving up.
        return false;
    }
    if (send_server_request(packet) == false)
    {
        // Unable to proceed.  Error code has been set.  Giving up.
        return false;
    }
    if (receive_server_reply(packet, timeout) == false)
    {
        // Unable to proceed.  Error code has been set.  Giving up.
        return false;
    }
    return true;
}

/**
 * @brief NTP server name.
 * 
 * @return String the name of the current NTP server as an URL.
 */
String NTPMessageTransport::serverName() const
{
    return _server_name_str;
}

/**
 * @brief Sets the NTP server to use.
 * 
 * @param ntp_server_name URL of the NTP server to use.
 */
void NTPMessageTransport::setServerName(const char *ntp_server_name)
{
    _server_name_str = ntp_server_name;
}


double NTPMessageTransport::getFraction(const tstamp32_t &ts)
{
    uint16_t fric;

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    fric = ntohs((uint16_t)((ts >> 16) & 0xffff));
#else
    fric = (uint16_t)(ts & 0xffff);
#endif
    return fric / FRIC;
}

double NTPMessageTransport::getFraction(const tstamp64_t &ts)
{
    uint32_t frac;

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    frac = ntohl((uint32_t)((ts >> 32) & 0xffffffff));
#else
    frac = (uint32_t)(ts & 0xffffffff);
#endif
    return frac / FRAC;
}

uint16_t NTPMessageTransport::getSeconds(const tstamp32_t &ts)
{
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return ntohs((uint16_t)((ts >> 00) & 0xffff));
#else
    return (uint16_t)(ts & 0xffff);
#endif
}

uint32_t NTPMessageTransport::getSeconds(const tstamp64_t &ts)
{
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return ntohl((uint32_t)((ts >> 00) & 0xffffffff));
#else
    return (uint32_t)(ts & 0xffffffff);
#endif
}

void NTPMessageTransport::generateTstamp(tstamp32_t *dst, const uint16_t &secs, const double &fric)
{
    uint16_t raw = fric * FRIC;

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    *dst = (uint32_t)htons(secs) | ((uint32_t)htons(raw) << 16);
#else
    *dst = (uint32_t)secs << 16 | ((uint32_t)raw);
#endif
}

void NTPMessageTransport::generateTstamp(tstamp64_t *dst, const uint32_t &secs, const double &frac)
{
    uint32_t raw = frac * FRAC;

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    *dst = (uint64_t)htonl(secs) | ((uint64_t)htonl(raw) << 32);
#else
    *dst = (uint64_t)secs << 32 | ((uint64_t)raw);
#endif
}

//********************************************************************
// protected section
//********************************************************************

/**
 * @brief Assures the network resources are avaible.
 * @return true if everything is ok else false.
 * 
 * If something goes wrong this functions sets the errno variable.
 */
bool NTPMessageTransport::net_provider()
{
    // Assure the network resources are avaible.
    // a.) Missing network connection is a fatal error (ENETDOWN).
    // b.) We might be able to cleanup an occupied UDP port by
    // calling stop(). But this seems to produce a memory leak
    // these days because of missing delete operator (EISCONN).
    if (WiFi.status() != WL_CONNECTED)
    {
        // Unable to handle this here.  Giving up.
        errno = ENETDOWN;
        return false;
    }
    // If the UDP client port is open everything is ok.
    if (_datagram.localPort() == UDP_LOCAL_PORT)
    {
        return true;
    }
    // Try to open a new UDP client port.
    if (_datagram.localPort() == 0 && _datagram.begin(UDP_LOCAL_PORT) == true)
    {
        return true;
    }
    else
    {
        // Unwilling to handle this here, because of comment "b.)".  Giving up.
        errno = EISCONN;
        return false;
    }
}

bool NTPMessageTransport::send_server_request(struct ntp_packet *ntp_request)
{
    if (ntp_request == nullptr)
    {
        // Invalid argument.
        errno = EINVAL;
        return false;
    }

    // Execute server request.
    if ((_datagram.beginPacket(_server_name_str.c_str(), NTP_SERVER_PORT)) != true)
    {
        // Cannot resolve DNS name of server.
        errno = EADDRNOTAVAIL;
        return false;
    }
    if ((_datagram.write((const uint8_t *)ntp_request, sizeof(struct ntp_packet))) != sizeof(struct ntp_packet))
    {
        // The I/O buffer was lost or too small to hold this amount of data.
        errno = EOVERFLOW;
        return false;
    }
    if ((_datagram.endPacket()) != true)
    {
        // The packet has not been sent correctly: Dubious error / Don't know why.
        errno = EIO;
        return false;
    }
    return true;
}

bool NTPMessageTransport::receive_server_reply(struct ntp_packet *ntp_reply, unsigned long timeout)
{
    if ((ntp_reply == nullptr) || (timeout == 0))
    {
        // Invalid argument.
        errno = EINVAL;
        return false;
    }

    // Network traffic section
    int rply_size = 0;
    unsigned long ms_cycles = 0;
    do
    {
        rply_size = _datagram.parsePacket();
        if (rply_size != 0)
            break;
        delay(1UL);
    } while (++ms_cycles < timeout);

    if (rply_size < (int)sizeof(struct ntp_packet))
    {
        // The datagram is too small to be valid.
        errno = EPROTONOSUPPORT;
        return false;
    }
    if (_datagram.read((char *)ntp_reply, sizeof(struct ntp_packet)) != sizeof(struct ntp_packet))
    {
        // The I/O buffer was lost or too small to hold this amount of data.
        errno = EOVERFLOW;
        return false;
    }
    // Finish reading the current packet
    _datagram.flush();
    return true;
}
