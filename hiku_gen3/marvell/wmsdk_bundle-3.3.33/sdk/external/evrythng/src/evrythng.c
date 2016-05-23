#include <string.h>
#include <FreeRTOS.h>
#include <task.h>
#include <json_parser.h>
#include "MQTTClient.h"
#include "MQTTClientPersistence.h"
#include "marvell_api.h"
#include "evrythng.h"

#define KEEP_ALIVE_TIME_INTERVAL 60

extern int wmprintf(const char *format, ...);
//#define dbg(_fmt_, ...) wmprintf(_fmt_"\n\r", ##__VA_ARGS__)
//#define log(_fmt_, ...) wmprintf(_fmt_"\n\r", ##__VA_ARGS__)

#ifdef EVRYTHNG_DEBUG
 #define err(_fmt_, ...) wmprintf("Error: "_fmt_"\n\r", ##__VA_ARGS__)
 #define info(_fmt_, ...) wmprintf(_fmt_"\n\r", ##__VA_ARGS__)
#else
#define err(...)
#define info(...)
#endif
#define dbg(...)
#define log(...)

#if defined (OPENSSL) || defined (TLSSOCKET)
MQTTClient_SSLOptions ssl_opts = MQTTClient_SSLOptions_initializer;
static char *uristring;
#endif

#define JSON_NUM_TOKENS	40

#define TOPIC_MAX_LEN 128

evrythng_return_e EvrythngConnect(void);

MQTTClient evrythng_client;
MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
int evrythng_initialized = 0;

static connection_status_callback* conn_cb;

typedef struct t_sub_callback {
  char*                   topic;
  int                     qos;
  sub_callback*           callback;
  struct t_sub_callback*  next;
} t_sub_callback;

t_sub_callback *t_sub_callbacks=NULL;

typedef struct t_pub_callback {
  MQTTClient_deliveryToken  dt;
  pub_callback*             callback;
  struct t_pub_callback*    next;
} t_pub_callback;

t_pub_callback *t_pub_callbacks=NULL;

static evrythng_return_e add_sub_callback(char* topic, int qos, sub_callback *callback)
{
  t_sub_callback **_t_sub_callbacks=&t_sub_callbacks;
  while(*_t_sub_callbacks) {
	if (strcmp((*_t_sub_callbacks)->topic, topic) == 0) {
	  (*_t_sub_callbacks)->qos = qos;
	  (*_t_sub_callbacks)->callback = callback;
	  return EVRYTHNG_SUCCESS;
	}
	_t_sub_callbacks = &(*_t_sub_callbacks)->next;
  }
  if ((*_t_sub_callbacks = (t_sub_callback*)malloc(sizeof(t_sub_callback))) == NULL) {
	return EVRYTHNG_FAILURE;
  }
  if (((*_t_sub_callbacks)->topic = (char*)malloc(strlen(topic) + 1)) == NULL) {
	free(*_t_sub_callbacks);
	return EVRYTHNG_FAILURE;
  }
  strcpy((*_t_sub_callbacks)->topic, topic);
  (*_t_sub_callbacks)->qos = qos;
  (*_t_sub_callbacks)->callback = callback;
  (*_t_sub_callbacks)->next = NULL;

  return EVRYTHNG_SUCCESS;
}

static void rm_sub_callback(char* topic)
{
  t_sub_callback **_t_sub_callbacks=&t_sub_callbacks;
  while(*_t_sub_callbacks) {
	if (strcmp((*_t_sub_callbacks)->topic, topic) == 0) {
	  t_sub_callback *next_node = (*_t_sub_callbacks)->next;
	  free((*_t_sub_callbacks)->topic);
	  free(*_t_sub_callbacks);
	  *_t_sub_callbacks = next_node;
	  break;
	}
	_t_sub_callbacks = &(*_t_sub_callbacks)->next;
  }
}

