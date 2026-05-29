#!/usr/bin/env bash
set -euo pipefail

if [ "${ALLOW_JLCPCB_LOOKUP:-}" != "1" ]; then
  cat >&2 <<'EOF'
This command sends BOM-derived search terms to jlcpcb.com.
Set ALLOW_JLCPCB_LOOKUP=1 when you are comfortable disclosing those terms:

  ALLOW_JLCPCB_LOOKUP=1 ./deploy/find-jlcpcb-parts.sh
EOF
  exit 2
fi

node deploy/find-jlcpcb-parts.mjs "$@"
