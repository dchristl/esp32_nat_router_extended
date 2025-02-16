/* Various global declarations for the esp32_nat_router

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "cc.h"
#include "esp_wifi.h"

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#define PARAM_NAMESPACE "esp32_nat"



#define PROTO_TCP 6
#define PROTO_UDP 17
#define PORTMAP_MAX 32

   struct portmap_table_entry
   {
      u32_t daddr;
      u16_t mport;
      u16_t dport;
      u8_t proto;
      u8_t valid;
   };
   extern struct portmap_table_entry portmap_tab[PORTMAP_MAX];

   extern char *ssid;
   extern char *passwd;
   extern char *gateway_addr;
   extern char *ap_ssid;
   extern char *ap_passwd;

  
   extern bool ap_connect;

   extern uint32_t my_ip;
   extern uint32_t my_ap_ip;

   extern esp_netif_t *wifiSTA;
   int set_sta(int argc, char **argv);
   int set_sta_static(int argc, char **argv);
   int set_ap(int argc, char **argv);

   esp_err_t get_config_param_int(char *name, int32_t *param);
   esp_err_t get_config_param_str(char *name, char **param);
   esp_err_t get_config_param_blob(char *name, char **param, size_t *blob_len);
   esp_err_t get_config_param_blob2(char *name, uint8_t *blob, size_t blob_len);
   esp_err_t erase_key(char *name);

   void print_portmap_tab();
   esp_err_t add_portmap(u8_t proto, u16_t mport, u32_t daddr, u16_t dport);
   esp_err_t del_portmap(u8_t proto, u16_t mport, u32_t daddr, u16_t dport);

   char *getDefaultIPByNetmask();
   char *getNetmask();

#define DEFAULT_NETMASK_CLASS_A "255.0.0.0"
#define DEFAULT_NETMASK_CLASS_B "255.255.0.0"
#define DEFAULT_NETMASK_CLASS_C "255.255.255.0"

#define DEFAULT_AP_IP_CLASS_A "10.0.%lu.1"
#define DEFAULT_AP_IP_CLASS_B "172.16.%lu.1"
#define DEFAULT_AP_IP_CLASS_C "192.168.%lu.1"

   /**
    * @brief Set ups and starts a simple DNS server that will respond to all queries
    * with the soft AP's IP address
    *
    */
   void start_dns_server();
   void stop_dns_server();
   bool isDnsStarted();
   uint16_t getConnectCount();

#define DEFAULT_SCAN_LIST_SIZE 15

#ifdef __cplusplus
}
#endif