static evrythng_return_e add_pub_callback(MQTTClient_deliveryToken dt, pub_callback *callback)
{
  t_pub_callback **_t_pub_callbacks=&t_pub_callbacks;
  while(*_t_pub_callbacks) {
	if ((*_t_pub_callbacks)->callback == NULL) {
	  (*_t_pub_callbacks)->dt = dt;
	  (*_t_pub_callbacks)->callback = callback;
	  return EVRYTHNG_SUCCESS;
	}
	_t_pub_callbacks = &(*_t_pub_callbacks)->next;
  }
  if ((*_t_pub_callbacks = (t_pub_callback*)malloc(sizeof(t_pub_callback))) == NULL) {
	return EVRYTHNG_FAILURE;
  }
  (*_t_pub_callbacks)->dt = dt;
  (*_t_pub_callbacks)->callback = callback;
  (*_t_pub_callbacks)->next = NULL;

  return EVRYTHNG_SUCCESS;
}

void connectionLost_callback(void* context, char* cause)
{
  err("Callback: connection lost");

  if (conn_cb)
      (*conn_cb)(EVRYTHNG_CONNECTION_LOST);

  t_pub_callback **_t_pub_callbacks=&t_pub_callbacks;
  while(*_t_pub_callbacks) {
	(*_t_pub_callbacks)->callback = NULL;
	_t_pub_callbacks = &(*_t_pub_callbacks)->next;
  }

  while (EvrythngConnect() != EVRYTHNG_SUCCESS)
  {
	vTaskDelay(5000/portTICK_RATE_MS);
  }

  if (conn_cb)
      (*conn_cb)(EVRYTHNG_CONNECTION_RESTORED);
}

int message_callback(void* context, char* topic_name, int topic_len, MQTTClient_message* message)
{
  log("topic: %s", topic_name);
  log("Received: %s", (char*)message->payload);

  if (message->payloadlen < 3) {
	err("Wrong message lenght");
	goto exit;
  }

  jsontok_t json_tokens[JSON_NUM_TOKENS];
  jobj_t json_obj;
  int err = json_init(&json_obj, json_tokens, JSON_NUM_TOKENS, message->payload, message->payloadlen);
  if (err != WM_SUCCESS) {
	err("Wrong json string");
	goto exit;
  }

  t_sub_callback **_t_sub_callbacks=&t_sub_callbacks;
  while(*_t_sub_callbacks) {
	if (strcmp((*_t_sub_callbacks)->topic, topic_name) == 0) {
	  (*((*_t_sub_callbacks)->callback))(&json_obj);
	}
	_t_sub_callbacks = &(*_t_sub_callbacks)->next;
  }

exit:
  MQTTClient_freeMessage(&message);
  MQTTClient_free(topic_name);

  return 1;
}

void deliveryComplete_callback(void* context, MQTTClient_deliveryToken dt)
{
  t_pub_callback **_t_pub_callbacks=&t_pub_callbacks;
  while(*_t_pub_callbacks) {
	if ((*_t_pub_callbacks)->dt == dt) {
	  if (((*_t_pub_callbacks)->callback) != NULL) {
		(*((*_t_pub_callbacks)->callback))();
		(*_t_pub_callbacks)->callback = NULL;
	  }
	  return;
	}
	_t_pub_callbacks = &(*_t_pub_callbacks)->next;
  }
}

