
#ifndef CERTIFICATES_H
#define CERTIFICATES_H
#include <TinyGsmClient.h>
#include <FS.h>
#include <SPIFFS.h>
#include "mbedtls/ssl.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/error.h"
#include "mbedtls/x509_crt.h"
#include "mbedtls/pk.h"

class Certificates
{
public:
    Certificates(TinyGsmClient &client);
    ~Certificates();
    bool init();
    bool connect(const char *hostname, uint16_t port);
    int send(const unsigned char *buf, size_t len);
    int receive(unsigned char *buf, size_t len);
    void cleanup();

private:
    TinyGsmClient &client;
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_entropy_context entropy;
    mbedtls_x509_crt rootCA;
    mbedtls_x509_crt clientCert;
    mbedtls_pk_context privateKey;

    bool loadCertificate(const char *path, mbedtls_x509_crt *cert);
    bool loadPrivateKey(const char *path, mbedtls_pk_context *key);
    static int sendCallback(void *ctx, const unsigned char *buf, size_t len);
    static int recvCallback(void *ctx, unsigned char *buf, size_t len);
};
#endif // CERTIFICATES_H