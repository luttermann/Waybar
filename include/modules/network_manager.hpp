#pragma once

#include <libnm/NetworkManager.h>
#include "ALabel.hpp"

namespace waybar::modules {
enum class NetworkManagerIPv6Type : uint8_t {
  UNKNOWN,
  LOOPBACK,
  LINKLOCAL,
  SITELOCAL,
  V4MAPPED,
  V4COMPAT,
};

struct WifiInfo {
  std::string ssid;
  guint32 bitrate;
  guint8 strength;
  guint32 frequency;
  std::string toString();
};

class NetworkManager : public ALabel {
 public:
  NetworkManager(const std::string& id, const Json::Value& config);
  void update() override;

 private:
  static void deviceAddedCb(NMClient *client, NMDevice *device, gpointer data);
  static void activeConnectionAdded(NMClient *client, NMActiveConnection *activeConnection, gpointer data);
  static void activeConnectionRemoved(NMClient *client, NMActiveConnection *activeConnection, gpointer data);
  void updateDevices();
  void findDev();
  static NetworkManagerIPv6Type v6AddressType(const char * ipv6);
  static struct WifiInfo getWifiInfo(NMDeviceWifi *wifiDev);
};
}  // namespace waybar::modules
