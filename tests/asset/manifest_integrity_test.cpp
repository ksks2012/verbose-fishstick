#include "log.hpp"
#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

// NOTO3D_ASSETS_DIR is defined by CMake (absolute path to assets/).
// Ticket 0016 — M3 Noto asset set verification & freeze: this test validates the
// manifest contract DIRECTLY (parse JSON, resolve paths, SHA-256 the files).  It
// must NOT call the production AssetRegistry — it is the independent contract
// check that the frozen fixture has not drifted, so drift on either side (the
// manifest or the registry's assumptions) is caught.
#ifndef NOTO3D_ASSETS_DIR
#error "NOTO3D_ASSETS_DIR must be set by the build system"
#endif

namespace {

int g_failures = 0;

void check(bool cond, const char* name) {
    if (cond) { NOTO3D_INFO("PASS  {}", name); }
    else       { NOTO3D_ERROR("FAIL  {}", name); ++g_failures; }
}

// ---------------------------------------------------------------------------
// Minimal FIPS 180-4 SHA-256 (self-contained; verified by known-answer vectors).
// ---------------------------------------------------------------------------
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using u8  = std::uint8_t;

const u32 kK[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u,
    0x923f82a4u, 0xab1c5ed5u, 0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
    0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u, 0xe49b69c1u, 0xefbe4786u,
    0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u,
    0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
    0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u, 0xa2bfe8a1u, 0xa81a664bu,
    0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au,
    0x5b9cca4fu, 0x682e6ff3u, 0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
    0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u,
};

u32 rotr(u32 x, u32 n) { return (x >> n) | (x << (32u - n)); }

u32 loadBe(const u8* p) {
    return (static_cast<u32>(p[0]) << 24) | (static_cast<u32>(p[1]) << 16) |
           (static_cast<u32>(p[2]) << 8) | static_cast<u32>(p[3]);
}

void storeBe(u8* p, u32 v) {
    p[0] = static_cast<u8>(v >> 24);
    p[1] = static_cast<u8>(v >> 16);
    p[2] = static_cast<u8>(v >> 8);
    p[3] = static_cast<u8>(v);
}

std::array<u8, 32> sha256(const std::vector<u8>& msg) {
    u32 h[8] = {
        0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
        0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u,
    };

    const u64 bitlen = static_cast<u64>(msg.size()) * 8u;
    std::vector<u8> m(msg);
    m.push_back(0x80);
    while (m.size() % 64 != 56) m.push_back(0);
    for (int i = 0; i < 8; ++i) m.push_back(static_cast<u8>(bitlen >> (56 - 8 * i)));

    std::array<u32, 64> w{};
    for (std::size_t off = 0; off < m.size(); off += 64) {
        for (int t = 0; t < 16; ++t) {
            w[t] = loadBe(&m[off + 4 * static_cast<std::size_t>(t)]);
        }
        for (int t = 16; t < 64; ++t) {
            const u32 s0 = rotr(w[t - 15], 7) ^ rotr(w[t - 15], 18) ^ (w[t - 15] >> 3);
            const u32 s1 = rotr(w[t - 2], 17) ^ rotr(w[t - 2], 19) ^ (w[t - 2] >> 10);
            w[t] = w[t - 16] + s0 + w[t - 7] + s1;
        }
        u32 a = h[0], b = h[1], c = h[2], d = h[3];
        u32 e = h[4], f = h[5], g = h[6], hh = h[7];
        for (int t = 0; t < 64; ++t) {
            const u32 s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
            const u32 ch = (e & f) ^ (~e & g);
            const u32 t1 = hh + s1 + ch + kK[t] + w[t];
            const u32 s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
            const u32 maj = (a & b) ^ (a & c) ^ (b & c);
            const u32 t2 = s0 + maj;
            hh = g; g = f; f = e;
            e = d + t1;
            d = c; c = b; b = a;
            a = t1 + t2;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d;
        h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
    }

    std::array<u8, 32> out{};
    for (int i = 0; i < 8; ++i) storeBe(&out[4 * static_cast<std::size_t>(i)], h[i]);
    return out;
}

std::string hex(const std::array<u8, 32>& d) {
    static const char* kDigits = "0123456789abcdef";
    std::string s;
    s.reserve(64);
    for (u8 b : d) {
        s.push_back(kDigits[b >> 4]);
        s.push_back(kDigits[b & 0x0f]);
    }
    return s;
}

std::string sha256Hex(const std::vector<u8>& bytes) { return hex(sha256(bytes)); }

std::vector<u8> readFileBytes(const std::filesystem::path& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return {};
    return std::vector<u8>(std::istreambuf_iterator<char>(f),
                           std::istreambuf_iterator<char>());
}

void testSha256Kat() {
    check(sha256Hex({}) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
          "sha256: empty-message KAT");
    const std::vector<u8> abc{'a', 'b', 'c'};
    check(sha256Hex(abc) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
          "sha256: 'abc' KAT");
}

// ---------------------------------------------------------------------------
// M3 frozen fixture contract — direct manifest validation (never via AssetRegistry)
// ---------------------------------------------------------------------------
const std::vector<std::string> kM3FrozenAssetIds = {
    "smile", "warm", "cool", "crown", "top_hat", "sunglasses",
};

struct PlaceholderMapping {
    std::string id;
    std::string geometryRel;  // relative to assets/
};

const std::vector<PlaceholderMapping> kM3PlaceholderMapping = {
    {"smile",      "geometry/placeholders/base_face.obj"},
    {"warm",       "geometry/placeholders/base_face.obj"},
    {"cool",       "geometry/placeholders/base_face.obj"},
    {"crown",      "geometry/placeholders/accessory.obj"},
    {"top_hat",    "geometry/placeholders/accessory.obj"},
    {"sunglasses", "geometry/placeholders/generic.obj"},
};

void testManifestIntegrity() {
    const std::filesystem::path assetsDir(NOTO3D_ASSETS_DIR);
    const std::filesystem::path manifestPath = assetsDir / "manifest.json";

    std::ifstream f(manifestPath);
    check(static_cast<bool>(f), "manifest.json exists under NOTO3D_ASSETS_DIR");
    if (!f) return;

    nlohmann::json manifest;
    try {
        f >> manifest;
        check(true, "manifest.json parses as JSON");
    } catch (const std::exception& e) {
        check(false, "manifest.json parses as JSON");
        NOTO3D_ERROR("      parse error: {}", e.what());
        return;
    }
    check(manifest["manifest_version"] == 1, "manifest_version == 1");

    const auto& assets = manifest["assets"];
    check(assets.is_array(), "assets is an array");

    // Frozen membership: exactly the 6 M3 IDs, in any order.
    std::vector<std::string> ids;
    for (const auto& a : assets) ids.push_back(a["id"].get<std::string>());
    bool sameSet = ids.size() == kM3FrozenAssetIds.size();
    if (sameSet) {
        for (const auto& id : kM3FrozenAssetIds) {
            if (std::find(ids.begin(), ids.end(), id) == ids.end()) { sameSet = false; break; }
        }
    }
    check(sameSet, "manifest asset IDs == M3 frozen set (exactly 6)");
    if (!sameSet) {
        std::string joined;
        for (const auto& id : ids) joined += " " + id;
        NOTO3D_ERROR("      found IDs:{}", joined);
        return;
    }

    const bool hasDemoChecker =
        std::find(ids.begin(), ids.end(), "demo_checker") != ids.end();
    check(!hasDemoChecker, "demo_checker excluded from manifest (0016 fixture contract)");

    // Per-entry contract.
    for (const auto& a : assets) {
        const std::string id   = a["id"].get<std::string>();
        const auto&        meta = a["metadata"];

        const bool metaOk = meta.contains("source") && meta.contains("revision") &&
                            meta.contains("license") && meta.contains("reference_sha256") &&
                            meta.contains("geometry_sha256");
        check(metaOk, ("metadata fields source/revision/license/reference_sha256/"
                       "geometry_sha256 complete (" + id + ")").c_str());

        const bool axisOk = a["axis_data"]["up_axis"] == "x" ||
                            a["axis_data"]["up_axis"] == "y" ||
                            a["axis_data"]["up_axis"] == "z";
        check(axisOk, ("axis_data.up_axis in {x,y,z} (" + id + ")").c_str());

        const bool geomNonNull = a.contains("geometry") && !a["geometry"].is_null();
        const bool srcNonNull  = a.contains("geometry_source") && !a["geometry_source"].is_null();
        check(geomNonNull && srcNonNull, ("geometry + geometry_source present (" + id + ")").c_str());
        check(geomNonNull == srcNonNull,
              ("invariant geometry<->geometry_source nullness (" + id + ")").c_str());
        check(srcNonNull && a["geometry_source"] == "placeholder",
              ("geometry_source == placeholder (" + id + ")").c_str());

        std::filesystem::path geomPath;
        std::filesystem::path refPath;
        bool geomExists = false;
        bool refExists  = false;
        if (geomNonNull) {
            geomPath   = assetsDir / a["geometry"].get<std::string>();
            geomExists = std::filesystem::is_regular_file(geomPath);
        }
        if (a.contains("reference_image") && !a["reference_image"].is_null()) {
            refPath   = assetsDir / a["reference_image"].get<std::string>();
            refExists = std::filesystem::is_regular_file(refPath);
        }
        check(geomExists, ("geometry file exists (" + id + ")").c_str());

        bool refShaOk = false;
        if (refExists && meta.contains("reference_sha256")) {
            refShaOk = sha256Hex(readFileBytes(refPath)) ==
                       meta["reference_sha256"].get<std::string>();
        }
        check(refExists, ("reference_image exists (" + id + ")").c_str());
        check(refShaOk, ("reference SHA-256 == metadata.reference_sha256 (" + id + ")").c_str());

        bool geomShaOk = false;
        if (geomExists && meta.contains("geometry_sha256")) {
            geomShaOk = sha256Hex(readFileBytes(geomPath)) ==
                        meta["geometry_sha256"].get<std::string>();
        }
        check(geomShaOk, ("geometry SHA-256 == metadata.geometry_sha256 (" + id + ")").c_str());
    }

    // Expected placeholder mapping — fixture contract (shared-geometry invariant:
    // base_face.obj must hash equal across smile/warm/cool, etc.).
    for (const auto& expect : kM3PlaceholderMapping) {
        bool found = false;
        for (const auto& a : assets) {
            if (a["id"] == expect.id) {
                found = true;
                check(a["geometry"] == expect.geometryRel,
                      ("geometry maps to " + expect.geometryRel + " (" + expect.id + ")").c_str());
                break;
            }
        }
        check(found, ("fixture mapping entry present (" + expect.id + ")").c_str());
    }
}

}  // namespace

int main() {
    noto3d::core::log_init("noto3d-manifest-tests");

    testSha256Kat();
    testManifestIntegrity();

    if (g_failures == 0) { NOTO3D_INFO("ALL PASS"); return EXIT_SUCCESS; }
    NOTO3D_ERROR("{} FAILURE(S)", g_failures);
    return EXIT_FAILURE;
}