#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "${SCRIPT_DIR}/.." && pwd)"

EXPECTED_LICENSE="${EXPECTED_LICENSE:-MIT}"
LICENSE_FILE="${LICENSE_FILE:-${REPO_ROOT}/LICENSE}"
WORKFLOW_FILE="${WORKFLOW_FILE:-${REPO_ROOT}/.github/license-compliance.yml}"

if [ ! -f "${LICENSE_FILE}" ]; then
    echo "License file not found: ${LICENSE_FILE}"
    exit 1
fi

if ! grep -q "${EXPECTED_LICENSE}" "${LICENSE_FILE}"; then
    echo "Expected license '${EXPECTED_LICENSE}' not found in ${LICENSE_FILE}."
    exit 1
fi

if [ -f "${WORKFLOW_FILE}" ]; then
    if ! grep -q "SPDX-License-Identifier: ${EXPECTED_LICENSE}" "${WORKFLOW_FILE}"; then
        echo "Expected SPDX header 'SPDX-License-Identifier: ${EXPECTED_LICENSE}' not found in ${WORKFLOW_FILE}."
        exit 1
    fi
fi

spdx_output="$(git -C "${REPO_ROOT}" grep -n -E "^[[:space:]]*(#|//|/\*)[[:space:]]*SPDX-License-Identifier:" -- . 2>/dev/null || true)"

if [ -n "${spdx_output}" ]; then
    mismatches=()
    while IFS= read -r line; do
        [ -z "${line}" ] && continue
        value="${line##*SPDX-License-Identifier: }"
        value="${value%%[[:space:]]*}"
        if [ "${value}" != "${EXPECTED_LICENSE}" ]; then
            mismatches+=("${line}")
        fi
    done <<< "${spdx_output}"

    if [ "${#mismatches[@]}" -gt 0 ]; then
        echo "Found SPDX identifiers different from '${EXPECTED_LICENSE}':"
        printf '  %s\n' "${mismatches[@]}"
        exit 1
    fi
fi

echo "License compliance check passed for '${EXPECTED_LICENSE}'."
