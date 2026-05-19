// Copyright 2024 klzgrad <kizdiv@gmail.com>. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Multi-tunnel configuration support.
//
// This file provides a NON-INVASIVE extension to NaiveProxy that allows a
// single process to bind multiple local ports, each routed to a different
// upstream proxy server.
//
// Design goals:
//  - Zero changes to upstream source files (naive_config.*, naive_proxy_bin.cc
//    changes are additive-only: one #include + one call).
//  - Self-contained: all logic lives in naive_multi_config.{h,cc}.
//  - Backward compatible: configs without "tunnels" pass through unchanged.
//  - Easy to rebase: no structural changes to NaiveConfig or NaiveProxy.
//
// New JSON format
// ---------------
// Instead of the flat format:
//
//   { "listen": "socks://127.0.0.1:1080",
//     "proxy": "https://user:pass@server.com" }
//
// Use the "tunnels" array to declare N independent listen/proxy pairs:
//
//   {
//     "tunnels": [
//       {
//         "listen": "socks://127.0.0.1:1080",
//         "proxy": "https://user:pass@server1.com"
//       },
//       {
//         "listen": "http://127.0.0.1:1081",
//         "proxy": "https://user:pass@server2.com"
//       }
//     ],
//     "log": "naive.log",
//     "no-post-quantum": true
//   }
//
// Global options (log, extra-headers, host-resolver-rules, etc.) are declared
// at the top level and apply to all tunnels.
//
// Thread model
// ------------
// All NaiveProxy instances run on the same single IO thread using async I/O.
// Each instance has its own HttpNetworkSession connected to its upstream proxy.
// No additional threads are created; the existing model is preserved.
//
//   ┌──────────────────────────────────────────────────────┐
//   │                 Main IO Thread (event loop)           │
//   │                                                      │
//   │  ┌─────────────────┐  ┌─────────────────┐  ...      │
//   │  │ NaiveProxy[0]   │  │ NaiveProxy[1]   │           │
//   │  │ :1080 → server1 │  │ :1081 → server2 │           │
//   │  └─────────────────┘  └─────────────────┘           │
//   │                                                      │
//   │  ┌────────────────────────────────────────────────┐  │
//   │  │        ThreadPool (DNS / cert / crypto)         │  │
//   │  └────────────────────────────────────────────────┘  │
//   └──────────────────────────────────────────────────────┘

#ifndef NET_TOOLS_NAIVE_NAIVE_MULTI_CONFIG_H_
#define NET_TOOLS_NAIVE_NAIVE_MULTI_CONFIG_H_

#include "base/values.h"

namespace net {

// Checks whether |config_dict| uses the multi-tunnel format (i.e. contains a
// "tunnels" key).
bool IsMultiTunnelConfig(const base::DictValue& config_dict);

// Expands a multi-tunnel config dict in-place into the flat format understood
// by NaiveConfig::Parse().
//
// If the config does not contain "tunnels", this is a no-op and returns true.
//
// On success, |config_dict| is transformed:
//   - "tunnels" is removed.
//   - "listen" is set to the array of listen URIs from each tunnel entry.
//   - "proxy" is set to the array of proxy URIs from each tunnel entry.
//   - All other top-level keys are left unchanged.
//
// Returns false (with a message to stderr) if the config is malformed.
bool ExpandMultiTunnelConfig(base::DictValue& config_dict);

}  // namespace net

#endif  // NET_TOOLS_NAIVE_NAIVE_MULTI_CONFIG_H_
