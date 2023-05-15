/**
 * @file ntpclient.h
 * @author Michael Hoffmann
 * @author Dieter Zumkehr
 * @brief 
 * @version 0.9.0
 * @date 2021-11-11
 * 
 * @copyright Copyright (c) 2021
 * 
 */
#pragma once

#include "MessageTransport.h"
#include <WString.h>
#include <cstdbool>
#include <ctime>

/**
 * @brief NTP client stuff using the On-Wire protocol to calculate the NTP time.
 * 
 * This is the class you need when using the SNTPv4 library.
 * 
 * \sa NTPMessageTransport
 */
class NTPClient
{
public:
    void begin(const char *ntp_server_name);
    String serverName() const;
    void setServerName(const char *ntp_server_name);
    time_t time(time_t *tloc = nullptr);
    static void lastErrorString(String *error = nullptr);

protected:
    static constexpr char DEFAULT_NTP_SERVER[] = "europe.pool.ntp.org";
    static constexpr time_t ERA_OFFSET0_1_JAN_1970 = 2208988800LL;
    bool on_wire_exchange(NTPMessageTransport::ntp_packet *packet);

private:
    NTPMessageTransport _ntp;
};
