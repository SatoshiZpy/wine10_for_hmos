// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Split a wine command-line argument string into an argv array, honoring
 * double quotes.  Shared by the ability base classes, which receive the
 * arguments as a single 'wineArgs' string via the launch Want.
 */
export default class ArgvUtil {
  static parseArgs(args: string | undefined): string[] | undefined {
    if (!args || args.trim().length === 0) {
      return undefined;
    }
    const argv: string[] = [];
    let current = '';
    let inQuote = false;
    for (const ch of args.trim()) {
      if (ch === '"') {
        inQuote = !inQuote;
      } else if (!inQuote && (ch === ' ' || ch === '\t')) {
        if (current.length > 0) {
          argv.push(current);
          current = '';
        }
      } else {
        current += ch;
      }
    }
    if (current.length > 0) {
      argv.push(current);
    }
    return argv.length > 0 ? argv : undefined;
  }
}