evrythng_return_e EvrythngConfigure(evrythng_config_t* config)
{
  if (evrythng_initialized) {
	return EvrythngConnect();
  }

  conn_opts.keepAliveInterval = KEEP_ALIVE_TIME_INTERVAL;
  conn_opts.reliable = 0;
  conn_opts.cleansession = 1;
  conn_opts.username = (char*)pvPortMalloc(strlen("authorization")*sizeof(char)+1);
  strcpy((char*)conn_opts.username, "authorization");
  conn_opts.password = (char*)pvPortMalloc(strlen(config->api_key)*sizeof(char)+1);
  strcpy((char*)conn_opts.password, config->api_key);
  conn_opts.struct_version = 1;
#if defined (OPENSSL) || defined (TLSSOCKET)
  if (config->enable_ssl)
  {
	ssl_opts.enableServerCertAuth = 0;
	ssl_opts.trustStore = (char*)pvPortMalloc(config->cert_buffer_size+1); 
	strcpy(ssl_opts.trustStore, config->cert_buffer);
	ssl_opts.trustStore_size = config->cert_buffer_size;

	conn_opts.serverURIcount = 1;
	uristring = (char*)pvPortMalloc(strlen(config->tls_server_uri)*sizeof(char)+1);
	strcpy(uristring, config->tls_server_uri);
	conn_opts.serverURIs = (char* const*)&uristring;
	conn_opts.struct_version = 4;
	conn_opts.ssl = &ssl_opts;
  }
#endif

  if (config->connection_status_cb)
      conn_cb = config->connection_status_cb;
  else
      conn_cb = 0;

  MQTTClient_init();

  if(MQTTClient_create(&evrythng_client, config->url, config->client_id, MQTTCLIENT_PERSISTENCE_NONE, NULL) != MQTTCLIENT_SUCCESS)
  {
	err("Can't create client");

	vPortFree((char*)conn_opts.username);
	vPortFree((char*)conn_opts.password);
#if defined (OPENSSL) || defined (TLSSOCKET)
	if (config->enable_ssl)
	{
		vPortFree(ssl_opts.trustStore);
		vPortFree(uristring);
	}
#endif
	return EVRYTHNG_FAILURE;
  }

  if(MQTTClient_setCallbacks(evrythng_client, evrythng_client, connectionLost_callback, message_callback, deliveryComplete_callback) != MQTTCLIENT_SUCCESS)
  {
	err("Can't set callback");

	vPortFree((char*)conn_opts.username);
	vPortFree((char*)conn_opts.password);
#if defined (OPENSSL) || defined (TLSSOCKET)
	if (config->enable_ssl)
	{
		vPortFree(ssl_opts.trustStore);
		vPortFree(uristring);
	}
#endif
	return EVRYTHNG_FAILURE;
  }

  evrythng_initialized = 1;

  return EvrythngConnect();
}

evrythng_return_e EvrythngConnect(void)
{
  if (MQTTClient_isConnected(evrythng_client)) {
	return EVRYTHNG_SUCCESS;
  }

  int rc;

  info("Connecting to Evrythng cloud");
  if ((rc = MQTTClient_connect(evrythng_client, &conn_opts)) != MQTTCLIENT_SUCCESS)
  {
	err("Failed to connect, return code %d", rc);
	return EVRYTHNG_FAILURE;
  }
  info("Connected");

  t_sub_callback **_t_sub_callbacks=&t_sub_callbacks;
  while(*_t_sub_callbacks) {
	int rc = MQTTClient_subscribe(evrythng_client, (*_t_sub_callbacks)->topic, (*_t_sub_callbacks)->qos);
	if (rc == MQTTCLIENT_SUCCESS) {
	  log("Subscribed: %s", (*_t_sub_callbacks)->topic);
	}
	else {
	  err("rc=%d", rc);
	}
	_t_sub_callbacks = &(*_t_sub_callbacks)->next;
  }

  return EVRYTHNG_SUCCESS;
}

