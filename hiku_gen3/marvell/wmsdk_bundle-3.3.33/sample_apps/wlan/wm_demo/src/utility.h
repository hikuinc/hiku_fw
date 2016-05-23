#include <appln_dbg.h>
#include <appln_cb.h>

#define        EZCONN_MAX_KEY_LEN   32
#define        EZCONN_MIN_KEY_LEN    8

/* Initialize the various modules that will be used by this application */
void modules_init();

/* Set the final action handler to be called by the healthmon before device
 * resets due to the watchdog. Typically this can be used to log diagnostics
 * information captured by all the other about_to_die handlers, or any other
 * application specific functions.
 */
void final_about_to_die();

/* This function takes micro-AP network prefix and returns the SSID for the
 * micro-AP network. It appends the last two bytes of the MAC address of the
 * device to the network prefix.
 *
 * \param[in] uap_prefix Pointer to micro-AP network prefix.
 * \param[in] sizeof_prefix len of the micro-AP network prefix.
 * \param[out] uap_ssid Pointer to micro-AP network SSID buffer.
 * \param[out] sizeof_ssid len of the micro-AP network SSID buffer.
 * \return WM_SUCCESS if network SSID is generated successfully
 *	   -WM_FAIL if network SSID generation fails
 */
int get_prov_ssid(char *uap_prefix, int sizeof_prefix,
		  char *uap_ssid, int sizeof_ssid);

/* The device pin is used in EZConnect provisioning to decrypt network
 * information received by sniffing multicast packets and in uAP based
 * provisioning with secured web handlers.
 *
 * The provisioning key length range is between 8 to 32 bytes. Application
 * user can use a unique provisioning key for each device which can be
 * programmed into the device at the time of manufacturing.
 *
 * \param[in] key Pointer to provisioning key string for encryption.
 * \param[in] sizeof_key len of provisioning key.
 * \return WM_SUCCESS if key registered successfully
 *	   -WM_FAIL if key registration fails
 */
int set_prov_key(uint8_t *key, int sizeof_key);

/* This function returns the provisioning key.
 *
 * \param[out] key Pointer to the provisioning key buffer.
 * \param[out] sizeof_key len of the provisioning key buffer.
 * \return key_len Length of the key, if valid key is available
 *	   -WM_FAIL if key is not registered or invalid length
 *
 */
int get_prov_key(uint8_t *key, int sizeof_key);
