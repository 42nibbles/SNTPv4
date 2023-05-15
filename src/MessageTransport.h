/**
 * @file MessageTransport.h
 * @author Michael Hoffmann
 * @author Dieter Zumkehr
 * @brief 
 * @version 0.9.0
 * @date 2021-11-10
 * 
 * @copyright Copyright (c) 2021
 * 
 */
#pragma once

#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <WString.h>
#include <cstdbool>
#include <cstdint>

/**
 * @brief Low level layer of message exchange between client and server.
 * 
 * - It does know about the details of NTP messages and data formats but does
 *  not do protocol stuff by itself.
 * - It implements the interchange between client and server, but does not test
 *  or work with the content.
 * 
 * @sa NTPClient
 */
class NTPMessageTransport
{

public:
    // The timestamp data are a subset of the of the definitions of the reference design
    // given in https://github.com/ntp-project/ntp/blob/master-no-authorname/include/ntp_fp.h .
    // Especially I do not like to reimplement what they themself call "a big hack" that helps
    // them "rewriting all their operators twice".

    /// Short timestamp, q.v. RFC 5906, 6. Data Types, NTP Short Format \sa https://tools.ietf.org/html/rfc5905#section-6
    typedef uint32_t tstamp32_t;

    /// Long timestamp, q.v. RFC 5906, 6. Data Types, NTP Timestamp Format \sa https://tools.ietf.org/html/rfc5905#section-6
    typedef uint64_t tstamp64_t;

    /**
     * \brief Forepart of NTP packet, q.v. RFC 5906, 7.3 Packet Header Variables, Fig. 8
     * \sa https://tools.ietf.org/html/rfc5905#section-7.3
     * 
     * The trailing part was omited by intend and would be used for cryptographic stuff that is
     * not handled here.
     */
    struct ntp_packet
    {
        uint8_t li_vn_mode;   ///< peer leap indicator, version, mode
        uint8_t stratum;      ///< peer stratum
        int8_t ppoll;         ///< peer poll interval
        int8_t precision;     ///< peer clock precision
        tstamp32_t rootdelay; ///< roundtrip delay to primary source
        tstamp32_t rootdisp;  ///< dispersion to primary source
        uint32_t refid;       ///< reference id
        tstamp64_t reftime;   ///< last update time
        tstamp64_t org;       ///< originate time stamp
        tstamp64_t rec;       ///< receive time stamp
        tstamp64_t xmt;       ///< transmit time stamp
        // Omitted by intention - trailing data will not be handled.
    };

    // Transport methods
    bool packetExchange(struct ntp_packet *packet, unsigned long timeout);
    String serverName() const;
    void setServerName(const char *ntp_server_name);

    // Timestamp handling
    static uint16_t getSeconds(const tstamp32_t &ts);
    static uint32_t getSeconds(const tstamp64_t &ts);
    static double getFraction(const tstamp32_t &ts);
    static double getFraction(const tstamp64_t &ts);
    static void generateTstamp(tstamp32_t *dst, const uint16_t &secs, const double &fric);
    static void generateTstamp(tstamp64_t *dst, const uint32_t &secs, const double &frac);
    static bool printKissCode(const char *code);

protected:
    // Q.v. http://www.iana.org/assignments/port-numbers
    static constexpr uint16_t NTP_SERVER_PORT = 123U; ///< Server UDP port given by IANA.
    static constexpr uint16_t UDP_LOCAL_PORT = 8123U; ///< Client UDP port after my fancy.
    // Taken from the RFC 5905 reference implementation 'A.1.1.'
    // q.v. https://tools.ietf.org/html/rfc5905#appendix-A.1.1
    static constexpr double FRIC = 65536.;      ///< 2^16 as a double
    static constexpr double FRAC = 4294967296.; ///< 2^32 as a double

    bool net_provider();
    bool send_server_request(struct ntp_packet *ntp_request);
    bool receive_server_reply(struct ntp_packet *ntp_reply, unsigned long timeout);

private:
    String _server_name_str;
    WiFiUDP _datagram;
};
