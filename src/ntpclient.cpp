/**
 * @file ntpclient.cpp
 * @author Michael Hoffmann
 * @author Dieter Zumkehr
 * @brief 
 * @version 0.9.0
 * @date 2021-11-11
 * 
 * @copyright Copyright (c) 2021
 * 
 */
#include "ntpclient.h"
#include <Arduino.h>
#include <cerrno>
#include <cmath>
#include <cstdbool>
#include <cstring>

/**
 * @brief Use this first.
 * 
 * @param ntp_server_name The name (URL) of your favorite NTP server.
 * 
 * If you give a nullpointer as ntp_server_name the DEFAULT_NTP_SERVER
 * will be used.
 */
void NTPClient::begin(const char *ntp_server_name)
{
    if (ntp_server_name != nullptr)
        _ntp.setServerName(ntp_server_name);
    else
        _ntp.setServerName(DEFAULT_NTP_SERVER);
}

String NTPClient::serverName() const
{
    return _ntp.serverName();
}

void NTPClient::setServerName(const char *ntp_server_name)
{
    if (ntp_server_name == nullptr)
    {
        // Invalid argument.
        errno = EINVAL;
        return;
    }
    _ntp.setServerName(ntp_server_name);
}

/**
 * @brief The time function returns the UTC current time stamp in Unix format.
 * 
 * @param tloc If tloc is not a null pointer, the return value is also assigned to the object it points to.
 * @param system_time The current time as far as it is known.  Leave it unused assumes 1. Jan. 1970Z00:00:00
 * @return time_t current time if ok, -1 if something went wrong.
 */
time_t NTPClient::time(time_t *tloc)
{
    // NTP era 0 starts at 1. Jan .1900Z00:00.  If system time was given we can set the clock to an interim
    // time.  This is a good idea, because it creates our UDP packets with non constant timestamps.  So the
    // values Transmit Timestamp and Originate Timestamp can be distinguished from old packets arriving late.
    // Not least this enables the "suggested check 3." as demanded in RFC 4330 "5. SNTP Client Operations".
    time_t ntp_time = ERA_OFFSET0_1_JAN_1970 + 1637244065; // TODO: Offset added for testing purposes.

    // On-wire protocol needs four timestamps called T1, T2, T3, T4.  You can find the On-Wire algorithm
    // in RFC 4330, 5. SNTP Client Operations or at https://www.eecis.udel.edu/~mills/onwire.html.  T4 is
    // the final arrive time at the client in relation to T1.  It will be deviated later from the values
    // given by our system internal milliseconds clock.
    NTPMessageTransport::tstamp64_t t1, t2, t3;
    NTPMessageTransport::generateTstamp(&t1, ntp_time, 0.0);

    // Make the exchange with the NTP server.  We want to know the elapsed time until we get our answer back
    // so we count the milliseconds.  Our agreement is that we start our interaction at fraction 0.0s so it
    // will be possible to calculate the offsets of communication delays later.
    NTPMessageTransport::ntp_packet ntp_packet;
    ntp_packet.xmt = t1;
    unsigned long millis_start = millis();
    if (on_wire_exchange(&ntp_packet) == false)
    {
        lastErrorString();
        return (time_t)-1LL;
    }
    unsigned long millis_delta = millis() - millis_start;
    millis_start = millis(); // Just want to correct the time the algorithm with its serial logger consumes.
    t2 = ntp_packet.rec;     // Receive Timestamp measured by the server
    t3 = ntp_packet.xmt;     // Transmit Timestamp when the server sent its message
    // I like doing the computation on double variables.  As described in https://de.wikipedia.org/wiki/Doppelte_Genauigkeit
    // we will have an approx accuracy of "52log10(2) approx 15.7 digits" for IEEE 754 52 bit fraction.
    // E.g. this looks like 3846310349.xxxxx.  I.e. the error might be somewhere in the <=10 us area on
    // a 64 bit real number.  Arduino AVR claims double but in my reminisce just uses 32 bit. If this
    // is IEEE it should only have an accuracy of 6 digits (23log10(2) approx 6.9) and would not be
    // sufficent for anything done here.
    double t1d = NTPMessageTransport::getSeconds(t1) + 0.0; // Fraction 0.0 is given by agreement.
    double t4d = t1d + millis_delta / 1e3; // Destination Timestamp: t1d + delay of exchange with server
    double t2d = NTPMessageTransport::getSeconds(t2) + NTPMessageTransport::getFraction(t2);
    double t3d = NTPMessageTransport::getSeconds(t3) + NTPMessageTransport::getFraction(t3);
    double roundtrip_delay = (t4d - t1d) - (t3d - t2d);
    double clock_offset = ((t2d - t1d) + (t3d - t4d)) / 2.0;
    Serial.print(F("--> T1: "));
    Serial.println(t1d);
    Serial.print(F("--> T2: "));
    Serial.println(t2d);
    Serial.print(F("--> T3: "));
    Serial.println(t3d);
    Serial.print(F("--> T4: "));
    Serial.println(t4d);
    Serial.print(F("--> Clock offset: "));
    Serial.println(clock_offset);
    Serial.print(F("--> Round-trip delay: "));
    Serial.print(roundtrip_delay);
    Serial.println(F(" ms"));
    // Now we can calculate the unix_time with a fraction part.  But our time system in the upper
    // layers normally does not have any millisecond counter.  Now we try to offer a synchronization
    // against the NEXT full second.  Therefore the timestamp to be returned will give the next full
    // second.  We just need to fiddle away until the next second arrives by calling delay() and
    // then return.  Because delay does not do active waiting, it will not harm WiFi, Bluetooth and
    // other fragile good.  On the other hand that is not the most high-precision approach.
    double unix_time_d = 1.0 + clock_offset + ntp_time - ERA_OFFSET0_1_JAN_1970 + (millis() - millis_start) / 1e3;
    Serial.print(F("--> unix_time_d: "));
    Serial.println(unix_time_d);
    double unix_time_d_intpart;
    uint_least32_t sync_ms_delay = 1e3 - modf(unix_time_d, &unix_time_d_intpart) * 1e3;
    Serial.print(F("--> delta time: "));
    Serial.print(millis() - millis_start);
    Serial.println(F(" ms"));
    delay(sync_ms_delay);

    time_t unix_time = (time_t)unix_time_d_intpart;
    if (tloc)
        *tloc = unix_time;
    return (time_t)unix_time;
    // TODO: Remind ERA_OFFSET1 -> secs_since_8_feb_2036
}

