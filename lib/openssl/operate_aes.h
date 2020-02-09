#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/aes.h>
#include <openssl/bn.h>
#include "../cJSON/cJSON.h"
#define KEY_LENGTH  256
#define TRY_TIMES   20  //失败重试次数  一般为1-3次

int aes_decrypt(char* in, char* key, char* out);
int aes_encrypt(char* in, char* key, char* out);
char *getRightEncrypt(char * in,char * key);

/**
 * 功能 解决有时加密后不能正确解密的情况
 * 要求 第一个参数必须是可序列化的json数据
 * 机制 在不能正确解密的字符串前拼接空格(不影响json解析但是改变了加密结果)
*/
char *getRightEncrypt(char * in,char * key)
{
    cJSON *json;
    char tem[KEY_LENGTH];
    char decrypt_buffer[KEY_LENGTH];
    char *encrypt_buffer=(char*)malloc(KEY_LENGTH);

    strcpy(tem,in);
    for (int i = 1; i <= TRY_TIMES; i++)
    {
        //printf("__第%d次尝试!\n",i);
        memset(encrypt_buffer, 0, KEY_LENGTH);
	    memset(decrypt_buffer, 0, KEY_LENGTH);
        aes_encrypt(tem, key, encrypt_buffer);
        //printf("长度:%d 密文：\n%s\n", (int)strlen(encrypt_buffer),encrypt_buffer);
        aes_decrypt(encrypt_buffer, key, decrypt_buffer);
       //printf("解密：%s\n", decrypt_buffer);

        json=cJSON_Parse(decrypt_buffer);
        if (json)
        {
            cJSON_Delete(json);
            // char cmd[64];
            // sprintf(cmd,"echo %d >> log.txt",i);
            // system(cmd);
            return encrypt_buffer;
        }
        //解密失败了
        memset(tem, 0, KEY_LENGTH);
        for(int j=0;j<i;j++)
        {
            strcat(tem," ");
        }
        strcat(tem,in);
    }
    cJSON_Delete(json);
    return NULL;
}

// out的内存大小需要注意 后边有 out += AES_BLOCK_SIZE 
// 所以out 内存最小不能小于 AES_BLOCK_SIZE 
int aes_encrypt(char* in, char* key, char* out)
{
    if (!in || !key || !out)
    {
        return 0;
    }
 
    AES_KEY aes;
    if (AES_set_encrypt_key((unsigned char*)key, KEY_LENGTH, &aes) < 0)
    {
        return 0;
    }
 
    int len = strlen(in), en_len = 0;
 
    //输入输出字符串够长。而且是AES_BLOCK_SIZE的整数倍，须要严格限制
    while (en_len < len)
    {
        AES_encrypt((unsigned char*)in, (unsigned char*)out, &aes);
        in	+= AES_BLOCK_SIZE;
        out += AES_BLOCK_SIZE;
        en_len += AES_BLOCK_SIZE;
    }
 
    return 1;
}
 
int aes_decrypt(char* in, char* key, char* out)
{
    if (!in || !key || !out)
    {
        return 0;
    }
 
    AES_KEY aes;
    if (AES_set_decrypt_key((unsigned char*)key, KEY_LENGTH, &aes) < 0)
    {
        return 0;
    }
 
    int len = strlen(in), en_len = 0;
    while (en_len < len)
    {
        AES_decrypt((unsigned char*)in, (unsigned char*)out, &aes);
        in	+= AES_BLOCK_SIZE;
        out += AES_BLOCK_SIZE;
        en_len += AES_BLOCK_SIZE;
    }
 
    return 1;
}