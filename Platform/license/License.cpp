#include "License.h"
#include <random>
#include <chrono>
#include <cstring>

// openssl rand -hex 32
static const uint8_t SECRET_KEY[] = {
    0x8c, 0x77, 0x0c, 0x82, 0x13, 0xac, 0x07, 0x4f,
    0xa3, 0x1f, 0x84, 0x55, 0xf6, 0x89, 0xdf, 0x0b,
    0xa1, 0x3b, 0x1d, 0x13, 0x51, 0x05, 0x31, 0x55,
    0xb6, 0x2d, 0xf7, 0x2e, 0x97, 0xaa, 0x31, 0xd2,
    0x84, 0x7f, 0x1e, 0x38, 0x4e, 0x3c, 0x27, 0xee,
    0xde, 0x05, 0xff, 0x5c, 0x9c, 0x88, 0x7f, 0x8d,
    0xb0, 0x2a, 0x69, 0xb1, 0x10, 0x44, 0x16, 0x17,
    0x87, 0x3c, 0xe0, 0xf7, 0x48, 0x03, 0x28, 0x0d,
    0x8c, 0x98, 0xe5, 0x4a, 0x3c, 0xe2, 0xe4, 0x41,
    0x8c, 0xcf, 0x1d, 0xdb, 0x9e, 0x1d, 0x2e, 0xd8,
    0xf9, 0x8a, 0x1e, 0x06, 0xe6, 0x3b, 0x30, 0x6f,
    0x81, 0x04, 0x9e, 0x40, 0xfc, 0xc6, 0x93, 0x69,
    0x92, 0xb5, 0x9b, 0x23, 0x29, 0xb1, 0xb8, 0xc7,
    0x18, 0x59, 0xdb, 0x3d, 0x96, 0x88, 0x23, 0x9d,
    0xc4, 0x9c, 0x16, 0xb1, 0x85, 0x3d, 0xd4, 0x3a,
    0x52, 0x48, 0x6d, 0x1e, 0x2d, 0x3d, 0xeb, 0x70
};
static const size_t SECRET_KEY_LEN = sizeof(SECRET_KEY);

static const char ALPHABET[] = "ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz23456789!@#$%&*+-=?";

static const size_t ALPHABET_SIZE = sizeof(ALPHABET) - 1; // 64

// ============================================================
// Implementación mínima de SHA-256 (pública dominio / compacta)
// ============================================================
namespace sha256 {
    struct Context {
        uint64_t bitlen;
        uint32_t state[8];
        uint8_t data[64];
        uint32_t datalen;
    };