static evrythng_return_e EvrythngSub(char* entity, char* entity_id, char* data_type, char* data_name, int pub_states, int qos, sub_callback *callback)
{
  if (!MQTTClient_isConnected(evrythng_client)) {
	err("Client doesn't started");
	return EVRYTHNG_FAILURE;
  }

  int rc;
  char sub_topic[TOPIC_MAX_LEN];

  if (entity_id == NULL) {
	rc = snprintf(sub_topic, TOPIC_MAX_LEN, "%s/%s?pubStates=%d", entity, data_name, pub_states);
	if (rc < 0 || rc >= TOPIC_MAX_LEN) {
	  err("Topic overflow");
	  return EVRYTHNG_FAILURE;
	}
  } else if (data_name == NULL) {
	rc = snprintf(sub_topic, TOPIC_MAX_LEN, "%s/%s/%s?pubStates=%d", entity, entity_id, data_type, pub_states);
	if (rc < 0 || rc >= TOPIC_MAX_LEN) {
	  err("Topic overflow");
	  return EVRYTHNG_FAILURE;
	}
  } else {
	rc = snprintf(sub_topic, TOPIC_MAX_LEN, "%s/%s/%s/%s?pubStates=%d", entity, entity_id, data_type, data_name, pub_states);
	if (rc < 0 || rc >= TOPIC_MAX_LEN) {
	  err("Topic overflow");
	  return EVRYTHNG_FAILURE;
	}
  }

  rc = add_sub_callback(sub_topic, qos, callback);
  if (rc != EVRYTHNG_SUCCESS) {
	err("Can't allocate memory");
	return EVRYTHNG_FAILURE;
  }

  log("s_t: %s", sub_topic);

  rc = MQTTClient_subscribe(evrythng_client, sub_topic, qos);
  if (rc == MQTTCLIENT_SUCCESS) {
	log("Subscribed: %s", sub_topic);
  }
  else {
	err("rc=%d", rc);
	return EVRYTHNG_FAILURE;
  }

  return EVRYTHNG_SUCCESS;
}

static evrythng_return_e EvrythngUnsub(char* entity, char* entity_id, char* data_type, char* data_name, int pub_states)
{
  if (!MQTTClient_isConnected(evrythng_client)) {
	err("Client doesn't started");
	return EVRYTHNG_FAILURE;
  }

  int rc;
  char unsub_topic[TOPIC_MAX_LEN];

  if (entity_id == NULL) {
	rc = snprintf(unsub_topic, TOPIC_MAX_LEN, "%s/%s?pubStates=%d", entity, data_name, pub_states);
	if (rc < 0 || rc >= TOPIC_MAX_LEN) {
	  err("Topic overflow");
	  return EVRYTHNG_FAILURE;
	}
  } else if (data_name == NULL) {
	rc = snprintf(unsub_topic, TOPIC_MAX_LEN, "%s/%s/%s?pubStates=%d", entity, entity_id, data_type, pub_states);
	if (rc < 0 || rc >= TOPIC_MAX_LEN) {
	  err("Topic overflow");
	  return EVRYTHNG_FAILURE;
	}
  } else {
	rc = snprintf(unsub_topic, TOPIC_MAX_LEN, "%s/%s/%s/%s?pubStates=%d", entity, entity_id, data_type, data_name, pub_states);
	if (rc < 0 || rc >= TOPIC_MAX_LEN) {
	  err("Topic overflow");
	  return EVRYTHNG_FAILURE;
	}
  }

  rm_sub_callback(unsub_topic);

  rc = MQTTClient_unsubscribe(evrythng_client, unsub_topic);
  if (rc == MQTTCLIENT_SUCCESS) {
	log("Unsubscribed: %s", unsub_topic);
  }
  else {
	err("rc=%d", rc);
	return EVRYTHNG_FAILURE;
  }

  return EVRYTHNG_SUCCESS;
}

static evrythng_return_e EvrythngPub(char* entity, char* entity_id, char* data_type, char* data_name, char* property_json, int qos, pub_callback *callback)
{
  if (!MQTTClient_isConnected(evrythng_client)) {
	err("Client doesn't started");
	return EVRYTHNG_FAILURE;
  }

  int rc;
  char pub_topic[TOPIC_MAX_LEN];

  if (entity_id == NULL) {
	rc = snprintf(pub_topic, TOPIC_MAX_LEN, "%s/%s", entity, data_name);
	if (rc < 0 || rc >= TOPIC_MAX_LEN) {
	  err("Topic overflow");
	  return EVRYTHNG_FAILURE;
	}
  } else if (data_name == NULL) {
	rc = snprintf(pub_topic, TOPIC_MAX_LEN, "%s/%s/%s", entity, entity_id, data_type);
	if (rc < 0 || rc >= TOPIC_MAX_LEN) {
	  err("Topic overflow");
	  return EVRYTHNG_FAILURE;
	}
  } else {
	rc = snprintf(pub_topic, TOPIC_MAX_LEN, "%s/%s/%s/%s", entity, entity_id, data_type, data_name);
	if (rc < 0 || rc >= TOPIC_MAX_LEN) {
	  err("Topic overflow");
	  return EVRYTHNG_FAILURE;
	}
  }

  log("p_t: %s", pub_topic);

  MQTTClient_deliveryToken dt;
  rc = MQTTClient_publish(evrythng_client, pub_topic, strlen(property_json), property_json, qos, 0, &dt);
  if (rc == MQTTCLIENT_SUCCESS) {
	log("Published: %s", property_json);
  }
  else {
	err("rc=%d", rc);
	return EVRYTHNG_FAILURE;
  }

  if (callback != NULL && qos != 0) {
	if (add_pub_callback(dt, callback) != EVRYTHNG_SUCCESS) {
	  err("Can't add callback");
	  return EVRYTHNG_FAILURE;
	}
  }

  return EVRYTHNG_SUCCESS;
}

