/*
Copyright (c) 2013 Roger Light <roger@atchoo.org>
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. Neither the name of mosquitto nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
*/

#ifdef WITH_TLS

#ifdef WIN32
#  include <winsock2.h>
#endif

#include <string.h>
#include <openssl/conf.h>
#include <openssl/x509v3.h>
#include <openssl/ssl.h>

#ifdef WITH_BROKER
#  include "mosquitto_broker.h"
#endif
#include "mosquitto_internal.h"
#include "tls_mosq.h"

extern int tls_ex_index_mosq;

int _mosquitto_server_certificate_verify(int preverify_ok, X509_STORE_CTX *ctx)
{
	/* Preverify should have already checked expiry, revocation.
	 * We need to verify the hostname. */
	struct mosquitto *mosq;
	SSL *ssl;
	X509 *cert;

	/* Always reject if preverify_ok has failed. */
	if(!preverify_ok) return 0;

	ssl = X509_STORE_CTX_get_ex_data(ctx, SSL_get_ex_data_X509_STORE_CTX_idx());
	mosq = SSL_get_ex_data(ssl, tls_ex_index_mosq);
	if(!mosq) return 0;

	if(mosq->tls_insecure == false){
		if(X509_STORE_CTX_get_error_depth(ctx) == 0){
			/* FIXME - use X509_check_host() etc. for sufficiently new openssl (>=1.1.x) */
			cert = X509_STORE_CTX_get_current_cert(ctx);
			/* This is the peer certificate, all others are upwards in the chain. */
#if defined(WITH_BROKER)
			return _mosquitto_verify_certificate_hostname(cert, mosq->bridge->addresses[mosq->bridge->cur_address].address);
#else
			return _mosquitto_verify_certificate_hostname(cert, mosq->host);
#endif
		}else{
			return preverify_ok;
		}
	}else{
		return preverify_ok;
	}
}

/* This code is based heavily on the example provided in "Secure Programming
 * Cookbook for C and C++".
 */
int _mosquitto_verify_certificate_hostname(X509 *cert, const char *hostname)
{
	int san_index;
	int i;
	char name[256];
	X509_NAME *subj;
	CONF_VALUE *nval;
	const unsigned char *data;
	X509_EXTENSION *ext;
	const X509V3_EXT_METHOD *meth;
	STACK_OF(CONF_VALUE) *val;
	bool have_san_dns = false;

	san_index = X509_get_ext_by_NID(cert, NID_subject_alt_name, -1);
	ext = X509_get_ext(cert, san_index);
	if(ext){
		meth = X509V3_EXT_get(ext);
		if(meth){
			data = ext->value->data;
				
			val = meth->i2v(meth, meth->d2i(0, &data, ext->value->length), 0);
			for(i=0; i<sk_CONF_VALUE_num(val); i++){
				nval = sk_CONF_VALUE_value(val, i);
				if(!strcasecmp(nval->name, "DNS")){
					if(!strcasecmp(nval->value, hostname)){
						return 1;
					}else{
						have_san_dns = true;
					}
				}
			}
			if(have_san_dns){
				/* Only check CN if subjectAltName DNS entry does not exist. */
				return 0;
			}
		}
	}
	subj = X509_get_subject_name(cert);
	if(X509_NAME_get_text_by_NID(subj, NID_commonName, name, sizeof(name)) > 0){
		name[sizeof(name) - 1] = '\0';
		if (!strcasecmp(name, hostname)) return 1;
	}
	return 0;
}

#endif

