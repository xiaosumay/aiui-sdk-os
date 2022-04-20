/**
 *
 * @author  zrmei
 * @date    2021-07-21
 *
 * @brief
 */

#ifndef HTTP_POST_MD5_H
#define HTTP_POST_MD5_H

/*#ifdef __cplusplus
extern "C" {
#endif*/

int mbedtls_md5_ret(const unsigned char* input, size_t ilen, unsigned char output[16]);

/*#ifdef __cplusplus
};
#endif*/

#endif    //HTTP_POST_MD5_H
