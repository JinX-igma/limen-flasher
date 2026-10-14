#pragma once

// This firmware trusts a specific operator's gateway, not a global CA —
// each deployment pins its own gateway's TLS certificate (or the local
// CA that issued it), because there is no longer one fixed upstream
// host (api.openai.com) to hardcode a trust anchor for.
//
// Generate your real include/gateway_root_ca.h by running:
//   python3 scripts/embed_gateway_ca.py /path/to/your/gateway/ca.crt
// (ca.crt is produced by limen-gateway's scripts/provision-local.py)
//
// include/gateway_root_ca.h is gitignored — never commit a real one.
// This placeholder exists so an unconfigured build fails loudly instead
// of silently shipping firmware that trusts nothing real.

#error "Run scripts/embed_gateway_ca.py <path-to-your-gateway-ca.crt> before building. See README.md."

static const char kGatewayRootCa[] PROGMEM = "";
