extern "C" {
#include "aes.h"  // tiny-AES-c header
}

// Encryption
// AES params
#define AES_KEY_SIZE 16
#define AES_BLOCK_SIZE 16

uint8_t aes_key[AES_KEY_SIZE] = {
  0x60, 0x3D, 0xEB, 0x10, 0x15, 0xCA, 0x71, 0xBE,
  0x2B, 0x73, 0xAE, 0xF0, 0x85, 0x7D, 0x77, 0x81
};

// CTR requires a 16-byte IV (nonce + counter)
char aes_iv[AES_KEY_SIZE] = {
  0x00, 0x01, 0x02, 0x03,
  0x04, 0x05, 0x06, 0x07,
  0x08, 0x09, 0x0A, 0x0B,
  0x0C, 0x0D, 0x0E, 0x0F
};

size_t padBuffer(uint8_t *buf, size_t inputLen) {
  size_t paddedLen = ((inputLen + AES_BLOCK_SIZE - 1) / AES_BLOCK_SIZE) * AES_BLOCK_SIZE;
  uint8_t pad = paddedLen - inputLen;
  for (size_t i = inputLen; i < paddedLen; i++) {
    buf[i] = pad;
  }
  return paddedLen;
}

void removePadding(uint8_t *buf, size_t &len) {
  uint8_t pad = buf[len - 1];
  if (pad > 0 && pad <= AES_BLOCK_SIZE) {
    // Check padding validity
    bool valid = true;
    for (size_t i = len - pad; i < len; i++) {
      if (buf[i] != pad) valid = false;
    }
    if (valid) {
      len -= pad;  // Remove padding bytes from length
    }
  }
}

void encryptAndTransmit(const char *plaintext) {
  size_t len = strlen(plaintext);
  size_t paddedLen = ((len + AES_BLOCK_SIZE - 1) / AES_BLOCK_SIZE) * AES_BLOCK_SIZE;
  if (paddedLen > 256) return;  // guard: fits in buffer

  uint8_t buffer[256];
  memset(buffer, 0, sizeof(buffer));
  memcpy(buffer, plaintext, len);
  padBuffer(buffer, len);

  uint8_t iv[AES_BLOCK_SIZE];
  aes_iv[AES_BLOCK_SIZE];

  struct AES_ctx ctx;
  AES_init_ctx_iv(&ctx, aes_key, iv);
  AES_CBC_encrypt_buffer(&ctx, buffer, paddedLen);

  uint8_t packet[16 + paddedLen];
  memcpy(packet, iv, AES_BLOCK_SIZE);
  memcpy(packet + AES_BLOCK_SIZE, buffer, paddedLen);

  Serial.println("Sending encrypted message...");
  Radio.Send((uint8_t *)packet, AES_BLOCK_SIZE + paddedLen);
  delay(300);          // Wait for TX completion
  Radio.IrqProcess();  // Process IRQ after TX
}

void decryptMessage(uint8_t *packet, size_t packetLen, char *outText, size_t maxLen) {
  if (packetLen <= AES_BLOCK_SIZE) {
    Serial.println("Packet too short!");
    return;
  }

  uint8_t iv[AES_BLOCK_SIZE];
  memcpy(iv, packet, AES_BLOCK_SIZE);

  size_t cipherLen = packetLen - AES_BLOCK_SIZE;
  uint8_t cipher[cipherLen];
  memcpy(cipher, packet + AES_BLOCK_SIZE, cipherLen);

  struct AES_ctx ctx;
  AES_init_ctx_iv(&ctx, aes_key, iv);
  AES_CBC_decrypt_buffer(&ctx, cipher, cipherLen);

  size_t plainLen = cipherLen;
  removePadding(cipher, plainLen);

  // Copy to output buffer safely and null terminate
  size_t copyLen = plainLen < maxLen - 1 ? plainLen : maxLen - 1;
  memcpy(outText, cipher, copyLen);
  outText[copyLen] = '\0';
}