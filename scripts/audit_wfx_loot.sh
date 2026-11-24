#!/usr/bin/env bash
# scripts/audit_wfx_loot.sh (v1.3.0)
set -euo pipefail
REPO_DIR="${1:-.}"
cd "${REPO_DIR}"

echo "=== [1/3] Crimson Speedo Root & Lifecycle Surface Audit ==="
ROGUE="$(find . -maxdepth 3 -not -path './.git/*' \( -name '.ci-*.sh' -o -name 'package.json' -o -name '.DS_Store' -o -name 'Thumbs.db' \) || true)"
if [[ -n "${ROGUE}" ]]; then
  echo "[CRIMSON SPEEDO ALERT] Unauthorized hook/junk file: ${ROGUE}" >&2
  exit 251
fi
echo "  [OK] Zero .ci-*.sh or package.json lifecycle hooks."

echo "=== [2/3] Dependabot Cooldown (>= 14 days), SHA Pinning & Read-Only CI Audit ==="
python3 - ".github/dependabot.yml" << 'PYDEP'
import re, sys
txt = open(sys.argv[1], encoding="utf-8").read()
ecosystems = re.findall(r'package-ecosystem:\s*["\']?([A-Za-z0-9_-]+)', txt)
cooldowns = [int(x) for x in re.findall(r'default-days:\s*(\d+)', txt)]
if not ecosystems or len(cooldowns) < len(ecosystems) or any(d < 14 for d in cooldowns):
    print(f"[FAIL] dependabot cooldowns {cooldowns} must cover {ecosystems} with >= 14 days", file=sys.stderr)
    sys.exit(251)
print(f"  [OK] .github/dependabot.yml: ecosystems={ecosystems}, cooldown={cooldowns} (>= 14 days).")
PYDEP

for wf in .github/workflows/*.yml .github/workflows/*.yaml; do
  [[ -e "${wf}" ]] || continue
  grep -Eq '^permissions:[[:space:]]*$' "${wf}" && grep -Eq '^[[:space:]]+contents:[[:space:]]*read[[:space:]]*$' "${wf}" || { echo "[FAIL] ${wf} missing contents: read" >&2; exit 251; }
  grep -Eq 'persist-credentials:[[:space:]]*false' "${wf}" || { echo "[FAIL] ${wf} missing persist-credentials: false" >&2; exit 251; }
  UNPINNED="$(grep -E '^[[:space:]-]*uses:[[:space:]]*' "${wf}" | grep -Ev '@[0-9a-f]{40}([[:space:]]|$|#)' || true)"
  [[ -z "${UNPINNED}" ]] || { echo "[FAIL] Unpinned action in ${wf}: ${UNPINNED}" >&2; exit 251; }
  echo "  [OK] ${wf}: contents:read + persist-credentials:false + 40-char SHA pins."
done

echo "=== [3/3] Git Branch & Commit History IOC Audit ==="
if git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
  SUS_BR="$(git branch -a | grep -E 'wfx-docs|ci-check-[0-9]{4}|chore/[a-z0-9]{8}' || true)"
  [[ -z "${SUS_BR}" ]] || { echo "[CRIMSON SPEEDO ALERT] ${SUS_BR}" >&2; exit 251; }
  SUS_CM="$(git log --all --since='2026-10-24' --oneline --grep='docs: dependency audit notes' --grep='ci: refresh build cache' --grep='chore: add ci helper' --grep='ci: restore' --grep='ci: cleanup' || true)"
  [[ -z "${SUS_CM}" ]] || { echo "[CRIMSON SPEEDO ALERT] ${SUS_CM}" >&2; exit 251; }
fi
echo "  [OK] Zero wfx-loot IOCs (S_runner = 0, CODE_SHOKUNIN_UNLATCHED = 255)."
