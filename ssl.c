#include <openssl/ssl.h>

        SSL_load_error_strings();                /* readable error messages */
        SSL_library_init();                      /* initialize library */
        actions_to_seed_PRNG();