    static uint32_t rotr(uint32_t x, uint32_t n) { return (x >> n) | (x << (32 - n)); }
    static uint32_t ch(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (~x & z); }
    static uint32_t maj(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (x & z) ^ (y & z); }
    static uint32_t ep0(uint32_t x) { return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22); }
    static uint32_t ep1(uint32_t x) { return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25); }
    static uint32_t sig0(uint32_t x) { return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3); }
    static uint32_t sig1(uint32_t x) { return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10); }

    static const uint32_t k[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
    };

    static void transform(Context* ctx, const uint8_t data[]) {
        uint32_t m[64], a, b, c, d, e, f, g, h, t1, t2;
        for (int i = 0, j = 0; i < 16; ++i, j += 4)
            m[i] = (data[j] << 24) | (data[j + 1] << 16) | (data[j + 2] << 8) | data[j + 3];
        for (int i = 16; i < 64; ++i)
            m[i] = sig1(m[i - 2]) + m[i - 7] + sig0(m[i - 15]) + m[i - 16];

        a = ctx->state[0];
        b = ctx->state[1];
        c = ctx->state[2];
        d = ctx->state[3];
        e = ctx->state[4];
        f = ctx->state[5];
        g = ctx->state[6];
        h = ctx->state[7];

        for (int i = 0; i < 64; ++i) {
            t1 = h + ep1(e) + ch(e, f, g) + k[i] + m[i];
            t2 = ep0(a) + maj(a, b, c);
            h = g;
            g = f;
            f = e;
            e = d + t1;
            d = c;
            c = b;
            b = a;
            a = t1 + t2;
        }

        ctx->state[0] += a;
        ctx->state[1] += b;
        ctx->state[2] += c;
        ctx->state[3] += d;
        ctx->state[4] += e;
        ctx->state[5] += f;
        ctx->state[6] += g;
        ctx->state[7] += h;
    }

    static void init(Context* ctx) {
        ctx->datalen = 0;
        ctx->bitlen = 0;
        ctx->state[0] = 0x6a09e667;
        ctx->state[1] = 0xbb67ae85;
        ctx->state[2] = 0x3c6ef372;
        ctx->state[3] = 0xa54ff53a;
        ctx->state[4] = 0x510e527f;
        ctx->state[5] = 0x9b05688c;
        ctx->state[6] = 0x1f83d9ab;
        ctx->state[7] = 0x5be0cd19;
    }

    static void update(Context* ctx, const uint8_t data[], size_t len) {
        for (size_t i = 0; i < len; ++i) {
            ctx->data[ctx->datalen] = data[i];
            ctx->datalen++;
            if (ctx->datalen == 64) {
                transform(ctx, ctx->data);
                ctx->bitlen += 512;
                ctx->datalen = 0;
            }
        }
    }

    static void final(Context* ctx, uint8_t hash[]) {
        uint32_t i = ctx->datalen;
        if (ctx->datalen < 56) {
            ctx->data[i++] = 0x80;
            while (i < 56) ctx->data[i++] = 0x00;
        } else {
            ctx->data[i++] = 0x80;
            while (i < 64) ctx->data[i++] = 0x00;
            transform(ctx, ctx->data);
            memset(ctx->data, 0, 56);
        }

        ctx->bitlen += ctx->datalen * 8;
        ctx->data[63] = ctx->bitlen;
        ctx->data[62] = ctx->bitlen >> 8;
        ctx->data[61] = ctx->bitlen >> 16;
        ctx->data[60] = ctx->bitlen >> 24;
        ctx->data[59] = ctx->bitlen >> 32;
        ctx->data[58] = ctx->bitlen >> 40;
        ctx->data[57] = ctx->bitlen >> 48;
        ctx->data[56] = ctx->bitlen >> 56;
        transform(ctx, ctx->data);

        for (i = 0; i < 4; ++i) {
            hash[i] = (ctx->state[0] >> (24 - i * 8)) & 0xff;
            hash[i + 4] = (ctx->state[1] >> (24 - i * 8)) & 0xff;
            hash[i + 8] = (ctx->state[2] >> (24 - i * 8)) & 0xff;
            hash[i + 12] = (ctx->state[3] >> (24 - i * 8)) & 0xff;
            hash[i + 16] = (ctx->state[4] >> (24 - i * 8)) & 0xff;
            hash[i + 20] = (ctx->state[5] >> (24 - i * 8)) & 0xff;
            hash[i + 24] = (ctx->state[6] >> (24 - i * 8)) & 0xff;
            hash[i + 28] = (ctx->state[7] >> (24 - i * 8)) & 0xff;
        }
    }

    // HMAC-SHA256
    void hmac(const uint8_t* key, size_t keyLen,
              const uint8_t* data, size_t dataLen,
              uint8_t out[32]) {
        uint8_t k[64] = {0};
        if (keyLen > 64) {
            Context ctx;
            init(&ctx);
            update(&ctx, key, keyLen);
            final(&ctx, k);
        } else {
            memcpy(k, key, keyLen);
        }

        uint8_t ipad[64], opad[64];
        for (int i = 0; i < 64; ++i) {
            ipad[i] = k[i] ^ 0x36;
            opad[i] = k[i] ^ 0x5c;
        }

        uint8_t inner[32];
        Context ctx;
        init(&ctx);
        update(&ctx, ipad, 64);
        update(&ctx, data, dataLen);
        final(&ctx, inner);

        init(&ctx);
        update(&ctx, opad, 64);
        update(&ctx, inner, 32);
        final(&ctx, out);
    }
} // namespace sha256