evrythng_return_e EvrythngSubThngProperty(char* thng_id, char* property_name, int pub_states, int qos, sub_callback *callback)
{
  return EvrythngSub("thngs", thng_id, "properties", property_name, pub_states, qos, callback);
}

evrythng_return_e EvrythngUnsubThngProperty(char* thng_id, char* property_name, int pub_states)
{
  return EvrythngUnsub("thngs", thng_id, "properties", property_name, pub_states);
}

evrythng_return_e EvrythngSubThngProperties(char* thng_id, int pub_states, int qos, sub_callback *callback)
{
  return EvrythngSub("thngs", thng_id, "properties", NULL, pub_states, qos, callback);
}

evrythng_return_e EvrythngUnsubThngProperties(char* thng_id, int pub_states)
{
  return EvrythngUnsub("thngs", thng_id, "properties", NULL, pub_states);
}

evrythng_return_e EvrythngPubThngProperty(char* thng_id, char* property_name, char* property_json, int qos, pub_callback *callback)
{
  return EvrythngPub("thngs", thng_id, "properties", property_name, property_json, qos, callback);
}

evrythng_return_e EvrythngPubThngProperties(char* thng_id, char* properties_json, int qos, pub_callback *callback)
{
  return EvrythngPub("thngs", thng_id, "properties", NULL, properties_json, qos, callback);
}

evrythng_return_e EvrythngSubThngAction(char* thng_id, char* action_name, int pub_states, int qos, sub_callback *callback)
{
  return EvrythngSub("thngs", thng_id, "actions", action_name, pub_states, qos, callback);
}

evrythng_return_e EvrythngUnsubThngAction(char* thng_id, char* action_name, int pub_states)
{
  return EvrythngUnsub("thngs", thng_id, "actions", action_name, pub_states);
}

evrythng_return_e EvrythngSubThngActions(char* thng_id, int pub_states, int qos, sub_callback *callback)
{
  return EvrythngSub("thngs", thng_id, "actions", "all", pub_states, qos, callback);
}

evrythng_return_e EvrythngUnsubThngActions(char* thng_id, int pub_states)
{
  return EvrythngUnsub("thngs", thng_id, "actions", "all", pub_states);
}

evrythng_return_e EvrythngPubThngAction(char* thng_id, char* action_name, char* action_json, int qos, pub_callback *callback)
{
  return EvrythngPub("thngs", thng_id, "actions", action_name, action_json, qos, callback);
}

evrythng_return_e EvrythngPubThngActions(char* thng_id, char* actions_json, int qos, pub_callback *callback)
{
  return EvrythngPub("thngs", thng_id, "actions", "all", actions_json, qos, callback);
}

evrythng_return_e EvrythngSubThngLocation(char* thng_id, int pub_states, int qos, sub_callback *callback)
{
  return EvrythngSub("thngs", thng_id, "location", NULL, pub_states, qos, callback);
}

evrythng_return_e EvrythngUnsubThngLocation(char* thng_id, int pub_states)
{
  return EvrythngUnsub("thngs", thng_id, "location", NULL, pub_states);
}

