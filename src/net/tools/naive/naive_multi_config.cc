// Copyright 2024 klzgrad <kizdiv@gmail.com>. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/naive/naive_multi_config.h"

#include <iostream>
#include <string>

#include "base/values.h"

namespace net {

bool IsMultiTunnelConfig(const base::DictValue& config_dict) {
  return config_dict.Find("tunnels") != nullptr;
}

bool ExpandMultiTunnelConfig(base::DictValue& config_dict) {
  // Fast path: not a multi-tunnel config, nothing to do.
  if (!IsMultiTunnelConfig(config_dict)) {
    return true;
  }

  // Reject ambiguous configs that mix old and new formats.
  if (config_dict.Find("listen") != nullptr) {
    std::cerr << "Config error: 'listen' and 'tunnels' are mutually exclusive."
              << std::endl;
    return false;
  }
  if (config_dict.Find("proxy") != nullptr) {
    std::cerr << "Config error: 'proxy' and 'tunnels' are mutually exclusive."
              << std::endl;
    return false;
  }

  const base::Value* tunnels_value = config_dict.Find("tunnels");
  const base::ListValue* tunnels = tunnels_value->GetIfList();
  if (tunnels == nullptr) {
    std::cerr << "Config error: 'tunnels' must be an array." << std::endl;
    return false;
  }

  if (tunnels->empty()) {
    std::cerr << "Config error: 'tunnels' array must not be empty." << std::endl;
    return false;
  }

  base::ListValue listen_list;
  base::ListValue proxy_list;

  for (size_t i = 0; i < tunnels->size(); ++i) {
    const base::Value& tunnel_val = (*tunnels)[i];
    const base::DictValue* tunnel = tunnel_val.GetIfDict();
    if (tunnel == nullptr) {
      std::cerr << "Config error: tunnels[" << i << "] must be an object."
                << std::endl;
      return false;
    }

    // --- listen ---
    const base::Value* listen_val = tunnel->Find("listen");
    if (listen_val == nullptr) {
      std::cerr << "Config error: tunnels[" << i
                << "] is missing required field 'listen'." << std::endl;
      return false;
    }
    if (const std::string* s = listen_val->GetIfString()) {
      if (s->empty()) {
        std::cerr << "Config error: tunnels[" << i
                  << "].listen must not be empty." << std::endl;
        return false;
      }
      listen_list.Append(*s);
    } else {
      std::cerr << "Config error: tunnels[" << i
                << "].listen must be a string." << std::endl;
      return false;
    }

    // --- proxy ---
    const base::Value* proxy_val = tunnel->Find("proxy");
    if (proxy_val == nullptr) {
      std::cerr << "Config error: tunnels[" << i
                << "] is missing required field 'proxy'." << std::endl;
      return false;
    }
    // proxy can be a string (single hop) or an array of strings (chain).
    if (const std::string* s = proxy_val->GetIfString()) {
      if (s->empty()) {
        std::cerr << "Config error: tunnels[" << i
                  << "].proxy must not be empty." << std::endl;
        return false;
      }
      proxy_list.Append(*s);
    } else if (const base::ListValue* chain = proxy_val->GetIfList()) {
      // Proxy chain: join with comma, matching existing NaiveConfig parsing.
      if (chain->empty()) {
        std::cerr << "Config error: tunnels[" << i
                  << "].proxy array must not be empty." << std::endl;
        return false;
      }
      std::string joined;
      for (size_t j = 0; j < chain->size(); ++j) {
        const std::string* hop = (*chain)[j].GetIfString();
        if (hop == nullptr || hop->empty()) {
          std::cerr << "Config error: tunnels[" << i << "].proxy[" << j
                    << "] must be a non-empty string." << std::endl;
          return false;
        }
        if (j > 0) {
          joined += ',';
        }
        joined += *hop;
      }
      proxy_list.Append(joined);
    } else {
      std::cerr << "Config error: tunnels[" << i
                << "].proxy must be a string or array." << std::endl;
      return false;
    }

    // Warn about unrecognized per-tunnel keys (not fatal, but helpful).
    for (auto [key, unused_val] : *tunnel) {
      if (key != "listen" && key != "proxy") {
        std::cerr << "Warning: tunnels[" << i << "] key '" << key
                  << "' is not supported per-tunnel and will be ignored. "
                     "Set it at the top level." << std::endl;
      }
    }
  }

  // Rewrite config_dict: remove "tunnels", inject flat "listen" and "proxy".
  config_dict.Remove("tunnels");
  config_dict.Set("listen", std::move(listen_list));
  config_dict.Set("proxy", std::move(proxy_list));

  return true;
}

}  // namespace net
