#include "certificates.h"
void my_debug(void *ctx, int level, const char *file, int line, const char *str)
{
    ((void)level);
    Serial.printf("%s:%04d: %s", file, line, str);
}

Certificates::Certificates(TinyGsmClient &client) : client(client)
{
    mbedtls_ssl_init(&ssl);
    mbedtls_ssl_config_init(&conf);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    mbedtls_entropy_init(&entropy);
    mbedtls_x509_crt_init(&rootCA);
    mbedtls_x509_crt_init(&clientCert);
    mbedtls_pk_init(&privateKey);
}

Certificates::~Certificates()
{
    cleanup();
}

bool Certificates::init()
{
    const char *pers = "TinyGSM_MQTT";
    if (mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, (const unsigned char *)pers, strlen(pers)) != 0)
    {
        Serial.println("Failed to initialize RNG.");
        return false;
    }

    // Load certificates from SPIFFS
    if (!SPIFFS.begin(true))
    {
        Serial.println("Failed to initialize SPIFFS");
        return false;
    }
    if (!loadCertificate("/aws-root-ca.pem", &rootCA))
    {
        Serial.println("Failed to load root CA.");
        return false;
    }
    if (!loadCertificate("/aws-device-cert.pem", &clientCert))
    {
        Serial.println("Failed to load device certificate.");
        return false;
    }
    if (!loadPrivateKey("/aws-private-key.pem", &privateKey))
    {
        Serial.println("Failed to load private key.");
        return false;
    }

    if (mbedtls_ssl_config_defaults(&conf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT) != 0)
    {
        Serial.println("Failed to configure SSL.");
        return false;
    }

    mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_REQUIRED);
    mbedtls_ssl_conf_ca_chain(&conf, &rootCA, NULL);
    mbedtls_ssl_conf_own_cert(&conf, &clientCert, &privateKey);
    mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &ctr_drbg);

    if (mbedtls_ssl_setup(&ssl, &conf) != 0)
    {
        Serial.println("Failed to set up SSL.");
        return false;
    }

    mbedtls_ssl_set_bio(&ssl, this, sendCallback, recvCallback, NULL);
    return true;
}

bool Certificates::connect(const char *hostname, uint16_t port)
{
    if (!client.connect(hostname, port))
    {
        Serial.println("Failed to connect to server.");
        return false;
    }

    if (mbedtls_ssl_set_hostname(&ssl, hostname) != 0)
    {
        Serial.println("Failed to set hostname.");
        return false;
    }

    int ret;
    Serial.println("Starting SSL handshake...");

    while ((ret = mbedtls_ssl_handshake(&ssl)) != 0)
    {
        if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE)
        {
            char error_buf[100];
            mbedtls_strerror(ret, error_buf, sizeof(error_buf));
            Serial.printf("SSL handshake failed: -0x%x: %s\n", -ret, error_buf);
            return false;
        }
    }

    Serial.println("SSL/TLS handshake successful.");
    return true;
}

int Certificates::send(const unsigned char *buf, size_t len)
{
    return mbedtls_ssl_write(&ssl, buf, len);
}

int Certificates::receive(unsigned char *buf, size_t len)
{
    return mbedtls_ssl_read(&ssl, buf, len);
}

void Certificates::cleanup()
{
    mbedtls_ssl_free(&ssl);
    mbedtls_ssl_config_free(&conf);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
    mbedtls_x509_crt_free(&rootCA);
    mbedtls_x509_crt_free(&clientCert);
    mbedtls_pk_free(&privateKey);
}

bool Certificates::loadCertificate(const char *path, mbedtls_x509_crt *cert)
{
    File file = SPIFFS.open(path, "r");
    if (!file)
    {
        Serial.printf("Failed to open %s\n", path);
        return false;
    }
    String certStr = file.readString();
    file.close();

    int ret = mbedtls_x509_crt_parse(cert, (const unsigned char *)certStr.c_str(), certStr.length() + 1);
    if (ret != 0)
    {
        Serial.printf("Failed to parse certificate %s\n", path);
        return false;
    }
    return true;
}

bool Certificates::loadPrivateKey(const char *path, mbedtls_pk_context *key)
{
    // Open the private key file from SPIFFS
    File file = SPIFFS.open(path, "r");
    if (!file)
    {
        Serial.printf("Failed to open %s\n", path);
        return false;
    }

    // Read the private key into a String object
    String keyStr = file.readString();
    file.close();

    // Parse the private key
    int ret = mbedtls_pk_parse_key(
        key,
        reinterpret_cast<const unsigned char *>(keyStr.c_str()),
        keyStr.length() + 1,
        nullptr,
        0);

    // Check for parsing errors
    if (ret != 0)
    {
        char error_buf[100];
        mbedtls_strerror(ret, error_buf, sizeof(error_buf));
        Serial.printf("Failed to parse private key %s: %s\n", path, error_buf);
        return false;
    }

    Serial.printf("Successfully loaded private key from %s\n", path);
    return true;
}

// Callback function for sending data
int Certificates::sendCallback(void *ctx, const unsigned char *buf, size_t len)
{
    TinyGsmClient *client = static_cast<TinyGsmClient *>(ctx);
    return client->write(buf, len);
}

// Callback function for receiving data
int Certificates::recvCallback(void *ctx, unsigned char *buf, size_t len)
{
    TinyGsmClient *client = static_cast<TinyGsmClient *>(ctx);
    return client->read(buf, len);
}