/**
 * @brief Error reporting
 * 
 * @param error if nullptr the message will be printed on serial; if "String *" the message will be copied to the string.
 */
void NTPClient::lastErrorString(String *error)
{
    String error_str(strerror(errno));

    if (error == nullptr)
    {
        Serial.print(F("\n"));
        Serial.print(F("Last error: "));
        Serial.println(error_str);
    }
    else
    {
        *error = error_str;
    }
}

/**
 * @brief Interchange of timestamps T1, T2, T3 and T4 like in "Basic Symmetric Mode" of RFC 5905.
 * 
 * @param[in,out] packet The packet to be sent to the server which will be replaced by the packet received by the server.
 * @return true All is fine.
 * @return false In case of error.  You also can get information by reading the errno variable.
 * 
 * Q.v. https://www.eecis.udel.edu/~mills/onwire.html
 * 
 */
bool NTPClient::on_wire_exchange(NTPMessageTransport::ntp_packet *packet)
{
    if (packet == nullptr)
    {
        // Invalid argument.
        errno = EINVAL;
        return false;
    }

    // I am a client.  This is my request to my server.
    // Doing this like described in RFC 4330 '4. Message Format' and '5. SNTP Client Operations'.
    //
    constexpr uint8_t LEAP_NO_WARNING = 0b00'000000; // Clients do not announce leap seconds
    constexpr uint8_t NTP_VERSION_4 = 0b00'100'000;  // I want to use NTP protocol version 4
    constexpr uint8_t MODE_CLIENT = 0b00000'011;     // I am a client
    // Assembling client message
    NTPMessageTransport::tstamp64_t xmt_bak = packet->xmt;
    memset(packet, 0, sizeof(NTPMessageTransport::ntp_packet)); // Setting everything to zero / NIL
    packet->li_vn_mode = LEAP_NO_WARNING | NTP_VERSION_4 | MODE_CLIENT;
    packet->xmt = xmt_bak;

    // Doing exchange with the NTP server.
    constexpr unsigned long TIMEOUT = 1024UL;
    if (_ntp.packetExchange(packet, TIMEOUT) == false)
    {
        // Error code has been set.
        return false;
    }

    // This is the reply from my server.
    // Doing this like described in RFC 4330 '4. Message Format' and '5. SNTP Client Operations'.
    //
    // The expected answer should be sent from a server.
    constexpr uint8_t MODE_MASK = 0b00000'111;
    constexpr uint8_t MODE_SERVER = 0b00000'100;
    if ((packet->li_vn_mode & MODE_MASK) != MODE_SERVER)
    {
        // Operation not supported.
        errno = EOPNOTSUPP;
        return false;
    }
    // The answer protocol version must be identical to the protocol version we used before.
    constexpr uint8_t PROTOCOL_MASK = 0b00'111'000;
    if ((packet->li_vn_mode & PROTOCOL_MASK) != NTP_VERSION_4)
    {
        // Protocol not supported.
        errno = EPROTONOSUPPORT;
        return false;
    }
    // There are no valid data if the server clock is not synchronized.
    constexpr uint8_t LEAP_MASK = 0b11'000000;
    constexpr uint8_t LEAP_ALARM_CONDITION = 0b11'000000;
    if ((packet->li_vn_mode & LEAP_MASK) == LEAP_ALARM_CONDITION)
    {
        // No data available.
        errno = ENODATA;
        return false;
    }
    // Evaluate stratum ranges
    if (packet->stratum > 15)
    {
        // Stratum values from 16-255 are reserved and must not be handled.
        // Protocol family not supported.
        errno = EPFNOSUPPORT;
        return false;
    }
    if (packet->stratum == 0)
    {
        // Q.v. "RFC 4330, 6. SNTP Server Operations": "clients should discard the server message".
        // Print "kiss-o'-death message"
        NTPMessageTransport::printKissCode((const char *)packet->refid);
        // Resource temporarily unavailable.
        errno = EAGAIN;
        return false;
    }
    // Check timestamps.  The Originate Timestamp from the server should
    // be a copy of the old Transmit Timestamp from the client.
    if (xmt_bak != packet->org)
    {
        // Time stamps do not match.  Bad message.
        errno = EBADMSG;
        return false;
    }
    return true;
}