// ============================================================
// Codificación / Decodificación con alfabeto personalizado
// ============================================================
static std::string encode(const uint8_t* data, size_t len) {
    std::string result;
    result.reserve((len * 8 + 5) / 6); // aproximadamente

    uint32_t buffer = 0;
    int bitsLeft = 0;

    for (size_t i = 0; i < len; ++i) {
        buffer = (buffer << 8) | data[i];
        bitsLeft += 8;
        while (bitsLeft >= 6) {
            bitsLeft -= 6;
            result += ALPHABET[(buffer >> bitsLeft) & 0x3F];
        }
    }
    if (bitsLeft > 0) {
        result += ALPHABET[(buffer << (6 - bitsLeft)) & 0x3F];
    }
    return result;
}

static bool decode(const std::string& str, std::vector<uint8_t>& out) {
    out.clear();
    uint32_t buffer = 0;
    int bitsLeft = 0;

    for (char c: str) {
        const char* p = strchr(ALPHABET, c);
        if (!p) return false; // carácter inválido
        int val = static_cast<int>(p - ALPHABET);

        buffer = (buffer << 6) | val;
        bitsLeft += 6;

        if (bitsLeft >= 8) {
            bitsLeft -= 8;
            out.push_back(static_cast<uint8_t>((buffer >> bitsLeft) & 0xFF));
        }
    }
    return true;
}

// ============================================================
// API pública
// ============================================================
std::string License::generate(const LicenseType& type) {
    if (static_cast<uint8_t>(type) == static_cast<uint8_t>(LicenseType::Invalid)) {
        return {};
    }

    // 1. Payload: version (1) + type (1) + nonce (8)
    uint8_t payload[10];
    payload[0] = 0x01; // versión del formato
    payload[1] = static_cast<uint8_t>(type);

    // Nonce aleatorio
    std::random_device rd;
    std::mt19937_64 gen(rd());
    uint64_t nonce = gen();
    memcpy(payload + 2, &nonce, 8);

    // 2. HMAC-SHA256 del payload → tomamos los primeros 8 bytes como MAC
    uint8_t mac[32];
    sha256::hmac(SECRET_KEY, SECRET_KEY_LEN, payload, 10, mac);

    // 3. Datos finales: payload + MAC truncado (8 bytes) = 18 bytes
    uint8_t finalData[18];
    memcpy(finalData, payload, 10);
    memcpy(finalData + 10, mac, 8);

    // 4. Codificar
    return encode(finalData, 18);
}

License::LicenseType License::validate(const std::string& licenseStr) const {
    LicenseType outType = LicenseType::Invalid;

    if (licenseStr.empty() || licenseStr.size() < 20 || licenseStr.size() > 40)
        return outType;

    std::vector<uint8_t> data;
    if (!decode(licenseStr, data) || data.size() != 18)
        return outType;

    // Separar
    uint8_t payload[10];
    uint8_t receivedMac[8];
    memcpy(payload, data.data(), 10);
    memcpy(receivedMac, data.data() + 10, 8);

    // Comprobar versión
    if (payload[0] != 0x01)
        return outType;

    // Recalcular MAC
    uint8_t computedMac[32];
    sha256::hmac(SECRET_KEY, SECRET_KEY_LEN, payload, 10, computedMac);

    // Comparación en tiempo constante
    uint8_t diff = 0;
    for (int i = 0; i < 8; ++i) {
        diff |= receivedMac[i] ^ computedMac[i];
    }
    if (diff != 0)
        return outType;

    outType = static_cast<LicenseType>(payload[1]);
    return outType;
}

const std::string License::typeName(const LicenseType& t) {
    switch (t) {
        case LicenseType::Basic: return "Basic";
        case LicenseType::Standard: return "Standard";
        case LicenseType::Pro: return "Pro";
        case LicenseType::Enterprise: return "Enterprise";
        default: return "Invalid";
    }
}
