#ifndef SOFAR_EECONFIG_H
#define SOFAR_EECONFIG_H

#include <Arduino.h>
#include "Config.h"

class EEConfig {
public:
    void begin();
    bool load();
    void save();

    char*       name()     { return _name; }
    char*       mqttHost() { return _host; }
    char*       mqttPort() { return _port; }
    char*       mqttUser() { return _user; }
    char*       mqttPass() { return _pass; }

    const char* name()     const { return _name; }
    const char* mqttHost() const { return _host; }
    const char* mqttPort() const { return _port; }
    const char* mqttUser() const { return _user; }
    const char* mqttPass() const { return _pass; }

private:
    char _name[EE_NAME_LEN] = "Sofar";
    char _host[EE_HOST_LEN] = "";
    char _port[EE_PORT_LEN] = "1883";
    char _user[EE_USER_LEN] = "";
    char _pass[EE_PASS_LEN] = "";

    static String readEE(int off, int len);
    static void   writeEE(int off, int len, const char* v);
};

#endif // SOFAR_EECONFIG_H
