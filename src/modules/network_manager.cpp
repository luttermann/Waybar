#include "modules/network_manager.hpp"
#include <fmt/format.h>
#include <glibmm/main.h>
#include <giomm.h>
#include <spdlog/spdlog.h>
#include <json/json.h>
#include <libnm/NetworkManager.h>
#include <glib/garray.h>
#include <arpa/inet.h>
#include <iostream>

namespace waybar::modules {

NetworkManager::NetworkManager(const std::string &id, const Json::Value &config)
    : ALabel(config, "network_manager", id, "{id} {type}", 60, false, false, false) {
  label_.set_name(name_);
  label_.set_text("Hejsa");
  label_.set_tooltip_text("BlaBlaBla");
  NMClient *nmClient = nm_client_new(nullptr, nullptr);
  if (!nmClient) {
    throw std::runtime_error("Failed to create NetworkManager client");
  }
  g_signal_connect(nmClient, "device-added", G_CALLBACK(deviceAddedCb), this);
  g_signal_connect(nmClient, "active-connection-added", G_CALLBACK(activeConnectionAdded), this);
  g_signal_connect(nmClient, "active-connection-removed", G_CALLBACK(activeConnectionRemoved), this);
  this->updateDevices();
}

void NetworkManager::activeConnectionAdded(NMClient *client, NMActiveConnection *activeConnection,
                                           gpointer data) {
  NetworkManager *self{static_cast<NetworkManager *>(data)};
  spdlog::debug("::activeConnectionAdded(ac) -> {}", nm_active_connection_get_id(activeConnection));
  spdlog::debug("::activeConnectionAdded(cl) -> {}", nm_active_connection_get_id(nm_client_get_primary_connection(client)));
  self->updateDevices();
}
void NetworkManager::activeConnectionRemoved(NMClient *client, NMActiveConnection *activeConnection,
                                             gpointer data) {
  NetworkManager *self{static_cast<NetworkManager *>(data)};
  spdlog::debug("::activeConnectionRemoved(ac) -> {}", nm_active_connection_get_id(activeConnection));
  spdlog::debug("::activeConnectionRemoved(cl) -> {}", nm_active_connection_get_id(nm_client_get_primary_connection(client)));
  self->updateDevices();
}
void NetworkManager::deviceAddedCb(NMClient *client, NMDevice *device, gpointer data) {
  NetworkManager *self{static_cast<NetworkManager *>(data)};
  spdlog::debug("::deviceAdded()");
  self->updateDevices();
}

void NetworkManager::findDev() {
}

void NetworkManager::updateDevices() {
  std::string text;
  std::string tooltip;
  std::string css_class = "default";

  NMClient *nmClient = nm_client_new(nullptr, nullptr);
  if (!nmClient) {
    throw std::runtime_error("Failed to create NetworkManager client");
  }

  NMActiveConnection *con = nm_client_get_primary_connection(nmClient);

  spdlog::debug("Primary: {}", nm_active_connection_get_id(con));
  if (con == nullptr) {
    text = "No Connection";
    tooltip = "No active network connection";
    css_class = "disconnected";
  } else {
    const GPtrArray *devices = nm_active_connection_get_devices(con);

    NMDevice *dev;
    std::string iface;

    struct WifiInfo wifiInfo;

    if (devices->len > 0) {
      dev = (NMDevice *)g_ptr_array_index(devices, 0);
      iface = nm_device_get_iface(dev);
      if (nm_device_get_device_type(dev) == NMDeviceType::NM_DEVICE_TYPE_WIFI) {
        wifiInfo = getWifiInfo((NMDeviceWifi *)dev);
      }
    }
    spdlog::debug(wifiInfo.toString());

    const char *type = nm_active_connection_get_connection_type(con);

    // IPv4 address information
    NMIPConfig *ip4conf = nm_active_connection_get_ip4_config(con);
    std::string ip4;
    GPtrArray *ip4addrs = nm_ip_config_get_addresses(ip4conf);
    for (guint i = 0; i < ip4addrs->len; i++) {
      auto *addr = (NMIPAddress*)g_ptr_array_index(ip4addrs, i);
      if (ip4.empty())
        ip4 = fmt::format("{}/{}", nm_ip_address_get_address(addr), nm_ip_address_get_prefix(addr));
      else
        ip4 += fmt::format(";{}/{}", nm_ip_address_get_address(addr), nm_ip_address_get_prefix(addr));
    }

    // IPv6 address information
    NMIPConfig *ip6conf = nm_active_connection_get_ip6_config(con);
    std::string ip6;
    GPtrArray *ip6addrs = nm_ip_config_get_addresses(ip6conf);
    for (guint i = 0; i < ip6addrs->len; i++) {
      auto *addr = (NMIPAddress*)g_ptr_array_index(ip6addrs, i);
      if (ip6.empty())
        ip6 = fmt::format("{}/{}", nm_ip_address_get_address(addr), nm_ip_address_get_prefix(addr));
      else
        ip6 += fmt::format(";{}/{}", nm_ip_address_get_address(addr), nm_ip_address_get_prefix(addr));
    }

    const char *name = nm_active_connection_get_id(con);

    text = fmt::format("{} {} {}", name, iface, type);
    if (!ip4.empty()) text += " " + ip4;
    if (!ip6.empty()) text += " " + ip6;
  }

  label_.set_markup(text);
  label_.set_tooltip_text(tooltip);
  label_.get_style_context()->add_class(css_class);


  ALabel::update();
}

void NetworkManager::update() {
  NetworkManager::updateDevices();
}

NetworkManagerIPv6Type NetworkManager::v6AddressType(const char *ipv6) {
  struct in6_addr ipv6AddrBin;

  if (inet_pton(AF_INET6, ipv6, &ipv6AddrBin) != 1) {
    return NetworkManagerIPv6Type::UNKNOWN;
  }

  const auto *in6AddrConst = reinterpret_cast<const struct in6_addr *>(&ipv6AddrBin);

  if (IN6_IS_ADDR_LINKLOCAL(in6AddrConst)) return NetworkManagerIPv6Type::LINKLOCAL;
  if (IN6_IS_ADDR_LOOPBACK(in6AddrConst)) return NetworkManagerIPv6Type::LOOPBACK;
  if (IN6_IS_ADDR_SITELOCAL(in6AddrConst)) return NetworkManagerIPv6Type::SITELOCAL;
  if (IN6_IS_ADDR_V4MAPPED(in6AddrConst)) return NetworkManagerIPv6Type::V4MAPPED;
  if (IN6_IS_ADDR_V4COMPAT(in6AddrConst)) return NetworkManagerIPv6Type::V4COMPAT;

  return NetworkManagerIPv6Type::UNKNOWN;
}

struct WifiInfo NetworkManager::getWifiInfo(NMDeviceWifi *wifiDev) {
  struct WifiInfo info = WifiInfo();
  info.bitrate = nm_device_wifi_get_bitrate(wifiDev);

  NMAccessPoint *wifiAp = nm_device_wifi_get_active_access_point(wifiDev);

  info.strength = nm_access_point_get_strength(wifiAp);
  info.frequency = nm_access_point_get_frequency(wifiAp);

  // Extract SSID
  gsize ssidBytesSize;
  GBytes *ssidBytes = nm_access_point_get_ssid(wifiAp);
  const auto *data = static_cast<const guint8 *>(g_bytes_get_data(ssidBytes, &ssidBytesSize));
  info.ssid = reinterpret_cast<const char *>(data);
  return info;
}
std::string WifiInfo::toString() {
  return fmt::format("ssid:{}; bitrate:{}; strength:{}; freq:{}", ssid, bitrate, strength, frequency);
}
}