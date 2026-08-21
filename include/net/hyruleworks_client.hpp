#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace totk::net {

struct HyruleWorksBuildSummary {
    int id = 0;
    std::string name;
    std::string creator;
    std::string description;
    std::string imageUrl;    // raw hyruleworks.s3.*.amazonaws.com URL — rewrite via hyruleWorksImageCdnUrl()
    std::string caiFileUrl;  // raw hyruleworks-files.s3.*.amazonaws.com URL — rewrite via hyruleWorksBlueprintCdnUrl()
};

struct HyruleWorksBuildDetail : HyruleWorksBuildSummary {
    std::vector<std::string> imageUrls;
    std::vector<std::string> tags;
};

// GET https://www.hyruleworks.com/?page=N(&name=searchQuery) — the site's own
// public catalog listing (confirmed live 2026-08-17: 30 builds/page, plain
// GET with SSR search params, no auth or gated /api/** surface needed).
// Blocking network call — run off the UI thread (brls::async).
bool fetchHyruleWorksCatalogPage(int page, const std::string& searchQuery,
                                  std::vector<HyruleWorksBuildSummary>& outBuilds, bool& outHasNextPage,
                                  std::string& error);

// GET https://www.hyruleworks.com/builds/<id> for the full image gallery and
// tag list a catalog card doesn't carry.
bool fetchHyruleWorksBuildDetail(int buildId, HyruleWorksBuildDetail& outDetail, std::string& error);

// Downloads the raw .cai blueprint bytes for a build. Takes the caiFileUrl
// from a summary/detail (the private S3 bucket path) and fetches it through
// the same public CloudFront distribution the site's own "Download (.cai)"
// button uses — the raw S3 host 403s directly (confirmed live 2026-08-17).
bool downloadHyruleWorksBlueprint(const std::string& caiFileUrl, std::vector<uint8_t>& outBytes, std::string& error);

// Rewrites a raw hyruleworks.s3.*.amazonaws.com image URL (also 403s
// directly) to the site's public image CDN, requesting a specific pixel
// width and forcing JPEG output. HyruleWorks stores originals as a mix of
// webp/png/jpg and this app has no WebP decoder, but the CDN itself will
// re-encode on request (confirmed live 2026-08-17 via ?format=jpeg) — so
// every downloaded preview image can go through the existing
// totk::loadJpegFromMem() path unchanged.
std::string hyruleWorksImageCdnUrl(const std::string& rawImageUrl, int width);

}  // namespace totk::net