evrythng_return_e EvrythngPubThngLocation(char* thng_id, char* location_json, int qos, pub_callback *callback)
{
  return EvrythngPub("thngs", thng_id, "location", NULL, location_json, qos, callback);
}


evrythng_return_e EvrythngSubProductProperty(char* products_id, char* property_name, int pub_states, int qos, sub_callback *callback)
{
  return EvrythngSub("products", products_id, "properties", property_name, pub_states, qos, callback);
}

evrythng_return_e EvrythngUnsubProductProperty(char* products_id, char* property_name, int pub_states)
{
  return EvrythngUnsub("products", products_id, "properties", property_name, pub_states);
}

evrythng_return_e EvrythngSubProductProperties(char* products_id, int pub_states, int qos, sub_callback *callback)
{
  return EvrythngSub("products", products_id, "properties", NULL, pub_states, qos, callback);
}

evrythng_return_e EvrythngUnsubProductProperties(char* products_id, int pub_states)
{
  return EvrythngUnsub("products", products_id, "properties", NULL, pub_states);
}

evrythng_return_e EvrythngPubProductProperty(char* products_id, char* property_name, char* property_json, int qos, pub_callback *callback)
{
  return EvrythngPub("products", products_id, "properties", property_name, property_json, qos, callback);
}

evrythng_return_e EvrythngPubProductProperties(char* products_id, char* properties_json, int qos, pub_callback *callback)
{
  return EvrythngPub("products", products_id, "properties", NULL, properties_json, qos, callback);
}

evrythng_return_e EvrythngSubProductAction(char* products_id, char* action_name, int pub_states, int qos, sub_callback *callback)
{
  return EvrythngSub("products", products_id, "actions", action_name, pub_states, qos, callback);
}

evrythng_return_e EvrythngUnsubProductAction(char* products_id, char* action_name, int pub_states)
{
  return EvrythngUnsub("products", products_id, "actions", action_name, pub_states);
}

evrythng_return_e EvrythngSubProductActions(char* products_id, int pub_states, int qos, sub_callback *callback)
{
  return EvrythngSub("products", products_id, "actions", "all", pub_states, qos, callback);
}

evrythng_return_e EvrythngUnsubProductActions(char* products_id, int pub_states)
{
  return EvrythngUnsub("products", products_id, "actions", "all", pub_states);
}

evrythng_return_e EvrythngPubProductAction(char* products_id, char* action_name, char* action_json, int qos, pub_callback *callback)
{
  return EvrythngPub("products", products_id, "actions", action_name, action_json, qos, callback);
}

evrythng_return_e EvrythngPubProductActions(char* products_id, char* actions_json, int qos, pub_callback *callback)
{
  return EvrythngPub("products", products_id, "actions", "all", actions_json, qos, callback);
}


evrythng_return_e EvrythngSubAction(char* action_name, int pub_states, int qos, sub_callback *callback)
{
  return EvrythngSub("actions", NULL, NULL, action_name, pub_states, qos, callback);
}

evrythng_return_e EvrythngUnsubAction(char* action_name, int pub_states)
{
  return EvrythngUnsub("actions", NULL, NULL, action_name, pub_states);
}

evrythng_return_e EvrythngSubActions(int pub_states, int qos, sub_callback *callback)
{
  return EvrythngSub("actions", NULL, NULL, "all", pub_states, qos, callback);
}

evrythng_return_e EvrythngUnsubActions(int pub_states)
{
  return EvrythngUnsub("actions", NULL, NULL, "all", pub_states);
}

evrythng_return_e EvrythngPubAction(char* action_name, char* action_json, int qos, pub_callback *callback)
{
  return EvrythngPub("actions", NULL, NULL, action_name, action_json, qos, callback);
}

evrythng_return_e EvrythngPubActions(char* actions_json, int qos, pub_callback *callback)
{
  return EvrythngPub("actions", NULL, NULL, "all", actions_json, qos, callback);
}

void EvrythngSetLogLevel(int level)
{
  Log_setLevel(level);
}

